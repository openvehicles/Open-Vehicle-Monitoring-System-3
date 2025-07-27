/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd July 2025
;
;    (C) 2025       Carsten Schmiemann
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"

#include <stdio.h>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"

#include "vehicle_niu_gtevo.h"

const char *OvmsVehicleNiuGTEVO::s_tag = "v-niu-gtevo";

// Wakeup and battery detect
#define can_activity_threshold 15        // Threshold for detected can activity
#define can_activity_timeout_delay 10000 // Time (ms) of inactivity (msgs/s lower than threshold)

// Charger emulation frame modifier vars
int charger_counter = 0x00;
unsigned long charger_counter_timer = 0;

class OvmsVehicleNiuGTEVOInit
{
public:
  OvmsVehicleNiuGTEVOInit();
} MyOvmsVehicleNiuGTEVOInit __attribute__((init_priority(9000)));

OvmsVehicleNiuGTEVOInit::OvmsVehicleNiuGTEVOInit()
{
  ESP_LOGI(TAG, "Registering Vehicle: NIU MQi GT EVO/100 (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleNiuGTEVO>("NEVO", "NIU MQi GT EVO/100");
}

OvmsVehicleNiuGTEVO::OvmsVehicleNiuGTEVO()
{
  ESP_LOGI(TAG, "Start NIU MQi GT EVO/100 vehicle module");

  StandardMetrics.ms_v_type->SetValue("NEVO");

  MyConfig.RegisterParam("xnevo", "NIU MQi GT EVO/100 configuration", true, true);
  ConfigChanged(NULL);

  // Init GT EVO CAN connections
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // NIU MQi GT EVO/100 specific metrics
  mt_bus_awake = MyMetrics.InitBool("xnevo.v.bus.awake", SM_STALE_NONE, false);
  mt_bus_charger_emulation = MyMetrics.InitBool("xnevo.v.bus.charger.emulation", SM_STALE_NONE, false);
  mt_pos_odometer_start = MyMetrics.InitFloat("xnevo.v.pos.odometer.start", SM_STALE_NONE, 0, Kilometers, true);
  mt_batA_detected = MyMetrics.InitBool("xnevo.b.A.detected", SM_STALE_NONE, false);
  mt_batB_detected = MyMetrics.InitBool("xnevo.b.B.detected", SM_STALE_NONE, false);
  mt_batA_voltage = MyMetrics.InitFloat("xnevo.b.A.voltage", SM_STALE_MID, 0, Volts, false);
  mt_batB_voltage = MyMetrics.InitFloat("xnevo.b.B.voltage", SM_STALE_MID, 0, Volts, false);
  mt_batA_current = MyMetrics.InitFloat("xnevo.b.A.current", SM_STALE_MID, 0, Amps, false);
  mt_batB_current = MyMetrics.InitFloat("xnevo.b.B.current", SM_STALE_MID, 0, Amps, false);
  mt_batA_cycles = MyMetrics.InitFloat("xnevo.b.A.cycles", SM_STALE_MID, 0, Other, true);
  mt_batB_cycles = MyMetrics.InitFloat("xnevo.b.B.cycles", SM_STALE_MID, 0, Other, true);
  mt_batA_soc = MyMetrics.InitFloat("xnevo.b.A.soc", SM_STALE_MID, 0, Percentage, false);
  mt_batB_soc = MyMetrics.InitFloat("xnevo.b.B.soc", SM_STALE_MID, 0, Percentage, false);
  mt_batA_firmware = MyMetrics.InitString("xnevo.b.A.firmware", SM_STALE_NONE, "", Other);
  mt_batA_hardware = MyMetrics.InitString("xnevo.b.A.hardware", SM_STALE_NONE, "", Other);
  mt_batB_firmware = MyMetrics.InitString("xnevo.b.B.firmware", SM_STALE_NONE, "", Other);
  mt_batB_hardware = MyMetrics.InitString("xnevo.b.B.hardware", SM_STALE_NONE, "", Other);

  // BMS configuration:
  BmsSetCellLimitsVoltage(2.0, 4.5);
  BmsSetCellLimitsTemperature(-39, 60);
  BmsSetCellDefaultThresholdsVoltage(0.030, 0.050);
  BmsSetCellDefaultThresholdsTemperature(4.0, 5.0);

  // Init commands / menu structure
  OvmsCommand *cmd_xnevo = MyCommandApp.RegisterCommand("xnevo", "NIU MQi GT EVO/100");
  OvmsCommand *charger;

  cmd_xnevo->RegisterCommand("debug", "Debug output of rolling avg cons calc", CommandDebug);
  cmd_xnevo->RegisterCommand("trip", "Reset trip", CommandTripReset);

  charger = cmd_xnevo->RegisterCommand("charger", "Enable or disable charger emulation");
  charger->RegisterCommand("enable", "Enable charger emulation to use 3rd charger", CommandChargerEnable);
  charger->RegisterCommand("disable", "Disable charger emulation to use 3rd charger", CommandChargerDisable);

  // CAN1 Hardware filter
  // Filtering is not neccessary, the max busload is about 600msg/s, but since OVMS support filters, we use it
  // Hardware filter lets 0x18 and 0x19 prefix pass through, filter exact ID ranges via software filter
  /*esp32can_filter_config_t can1_hwflt = {
      .code = {.u32 = 0x18000000U << 3},
      .mask = {.u32 = 0x01FFFFFFU << 3},
      .single_filter = true,
  };*/

  // MyPeripherals->m_esp32can->SetAcceptanceFilter(can1_hwflt);

  // CAN1 Software filter
  MyPollers.AddFilter(1, 0x18f21302, 0x18f21302); // Light control unit (LCU)
  MyPollers.AddFilter(1, 0x18f42004, 0x18f42204); // Vehicle control unit (VCU)
  MyPollers.AddFilter(1, 0x19f0150f, 0x19f12b10); // Charger and Field oriented controller (FOC)
  MyPollers.AddFilter(1, 0x19f20411, 0x19f22511); // Battery A
  MyPollers.AddFilter(1, 0x19f30412, 0x19f32512); // Battery B
  MyPollers.AddFilter(1, 0x19f51414, 0x19f51414); // DCDC converter

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif
}

OvmsVehicleNiuGTEVO::~OvmsVehicleNiuGTEVO()
{
  ESP_LOGI(TAG, "Shutdown NIU MQi GT EVO/100 vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
}

// Reload all or single values from config storage
void OvmsVehicleNiuGTEVO::ConfigChanged(OvmsConfigParam *param)
{
  if (param && param->GetName() != "xnevo")
    return;

  m_range_ideal = MyConfig.GetParamValueInt("xnevo", "rangeideal", 70);
  m_soh_user = MyConfig.GetParamValueInt("xnevo", "soh_user", 100);
  m_battery_ah_user = MyConfig.GetParamValueInt("xnevo", "cap_user", 26);

  ESP_LOGI(TAG, "NIU MQi GT EVO/100 reload configuration: Range ideal: %d, SOH User: %d", m_range_ideal, m_soh_user);
}

// GT EVO wakeup macro, will be called if GT EVO wakes up
void OvmsVehicleNiuGTEVO::EvoWakeup()
{
  ESP_LOGI(TAG, "GT EVO woke up...");
  mt_bus_awake->SetValue(true);
  StandardMetrics.ms_v_env_aux12v->SetValue(true);
}

// GT EVO shutdown macro, will be called if GT EVO goes to sleep, some metrics will be set to zero/off to have a clean state
void OvmsVehicleNiuGTEVO::EvoShutdown()
{
  mt_bus_awake->SetValue(false);
  chargerEmulation = false;
  StandardMetrics.ms_v_env_awake->SetValue(false);
  StandardMetrics.ms_v_pos_speed->SetValue(0);
  StandardMetrics.ms_v_bat_current->SetValue(0);
  StandardMetrics.ms_v_bat_power->SetValue(0);
  StandardMetrics.ms_v_charge_12v_power->SetValue(0);
  StandardMetrics.ms_v_charge_12v_current->SetValue(0);
  StandardMetrics.ms_v_bat_12v_current->SetValue(0);
  StandardMetrics.ms_v_env_aux12v->SetValue(false);
  StandardMetrics.ms_v_env_charging12v->SetValue(false);
  StandardMetrics.ms_v_env_locked->SetValue(true);
  StandardMetrics.ms_v_charge_inprogress->SetValue(false);
  StandardMetrics.ms_v_charge_pilot->SetValue(false);
  StandardMetrics.ms_v_door_chargeport->SetValue(false);
}

void OvmsVehicleNiuGTEVO::Ticker1(uint32_t ticker)
{
  uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;

  if (can_message_counter >= can_activity_threshold)
  {
    // Update last activity timestamp on any message activity
    can_lastActivityTime = currentTime;

    if (!mt_bus_awake->AsBool())
    {
      ESP_LOGI(TAG, "CAN activity detected (Activity: %d msgs/sec)", can_message_counter);
      EvoWakeup();
    }
  }
  else
  {
    // If no activity, check inactivity timeout
    if (mt_bus_awake->AsBool() && ((currentTime - can_lastActivityTime) > 3000))
    {
      ESP_LOGI(TAG, "CAN activity timeout, GT EVO shutdown (Last Activity: %d msgs/sec)", can_message_counter);
      EvoShutdown();
    }
  }
  can_message_counter = 0; // Reset counter for the next second

  // Battery present detection
  if (mt_bus_awake->AsBool())
  {
    if ((currentTime - batteryA_lastActivityTime) > can_activity_timeout_delay)
    {
      BatteryAconnected = false;
    }

    if ((currentTime - batteryB_lastActivityTime) > can_activity_timeout_delay)
    {
      BatteryBconnected = false;
    }
  }

  // Ignition and charger frame detection
  if ((currentTime - ignition_lastActivityTime) > can_activity_timeout_delay)
  {
    ignitionDetected = false;
  }

  if (((currentTime - charger_lastActivityTime) > can_activity_timeout_delay) && !chargerEmulation)
  {
    chargerDetected = false;
  }

  // Ignition metrics
  if (!chargerDetected && ignitionDetected && !StandardMetrics.ms_v_env_awake->AsBool())
  {
    StandardMetrics.ms_v_env_awake->SetValue(true);
    StandardMetrics.ms_v_env_locked->SetValue(false);
    ESP_LOGI(TAG, "GT EVO is ready to drive");
  }
  else if (!chargerDetected && !ignitionDetected && StandardMetrics.ms_v_env_awake->AsBool())
  {
    StandardMetrics.ms_v_env_awake->SetValue(false);
    ESP_LOGI(TAG, "GT EVO drive mode turned off");
  }

  // DC-DC logic
  if (mt_bus_awake->AsBool() && StandardMetrics.ms_v_charge_12v_current->AsFloat() > 0 && !StandardMetrics.ms_v_env_charging12v->AsBool())
  {
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
  }
  else if ((!mt_bus_awake->AsBool() || StandardMetrics.ms_v_charge_12v_current->AsFloat() <= 0) && StandardMetrics.ms_v_env_charging12v->AsBool())
  {
    StandardMetrics.ms_v_env_charging12v->SetValue(false);
  }

  if (mt_bus_awake->AsBool())
  {
    // Sync CAN values to OVMS metrics
    SyncMetrics();
  }
}

void OvmsVehicleNiuGTEVO::Ticker10(uint32_t ticker)
{
  // SOC calc/display and BMS Layout depending on batteries connected, if we have only a single battery connected to port A or B,
  // use its SOC and BMS arrangement, if both batteries connected use a average SOC over both
  if (mt_bus_awake->AsBool())
  {
    bool batA_now = BatteryAconnected;
    bool batB_now = BatteryBconnected;
    bool batA_prev = mt_batA_detected->AsBool();
    bool batB_prev = mt_batB_detected->AsBool();

    // Detect and handle battery disconnection
    if (!batA_now && batA_prev)
    {
      mt_batA_detected->SetValue(false);
      BmsSetCellArrangementVoltage(20, 1);
      BmsSetCellArrangementTemperature(5, 1);
      ESP_LOGW(TAG, "Battery A disconnected");
      MyNotify.NotifyStringf("alert", "batt.A.disconnect", "Battery A disconnected");
    }

    if (!batB_now && batB_prev)
    {
      mt_batB_detected->SetValue(false);
      BmsSetCellArrangementVoltage(20, 1);
      BmsSetCellArrangementTemperature(5, 1);
      ESP_LOGW(TAG, "Battery B disconnected");
      MyNotify.NotifyStringf("alert", "batt.B.disconnect", "Battery B disconnected");
    }

    // Handle connection status
    if ((batA_now && !batB_now) || (!batA_now && batB_now))
    {
      if (batA_now && !batA_prev)
      {
        mt_batA_detected->SetValue(true);
        BmsSetCellArrangementVoltage(20, 1);
        BmsSetCellArrangementTemperature(5, 1);
        ESP_LOGI(TAG, "Battery A connected");
        MyNotify.NotifyStringf("info", "batt.A.connect", "Battery A connected");
      }
      else if (batB_now && !batB_prev)
      {
        mt_batB_detected->SetValue(true);
        BmsSetCellArrangementVoltage(20, 1);
        BmsSetCellArrangementTemperature(5, 1);
        ESP_LOGI(TAG, "Battery B connected");
        MyNotify.NotifyStringf("info", "batt.B.connect", "Battery B connected");
      }
    }
    else if (batA_now && batB_now)
    {
      if (!batA_prev || !batB_prev)
      {
        mt_batA_detected->SetValue(true);
        mt_batB_detected->SetValue(true);
        BmsSetCellArrangementVoltage(40, 2);
        BmsSetCellArrangementTemperature(10, 2);
        ESP_LOGI(TAG, "Both batteries connected");
      }
    }
    else
    {
      // No batteries connected
      StandardMetrics.ms_v_bat_soc->SetValue(0, Percentage);
      StandardMetrics.ms_v_bat_voltage->SetValue(0, Volts);
      StandardMetrics.ms_v_bat_current->SetValue(0, Amps);
      StandardMetrics.ms_v_bat_power->SetValue(0, Watts);
      StandardMetrics.ms_v_bat_cac->SetValue(0, AmpHours);
      StandardMetrics.ms_v_bat_capacity->SetValue(0, WattHours);
      // StandardMetrics.ms_v_bat_soh->SetValue(0);
      ESP_LOGE(TAG, "No battery connected");
    }

    // Define the minimum safe state of charge
    const float MIN_SAFE_SOC_PERCENT = 15.0f;

    float current_soc_percent = StandardMetrics.ms_v_bat_soc->AsFloat();
    float available_kwh = StandardMetrics.ms_v_bat_capacity->AsFloat(0);

    // Calculate the usable energy (from current SoC down to the minimum safe SoC)
    float usable_kwh = 0.0f;
    if (current_soc_percent > MIN_SAFE_SOC_PERCENT)
    {
      // Calculate the total capacity at the current state of health
      // This is derived from the available energy and current SoC
      float total_capacity_kwh = available_kwh / (current_soc_percent / 100.0f);

      float usable_soc_percent = current_soc_percent - MIN_SAFE_SOC_PERCENT;
      usable_kwh = total_capacity_kwh * (usable_soc_percent / 100.0f);
    }

    float usable_soc_for_ideal_range = 0.0f;
    if (current_soc_percent > MIN_SAFE_SOC_PERCENT)
    {
      usable_soc_for_ideal_range = (current_soc_percent - MIN_SAFE_SOC_PERCENT) / 100.0f;
    }
    StandardMetrics.ms_v_bat_range_ideal->SetValue(m_range_ideal * usable_soc_for_ideal_range);

    // theoretical maximum range on a full charge with the battery's current state of health.
    StandardMetrics.ms_v_bat_range_full->SetValue((m_range_ideal * (StandardMetrics.ms_v_bat_soh->AsFloat(100) * 0.01)), Kilometers);

    // Estimated Range: Based on the calculated usable energy.
    if (StandardMetrics.ms_v_bat_consumption->AsFloat(0, kWhP100K) != 0)
    {
      // Calculate estimated range with consumption history and usable battery capacity
      float consumption = StandardMetrics.ms_v_bat_consumption->AsFloat(1, kWhP100K);
      float estimated_range = (consumption > 0) ? ((usable_kwh / consumption) * 100) : 0;
      StandardMetrics.ms_v_bat_range_est->SetValue(estimated_range, Kilometers);

      ESP_LOGD(TAG, "Usable bat capacity to 15%%: %.1f kWh, Avg bat cons: %.2f kWh/100km, Range est calculated: %.1f km", usable_kwh, consumption, estimated_range);
    }
    else
    {
      // Use factory power consumption if we have no history, also with usable energy.
      StandardMetrics.ms_v_bat_range_est->SetValue((usable_kwh * 1000 / 45), Kilometers);
    }
    // Energy statistics calculation
    EnergyStatistics();

    // Charger message detection, state logic and energy calc
    // The original charger sends CAN messages to enable BMS and close mosfets, we can detect it or fake it
    if (chargerDetected && !StandardMetrics.ms_v_charge_pilot->AsBool())
    {
      StandardMetrics.ms_v_env_awake->SetValue(false);

      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      StandardMetrics.ms_v_door_chargeport->SetValue(true);
      StandardMetrics.ms_v_charge_state->SetValue("prepare");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");

      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      ESP_LOGI(TAG, "Charger connected");
    }
    else if (!chargerDetected && StandardMetrics.ms_v_charge_pilot->AsBool())
    {
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
      StandardMetrics.ms_v_door_chargeport->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");

      StandardMetrics.ms_v_charge_current->SetValue(0);
      StandardMetrics.ms_v_charge_voltage->SetValue(0);
      StandardMetrics.ms_v_charge_power->SetValue(0);

      StandardMetrics.ms_v_charge_duration_soc->SetValue(0);
      StandardMetrics.ms_v_charge_limit_soc->SetValue(0);

      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_kwh->SetValue(0);
      StandardMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
      ESP_LOGI(TAG, "Charger disconnected");
    }

    // Charger logic
    if (chargerDetected && StandardMetrics.ms_v_charge_pilot->AsBool() && StandardMetrics.ms_v_bat_current->AsFloat() < 0)
    {
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      ChargeStatistics();
    }
    else if (chargerDetected && StandardMetrics.ms_v_charge_pilot->AsBool() && StandardMetrics.ms_v_charge_inprogress->AsBool() && StandardMetrics.ms_v_bat_current->AsFloat() >= 0)
    {
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");

      if (chargerEmulation)
      {
        chargerEmulation = false;
      }
    }
  }
  // Reflect charger emulation status through metrics
  mt_bus_charger_emulation->SetValue(chargerEmulation);
  // We dont have a nice SoH from BMS, it will calculate it only through discharging till runout, which is not very convenient, so let user decide which SoH batteries has
  StandardMetrics.ms_v_bat_soh->SetValue(m_soh_user);
}

void OvmsVehicleNiuGTEVO::SyncMetrics()
{
  bool A = BatteryAconnected;
  bool B = BatteryBconnected;
  float bat_soh = StandardMetrics.ms_v_bat_soh->AsFloat(100);

  // Helper function to copy battery values from CAN to OVMS metrics
  auto set_cell_data = [this](int volt_offset, int temp_offset, float *voltages, float *temps, int count_v, int count_t)
  {
    for (int i = 0; i < count_v; ++i)
      BmsSetCellVoltage(volt_offset + i, voltages[i]);
    for (int i = 0; i < count_t; ++i)
      BmsSetCellTemperature(temp_offset + i, temps[i]);
  };

  // Configure the metrics based on whether one or two batteries are being used
  if (A && !B)
  {
    set_cell_data(0, 0, batteryA_cell_voltage, batteryA_cell_temperature, 20, 5);

    StandardMetrics.ms_v_bat_voltage->SetValue(batteryA_voltage, Volts);
    StandardMetrics.ms_v_bat_soc->SetValue(batteryA_soc, Percentage);
    StandardMetrics.ms_v_bat_current->SetValue(-batteryA_current, Amps);
    StandardMetrics.ms_v_bat_power->SetValue(-batteryA_power, Watts);
    StandardMetrics.ms_v_bat_cac->SetValue((batteryA_soc / 100.0f) * (bat_soh / 100.0f) * m_battery_ah_user, AmpHours);
    StandardMetrics.ms_v_bat_capacity->SetValue((batteryA_soc / 100.0f) * (bat_soh / 100.0f) * (m_battery_ah_user * 72.0f), WattHours);
    StandardMetrics.ms_v_charge_temp->SetValue(batteryA_cell_temperature[7]);
    m_battery_capacity = (bat_soh / 100.0f) * (m_battery_ah_user * 72.0f);
  }
  else if (B && !A)
  {
    set_cell_data(0, 0, batteryB_cell_voltage, batteryB_cell_temperature, 20, 5);

    StandardMetrics.ms_v_bat_voltage->SetValue(batteryB_voltage, Volts);
    StandardMetrics.ms_v_bat_soc->SetValue(batteryB_soc, Percentage);
    StandardMetrics.ms_v_bat_current->SetValue(-batteryB_current, Amps);
    StandardMetrics.ms_v_bat_power->SetValue(-batteryB_power, Watts);
    StandardMetrics.ms_v_bat_cac->SetValue((batteryB_soc / 100.0f) * (bat_soh / 100.0f) * m_battery_ah_user, AmpHours);
    StandardMetrics.ms_v_bat_capacity->SetValue((batteryB_soc / 100.0f) * (bat_soh / 100.0f) * (m_battery_ah_user * 72.0f), WattHours);
    StandardMetrics.ms_v_charge_temp->SetValue(batteryB_cell_temperature[7]);
    m_battery_capacity = (bat_soh / 100.0f) * (m_battery_ah_user * 72.0f);
  }
  else if (A && B)
  {
    set_cell_data(0, 0, batteryA_cell_voltage, batteryA_cell_temperature, 20, 5);
    set_cell_data(20, 5, batteryB_cell_voltage, batteryB_cell_temperature, 20, 5);

    float avg_voltage = (batteryA_voltage + batteryB_voltage) / 2.0f;
    float avg_soc = (batteryA_soc + batteryB_soc) / 2.0f;
    float total_current = batteryA_current + batteryB_current;
    float total_power = batteryA_power + batteryB_power;

    StandardMetrics.ms_v_bat_voltage->SetValue(avg_voltage, Volts);
    StandardMetrics.ms_v_bat_soc->SetValue(avg_soc, Percentage);
    StandardMetrics.ms_v_bat_current->SetValue(-total_current, Amps);
    StandardMetrics.ms_v_bat_power->SetValue(-total_power, Watts);
    StandardMetrics.ms_v_bat_cac->SetValue((avg_soc / 100.0f) * (bat_soh / 100.0f) * (m_battery_ah_user * 2), AmpHours);
    StandardMetrics.ms_v_bat_capacity->SetValue((avg_soc / 100.0f) * (bat_soh / 100.0f) * ((m_battery_ah_user * 2) * 72.0f), WattHours);
    m_battery_capacity = (bat_soh / 100.0f) * ((m_battery_ah_user * 2) * 72.0f);

    if (batteryA_cell_temperature[7] > batteryB_cell_temperature[7])
    {
      StandardMetrics.ms_v_charge_temp->SetValue(batteryA_cell_temperature[7]);
    }
    else
    {
      StandardMetrics.ms_v_charge_temp->SetValue(batteryB_cell_temperature[7]);
    }
  }
  else
  {
    // No battery present: clear main metrics
    StandardMetrics.ms_v_bat_soc->SetValue(0, Percentage);
    StandardMetrics.ms_v_bat_voltage->SetValue(0, Volts);
    StandardMetrics.ms_v_bat_current->SetValue(0, Amps);
    StandardMetrics.ms_v_bat_power->SetValue(0, Watts);
  }

  // Update the custom battery-related metrics to reflect their current state
  mt_batA_voltage->SetValue(batteryA_voltage, Volts);
  mt_batB_voltage->SetValue(batteryB_voltage, Volts);
  mt_batA_current->SetValue(batteryA_current, Amps);
  mt_batB_current->SetValue(batteryB_current, Amps);
  mt_batA_cycles->SetValue(batteryA_soc_cycles, Other);
  mt_batB_cycles->SetValue(batteryB_soc_cycles, Other);
  mt_batA_soc->SetValue(batteryA_soc, Percentage);
  mt_batB_soc->SetValue(batteryB_soc, Percentage);
  mt_batA_firmware->SetValue(batteryA_firmware_version);
  mt_batA_hardware->SetValue(batteryA_hardware_version);
  mt_batB_firmware->SetValue(batteryB_firmware_version);
  mt_batB_hardware->SetValue(batteryB_hardware_version);

  // Sync CAN metrics to OVMS metrics
  StandardMetrics.ms_v_bat_temp->SetValue(StandardMetrics.ms_v_bat_pack_tavg->AsFloat());
  StandardMetrics.ms_v_pos_speed->SetValue(can_vcu_speed);
  StandardMetrics.ms_v_charge_12v_voltage->SetValue(can_dcdc_12v_voltage, Volts);
  StandardMetrics.ms_v_charge_12v_current->SetValue(can_dcdc_12v_current, Amps);
  StandardMetrics.ms_v_charge_12v_temp->SetValue(can_dcdc_12v_temperature, Celcius);
  StandardMetrics.ms_v_bat_12v_current->SetValue(can_dcdc_12v_current, Amps);
  StandardMetrics.ms_v_env_on->SetValue(can_vcu_sw_ready);
  StandardMetrics.ms_v_pos_odometer->SetValue(can_vcu_odometer, Kilometers);
  StandardMetrics.ms_v_env_headlights->SetValue(can_lcu_lowbeam);
  StandardMetrics.ms_v_door_trunk->SetValue(can_vcu_sw_seat);
  StandardMetrics.ms_v_env_handbrake->SetValue(can_vcu_sw_sidestand);
  StandardMetrics.ms_v_env_gear->SetValue(can_foc_gear);
  StandardMetrics.ms_v_inv_temp->SetValue(can_foc_temperature);
  StandardMetrics.ms_v_mot_temp->SetValue(can_foc_motor_temperature);

  // Trip metric initial reset
  if (!m_first_boot && StandardMetrics.ms_v_pos_trip->AsFloat(0, Kilometers) == 0 && StandardMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers) != 0)
  {
    mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers), Kilometers);
    m_first_boot = true;
    ESP_LOGI(TAG, "Initial trip reset");
  }
  else
  {
    StandardMetrics.ms_v_pos_trip->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers) - mt_pos_odometer_start->AsFloat(0, Kilometers));
  }

  // DC-DC power calculation
  StandardMetrics.ms_v_charge_12v_power->SetValue(StandardMetrics.ms_v_charge_12v_voltage->AsFloat(0) * StandardMetrics.ms_v_charge_12v_current->AsFloat(0));

  // While driving, battery power is equal to inverter power; the small loss from the DC-DC converter is negligible
  if (can_vcu_speed > 0)
  {
    StandardMetrics.ms_v_inv_power->SetValue(StandardMetrics.ms_v_bat_power->AsFloat(0, Watts), Watts);
  }
  else
  {
    StandardMetrics.ms_v_inv_power->SetValue(0, Watts);
  }
}

// Average rolling consumption over last 25km
void OvmsVehicleNiuGTEVO::CalculateEfficiency()
{
  if (!mt_bus_awake->AsBool())
  {
    return;
  }

  float speed = StandardMetrics.ms_v_pos_speed->AsFloat();
  float power = StandardMetrics.ms_v_bat_power->AsFloat(0, Watts);
  float distanceTraveled = speed / 3600.0f; // km/h to km/s

  float consumption = 0;
  // Only calculate consumption at reasonable speeds to avoid division by zero
  // and artificially high values at near-standstill.
  if (speed >= 10.0f)
  {
    consumption = power / speed; // Results in Wh/km
  }

  // Only add valid, finite data points to the history
  if (distanceTraveled > 0 && std::isfinite(consumption))
  {
    // A data point is a pair of (distance_in_km, consumption_in_Wh/km)
    consumptionHistory.push_back(std::make_pair(distanceTraveled, consumption));
    totalDistance += distanceTraveled;
    // totalConsumption is a running sum of energy in Wh (distance * Wh/km)
    totalConsumption += distanceTraveled * consumption;
  }

  // --- Proportional Trimming Loop ---
  // Trim the history to maintain a smooth 25km window
  while (totalDistance > 25.0f && !consumptionHistory.empty())
  {
    // Use a reference to the oldest data point to allow modification
    auto &oldest = consumptionHistory.front();
    float excessDistance = totalDistance - 25.0f;

    if (excessDistance >= oldest.first)
    {
      // The entire oldest data point is outside the 25km window, so remove it completely.
      totalDistance -= oldest.first;
      totalConsumption -= oldest.first * oldest.second;
      consumptionHistory.pop_front();
    }
    else
    {
      totalConsumption -= excessDistance * oldest.second;
      totalDistance -= excessDistance;
      oldest.first -= excessDistance;
      break;
    }
  }

  // Safety checks to prevent corrupt data
  if (totalDistance < 0 || !std::isfinite(totalDistance))
    totalDistance = 0;
  if (totalConsumption < 0 || !std::isfinite(totalConsumption))
    totalConsumption = 0;

  // Calculate and set the final average consumption
  float avgConsumption = (totalDistance > 0) ? totalConsumption / totalDistance : 0;
  StandardMetrics.ms_v_bat_consumption->SetValue(TRUNCPREC(avgConsumption, 1)); // Wh/km

  // Logging
  ESP_LOGD(TAG, "Average battery consumption: %.1f Wh/km (over %.2f km, buffer size: %zu)",
           avgConsumption, totalDistance, consumptionHistory.size());

  // Hard reset if buffer grows out of control
  if (consumptionHistory.size() > 5000)
  {
    ESP_LOGW(TAG, "Consumption history too large (%.1f km stored, %zu entries) — resetting!",
             totalDistance, consumptionHistory.size());
    consumptionHistory.clear();
    totalDistance = 0;
    totalConsumption = 0;
  }
}

void OvmsVehicleNiuGTEVO::ChargeStatistics()
{
  float charge_current = fabs(StandardMetrics.ms_v_bat_current->AsFloat(0, Amps));
  float charge_voltage = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float battery_power = fabs(StandardMetrics.ms_v_bat_power->AsFloat(0, kW));

  int minsremaining = 0;

  // If the scooter is charging, populate the charging metrics and estimate the remaining time
  if (charge_voltage > 0 && charge_current > 0.1)
  {
    float power = charge_voltage * charge_current / 1000.0;
    float energy = power / 3600.0 * 10.0;

    StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat() + energy);

    minsremaining = calcMinutesRemaining(charge_voltage, charge_current);
    StandardMetrics.ms_v_charge_duration_full->SetValue(minsremaining, Minutes);

    StandardMetrics.ms_v_charge_power->SetValue(battery_power);
    StandardMetrics.ms_v_charge_current->SetValue(charge_current);
    StandardMetrics.ms_v_charge_voltage->SetValue(charge_voltage);
    ESP_LOGD(TAG, "Charge time remaining: %d mins, Charging power at %.2f kW", minsremaining, battery_power);
  }
}

void OvmsVehicleNiuGTEVO::EnergyStatistics()
{
  float power = StandardMetrics.ms_v_bat_power->AsFloat();

  if (power != 0.0 && StandardMetrics.ms_v_env_on->AsBool())
  {
    float energy = power / 3600.0 * 10.0;
    if (energy > 0.0f)
    {
      StandardMetrics.ms_v_bat_energy_used->SetValue(StandardMetrics.ms_v_bat_energy_used->AsFloat() + energy);
      StandardMetrics.ms_v_bat_energy_used_total->SetValue(StandardMetrics.ms_v_bat_energy_used_total->AsFloat() + energy);
    }
    else
    {
      StandardMetrics.ms_v_bat_energy_recd->SetValue(StandardMetrics.ms_v_bat_energy_recd->AsFloat() - energy);
      StandardMetrics.ms_v_bat_energy_recd_total->SetValue(StandardMetrics.ms_v_bat_energy_recd_total->AsFloat() - energy);
    }
  }
}

// Charge time remaining - helper function to get remaining charge time for ChargeStatistics
int OvmsVehicleNiuGTEVO::calcMinutesRemaining(float charge_voltage, float charge_current)
{
  float current_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
  if (current_soc >= 100)
  {
    StandardMetrics.ms_v_charge_duration_soc->SetValue(0);
    return 0; // Already full
  }

  StandardMetrics.ms_v_charge_limit_soc->SetValue(m_final_charge_phase_soc);

  float charge_power_wh = charge_voltage * charge_current;

  // Get total capacity from SOH (more stable than using remaining Wh)
  float total_wh = m_battery_capacity * (StandardMetrics.ms_v_bat_soh->AsFloat(100) / 100.0f);

  float remaining_mins = 0;

  if (current_soc < m_final_charge_phase_soc)
  {
    // --- STAGE 1: Constant Current (CC) Phase ---
    // Calculate time to reach the taper threshold (85%), the taper threshold is moving a bit, because BMS is not very precise
    float wh_to_taper = total_wh * ((m_final_charge_phase_soc - current_soc) / 100.0f);
    float hours_to_taper = wh_to_taper / charge_power_wh;
    float mins_to_taper = hours_to_taper * 60.0f;

    // Set the metric for time to the taper threshold ONLY
    StandardMetrics.ms_v_charge_duration_soc->SetValue(mins_to_taper);

    // Total remaining time is time to taper + the fixed time for the final phase
    remaining_mins = mins_to_taper + m_final_charge_phase_minutes;
  }
  else
  {
    // --- STAGE 2: Constant Voltage (CV) / Taper Phase ---
    // We are past the taper threshold, so time to taper is 0.
    StandardMetrics.ms_v_charge_duration_soc->SetValue(0);

    // Calculate remaining time proportionally.
    // The final 15% of SoC (from 85 to 100) will take m_final_charge_phase_minutes.
    float soc_into_final_phase = current_soc - m_final_charge_phase_soc;
    float total_final_phase_soc = 100.0f - m_final_charge_phase_soc;

    if (total_final_phase_soc > 0)
    {
      float proportion_remaining = 1.0f - (soc_into_final_phase / total_final_phase_soc);
      remaining_mins = m_final_charge_phase_minutes * proportion_remaining;
    }
  }

  return static_cast<int>(remaining_mins);
}

// Handles incoming CAN-frames on bus 1
void OvmsVehicleNiuGTEVO::IncomingFrameCan1(CAN_frame_t *p_frame)
{
  uint8_t *data = p_frame->data.u8;
  float temp = 0;
  uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;

  can_message_counter++;

  switch (p_frame->MsgID)
  {
  // Scooter specific CAN frames
  case 0x18f42204:
  { // Vehicle control unit (VCU) Speed
    can_vcu_speed = data[0];
    break;
  }

  case 0x19f11510:
  { // Field oriented controller (FOC) detect frame
    can_foc_gear = data[3];

    ignition_lastActivityTime = currentTime;
    ignitionDetected = true;
    break;
  }

  case 0x19f0150f:
  { // Charger present detect frame
    charger_lastActivityTime = currentTime;
    if (!chargerEmulation)
    {
      chargerDetected = true;
    }
    break;
  }

  case 0x18f21302:
  { // Light control unit (LCU) status
    can_lcu_autolight = CAN_BIT(3, 7);
    can_lcu_drl = CAN_BIT(3, 4);
    can_lcu_lowbeam = CAN_BIT(3, 5);
    break;
  }

  case 0x18f42004:
  { // VCU odometer frame
    uint32_t raw_odometer_value = ((uint32_t)data[3] << 24) |
                                  ((uint32_t)data[2] << 16) |
                                  ((uint32_t)data[1] << 8) |
                                  ((uint32_t)data[0]);
    can_vcu_odometer = raw_odometer_value / 1000.0f;
    break;
  }

  case 0x18f42104:
  { // VCU switch frame
    can_vcu_sw_ready = CAN_BIT(0, 1);
    can_vcu_sw_sidestand = CAN_BIT(5, 4);
    can_vcu_sw_seat = CAN_BIT(6, 6);
    break;
  }

  case 0x19f51414:
  { // DCDC status frame
    can_dcdc_12v_voltage = (float)(int16_t)(((uint16_t)data[1] << 8) | data[0]) * 0.1f;
    can_dcdc_12v_current = (float)(int16_t)(((uint16_t)data[3] << 8) | data[2]) * 0.1f;
    can_dcdc_12v_temperature = (float)(int16_t)(((uint16_t)data[5] << 8) | data[4]);
    // ESP_LOGD(TAG, "DCDC Voltage: %2.2f Current: %3.2f Temperature: %4.0f", can_dcdc_12v_voltage, can_dcdc_12v_current, can_dcdc_12v_temperature);
    break;
  }

  case 0x19f12b10:
  { // FOC temperature frame
    can_foc_temperature = (float)(int16_t)(((uint16_t)data[1] << 8) | data[0]) * 0.01f;
    can_foc_motor_temperature = (float)(int16_t)(((uint16_t)data[3] << 8) | data[2]) * 0.01f;
    break;
  }

  case 0x19f31b12:
  { // VCU charging request frame, we answer with charger emulation frames to use a 3rd party (fast) charger
    // The "CRC" is matched in about 1-3 seconds, so we dont need to reverse the full algorithm, just count :-)
    if (chargerEmulation)
    {
      chargerDetected = true;
      m_can1->WriteExtended(0x19f0140f, 8, charger_f1);
      // Modify CAN charger frame 1
      charger_f1[0] = charger_counter;

      m_can1->WriteExtended(0x19f0150f, 8, charger_f2);
      m_can1->WriteExtended(0x19f0160f, 8, charger_f3);
      m_can1->WriteExtended(0x19f0170f, 8, charger_f4);
      m_can1->WriteExtended(0x19f0180f, 8, charger_f5);

      // CAN charger frame 1 modifier
      if (currentTime - charger_counter_timer >= 1000)
      {
        if (charger_counter == 0xFF)
        {
          charger_counter = 0x00;
        }
        charger_counter += 0x01;
        charger_counter_timer = currentTime;
      }
    }
    break;
  }

  // Batteries related CAN frames
  case 0x19f21a11:
  { // Battery voltage and soc
    temp = ((data[1] << 8) + (data[0])) * 0.01;
    if (temp >= 0 && temp <= 100)
    {
      batteryA_voltage = temp;
      batteryA_lastActivityTime = currentTime;
      BatteryAconnected = true;
    }
    int16_t tempRaw = ((data[3] << 8) + (data[2]));
    temp = tempRaw * 0.01;
    if (temp >= -99 && temp <= 99)
    {
      batteryA_current = temp;
      batteryA_power = batteryA_voltage * temp;
    }
    else
    {
      batteryA_current = 0;
      batteryA_power = 0;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.1;
    if (temp >= 0 && temp <= 100)
    {
      batteryA_soc = temp;
    }
    // ESP_LOGD(TAG, "A RCVD Voltage: %2.2f Current: %3.2f Power: %4.0f SOC: %2.0f", batteryA_voltage, batteryA_current, batteryA_power, batteryA_soc);
    break;
  }
  case 0x19f31a12:
  { // Battery voltage and soc
    temp = ((data[1] << 8) + (data[0])) * 0.01;
    if (temp >= 0 && temp <= 100)
    {
      batteryB_voltage = temp;
      batteryB_lastActivityTime = currentTime;
      BatteryBconnected = true;
    }
    int16_t tempRaw = ((data[3] << 8) + (data[2]));
    temp = tempRaw * 0.01;
    if (temp >= -99 && temp <= 99)
    {
      batteryB_current = temp;
      batteryB_power = batteryB_voltage * temp;
    }
    else
    {
      batteryB_current = 0;
      batteryB_power = 0;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.1;
    if (temp >= 0 && temp <= 100)
    {
      batteryB_soc = temp;
    }
    // ESP_LOGD(TAG, "B RCVD Voltage: %2.2f Current: %3.2f Power: %4.0f SOC: %2.0f", batteryB_voltage, batteryB_current, batteryB_power, batteryB_soc);
    break;
  }
  case 0x19f20811:
  { // Battery hardware
    for (int i = 0; i < 8; i++)
    {
      batteryA_hardware_version[i] = data[i];
    }
    // ESP_LOGD(TAG, "A RCVD hardware ver: %s", batteryA_hardware_version);
    break;
  }
  case 0x19f30812:
  { // Battery hardware
    for (int i = 0; i < 8; i++)
    {
      batteryB_hardware_version[i] = data[i];
    }
    // ESP_LOGD(TAG, "B RCVD hardware ver: %s", batteryB_hardware_version);
    break;
  }
  case 0x19f20411:
  { // Battery firmware
    for (int i = 0; i < 8; i++)
    {
      batteryA_firmware_version[i] = data[i];
    }
    // ESP_LOGD(TAG, "A RCVD firmware ver: %s", batteryA_firmware_version);
    break;
  }
  case 0x19f30412:
  { // Battery firmware
    for (int i = 0; i < 8; i++)
    {
      batteryB_firmware_version[i] = data[i];
    }
    // ESP_LOGD(TAG, "B RCVD firmware ver: %s", batteryB_firmware_version);
    break;
  }
  case 0x19f21711:
  { // Battery type, cycles
    // battery_type = ((data[1]<<8) + (data[0]));
    temp = ((data[7] << 8) + (data[6]));
    if (temp >= 0 && temp <= 2000)
    {
      batteryA_soc_cycles = temp;
    }
    // ESP_LOGD(TAG, "A RCVD SOC cycles: %1.0f", batteryA_soc_cycles);
    break;
  }
  case 0x19f31712:
  { // Battery type, cycles
    // battery_type = ((data[1]<<8) + (data[0]));
    temp = ((data[7] << 8) + (data[6]));
    if (temp >= 0 && temp <= 2000)
    {
      batteryB_soc_cycles = temp;
    }
    // ESP_LOGD(TAG, "B RCVD SOC cycles: %1.0f", batteryB_soc_cycles);
    break;
  }
  case 0x19f21e11:
  { // cell1-4
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[0] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[1] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[2] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[3] = temp;
    }
    // ESP_LOGD(TAG, "A RCVD cell voltages 01: %1.3f 02: %1.3f 03: %1.3f 04: %1.3f", batteryA_cell_voltage[0], batteryA_cell_voltage[1], batteryA_cell_voltage[2], batteryA_cell_voltage[3]);
    break;
  }
  case 0x19f21f11:
  { // cell5-8
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[4] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[5] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[6] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[7] = temp;
    }
    // ESP_LOGD(TAG, "A RCVD cell voltages 05: %1.3f 06: %1.3f 07: %1.3f 08: %1.3f", batteryA_cell_voltage[4], batteryA_cell_voltage[5], batteryA_cell_voltage[6], batteryA_cell_voltage[7]);
    break;
  }
  case 0x19f22011:
  { // cell9-12
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[8] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[9] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[10] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[11] = temp;
    }
    // ESP_LOGD(TAG, "A RCVD cell voltages 09: %1.3f 10: %1.3f 11: %1.3f 12: %1.3f", batteryA_cell_voltage[8], batteryA_cell_voltage[9], batteryA_cell_voltage[10], batteryA_cell_voltage[11]);
    break;
  }
  case 0x19f22111:
  { // cell13-16
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[12] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[13] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[14] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[15] = temp;
    }
    // ESP_LOGD(TAG, "A RCVD cell voltages 13: %1.3f 14: %1.3f 15: %1.3f 16: %1.3f", batteryA_cell_voltage[12], batteryA_cell_voltage[13], batteryA_cell_voltage[14], batteryA_cell_voltage[15]);
    break;
  }
  case 0x19f22211:
  { // cell17-20
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[16] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[17] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[18] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryA_cell_voltage[19] = temp;
    }
    // ESP_LOGD(TAG, "A RCVD cell voltages 17: %1.3f 18: %1.3f 19: %1.3f 20: %1.3f", batteryA_cell_voltage[16], batteryA_cell_voltage[17], batteryA_cell_voltage[18], batteryA_cell_voltage[19]);
    break;
  }
  case 0x19f22411:
  { // temp1-4
    temp = ((data[1] << 8) + (data[0])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryA_cell_temperature[0] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryA_cell_temperature[1] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryA_cell_temperature[2] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryA_cell_temperature[3] = temp;
    }
    // ESP_LOGD(TAG, "A RCVD cell temperature 01: %2.0f 02: %2.0f 03: %2.0f 04: %2.0f", batteryA_cell_temperature[0], batteryA_cell_temperature[1], batteryA_cell_temperature[2], batteryA_cell_temperature[3]);
    break;
  }
  case 0x19f22511:
  { // temp5-8
    temp = ((data[1] << 8) + (data[0])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryA_cell_temperature[4] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryA_cell_temperature[5] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryA_cell_temperature[6] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryA_cell_temperature[7] = temp;
    }
    // ESP_LOGD(TAG, "A RCVD cell temperature 05: %2.0f 06: %2.0f 07: %2.0f 08: %2.0f", batteryA_cell_temperature[4], batteryA_cell_temperature[5], batteryA_cell_temperature[6], batteryA_cell_temperature[7]);
    break;
  }
  case 0x19f31e12:
  { // cell1-4
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[0] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[1] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[2] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[3] = temp;
    }
    // ESP_LOGD(TAG, "B RCVD cell voltages 01: %1.3f 02: %1.3f 03: %1.3f 04: %1.3f", batteryB_cell_voltage[0], batteryB_cell_voltage[1], batteryB_cell_voltage[2], batteryB_cell_voltage[3]);
    break;
  }
  case 0x19f31f12:
  { // cell5-8
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[4] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[5] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[6] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[7] = temp;
    }
    // ESP_LOGD(TAG, "B RCVD cell voltages 05: %1.3f 06: %1.3f 07: %1.3f 08: %1.3f", batteryB_cell_voltage[4], batteryB_cell_voltage[5], batteryB_cell_voltage[6], batteryB_cell_voltage[7]);
    break;
  }
  case 0x19f32012:
  { // cell9-12
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[8] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[9] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[10] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[11] = temp;
    }
    // ESP_LOGD(TAG, "B RCVD cell voltages 09: %1.3f 10: %1.3f 11: %1.3f 12: %1.3f", batteryB_cell_voltage[8], batteryB_cell_voltage[9], batteryB_cell_voltage[10], batteryB_cell_voltage[11]);
    break;
  }
  case 0x19f32112:
  { // cell13-16
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[12] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[13] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[14] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[15] = temp;
    }
    // ESP_LOGD(TAG, "B RCVD cell voltages 13: %1.3f 14: %1.3f 15: %1.3f 16: %1.3f", batteryB_cell_voltage[12], batteryB_cell_voltage[13], batteryB_cell_voltage[14], batteryB_cell_voltage[15]);
    break;
  }
  case 0x19f32212:
  { // cell17-20
    temp = ((data[1] << 8) + (data[0])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[16] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[17] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[18] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.001;
    if (temp >= 0 && temp <= 5)
    {
      batteryB_cell_voltage[19] = temp;
    }
    // ESP_LOGD(TAG, "B RCVD cell voltages 17: %1.3f 18: %1.3f 19: %1.3f 20: %1.3f", batteryB_cell_voltage[16], batteryB_cell_voltage[17], batteryB_cell_voltage[18], batteryB_cell_voltage[19]);
    break;
  }
  case 0x19f32412:
  { // temp1-4
    temp = ((data[1] << 8) + (data[0])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryB_cell_temperature[0] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryB_cell_temperature[1] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryB_cell_temperature[2] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryB_cell_temperature[3] = temp;
    }
    // ESP_LOGD(TAG, "B RCVD cell temperature 01: %2.0f 02: %2.0f 03: %2.0f 04: %2.0f", batteryB_cell_temperature[0], batteryB_cell_temperature[1], batteryB_cell_temperature[2], batteryB_cell_temperature[3]);
    break;
  }
  case 0x19f32512:
  { // temp5-8
    temp = ((data[1] << 8) + (data[0])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryB_cell_temperature[4] = temp;
    }
    temp = ((data[3] << 8) + (data[2])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryB_cell_temperature[5] = temp;
    }
    temp = ((data[5] << 8) + (data[4])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryB_cell_temperature[6] = temp;
    }
    temp = ((data[7] << 8) + (data[6])) * 0.1;
    if (temp >= 0 && temp <= 50)
    {
      batteryB_cell_temperature[7] = temp;
    }
    // ESP_LOGD(TAG, "B RCVD cell temperature 05: %2.0f 06: %2.0f 07: %2.0f 08: %2.0f", batteryB_cell_temperature[4], batteryB_cell_temperature[5], batteryB_cell_temperature[6], batteryB_cell_temperature[7]);
    break;
  }
  }
}
