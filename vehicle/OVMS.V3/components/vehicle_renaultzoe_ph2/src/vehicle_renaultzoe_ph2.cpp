/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th Mar 2025
;
;    (C) 2022       Carsten Schmiemann
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
  // Poller and trip calculation metrics
  mt_poller_inhibit = MyMetrics.InitBool("xrz2.v.poller.inhibit", SM_STALE_NONE, false, Other, true);
  mt_pos_odometer_start = MyMetrics.InitFloat("xrz2.v.pos.odometer.start", SM_STALE_NONE, 0, Kilometers, true);

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

  // Debug output of custom functions
  cmd_xrz2->RegisterCommand("debug", "Debug output of custom functions", CommandDebug);

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
  MyPollers.AddFilter(1, 0x18daf1da, 0x18daf1df);
  MyPollers.AddFilter(1, 0x763, 0x765);
  MyPollers.AddFilter(1, 0x76D, 0x76D);

  // CAN1 Software filter - V-CAN PIDs
  MyPollers.AddFilter(1, 0x0A4, 0x0A4);
  MyPollers.AddFilter(1, 0x3C0, 0x3C3);
  MyPollers.AddFilter(1, 0x418, 0x418);
  MyPollers.AddFilter(1, 0x46F, 0x46F);
  MyPollers.AddFilter(1, 0x437, 0x437);
  MyPollers.AddFilter(1, 0x480, 0x480);
  MyPollers.AddFilter(1, 0x491, 0x491);
  MyPollers.AddFilter(1, 0x43E, 0x43E);
  MyPollers.AddFilter(1, 0x46C, 0x46C);
  MyPollers.AddFilter(1, 0x47C, 0x47F);
  MyPollers.AddFilter(1, 0x5B9, 0x5BA);
  MyPollers.AddFilter(1, 0x5C2, 0x5C5);
  MyPollers.AddFilter(1, 0x625, 0x625);
  MyPollers.AddFilter(1, 0x6C9, 0x6C9);
  MyPollers.AddFilter(1, 0x6E9, 0x6E9);
  MyPollers.AddFilter(1, 0x6F2, 0x6F2);

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

  ESP_LOGI(TAG, "Renault Zoe Ph2 reload configuration: Range ideal: %d, Battery capacity: %d, Use BMS as energy counter: %s, V-CAN connected: %s", m_range_ideal, m_battery_capacity, m_UseBMScalculation ? "yes" : "no", m_vcan_enabled ? "yes" : "no");
  ESP_LOGI(TAG, "12V auto-recharge: %s, Threshold: %.1fV", m_auto_12v_recharge_enabled ? "enabled" : "disabled", m_auto_12v_threshold);
  ESP_LOGI(TAG, "Coming home function: %s", m_coming_home_enabled ? "enabled" : "disabled");
  ESP_LOGI(TAG, "Remote climate trigger: %s", m_remote_climate_enabled ? "enabled" : "disabled");
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
      ESP_LOGD(TAG, "Trip start, reset counters");
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
    ESP_LOGD(TAG, "Ignition turned off, saved headlight state: %s", m_last_headlight_state ? "ON" : "OFF");

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
        ESP_LOGD(TAG, "Avail bat capacity from BMS: %.1f kWh, Avg bat cons 200km: %.2f kWh/100km, range est calculated: %f km", StandardMetrics.ms_v_bat_capacity->AsFloat(), StandardMetrics.ms_v_bat_consumption->AsFloat(0, kWhP100K), StandardMetrics.ms_v_bat_range_est->AsFloat(0, Kilometers));
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

  // Coming home function - monitor lock state changes
  bool current_locked_state = StandardMetrics.ms_v_env_locked->AsBool();
  if (current_locked_state && !m_last_locked_state)
  {
    // Car just got locked (transition from unlocked to locked)
    ESP_LOGD(TAG, "Lock state changed: unlocked -> locked");
    TriggerComingHomeLighting();
  }
  m_last_locked_state = current_locked_state;
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
    // Update usable capacity from SoH value
    if (m_battery_capacity == 52000)
    {
      Bat_cell_capacity_Ah = 78.0 * 2 * (StandardMetrics.ms_v_bat_soh->AsFloat() / 100.0);
    }
    if (m_battery_capacity == 41000)
    {
      Bat_cell_capacity_Ah = 65.6 * 2 * (StandardMetrics.ms_v_bat_soh->AsFloat() / 100.0);
    }
    ESP_LOGD(TAG, "Update usable battery capacity from SoH: %.2f Ah", Bat_cell_capacity_Ah);
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
    if (LongPreCondCounter != 0 && !StandardMetrics.ms_v_env_hvac->AsBool())
    {
      LongPreCondCounter--;
      CommandClimateControl(true);
    }
    // Disable and reset LongPreCond logic if counter reaches zero or Zoe unlocked
    else if (LongPreCondCounter <= 0 || !StandardMetrics.ms_v_env_locked->AsBool())
    {
      LongPreCond = false;
      LongPreCondCounter = 0;
    }
  }
}

// Average rolling consumption over last 100km, if we have a V-CAN connection we use consumption from Zoe
void OvmsVehicleRenaultZoePh2::CalculateEfficiency()
{
  if (!m_vcan_enabled)
  {
    float speed = StdMetrics.ms_v_pos_speed->AsFloat();
    float power = StdMetrics.ms_v_bat_power->AsFloat(0, Watts);
    float distanceTraveled = speed / 3600.0f; // km/h to km/s

    float consumption = 0;
    if (speed >= 10)
    {
      consumption = power / speed; // Wh/km
    }

    // Only record valid entries
    if (distanceTraveled > 0 && std::isfinite(consumption))
    {
      consumptionHistory.push_back(std::make_pair(distanceTraveled, consumption));
      totalConsumption += consumption * distanceTraveled;
      totalDistance += distanceTraveled;
    }

    // Trim excess distance from front of buffer when over 100 km
    // Use 101.0 km as safe buffer to avoid edge hover
    while (totalDistance > 101.0f && !consumptionHistory.empty())
    {
      auto oldest = consumptionHistory.front();
      totalDistance -= oldest.first;
      totalConsumption -= oldest.first * oldest.second;
      consumptionHistory.pop_front();
    }

    // Ensure numbers stay clean
    if (totalDistance < 0 || !std::isfinite(totalDistance))
      totalDistance = 0;
    if (totalConsumption < 0 || !std::isfinite(totalConsumption))
      totalConsumption = 0;

    // Calculate and set average
    float avgConsumption = (totalDistance > 0) ? totalConsumption / totalDistance : 0;
    StdMetrics.ms_v_bat_consumption->SetValue(TRUNCPREC(avgConsumption, 1));

    // Logging
    ESP_LOGD(TAG, "Average battery consumption: %.1f Wh/km (over %.1f km, buffer size: %zu)",
             avgConsumption, totalDistance, consumptionHistory.size());

    // Hard reset if buffer grows out of control
    if (consumptionHistory.size() > 10000)
    {
      ESP_LOGW(TAG, "Consumption history too large (%.1f km stored, %zu entries) — resetting!",
               totalDistance, consumptionHistory.size());
      consumptionHistory.clear();
      totalDistance = 0;
      totalConsumption = 0;
    }
  }
}

// Handle Charge ernergy, charge time remaining and efficiency calucation
void OvmsVehicleRenaultZoePh2::ChargeStatistics()
{
  float charge_current = fabs(StandardMetrics.ms_v_bat_current->AsFloat(0, Amps));
  float charge_voltage = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float battery_power = fabs(StandardMetrics.ms_v_bat_power->AsFloat(0, kW));
  float charger_power = StandardMetrics.ms_v_charge_power->AsFloat(1, kW);
  float ac_current = StandardMetrics.ms_v_charge_current->AsFloat(0, Amps);
  string ac_phases = mt_main_phases->AsString();
  int minsremaining = 0;

  if (CarIsCharging != CarLastCharging)
  {
    if (CarIsCharging)
    {
      StandardMetrics.ms_v_charge_kwh->SetValue(0);
      StandardMetrics.ms_v_charge_inprogress->SetValue(CarIsCharging);
      if (m_UseBMScalculation)
      {
        mt_bat_chg_start->SetValue(StandardMetrics.ms_v_charge_kwh_grid_total->AsFloat());
      }
    }
    else
    {
      StandardMetrics.ms_v_charge_inprogress->SetValue(CarIsCharging);
    }
  }
  CarLastCharging = CarIsCharging;

  if (!StandardMetrics.ms_v_charge_pilot->AsBool() || !StandardMetrics.ms_v_charge_inprogress->AsBool() || (charge_current <= 0.0))
  {
    return;
  }

  if (charge_voltage > 0 && charge_current > 0)
  {
    float power = charge_voltage * charge_current / 1000.0;
    float energy = power / 3600.0 * 10.0;
    float c_efficiency;

    if (m_UseBMScalculation)
    {
      StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh_grid_total->AsFloat() - mt_bat_chg_start->AsFloat());
    }
    else
    {
      StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat() + energy);
    }

    // Calculate charge remaining time if we cannot get it from V-CAN
    if (m_vcan_enabled)
    {
      if (vcan_hvBat_ChargeTimeRemaining < 2550) // If charging is not started we got 2555 value from V-CAN
      {
        StandardMetrics.ms_v_charge_duration_full->SetValue(vcan_hvBat_ChargeTimeRemaining, Minutes);
        minsremaining = vcan_hvBat_ChargeTimeRemaining;
      }
    }
    else
    {
      if (charge_voltage != 0 && charge_current != 0)
      {
        minsremaining = calcMinutesRemaining(charge_voltage, charge_current);
        StandardMetrics.ms_v_charge_duration_full->SetValue(minsremaining, Minutes);
      }
    }

    if (StandardMetrics.ms_v_charge_type->AsString() == "type2")
    {
      if (charger_power != 0)
      {
        c_efficiency = (battery_power / charger_power) * 100.0;
        if (c_efficiency < 100.0)
        {
          StandardMetrics.ms_v_charge_efficiency->SetValue(c_efficiency);
        }
      }
      ESP_LOGI(TAG, "Charge time remaining: %d mins, AC Charge at %.2f kW with %.1f amps, %s at %.1f efficiency, %.2f powerfactor", minsremaining, charger_power, ac_current, ac_phases.c_str(), StandardMetrics.ms_v_charge_efficiency->AsFloat(100), ACInputPowerFactor);
    }
    else if (StandardMetrics.ms_v_charge_type->AsString() == "ccs" || StandardMetrics.ms_v_charge_type->AsString() == "chademo")
    {
      StandardMetrics.ms_v_charge_power->SetValue(battery_power);
      ESP_LOGI(TAG, "Charge time remaining: %d mins, DC Charge at %.2f kW", minsremaining, battery_power);
    }
  }
}

// Charge time remaining - helper function to get remaining charge time for ChargeStatistics
int OvmsVehicleRenaultZoePh2::calcMinutesRemaining(float charge_voltage, float charge_current)
{
  float bat_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
  float real_capacity = m_battery_capacity * (StandardMetrics.ms_v_bat_soh->AsFloat(100) / 100);

  float remaining_wh = real_capacity - (real_capacity * bat_soc / 100.0);
  float remaining_hours = remaining_wh / (charge_current * charge_voltage);
  float remaining_mins = remaining_hours * 60.0;

  ESP_LOGD(TAG, "SOC: %f, BattCap:%d, Current: %f, Voltage: %f, RemainWH: %f, RemainHour: %f, RemainMin: %f", bat_soc, m_battery_capacity, charge_current, charge_voltage, remaining_wh, remaining_hours, remaining_mins);
  return MIN(1440, (int)remaining_mins);
}

// Energy values calculation withing OVMS
void OvmsVehicleRenaultZoePh2::EnergyStatisticsOVMS()
{
  float voltage = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float current = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);

  float power = voltage * current / 1000.0;

  if (power != 0.0 && StandardMetrics.ms_v_env_on->AsBool())
  {
    float energy = power / 3600.0 * 10.0;
    if (energy > 0.0f)
      StandardMetrics.ms_v_bat_energy_used->SetValue(StandardMetrics.ms_v_bat_energy_used->AsFloat() + energy);
    else
      StandardMetrics.ms_v_bat_energy_recd->SetValue(StandardMetrics.ms_v_bat_energy_recd->AsFloat() - energy);
  }
}

// Energy values from BMS
void OvmsVehicleRenaultZoePh2::EnergyStatisticsBMS()
{
  if (StandardMetrics.ms_v_bat_energy_used_total->AsFloat() != 0 && StandardMetrics.ms_v_bat_energy_recd_total->AsFloat() != 0)
  {
    StandardMetrics.ms_v_bat_energy_used->SetValue(StandardMetrics.ms_v_bat_energy_used_total->AsFloat() - mt_bat_used_start->AsFloat());
    StandardMetrics.ms_v_bat_energy_recd->SetValue(StandardMetrics.ms_v_bat_energy_recd_total->AsFloat() - mt_bat_recd_start->AsFloat());
  }
}

// Handles incoming CAN-frames on bus 1
void OvmsVehicleRenaultZoePh2::IncomingFrameCan1(CAN_frame_t *p_frame)
{
  uint8_t *data = p_frame->data.u8;

  if (!mt_poller_inhibit->AsBool())
  // Inhibit poller if metric is set to true, for vehicle maintenance to not intefere with tester,
  // Renault shops typically stop working on cars with CAN-bus attached 3rd party "accessories" and blame them for all malfunctions
  {
    if (m_vcan_enabled)
    {
      // V-CAN wakeup and sleep detection by counting frames. No rely on TCU required. V-CAN is heavily loaded, about 1600 msgs/s full on
      vcan_message_counter++;
    }
    else
    {
      // OBD port wakeup and sleep detection, we need to rely on a TCU request
      if (mt_bus_awake->AsBool() && data[0] == 0x83 && data[1] == 0xc0)
      {
        ESP_LOGI(TAG, "Zoe wants to sleep (ECU wake up request detected)");
        ZoeShutdown();
      }
      else if (!mt_bus_awake->AsBool() && p_frame->MsgID == 0x18daf1db)
      {
        // Only start polling if TCU request to battery is incoming, ignore late poll replies if NAK message is received
        // The TCU is requesting battery statistics data (mainly for rental batteries) but the software never changed, even if you buied the battery. The ISO-TP requests from
        // TCU is falling through the OBD Port (seperate CAN line to Gateway), we use them to detect car wake up. The TCU must be installed and working if only OBD port is used.
        ESP_LOGI(TAG, "TCU requested battery statistics, so Zoe is woken up.");
        ZoeWakeUp();
      }
    }
  }

  // Use data from V-CAN to fill metrics
  // --
  // In DDT Database there is a CAN_MESSAGE_SET_C1A_Q4_2017_(...).XML file, there are most of the CAN IDs described floating on V-CAN
  // I converted this file to a Database CAN file to analyze my dumps and real traffic on my Zoe with SavvyCAN, this speeds up finding and decoding of metrics on V-CAN a lot
  // --
  // The message decoding is written to temporary variables, because V-CAN is very busy and use Ticker1 to store them in OvmsMetrics (recommendation from dexterbg, thanks!)
  // I remove found metrics from poller list, there is a "reduced" one for that
  // --
  // I choose the CAN IDs which has a refresh rate about 500ms to 1 sec to reduce load on the OVMS module, so not every metric is grabbed from the real source,
  // for example the vehicle speed is derrived from VCU (ABS ECU) which has all four wheel speeds, but this message is refreshed at 100Hz, so I grab the
  // vehicle speed send from the METER which is sending it every 500ms or 2Hz

  switch (p_frame->MsgID)
  {
  case HVAC_R3_ID:
  {
    // --- Cabin setpoint & Blower setting ---
    if (vcan_vehicle_ignition) // HVAC ECU is shutdown after 1 min after Iginition off and values will be zero, keep last setting
    {
      if (CAN_BYTE(0) == 0x08) // Low setting
      {
        vcan_hvac_cabin_setpoint = 14;
      }
      else if (CAN_BYTE(0) == 0x0A) // High setting
      {
        vcan_hvac_cabin_setpoint = 30;
      }
      else
      {
        vcan_hvac_cabin_setpoint = static_cast<float>(CAN_BYTE(0)) / 2.0f;
      }
    }

    vcan_hvac_blower = (CAN_BYTE(3) >> 4) & 0x0F; // Blower speed
    vcan_hvac_blower = vcan_hvac_blower * 12.5;   // Map 1-8 to 0-100%
    break;
  }

  case METER_A3_ID:
  {
    // --- ExternalTemperature ---
    // strange scaling

    // --- VehicleSpeedDisplayValue ---
    if (CAN_BYTE(7) < 250)
    {
      vcan_vehicle_speed = CAN_BYTE(7);
    }
    break;
  }

  case USM_A2_ID:
  {
    // --- Ignition state, DC DC charge voltage, engineHoodState ---
    vcan_vehicle_ignition = CAN_BIT(0, 5);
    vcan_dcdc_voltage = (CAN_BYTE(1) * 0.06) + 6.0;

    if (((CAN_BYTE(4) >> 6) & 0x03) == 1)
    {
      vcan_door_engineHood = true;
    }
    else
    {
      vcan_door_engineHood = false;
    }
    break;
  }

  case HEVC_A101_ID:
  {
    // --- Max charge power available from EVSE ---
    uint16_t rawSpotPwr = (((uint16_t)CAN_BYTE(0) << 1) | (CAN_BYTE(1) >> 7)) & 0x01FF;
    vcan_mains_available_chg_power = rawSpotPwr * 0.3;
    break;
  }

  case BCM_A7_ID:
  {
    // --- Door and light states ---
    if (((CAN_BYTE(2) >> 2) & 0x03) == 2)
    {
      vcan_door_trunk = true;
    }
    else
    {
      vcan_door_trunk = false;
    }

    if (((CAN_BYTE(3) >> 6) & 0x03) == 2)
    {
      vcan_door_fl = true;
    }
    else
    {
      vcan_door_fl = false;
    }

    if (((CAN_BYTE(3) >> 2) & 0x03) == 2)
    {
      vcan_door_fr = true;
    }
    else
    {
      vcan_door_fr = false;
    }

    if ((CAN_BYTE(3) & 0x03) == 2)
    {
      vcan_door_rl = true;
    }
    else
    {
      vcan_door_rl = false;
    }

    if (((CAN_BYTE(3) >> 4) & 0x03) == 2)
    {
      vcan_door_rr = true;
    }
    else
    {
      vcan_door_rr = false;
    }

    vcan_light_highbeam = CAN_BIT(6, 1);
    vcan_light_lowbeam = CAN_BIT(6, 5);
    break;
  }

  case HEVC_R17_ID:
  {
    // --- Available energy in battery ---
    vcan_hvBat_available_energy = ((((uint16_t)(CAN_BYTE(3) & 0x1F) << 6) | (CAN_BYTE(4) >> 2)) & 0x07FF) * 0.05;
    break;
  }

  case ATCU_A5_ID:
  {
    // --- Gear/direction; negative=reverse, 0=neutral, 1=forward ---
    // N/P = neutral, R = Reverse, CVT = forward
    uint8_t rawGear = (CAN_BYTE(2) >> 4) & 0x0F;
    switch (rawGear)
    {
    case 9:
      vcan_vehicle_gear = -1;
      break;
    case 10:
      vcan_vehicle_gear = 0;
      break;
    case 11:
      vcan_vehicle_gear = 1;
      break;
    }
    break;
  }

  case HEVC_R11_ID:
  {
    // --- Consumption and trip information ---
    uint16_t rawAuxCons = (((uint16_t)CAN_BYTE(0) << 4) | (CAN_BYTE(1) >> 4)) & 0x0FFF;
    vcan_lastTrip_AuxCons = rawAuxCons * 0.1;

    uint16_t rawAvgZEVCons = (((uint16_t)CAN_BYTE(3) << 2) | (CAN_BYTE(4) >> 6)) & 0x03FF;
    vcan_lastTrip_AvgCons = rawAvgZEVCons * 0.1;

    uint16_t rawAutonomyZEV = (((uint16_t)(CAN_BYTE(4) & 0x3F) << 4) | (CAN_BYTE(5) >> 4)) & 0x03FF;
    vcan_hvBat_estimatedRange = rawAutonomyZEV * 1.0;
    break;
  }

  case HEVC_R12_ID:
  {
    // --- Consumption and trip information ---
    uint16_t rawTotalCons = (((uint16_t)CAN_BYTE(0) << 4) | (CAN_BYTE(1) >> 4)) & 0x0FFF;
    vcan_lastTrip_Cons = rawTotalCons * 0.1;

    uint16_t rawTotalRec = CAN_BYTE(2);
    vcan_lastTrip_Recv = rawTotalRec * 0.1;
    break;
  }

  case BCM_A3_ID:
  {
    // --- Lights and PTC informations ---
    vcan_hvac_ptc_heater_active = (CAN_BYTE(3) >> 4) & 0x0F;
    vcan_light_fogFront = CAN_BIT(4, 7);
    vcan_light_brake = CAN_BIT(5, 6);
    vcan_light_fogRear = CAN_BIT(7, 0);

    break;
  }

  case HEVC_A22_ID:
  {
    // --- HV battery data ---
    uint16_t rawHVVolt = (((uint16_t)(CAN_BYTE(4) & 0x1F) << 8) | CAN_BYTE(5)) & 0x1FFF;
    vcan_hvBat_voltage = rawHVVolt * 0.1;

    break;
  }

  case HEVC_A1_ID:
  {
    // --- Plug and charge state ---
    uint8_t rawPlugState = (CAN_BYTE(0) >> 6) & 0x03;
    if (rawPlugState == 1 || rawPlugState == 2)
    {
      vcan_vehicle_ChgPlugState = true;
    }
    else
    {
      vcan_vehicle_ChgPlugState = false;
    }

    vcan_vehicle_ChargeState = (CAN_BYTE(2) >> 4) & 0x07;
    break;
  }

  case BCM_A2_ID:
  {
    // --- Tyre pressure ---
    vcan_tyrePressure_FL = CAN_BYTE(0) * 30.0;
    vcan_tyrePressure_FR = CAN_BYTE(1) * 30.0;
    vcan_tyrePressure_RL = CAN_BYTE(2) * 30.0;
    vcan_tyrePressure_RR = CAN_BYTE(3) * 30.0;
    break;
  }

  case HVAC_A2_ID:
  {
    // --- Compressor mode and power cons ---
    vcan_hvac_compressor_power = ((CAN_BYTE(1) >> 1) & 0x7F) * 50.0;
    break;
  }

  case BCM_A4_ID:
  {
    // --- Zoe "outside" lock state aka locked by HFM
    vcan_vehicle_LockState = CAN_BIT(2, 3);
    break;
  }

  case ECM_A6_ID:
  {
    // --- DC/DC Current and load ---
    vcan_dcdc_current = CAN_BYTE(2) * 1.0;
    vcan_dcdc_load = ((CAN_BYTE(4) >> 1) & 0x7F) * 1.0;
    break;
  }

  case BCM_A11_ID:
  {
    // --- TPMS alert states
    vcan_tyreState_FR = (CAN_BYTE(2) >> 2) & 0x07;
    vcan_tyreState_FL = (CAN_BYTE(2) >> 5) & 0x07;
    vcan_tyreState_RR = (CAN_BYTE(3) >> 2) & 0x07;
    vcan_tyreState_RL = (CAN_BYTE(3) >> 5) & 0x07;
    break;
  }

  case HEVC_A2_ID:
  {
    // --- HV battery SoC and Charge time remaining ---
    vcan_hvBat_SoC = (CAN_BYTE(3) & 0x7F) * 1.0;

    uint16_t rawRemTime = (((uint16_t)CAN_BYTE(2) << 1) | ((CAN_BYTE(3) >> 7) & 0x01)) & 0x01FF;
    vcan_hvBat_ChargeTimeRemaining = rawRemTime * 5.0;

    break;
  }

  case HEVC_A23_ID:
  {
    // --- HV battery allowed max charging / recuperating power ---
    uint16_t rawChargePwr = (((uint16_t)(CAN_BYTE(4) & 0x0F) << 8) | ((uint16_t)CAN_BYTE(5))) & 0x0FFF;
    vcan_hvBat_max_chg_power = rawChargePwr * 0.1;
    break;
  }

  case METER_A5_ID:
  {
    // --- Trip distance from METER ---
    uint32_t rawTripDist = (((uint32_t)CAN_BYTE(4) << 9) | ((uint32_t)CAN_BYTE(5) << 1) | ((CAN_BYTE(6) >> 7) & 0x01)) & 0x1FFFF;
    vcan_lastTrip_Distance = rawTripDist * 0.1;

    // read trip distance unit, later for other country usage
    // uint8_t rawTripUnit = (CAN_BYTE(3) >> 4) & 0x03;
    // const char *unitStr = (rawTripUnit == 1) ? "mi" : "km";

    break;
  }

  case HFM_A1_ID:
  {
    // --- HFM (Hand Free Module) commands for remote climate trigger ---
    // Monitor for "Remote Lighting" button press (HFM_RKE_Request = 8) to trigger climate control
    if (m_remote_climate_enabled && m_vcan_enabled)
    {
      uint8_t hfm_rke_request = (CAN_BYTE(3) >> 3) & 0x1F;

      // Value 8 = "Short Press - Remote Lighting"
      // Implement debouncing to prevent multiple triggers from single button press
      if (hfm_rke_request == 8 && m_remote_climate_last_button_state != 8)
      {
        // Button state changed from not-pressed to pressed (edge detection)
        uint32_t current_time = esp_log_timestamp();
        uint32_t time_since_last_trigger = current_time - m_remote_climate_last_trigger_time;

        // Require at least 30 seconds between triggers to prevent accidental repeated activations,
        // this is the time the headlights stays on
        if (time_since_last_trigger > 30000 || m_remote_climate_last_trigger_time == 0)
        {
          ESP_LOGI(TAG, "Remote lighting button pressed on key fob, triggering climate control");
          CommandClimateControl(true);
          m_remote_climate_last_trigger_time = current_time;
        }
        else
        {
          ESP_LOGD(TAG, "Remote lighting button pressed, but ignoring due to debounce (last trigger %dms ago)",
                   time_since_last_trigger);
        }
      }

      // Save current button state for next iteration
      m_remote_climate_last_button_state = hfm_rke_request;
    }
    break;
  }
  }
}

// Sync VCAN datapoints to OVMS metrics, called by Ticker1, because VCAN refresh rate is too high
void OvmsVehicleRenaultZoePh2::SyncVCANtoMetrics()
{
  // OVMS Standard and Zoe custom metrics
  // HVAC (ClimBox ECU)
  StandardMetrics.ms_v_env_cabinsetpoint->SetValue(vcan_hvac_cabin_setpoint, Celcius);
  StandardMetrics.ms_v_env_cabinfan->SetValue(vcan_hvac_blower, Percentage);
  mt_hvac_compressor_power->SetValue(vcan_hvac_compressor_power, Watts);
  // BCM (Body control module under steering wheel)
  StandardMetrics.ms_v_env_locked->SetValue(vcan_vehicle_LockState);
  StandardMetrics.ms_v_door_fl->SetValue(vcan_door_fl);
  StandardMetrics.ms_v_door_fr->SetValue(vcan_door_fr);
  StandardMetrics.ms_v_door_rl->SetValue(vcan_door_rl);
  StandardMetrics.ms_v_door_rr->SetValue(vcan_door_rr);
  StandardMetrics.ms_v_door_trunk->SetValue(vcan_door_trunk);
  StandardMetrics.ms_v_env_headlights->SetValue(vcan_light_lowbeam);
  mt_hvac_heating_ptc_req->SetValue(vcan_hvac_ptc_heater_active); // Command comes from HVAC and BCM switches relays
  // USM aka UPC (Switching box under Engine hood)
  StandardMetrics.ms_v_env_on->SetValue(vcan_vehicle_ignition);
  StandardMetrics.ms_v_env_awake->SetValue(vcan_vehicle_ignition);
  StandardMetrics.ms_v_door_hood->SetValue(vcan_door_engineHood);
  StandardMetrics.ms_v_charge_12v_voltage->SetValue(vcan_dcdc_voltage);
  // METER (Instrument cluster aka TdB)
  StandardMetrics.ms_v_pos_speed->SetValue(vcan_vehicle_speed, Kph);
  StandardMetrics.ms_v_pos_trip->SetValue(vcan_lastTrip_Distance);
  // HEVC, ECM (Electric vehicle controller)
  StandardMetrics.ms_v_bat_12v_current->SetValue(vcan_dcdc_current, Amps);
  StandardMetrics.ms_v_charge_12v_current->SetValue(vcan_dcdc_current, Amps);
  StandardMetrics.ms_v_bat_capacity->SetValue(vcan_hvBat_available_energy, kWh);
  StandardMetrics.ms_v_bat_energy_used->SetValue(vcan_lastTrip_Cons, kWh);
  StandardMetrics.ms_v_bat_energy_recd->SetValue(vcan_lastTrip_Recv, kWh);
  StandardMetrics.ms_v_bat_consumption->SetValue(vcan_lastTrip_AvgCons, kWhP100K);
  StandardMetrics.ms_v_bat_voltage->SetValue(vcan_hvBat_voltage, Volts);
  StandardMetrics.ms_v_charge_pilot->SetValue(vcan_vehicle_ChgPlugState);
  mt_bat_max_charge_power->SetValue(vcan_hvBat_max_chg_power, kW);
  mt_main_power_available->SetValue(vcan_mains_available_chg_power, kW);
  mt_bat_cons_aux->SetValue(vcan_lastTrip_AuxCons, kWh);
  // ATCU (Automatic shifter control ECU, Zoe didnt have that, these message comes from HEVC I think)
  StandardMetrics.ms_v_env_gear->SetValue(vcan_vehicle_gear);

  // Some metrics needs a bit logic to suppress invalid or out of range values at boot up
  // --
  // DCDC load will set to 100% if DCDC is switched off
  if (vcan_dcdc_current != 0)
  {
    mt_12v_dcdc_load->SetValue(vcan_dcdc_load, Percentage);
  }
  else
  {
    mt_12v_dcdc_load->SetValue(0, Percentage);
  }

  // HV battery SoC, stay at 99% if charging is running, like dashboard and official app
  if (vcan_hvBat_SoC == 100 && StandardMetrics.ms_v_charge_inprogress->AsBool())
  {
    StandardMetrics.ms_v_bat_soc->SetValue(99, Percentage);
  }
  else
  {
    // LBC boot up, suppress invalid values
    if (vcan_hvBat_SoC > 0 && vcan_hvBat_SoC < 101)
    {
      StandardMetrics.ms_v_bat_soc->SetValue(vcan_hvBat_SoC, Percentage);
    }
  }

  // HEVC boot up, suppress invalid values
  if (vcan_hvBat_estimatedRange < 500 && vcan_hvBat_estimatedRange > 0)
  {
    StandardMetrics.ms_v_bat_range_est->SetValue(vcan_hvBat_estimatedRange, Kilometers);
  }

  // Tyre pressure, if over 7000 millibars, BCM values are stale
  if (vcan_tyrePressure_FL < 7000)
  {
    StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, vcan_tyrePressure_FL * 0.001, Bar);
  }
  if (vcan_tyrePressure_FR < 7000)
  {
    StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, vcan_tyrePressure_FR * 0.001, Bar);
  }
  if (vcan_tyrePressure_RL < 7000)
  {
    StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, vcan_tyrePressure_RL * 0.001, Bar);
  }
  if (vcan_tyrePressure_RR < 7000)
  {
    StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, vcan_tyrePressure_RR * 0.001, Bar);
  }

  // Tyre state values to TPMS warning and alerts
  if (vcan_tyreState_FL == 0)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 0);
  }
  else if (vcan_tyrePressure_FL == 2)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 1);
  }
  else
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 2);
  }

  if (vcan_tyreState_FR == 0)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 0);
  }
  else if (vcan_tyrePressure_FR == 2)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 1);
  }
  else
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 2);
  }

  if (vcan_tyreState_RL == 0)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 0);
  }
  else if (vcan_tyrePressure_RL == 2)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 1);
  }
  else
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 2);
  }

  if (vcan_tyreState_RR == 0)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 0);
  }
  else if (vcan_tyrePressure_RR == 2)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 1);
  }
  else
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 2);
  }

  // Charge state machine from V-CAN
  if (vcan_vehicle_ChargeState == 0)
  {
    StandardMetrics.ms_v_charge_state->SetValue("stopped");
    StandardMetrics.ms_v_charge_substate->SetValue("stopped");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_door_chargeport->SetValue(false);
    StandardMetrics.ms_v_charge_climit->SetValue(0);
  }
  if (vcan_vehicle_ChargeState == 1)
  {
    StandardMetrics.ms_v_charge_state->SetValue("timerwait");
    StandardMetrics.ms_v_charge_substate->SetValue("timerwait");
  }
  if (vcan_vehicle_ChargeState == 2)
  {
    StandardMetrics.ms_v_charge_state->SetValue("done");
    StandardMetrics.ms_v_charge_substate->SetValue("stopped");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_power->SetValue(0);
  }
  if (vcan_vehicle_ChargeState == 3)
  {
    StandardMetrics.ms_v_charge_state->SetValue("charging");
    StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
    StandardMetrics.ms_v_charge_inprogress->SetValue(true);
    StandardMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
  }
  if (vcan_vehicle_ChargeState == 4)
  {
    StandardMetrics.ms_v_charge_state->SetValue("stopped");
    StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_power->SetValue(0);
  }
  if (vcan_vehicle_ChargeState == 5)
  {
    StandardMetrics.ms_v_charge_state->SetValue("stopped");
    StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_power->SetValue(0);
  }
  if (vcan_vehicle_ChargeState == 6)
  {
    StandardMetrics.ms_v_door_chargeport->SetValue(true);
  }
  if (vcan_vehicle_ChargeState == 8)
  {
    StandardMetrics.ms_v_charge_state->SetValue("prepare");
    StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_power->SetValue(0);
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

  // ESP_LOGV(TAG, "pid: %04x length: %d m_poll_ml_remain: %d mlframe: %d", pid, length, m_poll_ml_remain, m_poll_ml_frame);

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

// DCDC converter control functions for 12V battery recharging
void OvmsVehicleRenaultZoePh2::SendDCDCMessage()
{
  if (!m_vcan_enabled)
  {
    ESP_LOGW(TAG, "Cannot send DCDC message, V-CAN not enabled");
    return;
  }

  // Send the DCDC enable message, we are faking driver door open to the HEVC
  uint8_t dcdc_cmd[8] = {0x30, 0x06, 0x00, 0x03, 0x08, 0x74, 0x10, 0x20};

  esp_err_t result = m_can1->WriteStandard(0x418, 8, dcdc_cmd);

  if (result == ESP_OK || result == ESP_QUEUED)
  {
    ESP_LOGV(TAG, "DCDC message sent (result: %d)", result);
  }
  else
  {
    ESP_LOGD(TAG, "DCDC message send failed (result: %d), will retry in 1 second", result);
  }
}

void OvmsVehicleRenaultZoePh2::EnableDCDC(bool manual)
{
  if (manual)
  {
    m_dcdc_manual_enabled = true;
  }
  m_dcdc_active = true;
  m_dcdc_last_send_time = 0; // Force immediate send
  m_dcdc_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS; // Record start time
  ESP_LOGI(TAG, "DCDC converter %s (will run for 30 minutes)", manual ? "manually enabled" : "auto-enabled");
}

void OvmsVehicleRenaultZoePh2::DisableDCDC()
{
  m_dcdc_manual_enabled = false;
  m_dcdc_active = false;
  m_dcdc_start_time = 0;
  ESP_LOGI(TAG, "DCDC converter disabled");
}

void OvmsVehicleRenaultZoePh2::CheckAutoRecharge()
{
  uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
  bool is_locked = StandardMetrics.ms_v_env_locked->AsBool();

  // Check if DCDC is active and needs to be stopped
  if (m_dcdc_active)
  {
    // Stop if car is unlocked (applies to both manual and automatic mode)
    if (!is_locked)
    {
      ESP_LOGI(TAG, "Vehicle unlocked, stopping DCDC converter keepalive");
      DisableDCDC();
      return;
    }

    // Stop after 30 minutes (applies to both manual and automatic mode)
    uint32_t elapsed_time = currentTime - m_dcdc_start_time;
    if (elapsed_time >= (30 * 60 * 1000)) // 30 minutes in milliseconds
    {
      ESP_LOGI(TAG, "DCDC converter timeout (30 minutes), stopping");
      DisableDCDC();
      return;
    }
  }

  // Only check for starting auto-recharge if enabled and not manually controlled
  if (!m_auto_12v_recharge_enabled || !m_vcan_enabled || m_dcdc_manual_enabled)
  {
    return;
  }

  float voltage_12v = StandardMetrics.ms_v_bat_12v_voltage->AsFloat(0);

  // If car is locked and voltage is below threshold, enable DCDC
  if (is_locked && voltage_12v > 0 && voltage_12v < m_auto_12v_threshold && !m_dcdc_active)
  {
    ESP_LOGI(TAG, "12V voltage %.2fV below threshold %.2fV, enabling auto-recharge", voltage_12v, m_auto_12v_threshold);
    EnableDCDC(false);
  }
}

// Coming home function - triggers lighting after locking if headlights were on
void OvmsVehicleRenaultZoePh2::TriggerComingHomeLighting()
{
  if (!m_vcan_enabled)
  {
    ESP_LOGD(TAG, "Coming home function skipped - V-CAN not enabled");
    return;
  }

  if (!m_coming_home_enabled)
  {
    ESP_LOGD(TAG, "Coming home function skipped - not enabled");
    return;
  }

  // Check if headlights (lowbeam) were on when ignition was turned off
  // We use saved state because headlights turn off when driver door opens
  if (m_last_headlight_state)
  {
    ESP_LOGI(TAG, "Coming home function activated - headlights were on when ignition turned off, triggering lighting");

    uint8_t lighting_cmd[8] = {0xF4, 0xB7, 0xA8, 0x40, 0x24, 0x84, 0x07, 0x10};
    int max_attempts = (!mt_bus_awake->AsBool()) ? 10 : 3;
    bool tx_success = false;

    // Send frames until we get actual transmission confirmation from TxCallback
    for (int i = 0; i < max_attempts && !tx_success; i++)
    {
      // Clear success flag before sending
      m_tx_success_flag.store(false);

      // Send the frame
      m_can1->WriteStandard(HFM_A1_ID, 8, lighting_cmd);

      // Wait for transmission and callback
      vTaskDelay(100 / portTICK_PERIOD_MS);

      // Check if this specific frame was successfully transmitted
      if (m_tx_success_flag.load() && m_tx_success_id.load() == HFM_A1_ID)
      {
        tx_success = true;
        ESP_LOGI(TAG, "Coming home lighting command transmitted successfully on attempt %d", i + 1);
      }
      else
      {
        ESP_LOGW(TAG, "Coming home lighting command transmission failed (attempt %d/%d)", i + 1, max_attempts);
      }
    }

    if (tx_success)
    {
      ESP_LOGI(TAG, "Coming home lighting completed - headlights should turn on for ~30 seconds");
      // Reset the saved state after successful transmission
      m_last_headlight_state = false;
    }
    else
    {
      ESP_LOGE(TAG, "Coming home lighting failed - no successful transmission after %d attempts", max_attempts);
      // Keep the state so we can retry on next lock
    }
  }
  else
  {
    ESP_LOGD(TAG, "Coming home function skipped - headlights were not on when ignition turned off");
  }
}
