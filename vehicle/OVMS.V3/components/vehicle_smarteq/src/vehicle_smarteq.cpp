/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 ; http://ed.no-limit.de/wiki/index.php/Hauptseite
 */

#include "ovms_log.h"
static const char *TAG = "v-smarteq";

#define VERSION "1.0.0"

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

#include "vehicle_smarteq.h"

static const OvmsPoller::poll_pid_t obdii_polls[] =
{
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x80, {  0,120,999 }, 0, ISOTP_STD }, // rqIDpart OBL_7KW_Installed
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, {  0,120,999 }, 0, ISOTP_STD }, // rqBattTemperatures
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x41, {  0,120,999 }, 0, ISOTP_STD }, // rqBattVoltages_P1
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x42, {  0,120,999 }, 0, ISOTP_STD }, // rqBattVoltages_P2
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x200c, {  0,10,999 }, 0, ISOTP_STD }, // extern temp byte 2+3
 // { 0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x52, {  0,10,999 }, 0, ISOTP_STD }, // ,764,36,45,.1,400,1,°C,2152,6152,ff,IH_InCarTemp
  POLL_LIST_END
};

/**
 * Constructor & destructor
 */

OvmsVehicleSmartEQ::OvmsVehicleSmartEQ() {
  ESP_LOGI(TAG, "Start Smart EQ vehicle module");
  
  m_booter_start = false;

  // BMS configuration:
  BmsSetCellArrangementVoltage(96, 3);
  BmsSetCellArrangementTemperature(28, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
  
  mt_bms_temps = new OvmsMetricVector<float>("xsq.v.bms.temps", SM_STALE_HIGH, Celcius);
  mt_bus_awake = MyMetrics.InitBool("xsq.v.bus.awake", SM_STALE_MIN, false);
  
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  PollSetPidList(m_can1, obdii_polls);
  PollSetState(0);
  
  PollSetThrottling(5);
  PollSetResponseSeparationTime(20);

  MyConfig.RegisterParam("xsq", "Smart EQ", true, true);
  ConfigChanged(NULL);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif
}

OvmsVehicleSmartEQ::~OvmsVehicleSmartEQ() {
  ESP_LOGI(TAG, "Stop Smart EQ vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
}

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleSmartEQ::ConfigChanged(OvmsConfigParam* param) {
  if (param && param->GetName() != "xsq")
    return;

  ESP_LOGI(TAG, "Smart EQ reload configuration");
  
  m_enable_write  = MyConfig.GetParamValueBool("xsq", "canwrite", false);
}

uint64_t OvmsVehicleSmartEQ::swap_uint64(uint64_t val) {
  val = ((val << 8) & 0xFF00FF00FF00FF00ull) | ((val >> 8) & 0x00FF00FF00FF00FFull);
  val = ((val << 16) & 0xFFFF0000FFFF0000ull) | ((val >> 16) & 0x0000FFFF0000FFFFull);
  return (val << 32) | (val >> 32);
}

void OvmsVehicleSmartEQ::IncomingFrameCan1(CAN_frame_t* p_frame) {
  uint8_t *data = p_frame->data.u8;
  uint64_t c = swap_uint64(p_frame->data.u64);
  
  static bool isCharging = false;
  static bool lastCharging = false;
  float _range_est;

  if (m_candata_poll != 100 && StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts) > 100) {
    m_candata_poll++;
    if (m_candata_poll==100) {
      ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
      mt_bus_awake->SetValue(true);
      if (m_enable_write) PollSetState(1);
    }
  }
  m_candata_timer = SQ_CANDATA_TIMEOUT;
  
  switch (p_frame->MsgID) {
    case 0x392:
      StandardMetrics.ms_v_env_hvac->SetValue((CAN_BYTE(1) & 0x40) > 0);
      StandardMetrics.ms_v_env_cabintemp->SetValue(CAN_BYTE(5) - 40);
      break;
    case 0x42E: // HV Voltage
      StandardMetrics.ms_v_bat_voltage->SetValue((float) ((CAN_UINT(3)>>5)&0x3ff) / 2); // HV Voltage
      StandardMetrics.ms_v_bat_temp->SetValue(((c >> 13) & 0x7Fu) - 40); // HVBatteryTemp
      StandardMetrics.ms_v_charge_climit->SetValue((c >> 20) & 0x3Fu); // MaxChargingNegotiatedCurrent
      break;
    case 0x4F8:
      StandardMetrics.ms_v_env_handbrake->SetValue((CAN_BYTE(0) & 0x08) > 0);
      StandardMetrics.ms_v_env_awake->SetValue((CAN_BYTE(0) & 0x40) > 0); // Ignition on
      break;
    case 0x5D7: // Speed, ODO
      StandardMetrics.ms_v_pos_speed->SetValue((float) CAN_UINT(0) / 100);
      StandardMetrics.ms_v_pos_odometer->SetValue((float) (CAN_UINT32(2)>>4) / 100);
      break;
    case 0x5de:
      StandardMetrics.ms_v_env_headlights->SetValue((CAN_BYTE(0) & 0x04) > 0);
      StandardMetrics.ms_v_door_fl->SetValue((CAN_BYTE(1) & 0x08) > 0);
      StandardMetrics.ms_v_door_fr->SetValue((CAN_BYTE(1) & 0x02) > 0);
      StandardMetrics.ms_v_door_rl->SetValue((CAN_BYTE(2) & 0x40) > 0);
      StandardMetrics.ms_v_door_rr->SetValue((CAN_BYTE(2) & 0x10) > 0);
      StandardMetrics.ms_v_door_trunk->SetValue((CAN_BYTE(7) & 0x10) > 0);
      break;
    case 0x654: // SOC(b)
      StandardMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(3));
      StandardMetrics.ms_v_door_chargeport->SetValue((CAN_BYTE(0) & 0x20)); // ChargingPlugConnected
      StandardMetrics.ms_v_charge_duration_full->SetValue((((c >> 22) & 0x3ffu) < 0x3ff) ? (c >> 22) & 0x3ffu : 0);
      _range_est = ((c >> 12) & 0x3FFu); // VehicleAutonomy
      if ( _range_est != 1023.0 )
        StandardMetrics.ms_v_bat_range_est->SetValue(_range_est); // VehicleAutonomy
      break;
    case 0x65C: // ExternalTemp
      StandardMetrics.ms_v_env_temp->SetValue((CAN_BYTE(0) >> 1) - 40); // ExternalTemp ?
      break;
    case 0x658: //
      StandardMetrics.ms_v_bat_soh->SetValue(CAN_BYTE(4) & 0x7Fu); // SOH ?
      isCharging = (CAN_BYTE(5) & 0x20); // ChargeInProgress
      if (isCharging) { // STATE charge in progress
        //StandardMetrics.ms_v_charge_inprogress->SetValue(isCharging);
      }
      if (isCharging != lastCharging) { // EVENT charge state changed
        if (isCharging) { // EVENT started charging
          StandardMetrics.ms_v_charge_pilot->SetValue(true);
          StandardMetrics.ms_v_charge_inprogress->SetValue(isCharging);
          StandardMetrics.ms_v_charge_mode->SetValue("standard");
          StandardMetrics.ms_v_charge_type->SetValue("type2");
          StandardMetrics.ms_v_charge_state->SetValue("charging");
          StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        } else { // EVENT stopped charging
          StandardMetrics.ms_v_charge_pilot->SetValue(false);
          StandardMetrics.ms_v_charge_inprogress->SetValue(isCharging);
          StandardMetrics.ms_v_charge_mode->SetValue("standard");
          StandardMetrics.ms_v_charge_type->SetValue("type2");
          if (StandardMetrics.ms_v_bat_soc->AsInt() < 95) {
            // Assume the charge was interrupted
            ESP_LOGI(TAG,"Car charge session was interrupted");
            StandardMetrics.ms_v_charge_state->SetValue("stopped");
            StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
          } else {
            // Assume the charge completed normally
            ESP_LOGI(TAG,"Car charge session completed");
            StandardMetrics.ms_v_charge_state->SetValue("done");
            StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
          }
        }
      }
      lastCharging = isCharging;
      break;
    case 0x668:
      StandardMetrics.ms_v_env_on->SetValue((CAN_BYTE(0) & 0x40) > 0); // Drive Ready
      break;
    case 0x673:
      if (CAN_BYTE(2) != 0xff)
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, (float) CAN_BYTE(2)*3.1);
      if (CAN_BYTE(3) != 0xff)
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, (float) CAN_BYTE(3)*3.1);
      if (CAN_BYTE(4) != 0xff)
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, (float) CAN_BYTE(4)*3.1);
      if (CAN_BYTE(5) != 0xff)
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, (float) CAN_BYTE(5)*3.1);
      break;

    default:
      //ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
  }
}

/**
 * Update derived energy metrics while driving
 * Called once per second from Ticker1
 */
void OvmsVehicleSmartEQ::HandleEnergy() {
  float voltage  = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float current  = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);

  // Power (in kw) resulting from voltage and current
  float power = voltage * current / 1000.0;

  // Are we driving?
  if (power != 0.0 && StandardMetrics.ms_v_env_on->AsBool()) {
    // Update energy used and recovered
    float energy = power / 3600.0;    // 1 second worth of energy in kwh's
    if (energy < 0.0f)
      StandardMetrics.ms_v_bat_energy_used->SetValue( StandardMetrics.ms_v_bat_energy_used->AsFloat() - energy);
    else // (energy > 0.0f)
      StandardMetrics.ms_v_bat_energy_recd->SetValue( StandardMetrics.ms_v_bat_energy_recd->AsFloat() + energy);
  }
}

void OvmsVehicleSmartEQ::Ticker1(uint32_t ticker) {
  if (m_candata_timer > 0) {
    if (--m_candata_timer == 0) {
      // Car has gone to sleep
      ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
      mt_bus_awake->SetValue(false);
      m_candata_poll = 0;
      PollSetState(0);
    }
  }

  if (m_booter_start && StandardMetrics.ms_v_env_hvac->AsBool()) {
    m_booter_start = false;
    MyNotify.NotifyString("info", "hvac.enabled", "Booster on");
  }
}

// can can1 tx st 634 40 01 72 00
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandClimateControl(bool enable) {
  if(!m_enable_write) {
    ESP_LOGE(TAG, "CommandClimateControl failed / no write access");
    return Fail;
  }
  ESP_LOGI(TAG, "CommandClimateControl %s", enable ? "ON" : "OFF");

  OvmsVehicle::vehicle_command_t res;

  if (enable) {
    uint8_t data[4] = {0x40, 0x01, 0x00, 0x00};
    canbus *obd;
    obd = m_can1;

    res = CommandWakeup();
    if (res == Success) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      for (int i = 0; i < 10; i++) {
        obd->WriteStandard(0x634, 4, data);
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
      m_booter_start = true;
      res = Success;
    } else {
      res = Fail;
    }
  } else {
    res = NotImplemented;
  }

  // fallback to default implementation?
  if (res == NotImplemented) {
    res = OvmsVehicle::CommandClimateControl(enable);
  }
  return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandHomelink(int button, int durationms) {
  // This is needed to enable climate control via Homelink for the iOS app
  ESP_LOGI(TAG, "CommandHomelink button=%d durationms=%d", button, durationms);
  
  OvmsVehicle::vehicle_command_t res = NotImplemented;
  if (button == 0) {
    res = CommandClimateControl(true);
  }
  else if (button == 1) {
    res = CommandClimateControl(false);
  }

  // fallback to default implementation?
  if (res == NotImplemented) {
    res = OvmsVehicle::CommandHomelink(button, durationms);
  }
  return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandWakeup() {
  if(!m_enable_write) {
    ESP_LOGE(TAG, "CommandWakeup failed: no write access!");
    return Fail;
  }

  OvmsVehicle::vehicle_command_t res;

  ESP_LOGI(TAG, "Send Wakeup Command");
  res = Fail;
  if(!mt_bus_awake->AsBool()) {
    uint8_t data[4] = {0x40, 0x00, 0x00, 0x00};
    canbus *obd;
    obd = m_can1;

    for (int i = 0; i < 20; i++) {
      obd->WriteStandard(0x634, 4, data);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      if (mt_bus_awake->AsBool()) {
        res = Success;
        ESP_LOGI(TAG, "Vehicle is now awake");
        break;
      }
    }
  } else {
    res = Success;
    ESP_LOGI(TAG, "Vehicle is awake");
  }

  return res;
}

/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleSmartEQ::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 10:
      MyConfig.SetParamValue("xsq", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("xsq", "suffrange", value);
      return true;
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "canwrite",  (bits& 1)!=0);
      return true;
    }
    default:
      return OvmsVehicle::SetFeature(key, value);
  }
}

/**
 * GetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
const std::string OvmsVehicleSmartEQ::GetFeature(int key)
{
  switch (key)
  {
    case 10:
      return MyConfig.GetParamValue("xsq", "suffsoc", STR(0));
    case 11:
      return MyConfig.GetParamValue("xsq", "suffrange", STR(0));
    case 15:
    {
      int bits = ( MyConfig.GetParamValueBool("xsq", "canwrite",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
  }
}

class OvmsVehicleSmartEQInit {
  public:
  OvmsVehicleSmartEQInit();
} MyOvmsVehicleSmartEQInit __attribute__ ((init_priority (9000)));

OvmsVehicleSmartEQInit::OvmsVehicleSmartEQInit() {
  ESP_LOGI(TAG, "Registering Vehicle: SMART EQ (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleSmartEQ>("SQ", "Smart ED/EQ 4.Gen");
}

