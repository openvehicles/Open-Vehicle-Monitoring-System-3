/*
;    Project:       Open Vehicle Monitor System
;    Module:        Renault Zoe Ph2 - Main vehicle integration
;
;    (C) 2022-2026  Carsten Schmiemann
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

#include "vehicle_renaultzoe_ph2.h"

const char *OvmsVehicleRenaultZoePh2::s_tag = "v-zoe-ph2";

// V-CAN activity threshold to trigger wakeup or sleep state
#define vcan_activity_threshold 15     // Threshold for enabling poller (msgs/sec), OVMS filtered messages
#define vcan_poller_disable_delay 3000 // Time (ms) of inactivity (msgs/s lower than threshold) before stopping poller

class OvmsVehicleRenaultZoePh2Init
{
public:
  OvmsVehicleRenaultZoePh2Init();
} MyOvmsVehicleRenaultZoePh2Init __attribute__((init_priority(9000)));

OvmsVehicleRenaultZoePh2Init::OvmsVehicleRenaultZoePh2Init()
{
  ESP_LOGI(TAG, "Registering Vehicle: Renault Zoe Ph2 (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleRenaultZoePh2>("RZ2", "Renault Zoe Ph2");
}

OvmsVehicleRenaultZoePh2::OvmsVehicleRenaultZoePh2()
{
  ESP_LOGI(TAG, "Start Renault Zoe Ph2 vehicle module");

  StandardMetrics.ms_v_type->SetValue("RZ2");

  MyConfig.RegisterParam("xrz2", "Renault Zoe Ph2 configuration", true, true);
  ConfigChanged(NULL);

  // Initialize ignition state tracking from current state
  // This ensures after-ignition DCDC works even if system starts with ignition OFF
  m_last_ignition_state = StandardMetrics.ms_v_env_on->AsBool();
  ESP_LOGD(TAG, "Initial ignition state: %s", m_last_ignition_state ? "ON" : "OFF");

  // Init Zoe Ph2 CAN connections
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS); // OBD port or V-CAN direct connection

  // Set Ph2 poller speed and delay
  PollSetThrottling(10);
  PollSetResponseSeparationTime(20);

  // Renault ZOE specific metrics
  mt_12v_dcdc_load = MyMetrics.InitFloat("xrz2.v.c.12v.dcdc.load", SM_STALE_MIN, 0, Percentage);
  // HV Battery metrics
  mt_bat_chg_start = MyMetrics.InitFloat("xrz2.b.chg.start", SM_STALE_NONE, 0, kWh, true);
  mt_bat_cons_aux = MyMetrics.InitFloat("xrz2.v.b.consumption.aux", SM_STALE_MID, 0, kWh, true);
  mt_bat_cycles = MyMetrics.InitFloat("xrz2.b.cycles", SM_STALE_MID, 0, Other, true);
  mt_bat_max_charge_power = MyMetrics.InitFloat("xrz2.b.max.charge.power", SM_STALE_MID, 0, kW, true);
  mt_bat_recd_start = MyMetrics.InitFloat("xrz2.b.recd.start", SM_STALE_NONE, 0, kWh, true);
  mt_bat_used_start = MyMetrics.InitFloat("xrz2.b.used.start", SM_STALE_NONE, 0, kWh, true);
  // Bus awake indicator
  mt_bus_awake = MyMetrics.InitBool("xrz2.v.bus.awake", SM_STALE_NONE, false);
  mt_auto_ptc_state = MyMetrics.InitBool("xrz2.h.ptc.state", SM_STALE_NONE, false, Other, true);
  // HVAC metrics, ideal for debugging AC or heat pump issues
  mt_hvac_compressor_discharge_temp = MyMetrics.InitFloat("xrz2.h.compressor.temp.discharge", SM_STALE_MID, 0, Celcius);
  mt_hvac_compressor_mode = MyMetrics.InitString("xrz2.h.compressor.mode", SM_STALE_MID, 0, Other);
  mt_hvac_compressor_power = MyMetrics.InitFloat("xrz2.h.compressor.power", SM_STALE_MID, 0, Watts);
  mt_hvac_compressor_pressure = MyMetrics.InitFloat("xrz2.h.compressor.pressure", SM_STALE_MID, 0, Bar);
  mt_hvac_compressor_speed = MyMetrics.InitFloat("xrz2.h.compressor.speed", SM_STALE_MID, 0);
  mt_hvac_compressor_speed_req = MyMetrics.InitFloat("xrz2.h.compressor.speed.req", SM_STALE_MID, 0);
  mt_hvac_compressor_suction_temp = MyMetrics.InitFloat("xrz2.h.compressor.temp.suction", SM_STALE_MID, 0, Celcius);
  mt_hvac_cooling_evap_setpoint = MyMetrics.InitFloat("xrz2.h.cooling.evap.setpoint", SM_STALE_MID, 0, Celcius);
  mt_hvac_cooling_evap_temp = MyMetrics.InitFloat("xrz2.h.cooling.evap.temp", SM_STALE_MID, 0, Celcius);
  mt_hvac_heating_cond_setpoint = MyMetrics.InitFloat("xrz2.h.heating.cond.setpoint", SM_STALE_MID, 0, Celcius);
  mt_hvac_heating_cond_temp = MyMetrics.InitFloat("xrz2.h.heating.cond.temp", SM_STALE_MID, 0, Celcius);
  mt_hvac_heating_ptc_req = MyMetrics.InitFloat("xrz2.h.heating.ptc.req", SM_STALE_MID, 0, Other);
  mt_hvac_heating_temp = MyMetrics.InitFloat("xrz2.h.heating.temp", SM_STALE_MID, 0, Celcius);
  // Inverter metrics
  mt_inv_hv_current = MyMetrics.InitFloat("xrz2.i.current", SM_STALE_MIN, 0, Amps);
  mt_inv_hv_voltage = MyMetrics.InitFloat("xrz2.i.voltage", SM_STALE_MIN, 0, Volts);
  mt_inv_status = MyMetrics.InitString("xrz2.m.inverter.status", SM_STALE_MIN, 0);
  // AC charging metrics
  mt_main_phases = MyMetrics.InitString("xrz2.c.main.phases", SM_STALE_MIN, 0);
  mt_main_phases_num = MyMetrics.InitFloat("xrz2.c.main.phases.num", SM_STALE_MIN, 0);
  mt_main_power_available = MyMetrics.InitFloat("xrz2.c.main.power.available", SM_STALE_MIN, 0, kW);
  // Motor temperature sensors
  mt_mot_temp_stator1 = MyMetrics.InitFloat("xrz2.m.temp.stator1", SM_STALE_MIN, 0, Celcius);
  mt_mot_temp_stator2 = MyMetrics.InitFloat("xrz2.m.temp.stator2", SM_STALE_MIN, 0, Celcius);
  mt_mot_temp_rotor_raw = MyMetrics.InitFloat("xrz2.m.temp.rotor.raw", SM_STALE_MIN, 0, Celcius);
  mt_mot_temp_rotor = MyMetrics.InitFloat("xrz2.m.temp.rotor", SM_STALE_MIN, 0, Celcius);
  mt_mot_current_stator_u = MyMetrics.InitFloat("xrz2.m.current.stator.u", SM_STALE_MIN, 0, Amps);
  mt_mot_current_stator_v = MyMetrics.InitFloat("xrz2.m.current.stator.v", SM_STALE_MIN, 0, Amps);
  mt_mot_current_stator_w = MyMetrics.InitFloat("xrz2.m.current.stator.w", SM_STALE_MIN, 0, Amps);
  mt_mot_current_rotor1 = MyMetrics.InitFloat("xrz2.m.current.rotor1", SM_STALE_MIN, 0, Amps);
  mt_mot_current_rotor2 = MyMetrics.InitFloat("xrz2.m.current.rotor2", SM_STALE_MIN, 0, Amps);
  mt_mot_rotor_resistance = MyMetrics.InitFloat("xrz2.m.rotor.resistance", SM_STALE_MIN, 0);
  mt_mot_rotor_voltage = MyMetrics.InitFloat("xrz2.m.rotor.voltage", SM_STALE_MIN, 0, Volts);
  // Poller and trip calculation metrics
  mt_poller_inhibit = MyMetrics.InitBool("xrz2.v.poller.inhibit", SM_STALE_NONE, false, Other, true);
  mt_pos_odometer_start = MyMetrics.InitFloat("xrz2.v.pos.odometer.start", SM_STALE_NONE, 0, Kilometers, true);

  m_auto_ptc_currently_enabled = mt_auto_ptc_state->AsBool();

  // BMS configuration:
  BmsSetCellArrangementVoltage(96, 24);
  BmsSetCellArrangementTemperature(12, 3);
  BmsSetCellLimitsVoltage(2.0, 4.5);
  BmsSetCellLimitsTemperature(-39, 60);
  BmsSetCellDefaultThresholdsVoltage(0.030, 0.050);
  BmsSetCellDefaultThresholdsTemperature(4.0, 5.0);

  // Init commands / menu structure
  OvmsCommand *cmd_xrz2 = MyCommandApp.RegisterCommand("xrz2", "Renault Zoe Ph2");
  OvmsCommand *poller;
  OvmsCommand *ddt;
  OvmsCommand *hvac;
  OvmsCommand *compressor;
  OvmsCommand *ptc;
  OvmsCommand *dcdc;
  OvmsCommand *cluster;
  OvmsCommand *cluster_reset;

  // Poller custom commands
  poller = cmd_xrz2->RegisterCommand("poller", "Start, stop, inhibit or resume poller");
  poller->RegisterCommand("start", "Start poller in idle mode", CommandPollerStartIdle);
  poller->RegisterCommand("stop", "Stop poller", CommandPollerStop);
  poller->RegisterCommand("inhibit", "Disable poller for vehicle maintenance", CommandPollerInhibit);
  poller->RegisterCommand("resume", "Resume poller for normal operation", CommandPollerResume);

  // Lighting command (coming home function)
  cmd_xrz2->RegisterCommand("lighting", "Enable headlights for 30 seconds (coming home)", CommandLighting);

  // Unlock trunk command
  cmd_xrz2->RegisterCommand("unlocktrunk", "Unlock trunk/tailgate", CommandUnlockTrunkShell);

#ifdef RZ2_DEBUG_BUILD
  // Debug output of custom functions (only in debug builds)
  cmd_xrz2->RegisterCommand("debug", "Debug output of custom functions", CommandDebug);
#endif

  // DDT reconfiguration of ECU commands
  ddt = cmd_xrz2->RegisterCommand("ddt", "Modify configuration of ECUs on V-CAN");
  hvac = ddt->RegisterCommand("hvac", "Modify configuration of HVAC ECU");
  compressor = hvac->RegisterCommand("compressor", "Enable or disable compressor");
  compressor->RegisterCommand("enable", "Enable compressor for normal operation", CommandDdtHvacEnableCompressor);
  compressor->RegisterCommand("disable", "Disable compressor if there are problems with it", CommandDdtHvacDisableCompressor);
  ptc = hvac->RegisterCommand("ptc", "Enable or disable ptc");
  ptc->RegisterCommand("enable", "Enable PTC for permanent heating", CommandDdtHvacEnablePTC);
  ptc->RegisterCommand("disable", "Reset PTC control to Auto mode", CommandDdtHvacDisablePTC);

  // DCDC control commands
  dcdc = cmd_xrz2->RegisterCommand("dcdc", "Manual DCDC converter control");
  dcdc->RegisterCommand("enable", "Enable DCDC converter manually", CommandDcdcEnable);
  dcdc->RegisterCommand("disable", "Disable DCDC converter", CommandDcdcDisable);

  // Reset and service functions
  cluster = ddt->RegisterCommand("cluster", "Service functions of Instrument cluster");
  cluster_reset = cluster->RegisterCommand("reset", "Reset functions");
  cluster_reset->RegisterCommand("service", "Reset service reminder", CommandDdtClusterResetService);

  // CAN1 Software filter - Poll response
  MyPollers.AddFilter(1, 0x18daf1da, 0x18daf1df);  // EVC (0x18daf1da), LBC (0x18daf1db), Motor Inverter (0x18daf1df)
  MyPollers.AddFilter(1, 0x763, 0x765);            // METER_DDT_RESP_ID, HVAC_DDT_RESP_ID, BCM_DDT_RESP_ID
  MyPollers.AddFilter(1, 0x76D, 0x76D);            // UPC/UCM diagnostic response

  // CAN1 Software filter - V-CAN PIDs
  MyPollers.AddFilter(1, 0x0A4, 0x0A4);  // ATCU_A5_ID
  MyPollers.AddFilter(1, 0x3C0, 0x3C3);  // ECM_A6_ID, HEVC_A2_ID
  MyPollers.AddFilter(1, 0x418, 0x418);  // BCM_A4_ID
  MyPollers.AddFilter(1, 0x46F, 0x46F);  // HFM_A1_ID
  MyPollers.AddFilter(1, 0x437, 0x437);  // HEVC_A1_ID
  MyPollers.AddFilter(1, 0x480, 0x480);  // HEVC_R17_ID
  MyPollers.AddFilter(1, 0x491, 0x491);  // BCM_A11_ID
  MyPollers.AddFilter(1, 0x43E, 0x43E);  // HVAC_A2_ID
  MyPollers.AddFilter(1, 0x46C, 0x46C);  // BCM_A3_ID
  MyPollers.AddFilter(1, 0x47C, 0x47F);  // USM_A2_ID, BCM_A7_ID, METER_A3_ID
  MyPollers.AddFilter(1, 0x5B9, 0x5BA);  // METER_A5_ID, BCM_A2_ID
  MyPollers.AddFilter(1, 0x5C2, 0x5C5);  // HVAC_R3_ID, HEVC_A101_ID
  MyPollers.AddFilter(1, 0x625, 0x625);  // HEVC_A23_ID
  MyPollers.AddFilter(1, 0x6C9, 0x6C9);  // HEVC_A22_ID
  MyPollers.AddFilter(1, 0x6E9, 0x6E9);  // HEVC_R11_ID
  MyPollers.AddFilter(1, 0x6F2, 0x6F2);  // HEVC_R12_ID

  // Initialize TX callback tracking
  m_tx_success_id.store(0);
  m_tx_success_flag.store(false);

  // Register TX callback to track actual transmission success
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyCan.RegisterCallback(TAG, std::bind(&OvmsVehicleRenaultZoePh2::TxCallback, this, _1, _2), true);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif
}

OvmsVehicleRenaultZoePh2::~OvmsVehicleRenaultZoePh2()
{
  ESP_LOGI(TAG, "Shutdown Renault Zoe Ph2 vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
}

// Reload all or single values from config storage
void OvmsVehicleRenaultZoePh2::ConfigChanged(OvmsConfigParam *param)
{
  if (param && param->GetName() != "xrz2")
    return;

  m_range_ideal = MyConfig.GetParamValueInt("xrz2", "rangeideal", 350);
  m_battery_capacity = MyConfig.GetParamValueInt("xrz2", "battcapacity", 52000);
  m_UseBMScalculation = MyConfig.GetParamValueBool("xrz2", "UseBMScalculation", false);
  m_vcan_enabled = MyConfig.GetParamValueBool("xrz2", "vCanEnabled", false);
  m_auto_12v_recharge_enabled = MyConfig.GetParamValueBool("xrz2", "auto_12v_recharge_enabled", false);
  m_auto_12v_threshold = MyConfig.GetParamValueFloat("xrz2", "auto_12v_threshold", 12.4);
  m_coming_home_enabled = MyConfig.GetParamValueBool("xrz2", "coming_home_enabled", false);
  m_remote_climate_enabled = MyConfig.GetParamValueBool("xrz2", "remote_climate_enabled", false);
  m_alt_unlock_cmd = MyConfig.GetParamValueBool("xrz2", "alt_unlock_cmd", false);

  // Auto-enable PTC parameters
  m_auto_ptc_enabled = MyConfig.GetParamValueBool("xrz2", "auto_ptc_enabled", false);
  m_auto_ptc_temp_min = MyConfig.GetParamValueFloat("xrz2", "auto_ptc_temp_min", 9.5);
  m_auto_ptc_temp_max = MyConfig.GetParamValueFloat("xrz2", "auto_ptc_temp_max", 20.0);

  // PV Charging notification mode
  m_pv_charge_notification_enabled = MyConfig.GetParamValueBool("xrz2", "pv_charge_notification", false);

  // DCDC control parameters
  m_dcdc_soc_limit = MyConfig.GetParamValueInt("xrz2", "dcdc_soc_limit", 50);
  m_dcdc_time_limit = MyConfig.GetParamValueInt("xrz2", "dcdc_time_limit", 0);
  m_dcdc_after_ignition_time = MyConfig.GetParamValueInt("xrz2", "dcdc_after_ignition_time", 0);

  // Validate ranges
  if (m_dcdc_soc_limit < 5) m_dcdc_soc_limit = 5;
  if (m_dcdc_soc_limit > 100) m_dcdc_soc_limit = 100;
  if (m_dcdc_time_limit < 0) m_dcdc_time_limit = 0;
  if (m_dcdc_time_limit > 999) m_dcdc_time_limit = 999;
  if (m_dcdc_after_ignition_time < 0) m_dcdc_after_ignition_time = 0;
  if (m_dcdc_after_ignition_time > 60) m_dcdc_after_ignition_time = 60;

  // Validate auto-PTC temperature ranges
  if (m_auto_ptc_temp_min < -20.0) m_auto_ptc_temp_min = -20.0;
  if (m_auto_ptc_temp_min > 30.0) m_auto_ptc_temp_min = 30.0;
  if (m_auto_ptc_temp_max < -20.0) m_auto_ptc_temp_max = -20.0;
  if (m_auto_ptc_temp_max > 30.0) m_auto_ptc_temp_max = 30.0;
  if (m_auto_ptc_temp_max < m_auto_ptc_temp_min) m_auto_ptc_temp_max = m_auto_ptc_temp_min;

  ESP_LOGI(TAG, "Renault Zoe Ph2 reload configuration: Range ideal: %d, Battery capacity: %d, Use BMS as energy counter: %s, V-CAN connected: %s", m_range_ideal, m_battery_capacity, m_UseBMScalculation ? "yes" : "no", m_vcan_enabled ? "yes" : "no");
  ESP_LOGI(TAG, "12V auto-recharge: %s, Threshold: %.1fV", m_auto_12v_recharge_enabled ? "enabled" : "disabled", m_auto_12v_threshold);

  char time_limit_buf[32];
  if (m_dcdc_time_limit == 0) {
    snprintf(time_limit_buf, sizeof(time_limit_buf), "infinite");
  } else {
    snprintf(time_limit_buf, sizeof(time_limit_buf), "%d min", m_dcdc_time_limit);
  }
  ESP_LOGI(TAG, "DCDC manual limits - SOC: %d%%, Time: %s", m_dcdc_soc_limit, time_limit_buf);

  char after_ignition_buf[32];
  if (m_dcdc_after_ignition_time > 0) {
    snprintf(after_ignition_buf, sizeof(after_ignition_buf), "%d min", m_dcdc_after_ignition_time);
  } else {
    snprintf(after_ignition_buf, sizeof(after_ignition_buf), "disabled");
  }
  ESP_LOGI(TAG, "DCDC after-ignition keep-alive: %s", after_ignition_buf);
  ESP_LOGI(TAG, "Coming home function: %s", m_coming_home_enabled ? "enabled" : "disabled");
  ESP_LOGI(TAG, "Remote climate trigger: %s", m_remote_climate_enabled ? "enabled" : "disabled");
  ESP_LOGI(TAG, "Alternative unlock command: %s", m_alt_unlock_cmd ? "enabled" : "disabled");
  if (m_auto_ptc_enabled) {
    ESP_LOGI(TAG, "Auto-enable PTC: enabled, temp range: %.1f°C - %.1f°C", m_auto_ptc_temp_min, m_auto_ptc_temp_max);
  } else {
    ESP_LOGI(TAG, "Auto-enable PTC: disabled");
  }
  ESP_LOGI(TAG, "PV/Solar charging mode: %s", m_pv_charge_notification_enabled ? "enabled" : "disabled");
}

// Poller lists for Renault Zoe Ph2
//
// Pollstate 0 - POLLSTATE_OFF      - car is off
// Pollstate 1 - POLLSTATE_ON       - car is on
// Pollstate 2 - POLLSTATE_DRIVING  - car is driving
// Pollstate 3 - POLLSTATE_CHARGING - car is charging

// Poller list for OBD connection, all metrics must be polled
static const OvmsPoller::poll_pid_t zoe_ph2_polls_obd[] = {
    //***TX-ID, ***RX-ID, ***SID, ***PID, { Polltime (seconds) for Pollstate 0, 1, 2, 3}, ***CAN BUS Interface, ***FRAMETYPE

    // Motor Inverter
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x700C, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Inverter temperature
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x700F, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Stator Temperature 1
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7010, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Stator Temperature 2
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2004, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Battery voltage sense
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7049, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Current voltage sense
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7003, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Rotor current sensor 1
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7004, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Rotor current sensor 2
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7007, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Stator current phase U
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7008, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Stator current phase V
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7009, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Stator current phase W
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7038, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Rotor temp raw
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7088, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Rotor temp estimated
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7050, {0, 60, 60, 60}, 0, ISOTP_EXTFRAME}, // Rotor reference resistance

    // EVC-HCM-VCM
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2006, {0, 60, 10, 300}, 0, ISOTP_EXTFRAME},   // Odometer
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2003, {0, 60, 5, 300}, 0, ISOTP_EXTFRAME},    // Vehicle Speed
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2005, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME},    // 12Battery Voltage
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x21CF, {0, 10, 10, 300}, 0, ISOTP_EXTFRAME},   // Inverter Status
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2218, {0, 20, 20, 20}, 0, ISOTP_EXTFRAME},    // Ambient air temperature
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B85, {0, 2, 0, 10}, 0, ISOTP_EXTFRAME},      // Charge plug present
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B6D, {0, 2, 0, 10}, 0, ISOTP_EXTFRAME},      // Charge MMI states
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B7A, {0, 2, 0, 10}, 0, ISOTP_EXTFRAME},      // Charge type
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3064, {0, 10, 5, 10}, 0, ISOTP_EXTFRAME},     // Motor rpm
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x300F, {0, 2, 0, 3}, 0, ISOTP_EXTFRAME},       // AC charging power available
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x300D, {0, 10, 0, 3}, 0, ISOTP_EXTFRAME},      // AC mains current
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x300B, {0, 2, 0, 10}, 0, ISOTP_EXTFRAME},      // AC phases
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B8A, {0, 2, 0, 10}, 0, ISOTP_EXTFRAME},      // AC mains voltage

    // BCM
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6300, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS pressure - front left
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6301, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS pressure - front right
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6302, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS pressure - rear left
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6303, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS pressure - rear right
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6310, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS temp - front left
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6311, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS temp - front right
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6312, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS temp - rear left
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6313, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS temp - rear right
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4109, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS alert - front left
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x410A, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS alert - front right
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x410B, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS alert - rear left
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x410C, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS alert - rear right
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x8004, {0, 3, 3, 3}, 0, ISOTP_STD},   // Car secure aka vehicle locked
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6026, {0, 3, 3, 3}, 0, ISOTP_STD},   // Front left door
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6027, {0, 3, 3, 3}, 0, ISOTP_STD},   // Front right door
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x61B2, {0, 3, 3, 3}, 0, ISOTP_STD},   // Rear left door
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x61B3, {0, 3, 3, 3}, 0, ISOTP_STD},   // Rear right door
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x609B, {0, 3, 3, 3}, 0, ISOTP_STD},   // Tailgate
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4186, {0, 3, 3, 3}, 0, ISOTP_STD},   // Low beam lights
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x60C6, {0, 3, 6, 60}, 0, ISOTP_STD},  // Ignition relay (switch)
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4060, {0, 600, 0, 0}, 0, ISOTP_STD}, // Vehicle identificaftion number

    // LBC
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9002, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME},    // SOC
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9003, {0, 600, 600, 600}, 0, ISOTP_EXTFRAME}, // SOH
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9005, {0, 10, 2, 2}, 0, ISOTP_EXTFRAME},      // Battery Voltage
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x925D, {0, 10, 2, 2}, 0, ISOTP_EXTFRAME},      // Battery Current
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9012, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME},    // Battery Average Temperature
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x91C8, {0, 60, 60, 5}, 0, ISOTP_EXTFRAME},     // Battery Available Energy kWh
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9243, {0, 60, 0, 10}, 0, ISOTP_EXTFRAME},     // Energy charged kWh
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9245, {0, 60, 10, 60}, 0, ISOTP_EXTFRAME},    // Energy discharged kWh
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9247, {0, 60, 10, 60}, 0, ISOTP_EXTFRAME},    // Energy regenerated kWh
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9210, {0, 600, 600, 600}, 0, ISOTP_EXTFRAME}, // Number of complete cycles
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9018, {0, 60, 10, 10}, 0, ISOTP_EXTFRAME},    // Max Charge Power
    // LBC Cell voltages and temperatures
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9131, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 1
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9132, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 2
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9133, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 3
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9134, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 4
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9135, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 5
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9136, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 6
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9137, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 7
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9138, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 8
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9139, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 9
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 10
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 11
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 12
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9021, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 1
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9022, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 2
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9023, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 3
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9024, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 4
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9025, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 5
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9026, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 6
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9027, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 7
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9028, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 8
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9029, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 9
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 10
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 11
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 12
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 13
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 14
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 15
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9030, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 16
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9031, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 17
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9032, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 18
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9033, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 19
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9034, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 20
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9035, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 21
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9036, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 22
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9037, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 23
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9038, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 24
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9039, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 25
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 26
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 27
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 28
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 29
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 30
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 31
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9041, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 32
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9042, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 33
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9043, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 34
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9044, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 35
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9045, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 36
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9046, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 37
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9047, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 38
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9048, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 39
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9049, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 40
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 41
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 42
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 43
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 44
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 45
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 46
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9050, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 47
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9051, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 48
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9052, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 49
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9053, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 50
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9054, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 51
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9055, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 52
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9056, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 53
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9057, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 54
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9058, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 55
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9059, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 56
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 57
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 58
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 59
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 60
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 61
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 62
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9061, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 63
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9062, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 64
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9063, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 65
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9064, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 66
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9065, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 67
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9066, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 68
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9067, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 69
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9068, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 70
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9069, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 71
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 72
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 73
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 74
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 75
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 76
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 77
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9070, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 78
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9071, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 79
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9072, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 80
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9073, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 81
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9074, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 82
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9075, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 83
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9076, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 84
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9077, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 85
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9078, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 86
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9079, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 87
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 88
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 89
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 90
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 91
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 92
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 93
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9081, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 94
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9082, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 95
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9083, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 96

    // HVAC
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4009, {0, 10, 60, 10}, 0, ISOTP_STD}, // Cabin temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x43D8, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor speed
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4404, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor speed request
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x440C, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor discharge temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x440D, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor suction temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4368, {0, 10, 5, 10}, 0, ISOTP_STD},  // Cooling evap setpoint temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x400A, {0, 10, 5, 10}, 0, ISOTP_STD},  // Cooling evap temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x43A5, {0, 10, 5, 10}, 0, ISOTP_STD},  // Heating cond setpoint temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4429, {0, 10, 5, 10}, 0, ISOTP_STD},  // Heating cond temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x43A4, {0, 10, 5, 10}, 0, ISOTP_STD},  // Heating temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4341, {0, 10, 5, 10}, 0, ISOTP_STD},  // Heating ptc request (0-5)
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4402, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor state
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4369, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor pressure in BAR
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4436, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor power

    // UCM
    {0x74D, 0x76D, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6079, {0, 10, 10, 10}, 0, ISOTP_STD}, // 12V Battery current

    POLL_LIST_END};

// Poller list for V-CAN connection, only missing metrics must be polled
static const OvmsPoller::poll_pid_t zoe_ph2_polls_vcan[] = {
    //***TX-ID, ***RX-ID, ***SID, ***PID, { Polltime (seconds) for Pollstate 0, 1, 2, 3}, ***CAN BUS Interface, ***FRAMETYPE

    // Motor Inverter
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x700C, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Inverter temperature
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x700F, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Stator Temperature 1
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7010, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Stator Temperature 2
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2004, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Battery voltage sense
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7049, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Current voltage sense
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7003, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Rotor current sensor 1
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7004, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Rotor current sensor 2
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7007, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Stator current phase U
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7008, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Stator current phase V
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7009, {0, 60, 3, 60}, 0, ISOTP_EXTFRAME},  // Stator current phase W
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7038, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Rotor temp raw
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7088, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME}, // Rotor temp estimated
    {0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7050, {0, 60, 60, 60}, 0, ISOTP_EXTFRAME}, // Rotor reference resistance

    // EVC-HCM-VCM
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2006, {0, 60, 10, 300}, 0, ISOTP_EXTFRAME},   // Odometer
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2218, {0, 20, 20, 20}, 0, ISOTP_EXTFRAME},    // Ambient air temperature
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x21CF, {0, 10, 10, 300}, 0, ISOTP_EXTFRAME},   // Inverter Status
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B7A, {0, 2, 0, 10}, 0, ISOTP_EXTFRAME},      // Charge type
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3064, {0, 10, 5, 10}, 0, ISOTP_EXTFRAME},     // Motor rpm
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x300D, {0, 10, 0, 3}, 0, ISOTP_EXTFRAME},      // AC mains current
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x300B, {0, 2, 0, 10}, 0, ISOTP_EXTFRAME},      // AC phases
    {0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B8A, {0, 2, 0, 10}, 0, ISOTP_EXTFRAME},      // AC mains voltage

    // BCM
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6310, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS temp - front left
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6311, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS temp - front right
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6312, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS temp - rear left
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6313, {0, 0, 300, 0}, 0, ISOTP_STD}, // TPMS temp - rear right
    {0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4060, {0, 600, 0, 0}, 0, ISOTP_STD}, // Vehicle identificaftion number

    // LBC
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9003, {0, 600, 600, 600}, 0, ISOTP_EXTFRAME}, // SOH
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x925D, {0, 10, 2, 2}, 0, ISOTP_EXTFRAME},      // Battery Current
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9012, {0, 10, 10, 10}, 0, ISOTP_EXTFRAME},    // Battery Average Temperature
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9243, {0, 60, 0, 10}, 0, ISOTP_EXTFRAME},     // Energy charged kWh
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9245, {0, 60, 10, 60}, 0, ISOTP_EXTFRAME},    // Energy discharged kWh
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9247, {0, 60, 10, 60}, 0, ISOTP_EXTFRAME},    // Energy regenerated kWh
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9210, {0, 600, 600, 600}, 0, ISOTP_EXTFRAME}, // Number of complete cycles
    // LBC Cell voltages and temperatures
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9131, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 1
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9132, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 2
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9133, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 3
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9134, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 4
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9135, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 5
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9136, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 6
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9137, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 7
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9138, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 8
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9139, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 9
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 10
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 11
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Pack temperature 12
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9021, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 1
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9022, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 2
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9023, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 3
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9024, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 4
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9025, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 5
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9026, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 6
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9027, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 7
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9028, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 8
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9029, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 9
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 10
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 11
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 12
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 13
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 14
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 15
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9030, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 16
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9031, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 17
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9032, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 18
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9033, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 19
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9034, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 20
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9035, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 21
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9036, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 22
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9037, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 23
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9038, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 24
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9039, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 25
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 26
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 27
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 28
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 29
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 30
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 31
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9041, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 32
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9042, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 33
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9043, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 34
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9044, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 35
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9045, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 36
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9046, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 37
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9047, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 38
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9048, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 39
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9049, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 40
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 41
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 42
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 43
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 44
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 45
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 46
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9050, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 47
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9051, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 48
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9052, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 49
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9053, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 50
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9054, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 51
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9055, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 52
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9056, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 53
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9057, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 54
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9058, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 55
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9059, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 56
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 57
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 58
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 59
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 60
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 61
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 62
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9061, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 63
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9062, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 64
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9063, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 65
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9064, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 66
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9065, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 67
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9066, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 68
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9067, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 69
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9068, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 70
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9069, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 71
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 72
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 73
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 74
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 75
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 76
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 77
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9070, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 78
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9071, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 79
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9072, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 80
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9073, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 81
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9074, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 82
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9075, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 83
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9076, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 84
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9077, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 85
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9078, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 86
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9079, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 87
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907A, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 88
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907B, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 89
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907C, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 90
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907D, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 91
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907E, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 92
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907F, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 93
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9081, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 94
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9082, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 95
    {0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9083, {0, 60, 300, 60}, 0, ISOTP_EXTFRAME}, // Cell voltage 96

    // HVAC
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4009, {0, 10, 60, 10}, 0, ISOTP_STD}, // Cabin temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x43D8, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor speed
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4404, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor speed request
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x440C, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor discharge temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x440D, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor suction temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4368, {0, 10, 5, 10}, 0, ISOTP_STD},  // Cooling evap setpoint temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x400A, {0, 10, 5, 10}, 0, ISOTP_STD},  // Cooling evap temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x43A5, {0, 10, 5, 10}, 0, ISOTP_STD},  // Heating cond setpoint temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4429, {0, 10, 5, 10}, 0, ISOTP_STD},  // Heating cond temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x43A4, {0, 10, 5, 10}, 0, ISOTP_STD},  // Heating temp
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4402, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor state
    {0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4369, {0, 10, 5, 10}, 0, ISOTP_STD},  // Compressor pressure in BAR

    POLL_LIST_END};

// Zoe wakeup macro, will be called if Zoe wakes up
void OvmsVehicleRenaultZoePh2::ZoeWakeUp()
{
  ESP_LOGI(TAG, "Zoe woke up...");
  mt_bus_awake->SetValue(true);
  StandardMetrics.ms_v_env_aux12v->SetValue(true);

  // Derive poller list from vcan_enabled setting - if using V-CAN connection we can skip polling metrics already found
  if (m_vcan_enabled)
  {
    PollSetPidList(m_can1, zoe_ph2_polls_vcan);
  }
  else
  {
    PollSetPidList(m_can1, zoe_ph2_polls_obd);
  }

  POLLSTATE_ON;
  ESP_LOGI(TAG, "Pollstate switched to ON");
}

// Zoe shutdown macro, will be called if Zoe goes to sleep, some metrics will be set to zero/off to have a clean state
void OvmsVehicleRenaultZoePh2::ZoeShutdown()
{
  POLLSTATE_OFF;
  mt_bus_awake->SetValue(false);
  StandardMetrics.ms_v_env_awake->SetValue(false);
  StandardMetrics.ms_v_pos_speed->SetValue(0);
  StandardMetrics.ms_v_bat_12v_current->SetValue(0);
  StandardMetrics.ms_v_charge_12v_current->SetValue(0);
  StandardMetrics.ms_v_bat_current->SetValue(0);
  StandardMetrics.ms_v_bat_power->SetValue(0);
  StandardMetrics.ms_v_env_hvac->SetValue(false);
  StandardMetrics.ms_v_env_aux12v->SetValue(false);
  ESP_LOGI(TAG, "Pollstate switched to OFF");

  // Check if car is locked, if not send notify
  if (!StandardMetrics.ms_v_env_locked->AsBool())
  {
    MyNotify.NotifyString("alert", "vehicle.lock", "Zoe is not locked");
  }
}

void OvmsVehicleRenaultZoePh2::Ticker1(uint32_t ticker)
{
  // OVMS metrics and poller logic from polled ECU status
  // It switches some metrics to predefined states, because if we use OBD port, we have no vehicle state if we dont poll
  if (StandardMetrics.ms_v_env_on->AsBool() && !CarIsDriving)
  {
    CarIsDriving = true;
    // Start a new trip after each ignition on, only if we cant get trip data from V-CAN
    if (!m_vcan_enabled)
    {
      StandardMetrics.ms_v_pos_trip->SetValue(0);
      mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0));
      StandardMetrics.ms_v_bat_energy_used->SetValue(0);
      StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
      if (m_UseBMScalculation)
      {
        mt_bat_used_start->SetValue(StandardMetrics.ms_v_bat_energy_used_total->AsFloat());
        mt_bat_recd_start->SetValue(StandardMetrics.ms_v_bat_energy_recd_total->AsFloat());
      }
      ESP_LOGD_RZ2(TAG, "Trip start, reset counters");
    }
  }
  // Poller state machine mainly for OBD connection, because the port is silent, all metrics rely on polling
  if (CarIsDriving && !CarIsDrivingInit)
  {
    CarIsDrivingInit = true;
    ESP_LOGI(TAG, "Pollstate switched to DRIVING");
    POLLSTATE_DRIVING;
  }
  if (!StandardMetrics.ms_v_env_on->AsBool() && CarIsDriving)
  {
    CarIsDriving = false;
    CarIsDrivingInit = false;

    // Save headlight state for coming home function (before door opens and turns them off)
    m_last_headlight_state = vcan_light_lowbeam;
    ESP_LOGD_RZ2(TAG, "Ignition turned off, saved headlight state: %s", m_last_headlight_state ? "ON" : "OFF");

    ESP_LOGI(TAG, "Pollstate switched to ON");
    POLLSTATE_ON;
  }
  if (StandardMetrics.ms_v_charge_pilot->AsBool() && StandardMetrics.ms_v_charge_inprogress->AsBool() && !CarIsCharging)
  {
    CarIsCharging = true;
    ESP_LOGI(TAG, "Pollstate switched to CHARGING");
    POLLSTATE_CHARGING;
  }
  else if (!StandardMetrics.ms_v_charge_pilot->AsBool() && CarIsCharging)
  {
    CarIsCharging = false;
    StandardMetrics.ms_v_charge_duration_full->SetValue(0);
    ESP_LOGI(TAG, "Pollstate switched to ON");
    POLLSTATE_ON;
  }
  else if (StandardMetrics.ms_v_charge_pilot->AsBool() && CarIsCharging && StandardMetrics.ms_v_charge_substate->AsString() == "powerwait")
  {
    CarIsCharging = false;
    StandardMetrics.ms_v_charge_duration_full->SetValue(0);
    if (!m_vcan_enabled)
    {
      ESP_LOGI(TAG, "Pollstate switched to OFF, Wait for power...");
      POLLSTATE_OFF;
      mt_bus_awake->SetValue(false);
      StandardMetrics.ms_v_env_awake->SetValue(false);
      StandardMetrics.ms_v_bat_12v_current->SetValue(0);
      StandardMetrics.ms_v_charge_12v_current->SetValue(0);
      StandardMetrics.ms_v_bat_current->SetValue(0);
      StandardMetrics.ms_v_env_aux12v->SetValue(false);
    }
  }
  else if (StandardMetrics.ms_v_charge_pilot->AsBool() && CarIsCharging && StandardMetrics.ms_v_charge_state->AsString() == "done")
  {
    CarIsCharging = false;
    StandardMetrics.ms_v_charge_duration_full->SetValue(0);
    if (!m_vcan_enabled)
    {
      ESP_LOGI(TAG, "Pollstate switched to OFF, done charging...");
      POLLSTATE_OFF;
      mt_bus_awake->SetValue(false);
      StandardMetrics.ms_v_env_awake->SetValue(false);
      StandardMetrics.ms_v_bat_12v_current->SetValue(0);
      StandardMetrics.ms_v_charge_12v_current->SetValue(0);
      StandardMetrics.ms_v_bat_current->SetValue(0);
      StandardMetrics.ms_v_env_aux12v->SetValue(false);
    }
  }

  // If Zoe is switched on, save odometer value to calculate trip distance, with V-CAN connection we use trip counter from Zoe
  if (StandardMetrics.ms_v_env_on->AsBool() && !m_vcan_enabled)
  {
    StandardMetrics.ms_v_pos_trip->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start->AsFloat(0));
  }

  // DC-DC power calculation from 12V voltage and DC-DC current (Zoe has only one current sensor)
  StandardMetrics.ms_v_charge_12v_power->SetValue(StandardMetrics.ms_v_bat_12v_voltage->AsFloat(0) * StandardMetrics.ms_v_bat_12v_current->AsFloat(0));

  // Range calculation
  if (StandardMetrics.ms_v_bat_capacity->IsDefined() && StandardMetrics.ms_v_bat_consumption->IsDefined() && StandardMetrics.ms_v_bat_soc->IsDefined() && StandardMetrics.ms_v_bat_soh->IsDefined())
  {
    // Ideal range from settings multiplied with SOC and SoH
    StandardMetrics.ms_v_bat_range_ideal->SetValue(((m_range_ideal * (StandardMetrics.ms_v_bat_soc->AsFloat(1) * 0.01)) * (StandardMetrics.ms_v_bat_soh->AsFloat(100) * 0.01)), Kilometers);

    // Estimated range, Last 100km average consumption compared through available capacity from BMS (temperature and soc dependent) if not connected to V-CAN
    if (!m_vcan_enabled)
    {
      if ((StandardMetrics.ms_v_bat_consumption->AsFloat(0, kWhP100K)) != 0)
      {
        StandardMetrics.ms_v_bat_range_est->SetValue(((StandardMetrics.ms_v_bat_capacity->AsFloat(0) / (StandardMetrics.ms_v_bat_consumption->AsFloat(1, kWhP100K))) * 100), Kilometers);
        ESP_LOGD_RZ2(TAG, "Avail bat capacity from BMS: %.1f kWh, Avg bat cons 200km: %.2f kWh/100km, range est calculated: %f km", StandardMetrics.ms_v_bat_capacity->AsFloat(), StandardMetrics.ms_v_bat_consumption->AsFloat(0, kWhP100K), StandardMetrics.ms_v_bat_range_est->AsFloat(0, Kilometers));
      }
      else
      {
        // Use factory average consumption, if 100km averaging consumption is zero
        StandardMetrics.ms_v_bat_range_est->SetValue((StandardMetrics.ms_v_bat_capacity->AsFloat(0) / 17.4 * 100), Kilometers);
      }
    }
    // Range full, ideal range from settings multiplied with SoH
    StandardMetrics.ms_v_bat_range_full->SetValue((m_range_ideal * (StandardMetrics.ms_v_bat_soh->AsFloat(100) * 0.01)), Kilometers);
  }
  else
  {
    // If metrics are not available yet, use ideal range from settings
    StandardMetrics.ms_v_bat_range_ideal->SetValue(m_range_ideal);
    StandardMetrics.ms_v_bat_range_est->SetValue(m_range_ideal);
    StandardMetrics.ms_v_bat_range_full->SetValue(m_range_ideal);
  }

  // HVAC Logic
  if (mt_hvac_compressor_power->AsFloat(0) > 0.1 || mt_hvac_heating_ptc_req->AsFloat(0) > 1 || (mt_hvac_compressor_mode->AsString() != "idle" && mt_hvac_compressor_mode->AsString() != ""))
  {
    StandardMetrics.ms_v_env_hvac->SetValue(true);
    // Cooling and heating metric
    if (mt_hvac_compressor_mode->AsString() == "AC mode")
    {
      StandardMetrics.ms_v_env_cooling->SetValue(true);
    }
    else
    {
      StandardMetrics.ms_v_env_cooling->SetValue(false);
    }
    if (mt_hvac_compressor_mode->AsString() == "Heat pump mode" || mt_hvac_heating_ptc_req->AsFloat(0) > 1)
    {
      StandardMetrics.ms_v_env_heating->SetValue(true);
    }
    else
    {
      StandardMetrics.ms_v_env_heating->SetValue(false);
    }
  }
  else
  {
    StandardMetrics.ms_v_env_hvac->SetValue(false);
    StandardMetrics.ms_v_env_cooling->SetValue(false);
    StandardMetrics.ms_v_env_heating->SetValue(false);
  }

  // V-CAN message counter to trigger poller
  if (m_vcan_enabled)
  {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (vcan_message_counter >= vcan_activity_threshold)
    {
      // Update last activity timestamp on any message activity
      vcan_lastActivityTime = currentTime;

      if (!vcan_poller_state)
      {
        ESP_LOGI(TAG, "V-CAN activity detected (Activity: %d msgs/sec)", vcan_message_counter);
        if (StandardMetrics.ms_v_bat_12v_voltage->AsFloat(0) > 12.0)
        {
          ZoeWakeUp();
          vcan_poller_state = true;
        }
        else
        {
          // OVMS should not keep Zoe alive if 12V battery voltage is too low
          ESP_LOGW(TAG, "Insufficient battery voltage (<12.0V), not enabling poller");
        }
      }
    }
    else
    {
      // If no activity, check inactivity timeout
      if (vcan_poller_state && ((currentTime - vcan_lastActivityTime) > vcan_poller_disable_delay))
      {
        ESP_LOGI(TAG, "V-CAN activity timeout, stopping poller to let Zoe sleep (Last Activity: %d msgs/sec)", vcan_message_counter);
        ZoeShutdown();
        vcan_poller_state = false;
      }
    }
    vcan_message_counter = 0; // Reset counter for the next second
  }

  // V-CAN metrics - save temporary values to OvmsMetrics
  if (m_vcan_enabled && vcan_poller_state)
  {
    SyncVCANtoMetrics();
  }

  // 12V auto-recharge monitoring - check if auto-recharge should be enabled/disabled
  CheckAutoRecharge();

  // Send DCDC message every second if active
  if (m_dcdc_active)
  {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if ((currentTime - m_dcdc_last_send_time) >= 1000)
    {
      SendDCDCMessage();
      m_dcdc_last_send_time = currentTime;
    }
  }

  // Respect HVAC blower state for both manual and automatic PTC activation:
  // if the blower is off, force the PTC heaters off as well.
  if (vcan_hvac_blower == 0)
  {
    if (m_auto_ptc_currently_enabled)
    {
      ESP_LOGI(TAG, "PTC disable: Cabin blower is off, disabling PTC heaters");
      bool success = DisablePTCInternal();

      if (success)
      {
        ESP_LOGI(TAG, "PTC disable: PTC heaters disabled successfully because blower is off");
        m_auto_ptc_currently_enabled = false;
        mt_auto_ptc_state->SetValue(false);
      }
      else
      {
        ESP_LOGW(TAG, "PTC disable: Failed to disable PTC heaters while blower is off");
      }
    }
  }

  // Auto-enable PTC based on outside temperature
  if (m_auto_ptc_enabled &&
      StandardMetrics.ms_v_env_on->AsBool() &&  // Ignition ON
      vcan_hvac_blower > 0 &&                   // Blower ON
      !StandardMetrics.ms_v_charge_inprogress->AsBool())  // Not charging
  {
    // Get outside temperature
    float outside_temp = StandardMetrics.ms_v_env_temp->AsFloat(-999.0);
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (outside_temp != -999.0 &&
        outside_temp >= m_auto_ptc_temp_min &&
        outside_temp <= m_auto_ptc_temp_max &&
        !m_auto_ptc_currently_enabled)
    {
      // Debouncing: only try to enable PTCs once per 10 seconds
      if ((currentTime - m_auto_ptc_last_enable_time) >= 10000)
      {
        if (!m_auto_ptc_currently_enabled)
        {
          ESP_LOGI(TAG, "Auto-enable PTC: Outside temp %.1f°C in range [%.1f - %.1f°C], enabling PTCs",
                   outside_temp, m_auto_ptc_temp_min, m_auto_ptc_temp_max);

          // Enable PTCs in background
          bool success = EnablePTCInternal();

          if (success)
          {
            ESP_LOGI(TAG, "Auto-enable PTC: PTCs enabled successfully (temp: %.1f°C)", outside_temp);
            m_auto_ptc_currently_enabled = true;
            mt_auto_ptc_state->SetValue(true);
          }
          else
          {
            ESP_LOGW(TAG, "Auto-enable PTC: Failed to enable PTCs");
          }

          // Update timestamp to prevent repeated attempts
          m_auto_ptc_last_enable_time = currentTime;
        }
        else
        {
          ESP_LOGD_RZ2(TAG, "Auto-enable PTC: PTCs already active, skipping");
        }
      }
    }
  }

  // After-ignition keep-alive detection and timeout
  bool current_ignition_state = StandardMetrics.ms_v_env_on->AsBool();

  // Detect ignition OFF transition (edge: ON -> OFF)
  if (m_last_ignition_state && !current_ignition_state)
  {
    ESP_LOGI(TAG, "Ignition OFF transition detected");

    // Auto-disable PTCs on ignition off (before after-ignition keep-alive)
    // Note: Compressor is NOT auto-disabled (it's a config change, should persist)
    if (m_auto_ptc_currently_enabled)
    {
      ESP_LOGI(TAG, "Auto-disable PTC: PTCs active, disabling on ignition OFF");
      bool success = DisablePTCInternal();

      if (success)
      {
        ESP_LOGI(TAG, "Auto-disable PTC: PTCs disabled successfully on ignition OFF");
        m_auto_ptc_currently_enabled = false;
        mt_auto_ptc_state->SetValue(false);
      }
      else
      {
        ESP_LOGW(TAG, "Auto-disable PTC: Failed to disable PTCs on ignition OFF");
      }
    }
    else
    {
      ESP_LOGD_RZ2(TAG, "Auto-disable PTC: PTCs not active, skipping auto-disable");
    }

    // If after-ignition keep-alive is configured and enabled
    if (m_dcdc_after_ignition_time > 0 && m_vcan_enabled)
    {
      ESP_LOGI(TAG, "DCDC: After-ignition config check - time=%dmin, vcan=%d, dcdc_active=%d, charging=%d, headlights=%d",
               m_dcdc_after_ignition_time,
               m_vcan_enabled,
               m_dcdc_active,
               StandardMetrics.ms_v_charge_inprogress->AsBool(),
               vcan_light_lowbeam);

      // Only activate if DCDC is not already running (manual/auto) and NOT charging
      if (!m_dcdc_active && !StandardMetrics.ms_v_charge_inprogress->AsBool())
      {
        // Check if headlights are still on
        if (vcan_light_lowbeam)
        {
          // Headlights are on, set pending flag instead of immediately starting keep-alive
          ESP_LOGI(TAG, "DCDC: Headlights still on, waiting for them to turn off before starting keep-alive");
          m_dcdc_after_ignition_pending = true;
          m_dcdc_after_ignition_pending_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
        }
        else
        {
          // No headlights, start keep-alive immediately
          ESP_LOGI(TAG, "DCDC: Starting after-ignition keep-alive for %d minutes", m_dcdc_after_ignition_time);
          m_dcdc_after_ignition_active = true;
          m_dcdc_after_ignition_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

          // Enable DCDC in auto mode (not manual)
          bool enable_result = EnableDCDC(false);
          if (enable_result)
          {
            ESP_LOGI(TAG, "DCDC: After-ignition activation successful");
          }
          else
          {
            ESP_LOGW(TAG, "DCDC: After-ignition activation failed");
            m_dcdc_after_ignition_active = false;  // Clear flag if activation failed
          }
        }
      }
      else
      {
        if (m_dcdc_active)
        {
          ESP_LOGI(TAG, "DCDC: Already active, skipping after-ignition keep-alive");
        }
        if (StandardMetrics.ms_v_charge_inprogress->AsBool())
        {
          ESP_LOGI(TAG, "DCDC: Charging in progress, skipping after-ignition keep-alive");
        }
      }
    }
    else
    {
      if (m_dcdc_after_ignition_time == 0)
      {
        ESP_LOGD_RZ2(TAG, "DCDC: After-ignition disabled (time=0), not activating");
      }
      if (!m_vcan_enabled)
      {
        ESP_LOGD_RZ2(TAG, "DCDC: V-CAN not enabled, not activating after-ignition");
      }
    }
  }

  // Update last ignition state for next cycle
  m_last_ignition_state = current_ignition_state;

  // Handle pending after-ignition keep-alive (waiting for headlights to turn off)
  if (m_dcdc_after_ignition_pending && !current_ignition_state)
  {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t wait_time = currentTime - m_dcdc_after_ignition_pending_start;

    // If headlights turned off, start keep-alive
    if (!vcan_light_lowbeam)
    {
      ESP_LOGI(TAG, "DCDC: Headlights turned off, starting after-ignition keep-alive for %d minutes", m_dcdc_after_ignition_time);
      m_dcdc_after_ignition_pending = false;
      m_dcdc_after_ignition_active = true;
      m_dcdc_after_ignition_start_time = currentTime;

      // Enable DCDC in auto mode (not manual)
      bool enable_result = EnableDCDC(false);
      if (enable_result)
      {
        ESP_LOGI(TAG, "DCDC: After-ignition activation successful (after headlights off)");
      }
      else
      {
        ESP_LOGW(TAG, "DCDC: After-ignition activation failed (after headlights off)");
        m_dcdc_after_ignition_active = false;
      }
    }
    // Timeout after 60 seconds - DCDC likely already off
    else if (wait_time > 60000)
    {
      ESP_LOGW(TAG, "DCDC: Headlight wait timeout (60s), cancelling after-ignition keep-alive");
      m_dcdc_after_ignition_pending = false;
    }
  }

  // Check after-ignition keep-alive timeout
  if (m_dcdc_after_ignition_active)
  {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed_time = currentTime - m_dcdc_after_ignition_start_time;
    uint32_t timeout_ms = m_dcdc_after_ignition_time * 60 * 1000;

    ESP_LOGD_RZ2(TAG, "DCDC: After-ignition timeout check - elapsed=%us/%us",
             elapsed_time / 1000, timeout_ms / 1000);

    if (elapsed_time >= timeout_ms)
    {
      ESP_LOGI(TAG, "DCDC: After-ignition keep-alive timeout (%d min), stopping", m_dcdc_after_ignition_time);
      m_dcdc_after_ignition_active = false;
      DisableDCDC();
    }
  }

  // Coming home function - monitor lock state changes
  bool current_locked_state = StandardMetrics.ms_v_env_locked->AsBool();
  if (current_locked_state && !m_last_locked_state)
  {
    // Car just got locked (transition from unlocked to locked)
    ESP_LOGD_RZ2(TAG, "Lock state changed: unlocked -> locked");
    TriggerComingHomeLighting();
  }
  m_last_locked_state = current_locked_state;

  // PV Charging notification mode - track plug state changes using charge pilot
  if (m_pv_charge_notification_enabled)
  {
    bool current_pilot = StandardMetrics.ms_v_charge_pilot->AsBool();

    // Detect plug-in event (false -> true)
    if (current_pilot && !m_pv_charge_last_pilot)
    {
      ESP_LOGI(TAG, "PV Charge: Cable plugged in, starting session tracking");
      m_pv_charge_session_active = true;
      m_pv_charge_kwh_total = 0.0;
      m_pv_charge_session_count = 0;
      m_pv_charge_soc_start = StandardMetrics.ms_v_bat_soc->AsFloat(0);
      m_pv_charge_plug_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    // Detect unplug event (true -> false)
    else if (!current_pilot && m_pv_charge_last_pilot)
    {
      ESP_LOGI(TAG, "PV Charge: Cable unplugged, ending session tracking");

      // Only send summary if we actually had charge sessions
      if (m_pv_charge_session_count > 0)
      {
        // Calculate duration
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t duration_ms = current_time - m_pv_charge_plug_time;
        int duration_minutes = duration_ms / 60000;
        int duration_hours = duration_minutes / 60;
        duration_minutes = duration_minutes % 60;

        // Get final SOC
        float soc_end = StandardMetrics.ms_v_bat_soc->AsFloat(0);

        // Format and send summary notification
        char summary[256];
        if (duration_hours > 0)
        {
          snprintf(summary, sizeof(summary),
                   "PV Charge Summary: %.2f kWh total over %d session(s)\n"
                   "SOC: %.0f%% -> %.0f%% (+%.0f%%)\n"
                   "Duration: %dh %dm",
                   m_pv_charge_kwh_total, m_pv_charge_session_count,
                   m_pv_charge_soc_start, soc_end, soc_end - m_pv_charge_soc_start,
                   duration_hours, duration_minutes);
        }
        else
        {
          snprintf(summary, sizeof(summary),
                   "PV Charge Summary: %.2f kWh total over %d session(s)\n"
                   "SOC: %.0f%% -> %.0f%% (+%.0f%%)\n"
                   "Duration: %dm",
                   m_pv_charge_kwh_total, m_pv_charge_session_count,
                   m_pv_charge_soc_start, soc_end, soc_end - m_pv_charge_soc_start,
                   duration_minutes);
        }

        ESP_LOGI(TAG, "PV Charge: Sending summary - %s", summary);
        MyNotify.NotifyString("info", "charge.pv.summary", summary);

        // Reset counters but keep m_pv_charge_session_active = true
        // to suppress any delayed notifications after unplug
        m_pv_charge_kwh_total = 0.0;
        m_pv_charge_session_count = 0;
      }
      else
      {
        ESP_LOGD_RZ2(TAG, "PV Charge: No charge sessions occurred, skipping summary");
      }
      // Note: m_pv_charge_session_active stays true to suppress delayed notifications
      // It will be reset on next plug-in event
    }

    m_pv_charge_last_pilot = current_pilot;
  }
}

void OvmsVehicleRenaultZoePh2::Ticker10(uint32_t ticker)
{
  // Charge statistics calculation if Zoe is charging
  if (StandardMetrics.ms_v_charge_pilot->AsBool() && !StandardMetrics.ms_v_env_on->AsBool())
  {
    ChargeStatistics();
  }
  else
  {
    // Else calculate energy usage for trip, if LBC firmware is older than 530 these values are not correct, so calculation within OVMS is neccessary
    if (m_UseBMScalculation)
    {
      EnergyStatisticsBMS();
    }
    else
    {
      EnergyStatisticsOVMS();
    }
    // Update usable capacity from SoH value (only when SOH changes)
    float current_soh = StandardMetrics.ms_v_bat_soh->AsFloat(100);
    if (current_soh != m_last_soh_value)
    {
      m_last_soh_value = current_soh;

      if (m_battery_capacity == 52000)
      {
        Bat_cell_capacity_Ah = 78.0 * 2 * (current_soh / 100.0);
      }
      else if (m_battery_capacity == 41000)
      {
        Bat_cell_capacity_Ah = 65.6 * 2 * (current_soh / 100.0);
      }

      ESP_LOGD_RZ2(TAG, "SOH changed to %.2f%%, updated usable capacity to %.2f Ah",
                   current_soh, Bat_cell_capacity_Ah);
    }
  }

  // Fix rare race condition, if Zoe is connected to disabled charging point (for example PV charging), OVMS can poll without NAK till 12V battery dies
  // this will not prevent 12V battery depleting, but reduces current and thus runtime till powerloss.
  // A user need to interrupt this situation either through power on or start charging, may depend on EVC firmware version
  if (!m_vcan_enabled && mt_bus_awake->AsBool() && StandardMetrics.ms_v_charge_substate->AsString() == "powerwait" && StandardMetrics.ms_v_bat_12v_voltage->AsFloat() < 12.5)
  {
    ESP_LOGI(TAG, "Zoe is waiting for power, switch off OVMS poller, to let Zoe sleep.");
    ZoeShutdown();
  }

  // Detect 12V aux battery charging from dcdc current
  if (StandardMetrics.ms_v_charge_12v_current->AsFloat(0) > 5)
  {
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
  }
  else
  {
    StandardMetrics.ms_v_env_charging12v->SetValue(false);
  }

  // Long PreConditioning retrigger logic, retrigger PreCond before DC-DC shutdown and HV bat contactors opens to reduce wear
  if (LongPreCond)
  {
    bool hvac_on = StandardMetrics.ms_v_env_hvac->AsBool();

    // Track HVAC state changes
    if (hvac_on && LongPreCondWaitingForHvacOff)
    {
      // HVAC successfully turned on, now wait for it to turn off
      ESP_LOGD_RZ2(TAG, "LongPreCond: HVAC turned on, now monitoring for shutdown");
      LongPreCondWaitingForHvacOff = false;
    }
    else if (!hvac_on && !LongPreCondWaitingForHvacOff && LongPreCondCounter > 0)
    {
      // HVAC is off and we're not waiting for it to turn on, time to retrigger
      // Add small debounce: only retrigger if HVAC has been off for at least 10 seconds
      uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
      uint32_t time_since_last_trigger = current_time - LongPreCondLastTrigger;

      if (time_since_last_trigger >= 10000 || LongPreCondLastTrigger == 0)
      {
        ESP_LOGI(TAG, "LongPreCond: HVAC turned off, retriggering climate control (counter=%d)",
                 LongPreCondCounter);
        LongPreCondCounter--;
        CommandClimateControl(true);
        LongPreCondLastTrigger = current_time;
        LongPreCondWaitingForHvacOff = true;  // Now waiting for HVAC to turn on
      }
      else
      {
        ESP_LOGD_RZ2(TAG, "LongPreCond: HVAC off but debounce active (time_since_last=%ums)",
                     time_since_last_trigger);
      }
    }

    // Disable and reset LongPreCond logic if counter reaches zero or Zoe unlocked
    if (LongPreCondCounter <= 0 || !StandardMetrics.ms_v_env_locked->AsBool())
    {
      LongPreCond = false;
      LongPreCondCounter = 0;
      LongPreCondLastTrigger = 0;
      LongPreCondWaitingForHvacOff = false;
    }
  }
}

// TX callback to track actual CAN frame transmission success/failure, this is needed because of the heavy bus load (1700-2200msg/s)
void OvmsVehicleRenaultZoePh2::TxCallback(const CAN_frame_t* frame, bool success)
{
  // Only track frames on CAN1 (V-CAN)
  if (frame->origin == m_can1)
  {
    // Store the frame ID and success status atomically
    m_tx_success_id.store(frame->MsgID);
    m_tx_success_flag.store(success);
  }
}

// Handles incoming poll results on bus 1
void OvmsVehicleRenaultZoePh2::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t *data, uint8_t length)
{
  string &rxbuf = zoe_obd_rxbuf;

  // ESP_LOGV_RZ2(TAG, "pid: %04x length: %d m_poll_ml_remain: %d mlframe: %d", pid, length, m_poll_ml_remain, m_poll_ml_frame);

  if (job.mlframe == 0)
  {
    rxbuf.clear();
    rxbuf.reserve(length + job.mlremain);
  }
  rxbuf.append((char *)data, length);

  if (job.mlremain)
    return;

  switch (job.moduleid_rec)
  {
  // ****** Motor-Inverter / Power electric block (PEB) *****
  case 0x18daf1df:
    IncomingINV(job.type, job.pid, rxbuf.data(), rxbuf.size());
    break;
  // ****** Eletric vehicle controller (EVC aka "Engine ECU") *****
  case 0x18daf1da:
    IncomingEVC(job.type, job.pid, rxbuf.data(), rxbuf.size());
    break;
  // ****** Body control module (UCH) *****
  case 0x765:
    IncomingBCM(job.type, job.pid, rxbuf.data(), rxbuf.size());
    break;
  // ****** Lithium battery controller (LBC aka BMS) *****
  case 0x18daf1db:
    IncomingLBC(job.type, job.pid, rxbuf.data(), rxbuf.size());
    break;
  // ****** ClimBox Gen3 (aka HVAC ECU) *****
  case 0x764:
    IncomingHVAC(job.type, job.pid, rxbuf.data(), rxbuf.size());
    break;
  // ****** Switching and protection box (UPC / UCM) *****
  case 0x76D:
    IncomingUCM(job.type, job.pid, rxbuf.data(), rxbuf.size());
    break;
  }
}

// === PV CHARGING NOTIFICATION OVERRIDES ===
// These methods override the base class to suppress individual charge notifications
// when PV/solar charging mode is enabled. Instead, a summary is sent on cable unplug.

void OvmsVehicleRenaultZoePh2::NotifyChargeStart()
{
  // If PV mode is active and cable is plugged in, suppress notification and track session
  if (m_pv_charge_notification_enabled && m_pv_charge_session_active)
  {
    m_pv_charge_session_count++;
    ESP_LOGI(TAG, "PV Charge: Charge session #%d started, notification suppressed", m_pv_charge_session_count);
    return;
  }

  // Normal behavior: call base class
  OvmsVehicle::NotifyChargeStart();
}

void OvmsVehicleRenaultZoePh2::NotifyChargeStopped()
{
  // If PV mode is active and cable is plugged in, suppress notification and accumulate kWh
  if (m_pv_charge_notification_enabled && m_pv_charge_session_active)
  {
    float session_kwh = StandardMetrics.ms_v_charge_kwh->AsFloat(0);
    m_pv_charge_kwh_total += session_kwh;
    ESP_LOGI(TAG, "PV Charge: Charge stopped, session %.2f kWh added, total %.2f kWh, notification suppressed",
             session_kwh, m_pv_charge_kwh_total);
    return;
  }

  // Normal behavior: call base class
  OvmsVehicle::NotifyChargeStopped();
}

void OvmsVehicleRenaultZoePh2::NotifyChargeDone()
{
  // If PV mode is active and cable is plugged in, suppress notification and accumulate kWh
  if (m_pv_charge_notification_enabled && m_pv_charge_session_active)
  {
    float session_kwh = StandardMetrics.ms_v_charge_kwh->AsFloat(0);
    m_pv_charge_kwh_total += session_kwh;
    ESP_LOGI(TAG, "PV Charge: Charge done, session %.2f kWh added, total %.2f kWh, notification suppressed",
             session_kwh, m_pv_charge_kwh_total);
    return;
  }

  // Normal behavior: call base class
  OvmsVehicle::NotifyChargeDone();
}
