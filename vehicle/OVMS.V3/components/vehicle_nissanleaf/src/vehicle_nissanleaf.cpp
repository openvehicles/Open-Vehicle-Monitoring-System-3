/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th September 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Tom Parker
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
static const char *TAG = "v-nissanleaf";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_nissanleaf.h"
#include "ovms_events.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif
#include "ovms_command.h"
#include "ovms_config.h"

#define MAX_POLL_DATA_LEN         329
#define BMS_TXID                  0x79B
#define BMS_RXID                  0x7BB
#define CHARGER_TXID              0x797
#define CHARGER_RXID              0x79a
#define BROADCAST_TXID            0x7df
#define BROADCAST_RXID            0x0
// other pairs 743/763 744/764 745/765 784/78C 792/793 79D/7BD
#define VIN_PID                   0x81
#define QC_COUNT_PID              0x1203
#define L1L2_COUNT_PID            0x1205

enum poll_states
  {
  POLLSTATE_OFF,      //- car is off
  POLLSTATE_ON,       //- car is on
  POLLSTATE_RUNNING,  //- car is in drive/reverse
  POLLSTATE_CHARGING  //- car is charging
  };

// Leaf does not respond to polls when car is off
// So there is no point polling when car is off

static const OvmsPoller::poll_pid_t obdii_polls_ze1[] =
  {
    // BUS 2
    { CHARGER_TXID, CHARGER_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, VIN_PID, {  0, 3600, 0, 0 }, 2, ISOTP_STD },           // VIN [19] Never changes
    { CHARGER_TXID, CHARGER_RXID, VEHICLE_POLL_TYPE_OBDIIEXTENDED, QC_COUNT_PID, {  0, 0, 0, 3600 }, 2, ISOTP_STD },   // QC [2] Only changes when charging. Do not update when car is active to reduce traffic.
    { CHARGER_TXID, CHARGER_RXID, VEHICLE_POLL_TYPE_OBDIIEXTENDED, L1L2_COUNT_PID, {  0, 0, 0, 3600 }, 2, ISOTP_STD }, // L0/L1/L2 [2] Only changes when charging. Do not update when car is active to reduce traffic.
    // BUS 1
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x01, {  0, 60, 60, 60 }, 1, ISOTP_STD },   // bat [39/41]
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02, {  0, 60, 60, 60 }, 1, ISOTP_STD },   // battery voltages [196]
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x06, {  0, 0, 0, 60 }, 1, ISOTP_STD },   // battery shunts [96] Only in use when charging. Do not update when car is active to reduce traffic.
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, {  0, 180, 180, 180 }, 1, ISOTP_STD }, // battery temperatures [14]
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x61, {  0, 900, 900, 900 }, 1, ISOTP_STD }, // SOH for ZE1
    POLL_LIST_END
  };

  static const OvmsPoller::poll_pid_t obdii_polls_aze0[] =
  {
    // BUS 2
    { CHARGER_TXID, CHARGER_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, VIN_PID, {  0, 3600, 0, 0 }, 2, ISOTP_STD },           // VIN [19]
    { CHARGER_TXID, CHARGER_RXID, VEHICLE_POLL_TYPE_OBDIIEXTENDED, QC_COUNT_PID, {  0, 0, 0, 3600 }, 2, ISOTP_STD },   // QC [2]
    { CHARGER_TXID, CHARGER_RXID, VEHICLE_POLL_TYPE_OBDIIEXTENDED, L1L2_COUNT_PID, {  0, 0, 0, 3600 }, 2, ISOTP_STD }, // L0/L1/L2 [2]
    // BUS 1
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x01, {  0, 60, 60, 60 }, 1, ISOTP_STD },   // bat [39/41]
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02, {  0, 60, 60, 60 }, 1, ISOTP_STD },   // battery voltages [196]
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x06, {  0, 60, 60, 60 }, 1, ISOTP_STD },   // battery shunts [96]
    { BMS_TXID, BMS_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, {  0, 180, 180, 180 }, 1, ISOTP_STD }, // battery temperatures [14]
    POLL_LIST_END
  };

void MITMDisableTimerCallback(TimerHandle_t timer)
  {
  OvmsVehicleNissanLeaf* nl = (OvmsVehicleNissanLeaf*) pvTimerGetTimerID(timer);
  nl->MITMDisableTimer();
  }

void remoteCommandTimer(TimerHandle_t timer)
  {
  OvmsVehicleNissanLeaf* nl = (OvmsVehicleNissanLeaf*) pvTimerGetTimerID(timer);
  nl->RemoteCommandTimer();
  }

void ccDisableTimer(TimerHandle_t timer)
  {
  OvmsVehicleNissanLeaf* nl = (OvmsVehicleNissanLeaf*) pvTimerGetTimerID(timer);
  nl->CcDisableTimer();
  }

enum battery_type
  {
  BATTERY_TYPE_1_24kWh,
  BATTERY_TYPE_2_24kWh,
  BATTERY_TYPE_2_30kWh,
  // there may be more...
  };

enum charge_duration_index
  {
  CHARGE_DURATION_FULL_L2,
  CHARGE_DURATION_FULL_L1,
  CHARGE_DURATION_FULL_L0,
  CHARGE_DURATION_RANGE_L2,
  CHARGE_DURATION_RANGE_L1,
  CHARGE_DURATION_RANGE_L0,
  };

OvmsVehicleNissanLeaf* OvmsVehicleNissanLeaf::GetInstance(OvmsWriter* writer /*=NULL*/)
  {
    OvmsVehicleNissanLeaf* nl = (OvmsVehicleNissanLeaf*) MyVehicleFactory.ActiveVehicle();
    string type = StdMetrics.ms_v_type->AsString();
    if (!nl || type != "NL") {
      if (writer)
        writer->puts("Error: Nissan Leaf vehicle module not selected");
      return NULL;
    }
    return nl;
  }

OvmsVehicleNissanLeaf::OvmsVehicleNissanLeaf()
  : nl_obd_rxwait(1,1)
  {
  ESP_LOGI(TAG, "Nissan Leaf v3.0 vehicle module");

  BmsSetCellArrangementVoltage(96, 32);
  BmsSetCellArrangementTemperature(3, 1);

  m_gids = MyMetrics.InitInt("xnl.v.b.gids", SM_STALE_HIGH, 0);
  m_max_gids = MyMetrics.InitInt("xnl.v.b.max.gids", SM_STALE_HIGH, 0);
  m_hx = MyMetrics.InitFloat("xnl.v.b.hx", SM_STALE_HIGH, 0);
  m_soc_new_car = MyMetrics.InitFloat("xnl.v.b.soc.newcar", SM_STALE_HIGH, 0, Percentage);
  m_soc_instrument = MyMetrics.InitFloat("xnl.v.b.soc.instrument", SM_STALE_HIGH, 0, Percentage);
  m_range_instrument = MyMetrics.InitInt("xnl.v.b.range.instrument", SM_STALE_HIGH, 0, Kilometers);
  m_bms_thermistor = MyMetrics.InitVector<int>("xnl.bms.thermistor", SM_STALE_MIN, 0, Native);
  m_bms_temp_int = MyMetrics.InitVector<int>("xnl.bms.temp.int", SM_STALE_MIN, 0, Celcius);
  m_bms_balancing = MyMetrics.InitBitset<96>("xnl.bms.balancing", SM_STALE_HIGH, 0);
  m_soh_new_car = MyMetrics.InitFloat("xnl.v.b.soh.newcar", SM_STALE_HIGH, 0, Percentage);
  m_soh_instrument = MyMetrics.InitFloat("xnl.v.b.soh.instrument", SM_STALE_HIGH, 0, Percentage);
  m_battery_energy_capacity = MyMetrics.InitFloat("xnl.v.b.e.capacity", SM_STALE_HIGH, 0, kWh);
  m_battery_energy_available = MyMetrics.InitFloat("xnl.v.b.e.available", SM_STALE_HIGH, 0, kWh);
  m_battery_type = MyMetrics.InitInt("xnl.v.b.type", SM_STALE_HIGH, 0); // auto-detect version and size by can traffic
  m_battery_heaterpresent = MyMetrics.InitBool("xnl.v.b.heaterpresent", SM_STALE_HIGH, false);
  m_battery_heatrequested = MyMetrics.InitBool("xnl.v.b.heatrequested", SM_STALE_HIGH, false);
  m_battery_heatergranted = MyMetrics.InitBool("xnl.v.b.heatergranted", SM_STALE_HIGH, false);
  m_charge_duration = MyMetrics.InitVector<int>("xnl.v.c.duration", SM_STALE_HIGH, 0, Minutes);
  // note vector strings are not handled by ovms_metrics.h and cause web errors loading ev.data in ovms.js
  // this will need to be resolved before reinstating metrics
  // m_charge_duration_label = new OvmsMetricVector<string>("xnl.v.c.duration.label");
  // m_charge_duration_label->SetElemValue(CHARGE_DURATION_FULL_L2, "full.l2");
  // m_charge_duration_label->SetElemValue(CHARGE_DURATION_FULL_L1, "full.l1");
  // m_charge_duration_label->SetElemValue(CHARGE_DURATION_FULL_L0, "full.l0");
  // m_charge_duration_label->SetElemValue(CHARGE_DURATION_RANGE_L2, "range.l2");
  // m_charge_duration_label->SetElemValue(CHARGE_DURATION_RANGE_L1, "range.l1");
  // m_charge_duration_label->SetElemValue(CHARGE_DURATION_RANGE_L0, "range.l0");
  m_charge_minutes_3kW_remaining = MyMetrics.InitInt("xnl.v.c.chargeminutes3kW", SM_STALE_HIGH, 0);
  m_quick_charge = MyMetrics.InitInt("xnl.v.c.quick", SM_STALE_HIGH, 0);
  m_remaining_chargebars = MyMetrics.InitInt("xnl.v.c.chargebars", SM_STALE_HIGH, 0);
  m_capacitybars = MyMetrics.InitInt("xnl.v.b.capacitybars", SM_STALE_HIGH, 0);
  m_soc_nominal = MyMetrics.InitFloat("xnl.v.b.soc.nominal", SM_STALE_HIGH, 0, Percentage);
  m_battery_out_power_limit = MyMetrics.InitFloat("xnl.v.b.output.limit", SM_STALE_HIGH, 0, kW);
  m_battery_in_power_limit = MyMetrics.InitFloat("xnl.v.b.regen.limit", SM_STALE_HIGH, 0, kW);
  m_battery_chargerate_max = MyMetrics.InitFloat("xnl.v.b.charge.limit", SM_STALE_HIGH, 0, kW);
  m_charge_limit = MyMetrics.InitString("xnl.v.c.limit.reason", SM_STALE_MIN, 0);
  m_charge_count_qc     = MyMetrics.InitInt("xnl.v.c.count.qc",     SM_STALE_NONE, 0);
  m_charge_count_l0l1l2 = MyMetrics.InitInt("xnl.v.c.count.l0l1l2", SM_STALE_NONE, 0);
  m_climate_vent = MyMetrics.InitString("v.e.cabinvent", SM_STALE_MIN, 0);
  m_climate_intake = MyMetrics.InitString("v.e.cabinintake", SM_STALE_MIN, 0);
  m_climate_setpoint = MyMetrics.InitFloat("v.e.cabinsetpoint", SM_STALE_HIGH, 0, Celcius);
  m_climate_fan_speed = MyMetrics.InitInt("v.e.cabinfan", SM_STALE_MIN, 0);
  m_climate_fan_speed_limit = MyMetrics.InitInt("v.e.cabinfanlimit", SM_STALE_MIN, 0);
  m_climate_fan_only = MyMetrics.InitBool("xnl.cc.fan.only", SM_STALE_MIN, false);
  m_climate_remoteheat = MyMetrics.InitBool("xnl.cc.remoteheat", SM_STALE_MIN, false);
  m_climate_remotecool = MyMetrics.InitBool("xnl.cc.remotecool", SM_STALE_MIN, false);
  m_climate_rqinprogress = MyMetrics.InitBool("xnl.cc.rqinprogress", SM_STALE_MIN, false);
  m_charge_state_previous = MyMetrics.InitString("xnl.v.c.state.previous", SM_STALE_HIGH, 0);
  m_charge_user_notified = MyMetrics.InitString("xnl.v.c.event.notification", SM_STALE_HIGH, 0);
  m_charge_event_reason = MyMetrics.InitString("xnl.v.c.event.reason", SM_STALE_HIGH, 0);
  m_climate_auto = MyMetrics.InitBool("xnl.v.e.hvac.auto", SM_STALE_MIN, false);
  mt_pos_odometer_start   = MyMetrics.InitFloat("xnl.v.pos.odometer.start", SM_STALE_MID, 0, Kilometers);
  m_qc_relay_status = MyMetrics.InitFloat("xnl.v.c.relay.qc", SM_STALE_HIGH, 0);
  m_ac_relay_status = MyMetrics.InitFloat("xnl.v.c.relay.ac", SM_STALE_HIGH, 0);
  m_charge_mode = MyMetrics.InitInt("xnl.v.c.mode.raw", SM_STALE_HIGH, 0);
  MyMetrics.InitBool("v.e.on", SM_STALE_MIN, false);
  MyMetrics.InitBool("v.e.awake", SM_STALE_MID, false);
  MyMetrics.InitBool("v.e.locked", SM_STALE_MID, false);
  MyMetrics.InitString("v.c.state",SM_STALE_MID,"stopped");
  m_ZE0_charger = false;
  m_kWh_capacity_read = false;
  m_AZE0_charger = false;
  m_climate_really_off = false;

  cfg_soh_newcar = MyConfig.GetParamValueBool("xnl", "soh.newcar", false);

  // register but don't auto-power-off the busses.
  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS, nullptr, false);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS, nullptr, false);
  PollSetState(POLLSTATE_OFF);
  //PollSetResponseSeparationTime(0);


  MyConfig.RegisterParam("xnl", "Nissan Leaf", true, true);
  ConfigChanged(NULL);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif

  m_remoteCommandTimer = xTimerCreate("Nissan Leaf Remote Command", 100 / portTICK_PERIOD_MS, pdTRUE, this, remoteCommandTimer);
  m_ccDisableTimer = xTimerCreate("Nissan Leaf CC Disable", 1000 / portTICK_PERIOD_MS, pdFALSE, this, ccDisableTimer);
  m_MITMstop = xTimerCreate("Nissan Leaf MITM Chargestop Disable", 1000 / portTICK_PERIOD_MS, pdFALSE, this, MITMDisableTimerCallback);

  //load custom shell commands
  CommandInit();

  if (cfg_ze1)
    {
    PollSetPidList(m_can1,obdii_polls_ze1);
    }
  else
    {
    PollSetPidList(m_can1,obdii_polls_aze0);
    }

  using std::placeholders::_1;
  using std::placeholders::_2;
  }

unsigned char leafcrc(int l, unsigned char * b){
  const unsigned char crcTable[]={0x0, 0x85, 0x8F, 0x0A, 0x9B, 0x1E, 0x14, 0x91, 0xB3, 0x36, 0x3C, 0xB9, 0x28, 0xAD, 0xA7, 0x22,
                         0xE3, 0x66, 0x6C, 0xE9, 0x78, 0xFD, 0xF7, 0x72, 0x50, 0xD5, 0xDF, 0x5A, 0xCB, 0x4E, 0x44, 0xC1,
                         0x43, 0xC6, 0xCC, 0x49, 0xD8, 0x5D, 0x57, 0xD2, 0xF0, 0x75, 0x7F, 0xFA, 0x6B, 0xEE, 0xE4, 0x61,
                         0xA0, 0x25, 0x2F, 0xAA, 0x3B, 0xBE, 0xB4, 0x31, 0x13, 0x96, 0x9C, 0x19, 0x88, 0x0D, 0x07, 0x82,
                         0x86, 0x03, 0x09, 0x8C, 0x1D, 0x98, 0x92, 0x17, 0x35, 0xB0, 0xBA, 0x3F, 0xAE, 0x2B, 0x21, 0xA4,
                         0x65, 0xE0, 0xEA, 0x6F, 0xFE, 0x7B, 0x71, 0xF4, 0xD6, 0x53, 0x59, 0xDC, 0x4D, 0xC8, 0xC2, 0x47,
                         0xC5, 0x40, 0x4A, 0xCF, 0x5E, 0xDB, 0xD1, 0x54, 0x76, 0xF3, 0xF9, 0x7C, 0xED, 0x68, 0x62, 0xE7,
                         0x26, 0xA3, 0xA9, 0x2C, 0xBD, 0x38, 0x32, 0xB7, 0x95, 0x10, 0x1A, 0x9F, 0x0E, 0x8B, 0x81, 0x04,
                         0x89, 0x0C, 0x06, 0x83, 0x12, 0x97, 0x9D, 0x18, 0x3A, 0xBF, 0xB5, 0x30, 0xA1, 0x24, 0x2E, 0xAB,
                         0x6A, 0xEF, 0xE5, 0x60, 0xF1, 0x74, 0x7E, 0xFB, 0xD9, 0x5C, 0x56, 0xD3, 0x42, 0xC7, 0xCD, 0x48,
                         0xCA, 0x4F, 0x45, 0xC0, 0x51, 0xD4, 0xDE, 0x5B, 0x79, 0xFC, 0xF6, 0x73, 0xE2, 0x67, 0x6D, 0xE8,
                         0x29, 0xAC, 0xA6, 0x23, 0xB2, 0x37, 0x3D, 0xB8, 0x9A, 0x1F, 0x15, 0x90, 0x01, 0x84, 0x8E, 0x0B,
                         0x0F, 0x8A, 0x80, 0x05, 0x94, 0x11, 0x1B, 0x9E, 0xBC, 0x39, 0x33, 0xB6, 0x27, 0xA2, 0xA8, 0x2D,
                         0xEC, 0x69, 0x63, 0xE6, 0x77, 0xF2, 0xF8, 0x7D, 0x5F, 0xDA, 0xD0, 0x55, 0xC4, 0x41, 0x4B, 0xCE,
                         0x4C, 0xC9, 0xC3, 0x46, 0xD7, 0x52, 0x58, 0xDD, 0xFF, 0x7A, 0x70, 0xF5, 0x64, 0xE1, 0xEB, 0x6E,
                         0xAF, 0x2A, 0x20, 0xA5, 0x34, 0xB1, 0xBB, 0x3E, 0x1C, 0x99, 0x93, 0x16, 0x87, 0x02, 0x08, 0x8D



  };
  unsigned char crc = 0;
  for (int i = 0; i < l; i++)
    crc = crcTable[crc ^ b[i]];
  return crc;
}

OvmsVehicleNissanLeaf::~OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Shutdown Nissan Leaf vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
  }

void OvmsVehicleNissanLeaf::CommandInit()
  {
  cmd_xnl = MyCommandApp.RegisterCommand("xnl","Nissan Leaf framework");

  OvmsCommand* obd = cmd_xnl->RegisterCommand("obd", "OBD2 tools");
    OvmsCommand* cmd_can1 = obd->RegisterCommand("can1", "Send OBD2 request, output response to EV can bus");
  cmd_can1->RegisterCommand("device", "Send OBD2 request to an ECU",shell_obd_request,
    "<txid> <rxid> <request>\n"
    "Where <request> includes mode (01, 02, 09, 10, 1A, 21 or 22) and pid.\n"
    "Example: 79B 7BB 2101", 3, 3);
  cmd_can1->RegisterCommand("broadcast", "Send OBD2 request as broadcast", shell_obd_request, "<request>", 1, 1);
  OvmsCommand* cmd_can2 = obd->RegisterCommand("can2", "Send to CAR can bus");
  cmd_can2->RegisterCommand("device", "Send OBD2 request to a device", shell_obd_request,
    "<txid> <rxid> <request>\n"
    "Where <request> includes mode (01, 02, 09, 10, 1A, 21 or 22) and pid.\n"
    "Example: 79B 7BB 2101", 3, 3);
  cmd_can2->RegisterCommand("broadcast", "Send OBD2 request as broadcast", shell_obd_request, "<request>", 1, 1);
  }

void OvmsVehicleNissanLeaf::ConfigChanged(OvmsConfigParam* param)
  {
  if (param && param->GetName() != "xnl")
    return;
  ESP_LOGD(TAG, "Nissan Leaf reload configuration");

  // Note that we do not store a configurable maxRange like Kia Soal and Renault Twizy.
  // Instead, we derive maxRange from maxGids

  // Instances:
  // xnl
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)

  StandardMetrics.ms_v_charge_limit_soc->SetValue(   (float) MyConfig.GetParamValueInt("xnl", "suffsoc"),   Percentage );
  StandardMetrics.ms_v_charge_limit_range->SetValue( (float) MyConfig.GetParamValueInt("xnl", "suffrange"), Kilometers );

  cfg_ev_request_port       = MyConfig.GetParamValueInt("xnl", "cfg_ev_request_port");
  cfg_limit_range_calc      = MyConfig.GetParamValue("xnl", "suffrangecalc", DEFAULT_SUFF_RANGE_CALC);
  cfg_allowed_rangedrop     = MyConfig.GetParamValueInt("xnl", "rangedrop", DEFAULT_RANGEDROP);
  cfg_allowed_socdrop       = MyConfig.GetParamValueInt("xnl", "socdrop", DEFAULT_SOCDROP);
  cfg_enable_autocharge     = MyConfig.GetParamValueBool("xnl", "autocharge", DEFAULT_AUTOCHARGE_ENABLED);
  cfg_speed_divisor         = MyConfig.GetParamValueFloat("xnl", "speeddivisor", DEFAULT_SPEED_DIVISOR);


  //TODO nl_enable_write = MyConfig.GetParamValueBool("xnl", "canwrite", false);
  cfg_enable_write = MyConfig.GetParamValueBool("xnl", "canwrite", false);
  if (!cfg_enable_write) PollSetState(POLLSTATE_OFF);

  cfg_ze1 = MyConfig.GetParamValueBool("xnl", "ze1", false);
  }


// Takes care of setting all the state appropriate when the car is on
// or off.
//
void OvmsVehicleNissanLeaf::vehicle_nissanleaf_car_on(bool isOn)
  {
  if (isOn && !StandardMetrics.ms_v_env_on->AsBool())
    {
    // Log once that car is being turned on
    ESP_LOGI(TAG,"CAR IS ON");
    if (cfg_enable_write) PollSetState(POLLSTATE_ON);
    // if a can message is found with the state of charge port this can removed
    StandardMetrics.ms_v_door_chargeport->SetValue(false);
    // Reset trip values
    ResetTripCounters();
    StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
    StandardMetrics.ms_v_bat_energy_used->SetValue(0);
    mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
    StandardMetrics.ms_v_pos_trip->SetValue(0);
    m_cum_energy_recd_wh = 0.0f;
    m_cum_energy_used_wh = 0.0f;
    }
  else if (!isOn && StandardMetrics.ms_v_env_on->AsBool())
    {
    // Log once that car is being turned off
    ESP_LOGI(TAG,"CAR IS OFF");
    //StandardMetrics.ms_v_env_awake->SetValue(false);
    if (StandardMetrics.ms_v_charge_state->AsString()  != "charging")
      {
      PollSetState(POLLSTATE_OFF);
      }
    }

  // Always set this value to prevent it from going stale
  StandardMetrics.ms_v_env_on->SetValue(isOn);
  //PollSetState( (isOn) ? 1 : 0 ); now based on vehicle states
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_charger_status()
// Takes care of setting all the charger state bit when the charger
// switches on or off. Separate from vehicle_nissanleaf_poll1() to make
// it clearer what is going on.
//
void OvmsVehicleNissanLeaf::vehicle_nissanleaf_charger_status(ChargerStatus status)
  {
  bool fast_charge  = false;
  switch (status)
    {
    case CHARGER_STATUS_IDLE:
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_gen_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_gen_substate->SetValue("stopped");
      StandardMetrics.ms_v_gen_state->SetValue("stopped");
      PollSetState(POLLSTATE_OFF); //This need for this is inferred only, it may not be needed.
      break;
    case CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT:
      StandardMetrics.ms_v_door_chargeport->SetValue(true); //see 0x35d, can't use as only open signal
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      if (StandardMetrics.ms_v_charge_pilot->AsBool())
        {
        StandardMetrics.ms_v_charge_state->SetValue("timerwait");
        }
      else
        {
        StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
        StandardMetrics.ms_v_charge_state->SetValue("stopped");
        }
        PollSetState(POLLSTATE_OFF);
      break;
    case CHARGER_STATUS_QUICK_CHARGING:
      fast_charge = true;
      FALLTHROUGH;
    case CHARGER_STATUS_CHARGING:
      if (!StandardMetrics.ms_v_charge_inprogress->AsBool())
        {
        StandardMetrics.ms_v_charge_kwh->SetValue(0); // Reset charge kWh
        m_cum_energy_charge_wh = 0.0f;
        }
      StandardMetrics.ms_v_door_chargeport->SetValue(true); //see 0x35d, can't use as only open signal
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_type->SetValue(fast_charge ? "chademo" : "type1");
      StdMetrics.ms_v_charge_mode->SetValue(fast_charge ? "performance" : "standard");
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      if (cfg_enable_write) PollSetState(POLLSTATE_CHARGING);
      // TODO only use battery current for Quick Charging, for regular charging
      // we should return AC line current and voltage, not battery
      // TODO does the leaf know the AC line current and voltage?
      // TODO v3 supports negative values here, what happens if we send a negative charge current to a v2 client?
      //if (StandardMetrics.ms_v_bat_current->AsFloat() < 0)
      //  {
        // battery current can go negative when climate control draws more than
        // is available from the charger. We're abusing the line current which
        // is unsigned, so don't underflow it
        //
        // TODO quick charging can draw current from the vehicle
        //StandardMetrics.ms_v_charge_current->SetValue(0);
      //  }
      //else
      //  {
        //StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsFloat());
      //  }
      if (m_ZE0_charger)
        {
        StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
        StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsFloat());
        }
      break;
    case CHARGER_STATUS_V2X:
      if (!StandardMetrics.ms_v_gen_inprogress->AsBool())
        {
          StandardMetrics.ms_v_gen_kwh->SetValue(0); // Reset charge kWh
          m_cum_energy_gen_wh = 0.0f;
        }
        StandardMetrics.ms_v_door_chargeport->SetValue(true); //see 0x35d, can't use as only open signal
        StandardMetrics.ms_v_gen_inprogress->SetValue(true);
        StandardMetrics.ms_v_gen_type->SetValue("chademo");
        StdMetrics.ms_v_gen_mode->SetValue("standard");
        StandardMetrics.ms_v_gen_substate->SetValue("onrequest");
        StandardMetrics.ms_v_gen_state->SetValue("exporting");

        if (cfg_enable_write) PollSetState(POLLSTATE_CHARGING);
      break;
    case CHARGER_STATUS_INTERRUPTED:
      // Charging stopped during charge by user
		StandardMetrics.ms_v_charge_current->SetValue(0);
      if (m_ZE0_charger)
        {
        StandardMetrics.ms_v_charge_voltage->SetValue(0);
        // TODO set this in ovms v2
        // TODO the charger probably knows the line voltage, when we find where it's
        // coded, don't zero it out when we're plugged in but not charging
        }
      if (m_AZE0_charger)
        {
        StandardMetrics.ms_v_charge_voltage->SetValue(0); //AZE0 doesn't know line voltage
        }
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      //StandardMetrics.ms_v_door_chargeport->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      PollSetState(POLLSTATE_OFF);
      break;
    case CHARGER_STATUS_FINISHED:
      // Charging finished
	  StandardMetrics.ms_v_charge_current->SetValue(0);
      if (m_ZE0_charger)
        {
        StandardMetrics.ms_v_charge_voltage->SetValue(0);
        // TODO set this in ovms v2
		// TODO? don't zero it out when we're plugged in but not charging
        }
      if (m_AZE0_charger)
        {
        StandardMetrics.ms_v_charge_voltage->SetValue(0); //AZE0 doesn't know line voltage
        }
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      //StandardMetrics.ms_v_door_chargeport->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("done");
      PollSetState(POLLSTATE_OFF);
      break;
    }
  if (status != CHARGER_STATUS_CHARGING && status != CHARGER_STATUS_QUICK_CHARGING)
    {
	  StandardMetrics.ms_v_charge_current->SetValue(0);
    if (m_ZE0_charger)
      {
      // TODO? don't zero it out when we're plugged in but not charging
      StandardMetrics.ms_v_charge_voltage->SetValue(0);
      }
    if (m_AZE0_charger)
      {
      StandardMetrics.ms_v_charge_voltage->SetValue(0); //AZE0 doesn't know line voltage
      }
    StandardMetrics.ms_v_charge_mode->SetValue("");
    }
  }

int OvmsVehicleNissanLeaf::GetNotifyChargeStateDelay(const char* state)
  {
  if (StandardMetrics.ms_m_monotonic->AsInt() < 10)
    return 0; //avoid notify on boot triggered by setting delay
  else return 8; //allow time for charger to handshake
  }

void OvmsVehicleNissanLeaf::shell_obd_request(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
    OvmsVehicleNissanLeaf* nl = GetInstance(writer);
    const char* strbus = cmd->GetParent()->GetName();
    if (!nl)
      return;
    if (!MyConfig.GetParamValueBool("xnl", "canwrite", false)) {
        writer->puts("ERROR: canwrite not enabled");
        return;
      }

    uint16_t txid = 0, rxid = 0;
    uint32_t req = 0;
    uint8_t bus = 2; // default to CAR can
    string response;

    // parse args:
    string device = cmd->GetName();
    if (device == "device") {
      if (argc < 3) {
        writer->puts("ERROR: too few args, need: txid rxid request");
        return;
      }
      txid = strtol(argv[0], NULL, 16);
      rxid = strtol(argv[1], NULL, 16);
      req = strtol(argv[2], NULL, 16);
      bus = (strcmp(strbus,"can1") == 0 ? 1 : 2);
    } else {
      if (argc < 1) {
        writer->puts("ERROR: too few args, need: request");
        return;
      }
      req = strtol(argv[0], NULL, 16);
      if (device == "broadcast") {
        txid = BROADCAST_TXID;
        rxid = BROADCAST_RXID;
      }
    }

    // validate request:
    uint8_t mode = (req <= 0xffff) ? ((req & 0xff00) >> 8) : ((req & 0xff0000) >> 16);
    if (mode != 0x01 && mode != 0x02 && mode != 0x09 &&
        mode != 0x10 && mode != 0x1A && mode != 0x21 && mode != 0x22) {
      writer->puts("ERROR: mode must be one of: 01, 02, 09, 10, 1A, 21 or 22");
      return;
    } else if (req > 0xffffff) {
      writer->puts("ERROR: PID must be 8 or 16 bit");
      return;
    }

    // execute request:
    if (!nl->ObdRequest(txid, rxid, req, response, 3000, bus)) {
      if (bus == 1) writer->puts("ERROR: timeout waiting for response on can1");
      if (bus == 2) writer->puts("ERROR: timeout waiting for response on can2");
      return;
    }

    // output response as hex dump:
    writer->puts("Response:");
    char *buf = NULL;
    size_t rlen = response.size(), offset = 0;
    do {
      rlen = FormatHexDump(&buf, response.data() + offset, rlen, 16);
      offset += 16;
      writer->puts(buf ? buf : "-");
    } while (rlen);
    if (buf)
      free(buf);
  }

bool OvmsVehicleNissanLeaf::ObdRequest(uint16_t txid, uint16_t rxid, uint32_t request, string& response, int timeout_ms /*=3000*/, uint8_t bus)
  {
  OvmsMutexLock lock(&nl_obd_request);
  // prepare single poll:
  OvmsPoller::poll_pid_t poll[] = {
    { txid, rxid, 0, 0, { 1, 1, 1, 1 }, 0, ISOTP_STD },
    POLL_LIST_END
  };
  if (request < 0x10000) {
    poll[0].type = (request & 0xff00) >> 8;
    poll[0].pid = request & 0xff;
  } else {
    poll[0].type = (request & 0xff0000) >> 16;
    poll[0].pid = request & 0xffff;
  }
  poll[0].pollbus = bus;
  // stop default polling:
  PollSetPidList(NULL);
  vTaskDelay(pdMS_TO_TICKS(100));

  // clear rx semaphore, start single poll:
  nl_obd_rxwait.Take(0);
  nl_obd_rxbuf.clear();
  PollSetPidList(poll);

  // wait for response:
  bool rxok = nl_obd_rxwait.Take(pdMS_TO_TICKS(timeout_ms));
  if (rxok == pdTRUE) {
    response = nl_obd_rxbuf;
    nl_obd_rxbuf.clear();
    }

  // restore default polling:
  nl_obd_rxwait.Give();
  vTaskDelay(pdMS_TO_TICKS(100));
  if (cfg_ze1)
    {
    PollSetPidList(m_can1,obdii_polls_ze1);
    }
  else
    {
    PollSetPidList(m_can1,obdii_polls_aze0);
    }

  return (rxok == pdTRUE);
  }

void OvmsVehicleNissanLeaf::PollReply_Battery(const uint8_t *reply_data, uint16_t reply_len)
  {
  if (reply_len != 39 &&    // 24 KWh Leafs
      reply_len != 41 &&    // 30 KWh Leafs
      reply_len != 51)      // ZE1 Leafs respond with 51 bytes
                            // TODO: on startup the ZE1 Leafs respond with 42 bytes
    {
    ESP_LOGI(TAG, "PollReply_Battery: len=%d != 39 && != 41 && != 51", reply_len);
    return;
    }

  //  > 0x79b 21 01
  //  < 0x7bb 61 01
  // [ 0.. 5] 0000020d 0287
  // [ 6..15] 00000363 ffffffff001c
  // [16..23] 2af89a89 2e4b 03a4
  // [24..31] 005c 269a 000ecf81
  // [32..38] 0009d7b0 800001
  //
  // or
  // > 0x79b 21 01
  // < 0x7bb 61 01
  // [ 0,, 5] fffffc4e 028a
  // [ 6..15] ffffff46 ffffffff 1290
  // [16..23] 2af88fcd 32de 038b
  // [24..31] 005c 1f3d 000896c6
  // [32..38] 000b3290 800001
  // [39..40] 0000

  // ZE1 Leafs respond with 51 bytes
  // > 0x79b 21 01
  // < 0x7bb 61 01
  //  0x7BB 10 35 61 01 FF FF FC 18		0..3
  //  0x7BB 21 02 AF FF FF FB 62 FF		4..10
  //  0x7BB 22 FF F0 DD 0B 1C 30 D4		11..17
  //  0x7BB 23 95 1D 33 06 03 95 00		18..24
  //  0x7BB 24 01 70 00 26 9A 00 0C		25..31
  //  0x7BB 25 44 B5 00 11 0B B8 80		32..38
  //  0x7BB 26 00 01 FF FF FB 62 FF		39..45
  //  0x7BB 27 FF FC AA 01 AD FF FF		46..52

  uint16_t hx;
  uint32_t ah10000;
  uint32_t soc=0;

  if (reply_len == 51)  // ZE1 Leafs
  {
    hx = (reply_data[28] << 8) | reply_data[29];
    ah10000 = (reply_data[35] << 16) | (reply_data[36] << 8) |  reply_data[37];

    soc = (reply_data[31] << 16) | (reply_data[32] << 8) |  reply_data[33];
    ESP_LOGD(TAG, "0x7BB SOC response: %d", soc);
    StandardMetrics.ms_v_bat_soc->SetValue(soc / 10000.0);
    m_hx->SetValue(hx / 102.4); // from Dala's Leaf2018-CAN pdf use 102.4 not 100

  } else // Eveything else
  {
    hx = (reply_data[26] << 8) | reply_data[27];
    ah10000 = (reply_data[33] << 16) | (reply_data[34] << 8) |  reply_data[35];
    m_hx->SetValue(hx / 100.0);
  }

  float ah = ah10000 / 10000.0;
  StandardMetrics.ms_v_bat_cac->SetValue(ah);

  if (!m_kWh_capacity_read) {
    // Because older LEAF models seem not to transmit 0x59e, calculate based on Ah and pack voltage
    m_battery_energy_capacity->SetValue((ah*StandardMetrics.ms_v_bat_voltage->AsFloat())/1000.0);
  }



  // there may be a way to get the SoH directly from the BMS, but for now
  // divide by a configurable battery size
  // - For 24 KWh : xnl.newCarAh = 66 (default)
  // - For 30 KWh : xnl.newCarAh = 80 (i.e. shell command "config set xnl newCarAh 80")

  /// TODO: this can be read directly from the BMS (group 61) for ZE1 Leafs

  float newCarAh = MyConfig.GetParamValueFloat("xnl", "newCarAh", GEN_1_NEW_CAR_AH);
  float soh = ah / newCarAh * 100;
  m_soh_new_car->SetValue(soh);
  if (cfg_soh_newcar)
    {
    StandardMetrics.ms_v_bat_soh->SetValue(soh);
    }
  }

void OvmsVehicleNissanLeaf::PollReply_BMS_Volt(const uint8_t *reply_data, uint16_t reply_len)
  {
  if (reply_len != 196)
    {
    ESP_LOGI(TAG, "PollReply_BMS_Volt: len=%d != 196", reply_len);
    return;
    }
  //  > 0x79b 21 02
  //  < 0x7bb 61 02
  // [ 0..191]: Contains all 96 of the cell voltages, in volts/1000 (2 bytes per cell)
  // [192,193]: Pack voltage, in volts/100
  // [194,195]: Bus voltage, in volts/100

  int i;
  for(i=0; i<96; i++)
    {
    int millivolt = reply_data[i*2] << 8 | reply_data[i*2+1];
    if (millivolt < 5000) //ignore invalid readings
      {
      BmsSetCellVoltage(i, millivolt / 1000.0);
      }
    }
  }

void OvmsVehicleNissanLeaf::PollReply_BMS_Shunt(const uint8_t *reply_data, uint16_t reply_len)
  {
  if (reply_len != 24)
    {
    ESP_LOGI(TAG, "PollReply_BMS_Shunt: len=%d != 24", reply_len);
    return;
    }
    //  > 0x79b 21 06
    //  < 0x7bb 61 06
    // [ 0..23]: Contains all 96 of the cell shunts, where cell1=bit3, cell2=bit2, cell3=bit1, cell4=bit0 of byte0 etc
    // referred to as shunt order 8421
  std::bitset<96> balancing;
  int i;
  for(i=0; i<24; i++)
    {
    if ((reply_data[i] & 0x08) == 0x08) balancing.set(i*4 + 0);
    if ((reply_data[i] & 0x04) == 0x04) balancing.set(i*4 + 1);
    if ((reply_data[i] & 0x02) == 0x02) balancing.set(i*4 + 2);
    if ((reply_data[i] & 0x01) == 0x01) balancing.set(i*4 + 3);
    }
  m_bms_balancing->SetValue(balancing.flip());
  }


void OvmsVehicleNissanLeaf::PollReply_BMS_Temp(const uint8_t *reply_data, uint16_t reply_len)
  {
  if (reply_len != 14 && reply_len != 29)  // 14 bytes for ZE0 and AZE0, 29 bytes for ZE1
    {
    ESP_LOGI(TAG, "PollReply_BMS_Temp: len=%d != 14 or != 29", reply_len);
    return;
    }
  //  > 0x79b 21 04
  //  < 0x7bb 61 04
  // [ 0, 1] pack 1 thermistor
  // [    2] pack 1 temp in degC
  // [ 3, 4] pack 2 thermistor
  // [    5] pack 2 temp in degC
  // [ 6, 7] pack 3 thermistor (unused)
  // [    8] pack 3 temp in degC (unused)
  // [ 9,10] pack 4 thermistor
  // [   11] pack 4 temp in degC
  // [   12] pack 5 temp in degC
  // [   13] unknown; varies in interesting ways
  // 14 bytes:
  //      0  1  2   3  4  5   6  7  8   9 10 11  12 13
  // 14 [02 51 0c  02 4d 0d  ff ff ff  02 4d 0d  0c 00 ]
  // 14 [02 52 0c  02 4e 0c  ff ff ff  02 4e 0c  0c 00 ]
  // 14 [02 57 0c  02 57 0c  ff ff ff  02 58 0b  0b 00 ]
  // 14 [02 5a 0b  02 59 0b  ff ff ff  02 5a 0b  0b 00 ]
  //

  // ZE1 Leafs respond with 29 bytes
  //  0x7BB 10 1F 61 04 02 0D 13 02  0..3
  //  0x7BB 21 03 14 FF FF FF 02 0B  4..10
  //  0x7BB 22 13 13 00 FF FF FF FF  11..17

  // TODO: The above capture is incomplete

  int thermistor[4];
  int temp_int[6];
  int i;
  int out = 0;
  for(i=0; i<4; i++)
    {
    thermistor[i] = reply_data[i*3] << 8 | reply_data[i*3+1];
    temp_int[i] = reply_data[i*3+2];
    if(thermistor[i] != 0xffff)
      {
      BmsSetCellTemperature(out++, -0.102 * (thermistor[i] - 710));
      }
    }
  temp_int[i++] = reply_data[12];
  temp_int[i++] = reply_data[13];
  m_bms_thermistor->SetElemValues(0, 4, thermistor);
  m_bms_temp_int->SetElemValues(0, 6, temp_int);

  StandardMetrics.ms_v_bat_temp->SetValue(StandardMetrics.ms_v_bat_pack_tmax->AsFloat()); // copy this to v.b.temp for the app to display
  }

void OvmsVehicleNissanLeaf::PollReply_BMS_SOH(const uint8_t *reply_data, uint16_t reply_len)
  {
  if (reply_len != 329)
    {
    ESP_LOGI(TAG, "PollReply_BMS_SOH: len=%d != 329", reply_len);
    return;
    }

    uint16_t uint_soh = (reply_data[2] << 8) | reply_data[3];
    float soh = uint_soh / 100.0f;
    ESP_LOGD(TAG, "BMS SOH: %.2f", soh);
    m_soh_instrument->SetValue(soh);
    if (!cfg_soh_newcar)
      {
      StandardMetrics.ms_v_bat_soh->SetValue(soh);
      }
  }

void OvmsVehicleNissanLeaf::PollReply_QC(const uint8_t *reply_data, uint16_t reply_len)
  {
  if (reply_len != 2)
    {
    ESP_LOGI(TAG, "PollReply_QC: len=%d != 2", reply_len);
    return;
    }
  //  > 0x797 22 12 03
  //  < 0x79a 62 12 03
  // [ 0..3] 00 0C 00 00

  uint16_t count = reply_data[0] * 0x100 + reply_data[1];
  if (count != 0xFFFF)
    {
    m_charge_count_qc->SetValue(count);
    }
  }

void OvmsVehicleNissanLeaf::PollReply_L0L1L2(const uint8_t *reply_data, uint16_t reply_len)
  {
  if (reply_len != 2)
    {
    ESP_LOGI(TAG, "PollReply_L0L1L2: len=%d != 2", reply_len);
    return;
    }
  //  > 0x797 22 12 05
  //  < 0x79a 62 12 05
  // [ 0..3] 00 5D 00 00

  uint16_t count = reply_data[0] * 0x100 + reply_data[1];
  if (count != 0xFFFF)
    {
    m_charge_count_l0l1l2->SetValue(count);
    }
  }

void OvmsVehicleNissanLeaf::PollReply_VIN(const uint8_t *reply_data, uint16_t reply_len)
  {
  if (reply_len != 19)
    {
    ESP_LOGI(TAG, "PollReply_VIN: len=%d != 19", reply_len);
    return;
    }
  //  > 0x797 21 81
  //  < 0x79a 61 81
  // [ 0..16] 53 4a 4e 46 41 41 5a 45 30 55 3X 3X 3X 3X 3X 3X 3X
  // [17..18] 00 00
  char buf[19];
  strncpy(buf,(char*)reply_data,reply_len);
  string strbuf(buf);
  std::replace(strbuf.begin(), strbuf.end(), 0x1b, 0x20); // remove ESC character returned by AZE0 models

  StandardMetrics.ms_v_vin->SetValue(strbuf); //(char*)reply_data

  ESP_LOGD(TAG, "VIN: %s", strbuf.c_str());
  }

// Reassemble all pieces of a multi-frame reply.
void OvmsVehicleNissanLeaf::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) {
  string& rxbuf = nl_obd_rxbuf;

  // init / fill rx buffer:
  if (job.mlframe == 0) {
    rxbuf.clear();
    rxbuf.reserve(std::max(MAX_POLL_DATA_LEN, length + job.mlremain));
  }
  rxbuf.append((char*)data, length);
  if (job.mlremain)
    return;

  const uint8_t *buf = reinterpret_cast<const uint8_t *>(rxbuf.c_str());

  uint32_t id_pid = job.moduleid_rec<<16 | job.pid;
    switch (id_pid)
      {
      case BMS_RXID<<16 | 0x01: // battery
        PollReply_Battery(buf, rxbuf.size());
        break;
      case BMS_RXID<<16 | 0x02:
        PollReply_BMS_Volt(buf, rxbuf.size());
        break;
      case BMS_RXID<<16 | 0x06:
        PollReply_BMS_Shunt(buf, rxbuf.size());
        break;
      case BMS_RXID<<16 | 0x04:
        PollReply_BMS_Temp(buf, rxbuf.size());
        break;
      case BMS_RXID<<16 | 0x61:
        PollReply_BMS_SOH(buf, rxbuf.size());
        break;
      case CHARGER_RXID<<16 | QC_COUNT_PID: // QC
        PollReply_QC(buf, rxbuf.size());
        break;
      case CHARGER_RXID<<16 | L1L2_COUNT_PID: // L0/L1/L2
        PollReply_L0L1L2(buf, rxbuf.size());
        break;
      case CHARGER_RXID<<16 | VIN_PID: // VIN
        PollReply_VIN(buf, rxbuf.size());
        break;
      default:
        ESP_LOGI(TAG, "IncomingPollReply: unknown reply module|pid=%#" PRIx32 " len=%d", id_pid, rxbuf.size());
        break;
      }

    // single poll?
    if (!nl_obd_rxwait.IsAvail()) {
      // yes: stop poller & signal response
      PollSetPidList(NULL);
      nl_obd_rxwait.Give();
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrameCan1(CAN_frame_t* p_frame)
  { // CAN1 is connected to EV-CAN
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x1d4:
    { // additional charge status see https://github.com/dalathegreat/leaf_can_bus_messages
      if ( (d[6] >> 7) == 0 && StandardMetrics.ms_v_charge_inprogress->AsBool() )
        {
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_INTERRUPTED);
        }
    }
    break;
    case 0x1da:
    { // Motor and inverter messages
      // Signed value, negative for reverse
      // Values 0x7fff and 0x7ffe are seen during turning on of car
      // http://productions.8dromeda.net/c55-leaf-inverter-protocol.html
      // d[2] bits[0-2] used, unclear what bits[3-7] represent
      // d[4] bit[7] can be 0 or 1
      int16_t nl_mot_torq = (int16_t)( (d[2] & 0x07) << 8 | d[3] );
      int16_t nl_rpm =      (int16_t)( d[4] << 8 | d[5] );
      // int16_t nl_inv_volt = (int16_t)( d[0] ) * 2; not currently used
      if ( (d[2] & 0x04) == 0x04 ) // indicates negative value
        { // pad leading 1s for 2s complement signed
        nl_mot_torq = nl_mot_torq | 0xf800;
        }
      if ( (d[4] & 0x40) == 0x40 ) // indicates negative value
        { // pad leading 1s for 2s complement signed
        nl_rpm = nl_rpm | 0x8000;
        }
      nl_rpm = nl_rpm / 2;
      nl_mot_torq = nl_mot_torq / 2; // guess based on rpm
      StandardMetrics.ms_v_mot_rpm  ->SetValue(nl_rpm);
      // torque (Nm) to power (W) = 2 x pi / 60 * rpm * torque
      StandardMetrics.ms_v_inv_power->SetValue(nl_rpm * nl_mot_torq * 0.10472 / 1000.0);
    }
      break;
    case 0x1db:
    {
      if (m_MITM>0) { // This section used to stop charging upon request
        unsigned char new1db[8];
        memcpy(new1db, d, 8); // Store current 1DB message content
        new1db[1] = (new1db[1] & 0xe0) | 2; // Charging Mode Stop Request
        new1db[6] = (new1db[6] + 1) % 4; // Increment PRUN counter
        new1db[7] = leafcrc(7, new1db); // Sign message with correct CRC
        m_can1->WriteStandard(0x1db, 8, new1db); //Send completed CAN message
        if (1==--m_MITM) MITMDisableTimer();
      }
      // sent by the LBC, measured inside the battery box
      // current is 11 bit twos complement big endian starting at bit 0
      int16_t nl_battery_current = ((int16_t) d[0] << 3) | (d[1] & 0xe0) >> 5;
      if (nl_battery_current & 0x0400)
        {
        // negative so extend the sign bit
        nl_battery_current |= 0xf800;
        }
      // sign updated to match standard metric definition where battery output is positive
      float battery_current = -nl_battery_current / 2.0f;

      // voltage is 10 bits unsigned big endian starting at bit 16
      int16_t nl_battery_voltage = ((uint16_t) d[2] << 2) | (d[3] & 0xc0) >> 6;
      float battery_voltage = nl_battery_voltage / 2.0f;

      // Power (in kw) resulting from voltage and current
      float battery_power = battery_voltage * battery_current / 1000.0f;

      StandardMetrics.ms_v_bat_current->SetValue(battery_current, Amps);
      StandardMetrics.ms_v_bat_voltage->SetValue(battery_voltage, Volts);
      StandardMetrics.ms_v_bat_power->SetValue(battery_power, kW);

      // Energy (in wh) from 10ms worth of power
      float energy = battery_power * 10 / 3600;
      if (energy < 0.0)
        {
        m_cum_energy_recd_wh -= energy;
        m_cum_energy_charge_wh -= energy;
        }
      else
        {
        m_cum_energy_used_wh += energy;
        m_cum_energy_gen_wh += energy;
        }

      // soc displayed on the instrument cluster
      if (!cfg_ze1)
        {
        uint8_t soc = d[4] & 0x7f;  // On the ZE1 this is always 0
        if (soc != 0x7f)
          {
          m_soc_instrument->SetValue(soc);
          // we use this unless the user has opted otherwise
          if (!MyConfig.GetParamValueBool("xnl", "soc.newcar", false))
            {
            StandardMetrics.ms_v_bat_soc->SetValue(soc);
            }
          }
        }

    }
      break;
    case 0x1dc:
    { // additional HVBAT messages see https://github.com/dalathegreat/leaf_can_bus_messages
      // power into and out of battery while driving
      m_battery_out_power_limit->SetValue( ( d[0] << 2 | d[1] >> 6 ) / 4.0 );
      m_battery_in_power_limit->SetValue( ( (d[1] & 0x3f) << 2 | d[2] >> 4 ) / 4.0 );
      // max allowed power into battery whilst charging, changes with SOC, temp etc
      m_battery_chargerate_max->SetValue( ( (d[2] & 0x0f) << 6 | d[3] >> 2 ) / 10.0 );
    }
      break;
    case 0x284:
    {
      // certain CAN bus activity counts as state "awake"
      //StandardMetrics.ms_v_env_awake->SetValue(true);

      uint16_t car_speed16 = d[4];
      car_speed16 = car_speed16 << 8;
      car_speed16 = car_speed16 | d[5];
      // old ratio was 1/92, previously approximated by comparison with dashboard speedo.
      // however, this figure appears to be ~4-4.5% lower than the speedometer speed, AND the
      // speedometer speed is itself 10% higher than the actual speed reported by OBD-II standard
      // PIDs and GPS speed (probably due to regulations allowing speedo speed to be up to 10%
      // higher but not any lower than actual speed)

      // dividing this value by 98 makes it approximately match OBD-II and GPS reported speeds,
      // allowing us to use it for deriving accurate trip odometer distances

      // verified by comparing derived trip odometer value with two ~20km GPS tracks
      StandardMetrics.ms_v_pos_speed->SetValue((float) car_speed16 / cfg_speed_divisor);

      // update our trip odometer estimate now that we've got a fresh speed reading
      UpdateTripCounters();
    }
      break;
    case 0x380:
    {
      m_ZE0_charger = true;

      // Gen 1 ZE0 Charger
      // see https://github.com/dalathegreat/leaf_can_bus_messages

      bool  ac_state = (d[4] & 0x40) != 0; // indicates ac charge state
      bool  qc_state = (d[4] & 0x20) != 0; // indicates chademo relay state

      m_qc_relay_status->SetValue((float)(d[4] & 0x20));
      m_ac_relay_status->SetValue((float)(d[4] & 0x40));

      if (ac_state || qc_state) {
          StandardMetrics.ms_v_charge_pilot->SetValue(true);
      } else {
          // This only represents charging relay state. If timer, or cable still plugged in, relay is open.
          // StandardMetrics.ms_v_charge_pilot->SetValue(false);
          StandardMetrics.ms_v_charge_voltage->SetValue(0);
      }

      float max_charge_power = ( (d[2] & 0x01) << 8 | d[3]) * 100; // in W
      float ac_voltage = ( ((d[5] & 0x07) << 8 | ( d[6] & 0xFC)) >> 2) * 2; // in V
      //ac_voltage = ac_voltage + 70; //Offset with 70V (why?)

      if (ac_state) {
         StandardMetrics.ms_v_charge_voltage->SetValue(ac_voltage);
      } else if (qc_state) {
         StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
      }

      // AC Current limit on ZE0 charger is sent in another CAN message
      if (StandardMetrics.ms_v_charge_voltage->AsFloat() > 0 && !ac_state)
      {
        StandardMetrics.ms_v_charge_climit->SetValue(max_charge_power / StandardMetrics.ms_v_charge_voltage->AsFloat());
      } else if (!qc_state) {
        StandardMetrics.ms_v_charge_power->SetValue(0);
      }

    }
      break;
    case 0x390:
    {
      m_AZE0_charger = true;

      // Gen 2 AZE0 Charger
      // see https://github.com/dalathegreat/leaf_can_bus_messages

      float charge_power =     ( (d[0] & 0x01) << 8 | d[1] ) * 100; // in W
      float max_charge_power = ( (d[5] & 0x01) << 8 | d[6] ) * 100; // in W
      bool  ac_state = (d[3] & 0x20) == 0x20; // indicates ac charge state
      bool  qc_state = (d[4] & 0x40) == 0x40; // indicates chademo relay state
      bool  vg_state = (d[0] >> 4 == 0x09);   // indicates v2x exporting state

      m_ac_relay_status->SetValue((float)(d[3] & 0x20));
      m_qc_relay_status->SetValue((float)(d[4] & 0x40));

      if ( (qc_state || ac_state) && !vg_state )
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
      else
        {
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_charge_voltage->SetValue(0);
        }

      if (vg_state)
        {
        StandardMetrics.ms_v_gen_pilot->SetValue(true);
        StandardMetrics.ms_v_gen_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
        }
      else
        {
        StandardMetrics.ms_v_gen_pilot->SetValue(false);
        StandardMetrics.ms_v_gen_voltage->SetValue(0);
        }

      if (qc_state && !vg_state)
        StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());

      if (ac_state) //Either 110V or 220V (Abnormal AC wave will report 330V with this method)
        StandardMetrics.ms_v_charge_voltage->SetValue( ((d[3] >> 3) & 0x03) * 110 );

      if (StandardMetrics.ms_v_charge_voltage->AsFloat() > 0 && !vg_state)
        {
        StandardMetrics.ms_v_charge_current->SetValue(charge_power / StandardMetrics.ms_v_charge_voltage->AsFloat());
        StandardMetrics.ms_v_charge_climit->SetValue(max_charge_power / StandardMetrics.ms_v_charge_voltage->AsFloat());
        StandardMetrics.ms_v_charge_power->SetValue(charge_power / 1000.0);
        }
      else if (StandardMetrics.ms_v_gen_voltage->AsFloat() > 0 && vg_state)
        { // may need modifying
        StandardMetrics.ms_v_gen_current->SetValue(StandardMetrics.ms_v_bat_current->AsFloat());
        StandardMetrics.ms_v_gen_climit->SetValue(max_charge_power / StandardMetrics.ms_v_gen_voltage->AsFloat());
        StandardMetrics.ms_v_gen_power->SetValue(StandardMetrics.ms_v_gen_voltage->AsFloat() * StandardMetrics.ms_v_gen_current->AsFloat() / 1000.0);
        }
      else
        {
        StandardMetrics.ms_v_charge_current->SetValue(0);
        StandardMetrics.ms_v_charge_climit->SetValue(0);
        StandardMetrics.ms_v_charge_power->SetValue(0);
        StandardMetrics.ms_v_gen_current->SetValue(0);
        StandardMetrics.ms_v_gen_climit->SetValue(0);
        StandardMetrics.ms_v_gen_power->SetValue(0);
        }

      switch ( (d[5] >> 1) & 0x3f )
        { // this appears to be ac charger status only, if qc then ac charger is idle
        case 0x01:
          if (qc_state && !vg_state)
            vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          else if (qc_state && vg_state)
            vehicle_nissanleaf_charger_status(CHARGER_STATUS_V2X);
          else
            vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x02:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_FINISHED);
          break;
        case 0x04:
          if (StandardMetrics.ms_v_charge_voltage->AsFloat() > 0)
            vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          else
            vehicle_nissanleaf_charger_status(CHARGER_STATUS_INTERRUPTED);
          break;
        case 0x08:
        case 0x09:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x0c:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        }
    }
      break;
    case 0x50a:
    { //Battery heating during below -17*C , request OK
    if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) >= 2013)
      {
      m_battery_heatergranted->SetValue(d[6] & 0x20); // Heater request granted
      }
    }
    break;
    case 0x54a:
    {
      // setpoint gets reset on heating or cooling off, so we only update it while they are running
      if (StandardMetrics.ms_v_env_heating->AsBool() || StandardMetrics.ms_v_env_cooling->AsBool())
      {
        float setpoint_float = d[4] / 2;
        m_climate_setpoint->SetValue(setpoint_float);
      }
    }
      break;
    case 0x54b:
    {
      int fanspeed_int = ((d[4] & 0x38) >> 3); //use bits 5:3 to read fan speed

      m_climate_fan_speed->SetValue(fanspeed_int / 7.0 * 100);
      m_climate_fan_speed_limit->SetValue(7);

      switch (d[2])
      {
      case 0x80:
        m_climate_vent->SetValue("off");
        break;
      case 0x88:
        m_climate_vent->SetValue("face");
        break;
      case 0x90:
        m_climate_vent->SetValue("face|feet");
        break;
      case 0x98:
        m_climate_vent->SetValue("feet");
        break;
      case 0xA0:
        m_climate_vent->SetValue("windscreen|feet");
        break;
      case 0xA8:
        m_climate_vent->SetValue("windscreen");
        break;

      default:
        m_climate_vent->SetValue("");
        break;
      }

      switch (d[3])
      {
      case 0x09:
        m_climate_intake->SetValue("recirc");
        break;
      case 0x12:
        m_climate_intake->SetValue("fresh");
        break;
      case 0x92:
        m_climate_intake->SetValue("defrost");
        break;

      default:
        m_climate_intake->SetValue("");
        break;
      }

      bool hvac_calculated = false;

      if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) < 2013)
      // leaving this legacy (mostly) code part until someone tests the new logic in <2013 cars.
      {
        // this might be a bit field? So far these 6 values indicate HVAC on
        hvac_calculated =
          d[1] == 0x0a || // Gen 1 Remote
          d[1] == 0x48 || // Manual Heating or Fan Only
          d[1] == 0x4b || // Gen 2 Remote Heating
          d[1] == 0x71 || // Gen 2 Remote Cooling
          d[1] == 0x76 || // Auto
          d[1] == 0x78 ||  // Manual A/C on
          (m_climate_fan_speed->AsInt() != 0); // Older leafs do not transmit anything except fan speed (?), backup

        /* These may only reflect the centre console LEDs, which
        * means they don't change when remote CC is operating.
        * They should be replaced when we find better signals that
        * indicate when heating or cooling is actually occuring.
        */
        StandardMetrics.ms_v_env_cooling->SetValue(d[1] & 0x10);
        StandardMetrics.ms_v_env_heating->SetValue(d[1] & 0x02);
        // The following 2 values work only when preheat is activated while connected to charger.
        m_climate_remoteheat->SetValue(d[1] == 0x4b);
        m_climate_remotecool->SetValue(d[1] == 0x71);
      }
      else if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) < 2016)
      {
        bool cooling = d[1] == 0x78 || /* cool only */
                       d[1] == 0x79 || /* cool + heat */
                       d[1] == 0x76;   /* cool + auto */

        // d[1] == 0x47 : heat + auto
        // d[1] == 0x49 : heat only
        // d[1] == 0x79 : heat + cool

        bool heating = (d[1] & 0x01);

        // The following value work only when car is on, so we need to use fan/heat/cool values to indicate hvac on as described below
        bool climate_on = (d[0] == 0x10);


        StandardMetrics.ms_v_env_heating->SetValue(heating);
        StandardMetrics.ms_v_env_cooling->SetValue(cooling);
        // The following 2 values work only when preheat is activated while connected to charger.
        m_climate_remoteheat->SetValue(d[1] == 0x4b);
        m_climate_remotecool->SetValue(d[1] == 0x71);
        hvac_calculated = (climate_on
                        & (m_climate_fan_speed->AsInt() != 0)
                        & !m_climate_really_off);
      }
      else
      // More accurate climate control values for hvac, heating, cooling for 2016+ model year cars.
      {
        bool cooling = (d[1] & 0x30);
        //               d[1] == 0x7a || /* remote cool only */
        //               d[1] == 0x78 || /* cool only */
        //               d[1] == 0x79 || /* cool + heat */
        //               d[1] == 0x76;   /* cool + auto */

        // d[1] == 0x47 : heat + auto
        // d[1] == 0x49 : heat only
        // d[1] == 0x79 : heat + cool
        bool heating = (d[1] & 0x01);

        // The following value work only when car is on, so we need to use fan/heat/cool values to indicate hvac on as described below
        // bool climate_on = (d[0] == 0x10);
        StandardMetrics.ms_v_env_heating->SetValue(heating);
        StandardMetrics.ms_v_env_cooling->SetValue(cooling);
        // The following 2 values work only when climate control is activated while connected to charger.
        m_climate_remoteheat->SetValue(!StandardMetrics.ms_v_env_on->AsBool() && heating);
        m_climate_remotecool->SetValue(!StandardMetrics.ms_v_env_on->AsBool() && cooling);
        m_climate_auto->SetValue(d[1] & 0x02);

        hvac_calculated =  (d[1] != 0x08 && d[1] != 0x04);
        // if climate control is off set fan speed to 0 as can bus value seems to be fan speed setpoint
        if (!hvac_calculated) m_climate_fan_speed->SetValue(0);
      }

      if (hvac_calculated) {
        m_climate_rqinprogress->SetValue(false);
      }
      StandardMetrics.ms_v_env_hvac->SetValue(hvac_calculated);

      bool fan_only = ( (d[1] == 0x48 || d[1] == 0x46) && fanspeed_int != 0 );
      m_climate_fan_only->SetValue(fan_only);

    }
      break;
    case 0x54c:
      /* Ambient temperature.  This one has half-degree C resolution,
       * and seems to stay within a degree or two of the "eyebrow" temp display.
       * App label: AMBIENT
       */
      m_climate_really_off = (d[5] == 0x00 || d[5] == 0xf8);

      if (d[6] != 0xff)
        {
        StandardMetrics.ms_v_env_temp->SetValue(d[6] / 2.0 - 40);
        }

      m_climate_really_off = (d[5] == 0x00 || d[5] == 0xf8);

      break;
    case 0x54f:
      /* Climate control's measurement of temperature inside the car.
       * Appears to be in Fahrenheit. Unsure why the check for 20?
       * mjk: seeems like off by 6 celcius on 2013 models, so added cabintempoffset that can be set in GUI.
       */
      if (d[0] != 20)
        {
        StandardMetrics.ms_v_env_cabintemp->SetValue((5.0 / 9.0 * (d[0] - 32)) + MyConfig.GetParamValueFloat("xnl", "cabintempoffset", DEFAULT_CABINTEMP_OFFSET));
        // StandardMetrics.ms_v_env_cabintemp->SetValue(d[0] / 2.0 - 14);
        }
      break;
    case 0x55a:
      {
      /* Motor, charge and inverter temperature guesses in Fahrenheit?
       * http://productions.8dromeda.net/c55-leaf-inverter-protocol.html
       */
      StandardMetrics.ms_v_mot_temp->SetValue(5.0 / 9.0 * (d[1] - 32));
      StandardMetrics.ms_v_inv_temp->SetValue(5.0 / 9.0 * (d[2] - 32));
      }
      break;
    case 0x55b:
      {
      /* 10-bit SOC%. Actual value calculated by battery.
       * This value never reaches 100% (or 0%) due to charging safety buffer
       */
      uint16_t soc = d[0] << 2 | d[1] >> 6;
      if (soc != 0x3ff)
        {
        m_soc_nominal->SetValue(soc/10.0);
        }
      }
      break;
    case 0x59e:
      {
      switch(m_battery_type->AsInt(BATTERY_TYPE_2_24kWh))
        {
        case BATTERY_TYPE_1_24kWh:
        case BATTERY_TYPE_2_24kWh:
          {
          uint16_t cap_gid = d[2] << 4 | d[3] >> 4;
          m_battery_energy_capacity->SetValue(cap_gid * GEN_1_WH_PER_GID, WattHours);
          m_kWh_capacity_read = true;
          }
          break;
        case BATTERY_TYPE_2_30kWh:
          // This msg does not give a sensible capacity estimate for 30kWh battery,
          // instead m_battery_energy_capacity is set from message 0x5bc
          break;
        }
      }
      break;
    case 0x5bc:
      {
      switch (d[5] >> 5)
        { // additional charge messages see https://github.com/dalathegreat/leaf_can_bus_messages
        case NORMAL:
          m_charge_limit->SetValue("normal");
          break;
        case CAPACITY_DROP:
          m_charge_limit->SetValue("capacity drop");
          break;
        case LBC_MALFUNCTION:
          m_charge_limit->SetValue("malfunction");
          break;
        case HIGH_TEMP:
          m_charge_limit->SetValue("high temp");
          break;
        case LOW_TEMP:
          m_charge_limit->SetValue("low temp");
          break;
        }

      // ZE0 SOH
      if (m_battery_type->AsInt(0) == BATTERY_TYPE_1_24kWh) {
        uint8_t soh = (d[4] >> 1 & 0xF7);
        m_soh_instrument->SetValue(soh);
        if (!MyConfig.GetParamValueBool("xnl", "soh.newcar", false))
        {
          StandardMetrics.ms_v_bat_soh->SetValue(soh);
        }
      }


      uint16_t nl_gids = ((uint16_t) d[0] << 2) | ((d[1] & 0xc0) >> 6);
      uint8_t  mx_gids = (d[5] & 0x10) >> 4;
      uint8_t  mx_bars_ZE0 = (d[4] & 0x01);
      int type = -1;
      // gids is invalid during startup
      if (nl_gids != 1023)
      {
        // On LEAF ZE0 200X-2012, some values differ from AZE0
        switch (mx_bars_ZE0) {
         case 0x00:
           {
             if (m_battery_type->AsInt(0) == BATTERY_TYPE_1_24kWh) {
               uint8_t chargebars = (d[2] & 0x0F);
               m_remaining_chargebars->SetValue(chargebars);
             }
           }
           break;
          case 0x01:
           {
             uint8_t capbars = (d[2] & 0x0F);
             m_capacitybars->SetValue(capbars);
             type = BATTERY_TYPE_1_24kWh;
           }
           break;
        }
        switch (mx_gids)
          {
          case 0x00:
            {
            // Current gids on 24 and 30kwh models
            m_gids->SetValue(nl_gids);
            m_battery_energy_available->SetValue(nl_gids * GEN_1_WH_PER_GID, WattHours);

            // new car soc -- 100% when the battery is new, less when it's degraded
            uint16_t max_gids = MyConfig.GetParamValueInt("xnl", "maxGids", GEN_1_NEW_CAR_GIDS);
            float soc_new_car = (nl_gids * 100.0) / max_gids;
            m_soc_new_car->SetValue(soc_new_car);

            // we use the instrument cluster soc from 0x1db unless the user has opted otherwise
            if (MyConfig.GetParamValueBool("xnl", "soc.newcar", false))
              {
              StandardMetrics.ms_v_bat_soc->SetValue(soc_new_car);
              }
            // 2012 Leaf has no instrument soc, battery capacity and max gids will be set from config, because it is not available on CAN
            if (MyConfig.GetParamValueBool("xnl", "soc.newcar.capacity", false)) {
              m_battery_energy_capacity->SetValue(max_gids * GEN_1_WH_PER_GID, WattHours);
              m_kWh_capacity_read = true;
              }
            }
            break;
          case  0x01:
            {
            // Max gids, this mx value only occurs on 30kWh models and up
            m_max_gids->SetValue(nl_gids);
            m_battery_energy_capacity->SetValue(nl_gids * GEN_1_WH_PER_GID, WattHours);
            type = BATTERY_TYPE_2_30kWh;
            m_kWh_capacity_read = true;
            }
            break;
          }
        }
      /* Estimated charge time is a set of multiplexed values:
       *   d[5]     d[6]     d[7]
       * 76543210 76543210 76543210
       * ......mm mmmvvvvv vvvvvvvv
       */
      uint16_t mx  = ( d[5] << 3 | d[6] >> 5 ) & 0x1f;
      uint16_t val = ( d[6] << 8 | d[7] ) & 0x1fff;
      if (val != 0x1fff)
        {
        /* Battery type 1 and 2 use different (* and conficting)
         * mx values to identify the charge duration type:
         *         |    |  full 100% 	    | range 80%  	|
         *   type  | QC | 6.6kW  200V  100V | 6.6kW  200V  100V |
         *  ------ | -- |  --    --    --   |  --    --    --   |
         *   ZE0 1 |  0 |  NA    9     17   |  NA    10    18*	|
         *  AZE0 2 |  0 |  5	 8     11   |  18*   21    24	|
         *
         * Only type 1 and type 2 24kwh models from before 2016 will report a valid 'range 80%'.
         * Any type 2 24 or 30kwh models starting mid 2015 (USA/Jap) or 2016 (UK), will always
         * return 0x1fff, and therefore never enter this if with mx values 18, 21 or 24.
         * This is linked to Nissan removing the 'long life mode (80%)' from the car settings.
         */
        int cd = -1;
        switch (mx)
          {
          case  0: m_quick_charge->SetValue(val); break;
          case  5: cd = CHARGE_DURATION_FULL_L2;  break;
          case  8: cd = CHARGE_DURATION_FULL_L1;  break;
          case  9: cd = CHARGE_DURATION_FULL_L1;  type = BATTERY_TYPE_1_24kWh; break;
          case 10: cd = CHARGE_DURATION_RANGE_L1; type = BATTERY_TYPE_1_24kWh; break;
          case 11: cd = CHARGE_DURATION_FULL_L0;  break;
          case 17: cd = CHARGE_DURATION_FULL_L0;  type = BATTERY_TYPE_1_24kWh; break;
          case 18: // meaning of mx 18 differs by battery version
            switch(m_battery_type->AsInt(BATTERY_TYPE_2_24kWh))
              {
              case BATTERY_TYPE_1_24kWh: cd = CHARGE_DURATION_RANGE_L0; break;
              case BATTERY_TYPE_2_24kWh: cd = CHARGE_DURATION_RANGE_L2; break;
              case BATTERY_TYPE_2_30kWh: break;  // Will never occur with val != 0x1fff
              }
            break;
          case 21: cd = CHARGE_DURATION_RANGE_L1; type = BATTERY_TYPE_2_24kWh; break;
          case 24: cd = CHARGE_DURATION_RANGE_L0; type = BATTERY_TYPE_2_24kWh; break;
          }
        if (cd != -1) m_charge_duration->SetElemValue(cd, val/2);
        }
      // If detected, save battery type
      if (type != -1)
        {
        m_battery_type->SetValue(type);
        }
      }
      break;
    case 0x5bf:
		// ZE0 gen1 charger only
      if (d[4] == 0xb0)
        {
        // Quick Charging
        // TODO enum?
        //StandardMetrics.ms_v_charge_type->SetValue("chademo");
        StandardMetrics.ms_v_charge_climit->SetValue(120);
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        //StandardMetrics.ms_v_door_chargeport->SetValue(true);
        }
      else
        {
        // J1772 might be connected
        // d[2] is the J1772 maximum available current, 0 if we're not plugged in
        // NOTE: this message doesn't update except when charging.
        // Current limit doesn't seem to go to zero after ending
        // TODO enum?
        uint8_t current_limit = d[2] / 5;
        StandardMetrics.ms_v_charge_climit->SetValue(current_limit);
        if (current_limit > 0 && current_limit <= 32)
          {
          StandardMetrics.ms_v_charge_pilot->SetValue(true);
          }
		else
		  {
		  StandardMetrics.ms_v_charge_pilot->SetValue(false);
		  }
        }

      switch (d[4])
        {
        case 0x70: // Starting to charge
        case 0x18: // Signal cable plugged in (beep sound)
          StandardMetrics.ms_v_charge_pilot->SetValue(true);
          break;
        case 0x38: // Present when vehicle is off, no cable plugged or charger is "asleep" (with cable plugged)
          break;
        case 0x28: // Present when vehicle is on
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x30:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        case 0xb0: // Quick Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          break;
        case 0x60: // L2 Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x40:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_FINISHED);
          break;
        }
      break;
    case 0x5c0:
      /* Battery Temperature as reported by the LBC.
       * Effectively has only 7-bit precision, as the bottom bit is always 0.
       */
      if ( (d[0]>>6) == 1 )
        {
        StandardMetrics.ms_v_bat_temp->SetValue(d[2] / 2 - 40);
        }
      /* Battery Heater as reported by the LBC.
       * Battery heating needed below -17*C
       * TODO: Notify user in app that battery has entered this state!
       */
      if ( d[4] && MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) >= 2013 )
        {
        m_battery_heaterpresent->SetValue(d[4] & 0x01); // Heater pads present
        m_battery_heatrequested->SetValue(d[1] & 0x01); // Request permission to turn on
        }
      break;
    case 0x679:
      ESP_LOGI(TAG, "J1772/VCM Wakeup!");
      break;
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrameCan2(CAN_frame_t* p_frame)
  { // CAN2 is connected to CAR-CAN
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x180:
      if (d[5] != 0xff)
        {
        StandardMetrics.ms_v_env_throttle->SetValue(d[5] / 2.00);
        }
      break;
    case 0x292:
      if (d[6] != 0xff)
        {
        StandardMetrics.ms_v_env_footbrake->SetValue(d[6] / 1.39);
        }
      break;
   case 0x355:
     // The odometer value on the bus is always in units appropriate
     // for the car's intended market and is unaffected by the dash
     // display preference (in d[4] bit 5).
     if ((d[6]>>5) & 1)
       {
         m_odometer_units = Miles;
       }
     else
       {
         m_odometer_units = Kilometers;
       }
     break;
    case 0x385:
      if (d[2]) StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, d[2] / 4.0, PSI);
      if (d[3]) StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, d[3] / 4.0, PSI);
      if (d[4]) StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, d[4] / 4.0, PSI);
      if (d[5]) StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, d[5] / 4.0, PSI);
      break;
    case 0x421:
      switch ( (d[0] >> 3) & 7 )
        {
        case 0: // Parking
        case 1: // Park
        case 3: // Neutral
        case 5: // undefined
        case 6: // undefined
          StandardMetrics.ms_v_env_gear->SetValue(0);
          if (cfg_enable_write && StandardMetrics.ms_v_env_on->AsBool()) PollSetState(POLLSTATE_ON);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          if (cfg_enable_write) PollSetState(POLLSTATE_RUNNING);
          break;
        case 4: // Drive
        case 7: // Drive/B (ECO on some models)
          StandardMetrics.ms_v_env_gear->SetValue(1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          if (cfg_enable_write) PollSetState(POLLSTATE_RUNNING);
          break;
        }
      break;
    case 0x5a9:
      {
      uint16_t nl_range = d[1] << 4 | d[2] >> 4;
      if (nl_range != 0xfff)
        {
        m_range_instrument->SetValue(nl_range / 5, Kilometers);
        }
      break;
      }
    case 0x5b9:
      {
      uint8_t chargebars = ((d[1] & 0xF8) >> 3); // Range: 0-12
      m_remaining_chargebars->SetValue(chargebars);

      //Minutes from 3kW charging estimate on dashboard
      uint16_t minutes = ((d[1] & 0x07) | d[2]);
      m_charge_minutes_3kW_remaining->SetValue(minutes);
      }
      break;
    case 0x5b3:
      {
      // soh as percentage
      if (!cfg_ze1) // ZE1 gets SOH by polling group 61
        {
          uint8_t soh = d[1] >> 1;
          if (soh != 0)
            {
            m_soh_instrument->SetValue(soh);
            // we use this unless the user has opted otherwise
            if (!cfg_soh_newcar)
              {
              StandardMetrics.ms_v_bat_soh->SetValue(soh);
              }
            }
        }
      break;
      }
    case 0x5c5:
      // This is the parking brake (which is foot-operated on some models).
      StandardMetrics.ms_v_env_handbrake->SetValue(d[0] & 4);
      if (m_odometer_units != Other)
        {
        StandardMetrics.ms_v_pos_odometer->SetValue(d[1] << 16 | d[2] << 8 | d[3], m_odometer_units);
        }
      break;
    case 0x60d:
      StandardMetrics.ms_v_door_trunk->SetValue(d[0] & 0x80);
      StandardMetrics.ms_v_door_rr->SetValue(d[0] & 0x40);
      StandardMetrics.ms_v_door_rl->SetValue(d[0] & 0x20);
      StandardMetrics.ms_v_door_fr->SetValue(d[0] & 0x10);
      StandardMetrics.ms_v_door_fl->SetValue(d[0] & 0x08);
      StandardMetrics.ms_v_env_headlights->SetValue((d[0] & 0x02) | // dip beam
                                                    (d[1] & 0x08)); // main beam
      /* d[1] bits 1 and 2 indicate Start button states:
       *   No brake:   off -> accessory -> on -> off
       *   With brake: [off, accessory, on] -> ready to drive -> off
       * Using "ready to drive" state to set ms_v_env_on seems sensible,
       * though maybe "on, not ready to drive" should also count.
       */
       // The two lock bits are 0x10 driver door and 0x08 other doors.
       // We should only say "locked" if both are locked.
       // change to only check driver door, on AZE0 0x08 not always 1 when locked
      if ((d[2] & 0x10) == 0x10)
        {
        StandardMetrics.ms_v_env_locked->SetValue(true);
        // vehicle_nissanleaf_car_on(false); causes issues for cars that lock on driving
        }
      else
        {
        StandardMetrics.ms_v_env_locked->SetValue(false);
        }

      switch ((d[1]>>1) & 3)
        {
        case 0: // off
          vehicle_nissanleaf_car_on(false);
          break;
        case 1: // accessory
        case 2: // on (not ready to drive)
          StandardMetrics.ms_v_env_awake->SetValue(true);
          break;
        case 3: // ready to drive, is triggered on unlock
          StandardMetrics.ms_v_env_awake->SetValue(true);
          if (StandardMetrics.ms_v_env_footbrake->AsFloat() > 0) //check footbrake to avoid false positive
            {
            vehicle_nissanleaf_car_on(true);
            }
          break;
        }

      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// Send a RemoteCommand on the CAN bus.
// Handles pre and post 2016 model year cars based on xnl.modelyear config
//
// Does nothing if @command is out of range

void OvmsVehicleNissanLeaf::SendCommand(RemoteCommand command)
  {
  if (!cfg_enable_write) return; //disable commands unless canwrite is true
  uint8_t data[4] = {0, 0, 0, 0};
  esp_err_t result;
  uint8_t length;
  canbus *tcuBus;
  bool advancedCommand = false;

  if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) >= 2016)
    {
    ESP_LOGV(TAG, "Possible new TCU on CAR Bus");
    length = 4;
    tcuBus = m_can2;
    }
  else
    {
    ESP_LOGV(TAG, "Possible old TCU on EV Bus");
    length = 1;
    tcuBus = m_can1;
    }

  if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) > 2012) {
     CommandWakeup();
  } else if (MyConfig.GetParamValueBool("xnl", "command.wakeup", true)) {
     CommandWakeupTCU();
  }
  switch (command)
    {
    case ENABLE_CLIMATE_CONTROL:
      m_climate_rqinprogress->SetValue(true);
      ESP_LOGI(TAG, "Enable Climate Control");
      data[0] = 0x4e;
      data[1] = 0x08;
      data[2] = 0x12;
      data[3] = 0x00;
      break;
    case DISABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Disable Climate Control");
      data[0] = 0x56;
      data[1] = 0x00;
      data[2] = 0x01;
      data[3] = 0x00;
      break;
    case START_CHARGING:
      ESP_LOGI(TAG, "Start Charging");
      data[0] = 0x66;
      data[1] = 0x08;
      data[2] = 0x12;
      data[3] = 0x00;
      break;
    case AUTO_DISABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Auto Disable Climate Control");
      data[0] = 0x46;
      data[1] = 0x08;
      data[2] = 0x32;
      data[3] = 0x00;
      break;
    case UNLOCK_DOORS:
      ESP_LOGI(TAG, "Unlock Doors");
      data[0] = 0x11;
      data[1] = 0x00;
      data[2] = 0x00;
      data[3] = 0x00;
      break;
    case LOCK_DOORS:
      ESP_LOGI(TAG, "Lock Doors");
      data[0] = 0x60;
      data[1] = 0x80;
      data[2] = 0x00;
      data[3] = 0x00;
      break;
    default:
      // use for more advanced activation sequences
      advancedCommand = true;
      break;
    }
    if (advancedCommand)
    {
      switch (command)
      {
      case STOP_CHARGING:
        ESP_LOGI(TAG, "Stop Charging");
        if (!m_MITM) {
          m_MITM = 10;
          xTimerStart(m_MITMstop, 0);
        }
        break;
      default:
        return;
      }
    }
    else
    {
      result = tcuBus->WriteStandard(0x56e, length, data);
      if (result != ESP_OK)
      {
          ESP_LOGE(TAG, "CAN TX Error: 0x56e, result=%d", result);
      }
    }
  }

////////////////////////////////////////////////////////////////////////
// implements the repeated sending of remote commands and releases
// EV SYSTEM ACTIVATION REQUEST after an appropriate amount of time

void OvmsVehicleNissanLeaf::RemoteCommandTimer()
  {
  ESP_LOGI(TAG, "RemoteCommandTimer %d", nl_remote_command_ticker);
  // if lock or unlock is successful don't repeat command
  if (nl_remote_command == LOCK_DOORS && StandardMetrics.ms_v_env_locked->AsBool())
    {
    nl_remote_command_ticker = 0;
    }
  if (nl_remote_command == UNLOCK_DOORS && !StandardMetrics.ms_v_env_locked->AsBool())
    {
    nl_remote_command_ticker = 0;
    }
  if (nl_remote_command_ticker > 0)
    {
    nl_remote_command_ticker--;
    if (nl_remote_command != AUTO_DISABLE_CLIMATE_CONTROL)
      {
      SendCommand(nl_remote_command);
      }
    if (nl_remote_command_ticker == 1 && nl_remote_command == ENABLE_CLIMATE_CONTROL)
      {
      xTimerStart(m_ccDisableTimer, 0);
      }

    if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) < 2013)
    {
      // nl_remote_command_ticker is set to REMOTE_COMMAND_REPEAT_COUNT in
      // RemoteCommandHandler() and we decrement it every 10th of a
      // second, hence the following if statement evaluates to true
      // ACTIVATION_REQUEST_TIME tenths after we start
      if (nl_remote_command_ticker == (REMOTE_COMMAND_REPEAT_COUNT - ACTIVATION_REQUEST_TIME))
        {
        // release EV SYSTEM ACTIVATION REQUEST
        MyPeripherals->m_max7317->Output((uint8_t)cfg_ev_request_port, 0);
        ESP_LOGI(TAG, "EV SYSTEM ACTIVATION REQUEST OFF");
        }
      }
    }
    else
    {
      xTimerStop(m_remoteCommandTimer, 0);
    }
  }

void OvmsVehicleNissanLeaf::CcDisableTimer()
  {
  ESP_LOGI(TAG, "CcDisableTimer");
  SendCommand(AUTO_DISABLE_CLIMATE_CONTROL);
  }

/**
 * Ticker1: Called every second
 */
void OvmsVehicleNissanLeaf::Ticker1(uint32_t ticker)
  {
  // Update any derived values
  // Energy used varies a lot during driving
  HandleEnergy();
  }

/**
 * Ticker10: Called every 10 seconds
 */
void OvmsVehicleNissanLeaf::Ticker10(uint32_t ticker)
  {
  // Update any derived values
  // Range and Charging both mainly depend on SOC, which will change 1% in less than a minute when fast-charging.
  HandleRange();
  HandleCharging();
  HandleChargeEstimation();
  HandleExporting();
  if (StandardMetrics.ms_v_bat_12v_voltage->AsFloat() > 12.8)
    {
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    }
  else StandardMetrics.ms_v_env_charging12v->SetValue(false);
  // FIXME
  // detecting that on is stale and therefor should turn off probably shouldn't
  // be done like this
  // perhaps there should be a car on-off state tracker and event generator in
  // the core framework?
  // perhaps interested code should be able to subscribe to "onChange" and
  // "onStale" events for each metric?
  ESP_LOGD(TAG, "Poll state: %d", m_poll_state);
  if (StandardMetrics.ms_v_env_awake->AsBool() && StandardMetrics.ms_v_env_awake->IsStale())
    {
    StandardMetrics.ms_v_env_awake->SetValue(false);
    }

  if (StandardMetrics.ms_v_charge_pilot->AsBool()) {
    StandardMetrics.ms_v_door_chargeport->SetValue(true);
  }
  // assume charge port has been closed 2min after pilot signal finished
  else if ( StandardMetrics.ms_v_charge_pilot->IsStale()
    && ((MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) < 2013) || StandardMetrics.ms_v_gen_pilot->IsStale())
    && StandardMetrics.ms_v_door_chargeport->AsBool() )
    {
    std::string str = StandardMetrics.ms_v_charge_substate->AsString();
    if      (str == "timerwait") ;
    else if (str == "powerwait" && (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) > 2012));
    else StandardMetrics.ms_v_door_chargeport->SetValue(false);
    }
  }

/**
 * Update derived energy metrics while driving
 * Called once per second from Ticker1
 */
void OvmsVehicleNissanLeaf::HandleEnergy()
  {
  // Are we driving?
  if (StandardMetrics.ms_v_env_on->AsBool() &&
      (m_cum_energy_used_wh > 0.0f || m_cum_energy_recd_wh > 0.0f) )
    {
    // Update energy used and recovered
    StandardMetrics.ms_v_bat_energy_used->SetValue( StandardMetrics.ms_v_bat_energy_used->AsFloat() + m_cum_energy_used_wh / 1000.0, kWh);
    StandardMetrics.ms_v_bat_energy_recd->SetValue( StandardMetrics.ms_v_bat_energy_recd->AsFloat() + m_cum_energy_recd_wh / 1000.0, kWh);
    m_cum_energy_used_wh = 0.0f;
    m_cum_energy_recd_wh = 0.0f;
    // Calculate inverter efficiency
    float m_batt_power   = StandardMetrics.ms_v_bat_power->AsFloat(0);
    float m_inv_power    = StandardMetrics.ms_v_inv_power->AsFloat(0);
    if (m_batt_power != 0)
      { // Will include accessory power TODO subtract from battery power
      StandardMetrics.ms_v_inv_efficiency->SetValue(abs(m_inv_power / m_batt_power) * 100.0);
      // If power flow is negative, power is flowing from inverter into battery
      if (m_batt_power < 0) StandardMetrics.ms_v_inv_efficiency->SetValue(abs(m_batt_power / m_inv_power) * 100.0);
      }
    }
  else StandardMetrics.ms_v_inv_efficiency->SetValue(100);
  }

/**
 * Update derived metrics when exporting/V2X
 * Called once per 10 seconds from Ticker10
 */
void OvmsVehicleNissanLeaf::HandleExporting()
  {
  // Are we exporting energy?
  if ( !StandardMetrics.ms_v_gen_inprogress->AsBool() )
    {
    StandardMetrics.ms_v_gen_power->SetValue(0);
    return;
    }
  // Check if we have what is needed to calculate energy and remaining minutes
  float gen_power_w = 0;
  if (m_cum_energy_gen_wh > 0)
    {
    // Convert 10sec worth of energy back to an average charge power (in watts)
    gen_power_w = m_cum_energy_gen_wh * 3600 / 10;

    StandardMetrics.ms_v_gen_kwh->SetValue( StandardMetrics.ms_v_gen_kwh->AsFloat() + m_cum_energy_gen_wh / 1000.0, kWh);
    m_cum_energy_gen_wh = 0.0f;

    float limit_soc      = StandardMetrics.ms_v_gen_limit_soc->AsFloat(0);
    float limit_range    = StandardMetrics.ms_v_gen_limit_range->AsFloat(0, Kilometers);
    float max_range      = StandardMetrics.ms_v_bat_range_est->AsFloat(0, Kilometers);

    // always calculate remaining charge time to full
    float empty_soc           = 0.0f;     // 100%
    int   minsremaining_empty = calcMinutesRemaining(empty_soc, -gen_power_w);

    StandardMetrics.ms_v_gen_duration_empty->SetValue(minsremaining_empty, Minutes);
    ESP_LOGV(TAG, "Time remaining: %d mins to empty", minsremaining_empty);

    if (limit_soc > 0)
      {
      // if limit_soc is set, then calculate remaining time to limit_soc
      int minsremaining_soc = calcMinutesRemaining(limit_soc, -gen_power_w);

      StandardMetrics.ms_v_gen_duration_soc->SetValue(minsremaining_soc, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", minsremaining_soc, limit_soc);
      }

    if (limit_range > 0 && max_range > 0.0)
      {
      // if range limit is set, then compute required soc and then calculate remaining time to that soc
      float range_soc           = limit_range / max_range * 100.0;
      int   minsremaining_range = calcMinutesRemaining(range_soc, -gen_power_w);

      StandardMetrics.ms_v_gen_duration_range->SetValue(minsremaining_range, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins for %0.0f km (%0.0f%% soc)", minsremaining_range, limit_range, range_soc);
      }
    }
  }

/**
 * Update derived metrics when charging
 * Called once per 10 seconds from Ticker10
 */
void OvmsVehicleNissanLeaf::HandleChargeEstimation()
  {
  // Are we charging?
  if (!StandardMetrics.ms_v_charge_pilot->AsBool()      ||
      !StandardMetrics.ms_v_charge_inprogress->AsBool() )
    {
    StandardMetrics.ms_v_charge_power->SetValue(0);
    // default to 100% so it does not effect an overall efficiency calculation
    StandardMetrics.ms_v_charge_efficiency->SetValue(100);
    return;
    }
  // Check if we have what is needed to calculate energy and remaining minutes
  float charge_power_w  = 0;
  if (m_cum_energy_charge_wh > 0)
    {
    // Convert 10sec worth of energy back to an average charge power (in watts)
    charge_power_w = m_cum_energy_charge_wh * 3600 / 10;

    StandardMetrics.ms_v_charge_kwh->SetValue( StandardMetrics.ms_v_charge_kwh->AsFloat() + m_cum_energy_charge_wh / 1000.0, kWh);
    m_cum_energy_charge_wh = 0.0f;

    float limit_soc      = StandardMetrics.ms_v_charge_limit_soc->AsFloat(0);
    float limit_range    = StandardMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers);
    float max_range      = StandardMetrics.ms_v_bat_range_full->AsFloat(0, Kilometers);

    // always calculate remaining charge time to full
    float full_soc           = 100.0;     // 100%

    // adjust max range if 'ideal' is selected as estimation method
    if (cfg_limit_range_calc != "est")
    {
      max_range = (full_soc
                    * StandardMetrics.ms_v_bat_range_ideal->AsFloat(0, Kilometers)
                    / StandardMetrics.ms_v_bat_soc->AsFloat(0) );
    }

    int   minsremaining_full = calcMinutesRemaining(full_soc, charge_power_w);

    StandardMetrics.ms_v_charge_duration_full->SetValue(minsremaining_full, Minutes);
    ESP_LOGV(TAG, "Time remaining: %d mins to full", minsremaining_full);

    if (limit_soc > 0)
      {
      // if limit_soc is set, then calculate remaining time to limit_soc
      int minsremaining_soc = calcMinutesRemaining(limit_soc, charge_power_w);

      StandardMetrics.ms_v_charge_duration_soc->SetValue(minsremaining_soc, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", minsremaining_soc, limit_soc);
      }

    if (limit_range > 0 && max_range > 0.0)
      {
      // if range limit is set, then compute required soc and then calculate remaining time to that soc
      float range_soc           = limit_range / max_range * 100.0;
      int   minsremaining_range = calcMinutesRemaining(range_soc, charge_power_w);

      StandardMetrics.ms_v_charge_duration_range->SetValue(minsremaining_range, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins for %0.0f km (%0.0f%% soc)", minsremaining_range, limit_range, range_soc);
      }
    }
  // calculate charger power and efficiency
  float m_charge_power   = StandardMetrics.ms_v_charge_power->AsFloat();
  float m_batt_power     = StandardMetrics.ms_v_bat_power->AsFloat();
  if (m_charge_power != 0)
    {
    StandardMetrics.ms_v_charge_efficiency->SetValue(abs(m_batt_power / m_charge_power) * 100.0);
    }
  if (StandardMetrics.ms_v_charge_efficiency->AsFloat() > 100)
    { // due to rounding precision bat power can report > charger power at low charge rates
    StandardMetrics.ms_v_charge_efficiency->SetValue(100);
    }
  }

/**
 * Update charging substate and handle charge to target SOC/Range feature
 * Called once per 10 seconds from Ticker10
 */
void OvmsVehicleNissanLeaf::HandleCharging()
  {
  bool   chg_ctrl_activated = false;
  bool   cc_on_or_requested = (m_climate_rqinprogress->AsBool() || StandardMetrics.ms_v_env_hvac->AsBool());
  float  limit_soc          = StandardMetrics.ms_v_charge_limit_soc->AsFloat(0);
  float  bat_soc            = StandardMetrics.ms_v_bat_soc->AsFloat(0);
  float  limit_range        = StandardMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers);
  string charge_state       = StandardMetrics.ms_v_charge_state->AsString();
  string charge_substate    = StandardMetrics.ms_v_charge_substate->AsString();
  string prev_c_state       = m_charge_state_previous->AsString();
  string prev_notify_msg    = m_charge_user_notified->AsString();
  std::string notify_msg;
  float  controlled_range   = StandardMetrics.ms_v_bat_range_est->AsFloat(0, Kilometers);

  if (cfg_limit_range_calc != "est")
  {
    controlled_range        = StandardMetrics.ms_v_bat_range_ideal->AsFloat(0, Kilometers);
  }
  // handle charge interruption substate
  if (prev_c_state != charge_state) {
    if ( charge_state    == "stopped"
      && (prev_c_state   == "charging"
      || prev_c_state    == "timerwait")
      && charge_substate != "scheduledstop"
      && charge_substate != "powerwait")
    {
      StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
    }
    if ( charge_state    == "charging"
      && (prev_c_state   == "stopped"
      || prev_c_state    == "timerwait"
      || charge_substate == "powerwait")
      && charge_substate != "scheduledstart")
    {
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
    }
    if ( charge_state    == "timerwait"
      && charge_substate != "onrequest"
      && charge_substate != "scheduledstop")
    {
      if (StandardMetrics.ms_v_charge_pilot->AsBool()) {
        StandardMetrics.ms_v_charge_substate->SetValue("timerwait");
      }
      else {
        StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
      }
    }
    m_charge_state_previous->SetValue(charge_state);
    m_charge_user_notified->SetValue("reset");
  }
  // handle auto charge start/stop
  if (limit_soc > 0 && m_charge_event_reason->AsString() != "range")
    {
    // if limit_soc is set, then stop charging accordingly
    if (bat_soc >= limit_soc)
      {
        if (charge_state == "charging") {
          if (cfg_enable_autocharge && !cc_on_or_requested) {
            StandardMetrics.ms_v_charge_substate->SetValue("scheduledstop");
            RemoteCommandHandler(STOP_CHARGING);
            m_charge_event_reason->SetValue("");
          }
          notify_msg = "Target charge level reached";
          if (prev_notify_msg != notify_msg) {
            m_charge_user_notified->SetValue(notify_msg);
            MyNotify.NotifyStringf("info", "v-nissanleaf.charge.status",
              "%s (%.0f%%)", notify_msg.c_str(), limit_soc
            );
          }
          chg_ctrl_activated = true;
        }
      }
    else if ( charge_state != "charging"
          && ( (charge_state != "timerwait" && charge_substate != "onrequest" && charge_substate != "scheduledstop")
            || (bat_soc < limit_soc - cfg_allowed_socdrop && charge_substate == "scheduledstop")
          )
          && StandardMetrics.ms_v_charge_pilot->AsBool()
          )
        {

        if (cfg_enable_autocharge) {
          StandardMetrics.ms_v_charge_substate->SetValue("scheduledstart");
          RemoteCommandHandler(START_CHARGING);
          m_charge_event_reason->SetValue("soc");
        }
        notify_msg = "Charge level below target";
        if (prev_notify_msg != notify_msg) {
          m_charge_user_notified->SetValue(notify_msg);
          MyNotify.NotifyStringf("info", "v-nissanleaf.charge.status",
            "%s (%.0f%%)", notify_msg.c_str(), bat_soc
          );
        }
        chg_ctrl_activated = true;

      }
    }
  if (limit_range > 0 && bat_soc < 100 && !chg_ctrl_activated && m_charge_event_reason->AsString() != "soc")
    {
    // if limit_range is set, then stop charging accordingly
    if (controlled_range >= limit_range)
      {
        if (charge_state == "charging") {
          if (cfg_enable_autocharge && !cc_on_or_requested) {
            StandardMetrics.ms_v_charge_substate->SetValue("scheduledstop");
            RemoteCommandHandler(STOP_CHARGING);
            m_charge_event_reason->SetValue("");
          }
          notify_msg = "Target driving range reached";
          if (prev_notify_msg != notify_msg) {
            m_charge_user_notified->SetValue(notify_msg);
            MyNotify.NotifyStringf("info", "v-nissanleaf.charge.status",
              "%s (%.0f km)", notify_msg.c_str(), limit_range
            );
          }
        }
      }
    else if ( charge_state != "charging"
          && ( (charge_state != "timerwait" && charge_substate != "onrequest" && charge_substate != "scheduledstop")
            || (controlled_range < limit_range - cfg_allowed_rangedrop && charge_substate == "scheduledstop")
          )
          && StandardMetrics.ms_v_charge_pilot->AsBool()
          )
      {
        if (cfg_enable_autocharge) {
          StandardMetrics.ms_v_charge_substate->SetValue("scheduledstart");
          RemoteCommandHandler(START_CHARGING);
          m_charge_event_reason->SetValue("range");
        }
        notify_msg = "Driving range below target";
        if (prev_notify_msg != notify_msg) {
          m_charge_user_notified->SetValue(notify_msg);
          MyNotify.NotifyStringf("info", "v-nissanleaf.charge.status",
            "%s (%.0f km)", notify_msg.c_str(), controlled_range
          );
        }
      }
    }
  }


/**
 * Calculates minutes remaining before target is reached. Based on current charge speed.
 * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
 */
int OvmsVehicleNissanLeaf::calcMinutesRemaining(float target_soc, float charge_power_w)
  { // updated to allow for V2X calculation
  float bat_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
  if (bat_soc == 0) {
    // Cannot get instrument SOC, use soc from BMS
    bat_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
  }
  if ( (bat_soc > target_soc && charge_power_w > 0) || (bat_soc < target_soc && charge_power_w < 0) )
    {
    return 0;   // Done!
    }

  //if (charge_power_w <= 0.0f)
  //  {
  //  return 1440;
  //  }

  float bat_cap_kwh     = m_battery_energy_capacity->AsFloat(24, kWh);
  float remaining_wh    = bat_cap_kwh * 1000.0 * (target_soc - bat_soc) / 100.0;
  float remaining_hours = remaining_wh / charge_power_w;
  float remaining_mins  = remaining_hours * 60.0;

  if (remaining_mins < 0)
    remaining_mins = 1440;

  return MIN( 1440, (int)remaining_mins );
  }

/**
 * Update derived metrics related to range while charging or driving
 */
void OvmsVehicleNissanLeaf::HandleRange()
  {
  float bat_cap_kwh = m_battery_energy_capacity->AsFloat(24, kWh);  // TODO: check this.
  float bat_soc     = StandardMetrics.ms_v_bat_soc->AsFloat(0);
  float bat_soh     = StandardMetrics.ms_v_bat_soh->AsFloat(100);
  float bat_temp    = StandardMetrics.ms_v_bat_temp->AsFloat(20, Celcius);
  float amb_temp    = StandardMetrics.ms_v_env_temp->AsFloat(20, Celcius);

  float km_per_kwh  = MyConfig.GetParamValueFloat("xnl", "kmPerKWh", GEN_1_KM_PER_KWH);
  float wh_per_gid  = MyConfig.GetParamValueFloat("xnl", "whPerGid", GEN_1_WH_PER_GID);
  float max_gids    = MyConfig.GetParamValueFloat("xnl", "maxGids",  GEN_1_NEW_CAR_GIDS);

  // we use detected battery capacity unless the user has opted otherwise
  float max_kwh     = (!cfg_soh_newcar)
                            ? bat_cap_kwh
                            : max_gids * wh_per_gid / 1000.0;

  // Temperature compensation:
  //   - Assumes standard max range specified at 20 degrees C
  //   - Range halved at -20C and at +60C.
  // See: https://www.fleetcarma.com/nissan-leaf-chevrolet-volt-cold-weather-range-loss-electric-vehicle/
  //
  float avg_temp    = (amb_temp + bat_temp*3) / 4.0;
  float factor_temp = (100.0 - ABS(avg_temp - 20.0) * 1.25) / 100.0;

  // Derive max and ideal range, taking into account SOC, but not SOH or temperature
  float range_max   = max_kwh * km_per_kwh;
  float range_ideal = range_max * bat_soc / 100.0;

  // Derive max range when full and estimated range, taking into account SOC, SOH and temperature
  float range_full = range_max   * bat_soh / 100.0 * factor_temp;
  float range_est  = range_ideal * bat_soh / 100.0 * factor_temp;

  // Store the metrics
  if (range_ideal > 0.0)
    {
    StandardMetrics.ms_v_bat_range_ideal->SetValue(range_ideal, Kilometers);
    }

  if (range_full > 0.0)
	{
	StandardMetrics.ms_v_bat_range_full->SetValue(range_full, Kilometers);
	}

  if (range_est > 0.0)
	{
    StandardMetrics.ms_v_bat_range_est->SetValue(range_est, Kilometers);
    }

  // Trace
  ESP_LOGV(TAG, "Range: ideal=%0.0f km, est=%0.0f km, full=%0.0f km", range_ideal, range_est, range_full);
  }

/**
 * Reset trip counters when vehicle is turned on, including energy usage values
 */
void OvmsVehicleNissanLeaf::ResetTripCounters()
  {
  StdMetrics.ms_v_pos_trip->SetValue(0);
  m_trip_odo            = 0;
  m_trip_last_upd_time  = esp_log_timestamp();
  m_trip_last_upd_speed = StdMetrics.ms_v_pos_speed->AsFloat();
  }

/**
 * Increment trip odometer and v.p.trip based on distance derived from speed
 *
 * Odometer granularity seems to be 1km, that's not really precise enough
 */
void OvmsVehicleNissanLeaf::UpdateTripCounters()
  {
  uint32_t now = esp_log_timestamp();
  float speed = StdMetrics.ms_v_pos_speed->AsFloat();

  if (m_trip_last_upd_time && now > m_trip_last_upd_time) {
    float speed_avg = ABS(speed + m_trip_last_upd_speed) / 2;
    uint32_t time_ms = now - m_trip_last_upd_time;

    // 1 m/s = 3.6 km/h
    double dist_meters = speed_avg / 3.6 * time_ms / 1000;
    m_trip_odo += dist_meters / 1000;
    StdMetrics.ms_v_pos_trip->SetValue(TRUNCPREC(m_trip_odo, 3));
  }

  m_trip_last_upd_time = now;
  m_trip_last_upd_speed = speed;
  }

////////////////////////////////////////////////////////////////////////
// Wake up the car & send Climate Control or Remote Charge message to VCU,
// replaces Nissan's CARWINGS and TCU module, see
// http://www.mynissanleaf.com/viewtopic.php?f=44&t=4131&hilit=open+CAN+discussion&start=416
//
// On Generation 1 Cars, TCU pin 11's "EV system activation request signal" is
// driven to 12V to wake up the VCU. This function drives the configured pin high to
// activate the "EV system activation request signal". Without a circuit
// connecting the configured pin to the activation signal wire, remote climate control will
// only work during charging and for obvious reasons remote charging won't
// work at all.
//
// On Generation 2 Cars, a CAN bus message is sent to wake up the VCU. This
// function sends that message even to Generation 1 cars which doesn't seem to
// cause any problems.
// TODO (UPDATE for GEN1: It automatically prompts to starts charging
// TODO every time there is no command issued after it like turning on A/C)
OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandWakeup()
  {
  // Shotgun approach to waking up the vehicle. Send all kinds of wakeup messages
  if (!cfg_enable_write) return Fail; // Disable commands unless canwrite is true
  ESP_LOGI(TAG, "Sending Wakeup Frame");
  /*
  unsigned char data = 0;
  m_can1->WriteStandard(0x679, 1, &data); //Wakes up the modules by spoofing VCM startup message
  m_can1->WriteStandard(0x679, 1, &data); //Tops up the 12V battery if connected to EVSE
  m_can1->WriteStandard(0x5C0, 8, &data); //Wakes up the VCM (by spoofing empty battery request heating)
  */
  uint8_t d8[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t d1[1] = {0x00};
  m_can1->WriteStandard(0x679, 1, d1); //Wakes up the modules by spoofing VCM startup message
  m_can1->WriteStandard(0x5C0, 8, d8); //Wakes up the VCM (by spoofing empty battery request heating)

  return Success;
  }

// Wakeup VCM using command which allegedly is used by TCU (https://docs.google.com/spreadsheets/d/1EHa4R85BttuY4JZ-EnssH4YZddpsDVu6rUFm0P7ouwg/edit?gid=0#gid=0)
void OvmsVehicleNissanLeaf::CommandWakeupTCU()
  {
  if (!cfg_enable_write) return; // Disable commands unless canwrite is true
  ESP_LOGI(TAG, "Sending CarWings TCU->VCU Wakeup Frame");
  unsigned char data = 0;
  m_can1->WriteStandard(0x68c, 1, &data); //Wakes up VCM By sending a wakeup message
}

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::RemoteCommandHandler(RemoteCommand command)
  {
  if (!cfg_enable_write) return Fail; //disable commands unless canwrite is true
  ESP_LOGI(TAG, "RemoteCommandHandler");
  // Use the configured pin to wake up GEN 1 Leaf with EV SYSTEM ACTIVATION REQUEST
  if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) < 2013)
  {
    if (MyConfig.GetParamValueBool("xnl", "command.wakeup", true)) {
       CommandWakeupTCU();
    }
    MyPeripherals->m_max7317->Output((uint8_t)cfg_ev_request_port, 1);
    ESP_LOGI(TAG, "EV SYSTEM ACTIVATION REQUEST ON");
  } else {
    // Use wakeup only for newer cars.
    CommandWakeup();
  }
  // The GEN 2 Nissan TCU module sends the command repeatedly, so we start
  // m_remoteCommandTimer (which calls RemoteCommandTimer()) to do this
  // EV SYSTEM ACTIVATION REQUEST is released in the timer too
  nl_remote_command = command;
  nl_remote_command_ticker = REMOTE_COMMAND_REPEAT_COUNT;
  xTimerStart(m_remoteCommandTimer, 0);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandHomelink(int button, int durationms)
  {
  ESP_LOGI(TAG, "CommandHomelink");
  if (button == 0)
    {
    return RemoteCommandHandler(ENABLE_CLIMATE_CONTROL);
    }
  if (button == 1)
    {
    return RemoteCommandHandler(DISABLE_CLIMATE_CONTROL);
    }
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandClimateControl(bool climatecontrolon)
  {
  ESP_LOGI(TAG, "CommandClimateControl");
  return RemoteCommandHandler(climatecontrolon ? ENABLE_CLIMATE_CONTROL : DISABLE_CLIMATE_CONTROL);
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandLock(const char* pin)
  {
  return RemoteCommandHandler(LOCK_DOORS);
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandUnlock(const char* pin)
  {
  return RemoteCommandHandler(UNLOCK_DOORS);
  }

/**
 * ProcessMsgCommand: V2 compatibility protocol message command processing
 *  result: optional payload or message to return to the caller with the command response
 */
OvmsVehicleNissanLeaf::vehicle_command_t OvmsVehicleNissanLeaf::ProcessMsgCommand(string& result, int command, const char* args)
{
  switch (command)
  {
    case CMD_SetChargeAlerts:
      return MsgCommandCA(result, command, args);
    case CMD_QueryChargeAlerts:
      return MsgCommandCA(result, command, args);
    default:
      return NotImplemented;
  }
}

void OvmsVehicleNissanLeaf::MITMDisableTimer()
{
  ESP_LOGI(TAG, "MITM attempted off");
  if(m_MITM) {
    m_MITM = 0;
    xTimerStop(m_MITMstop, 0);
    ESP_LOGI(TAG, " MITM turned off");
  }
}

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandStopCharge()
{
  ESP_LOGI(TAG, "CommandStopCharge - MITM method");
  StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
  return RemoteCommandHandler(STOP_CHARGING);
}

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandStartCharge()
  {
  ESP_LOGI(TAG, "CommandStartCharge");
  StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
  return RemoteCommandHandler(START_CHARGING);
  }

/**
 * MsgCommandCA:
 *  - CMD_QueryChargeAlerts()
 *  - CMD_SetChargeAlerts(<range>,<soc>,[<power>],[<stopmode>])
 *  - Result: <range>,<soc>,<time_range>,<time_soc>,<time_full>,<power>,<stopmode>
 */
OvmsVehicleNissanLeaf::vehicle_command_t OvmsVehicleNissanLeaf::MsgCommandCA(std::string &result, int command, const char* args)
{
  if (command == CMD_SetChargeAlerts)
  {
    std::istringstream sentence(args);
    std::string token;

    // CMD_SetChargeAlerts(<range limit>,<soc limit>,<charge mode>,<range drop>, <soc drop>)
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xnl", "suffrange", atoi(token.c_str()));
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xnl", "suffsoc", atoi(token.c_str()));
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueBool("xnl", "autocharge", atoi(token.c_str()) == 1);
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xnl", "rangedrop", atoi(token.c_str()));
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xnl", "socdrop", atoi(token.c_str()));
    // Synchronize with config changes:
    ConfigChanged(NULL);
  }
  // Result: <range>,<soc>,<stopmode>,<rangedrop>,<socdrop>
  std::ostringstream buf;
  buf
    << std::setprecision(0)
    << MyConfig.GetParamValue("xnl", "suffrange", "0") << ","
    << MyConfig.GetParamValue("xnl", "suffsoc", "0") << ","
    << ((MyConfig.GetParamValueBool("xnl", "autocharge", true) == true) ? "1" : "0") << ","
    << MyConfig.GetParamValueInt("xnl", "rangedrop", DEFAULT_RANGEDROP) << ","
    << MyConfig.GetParamValueInt("xnl", "socdrop", DEFAULT_SOCDROP);
  result = buf.str();
  return Success;
}

/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleNissanLeaf::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 10:
      MyConfig.SetParamValue("xnl", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("xnl", "suffrange", value);
      return true;
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xnl", "canwrite",  (bits& 1)!=0);
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
const std::string OvmsVehicleNissanLeaf::GetFeature(int key)
{
  switch (key)
  {
    case 0:
    case 10:
      return MyConfig.GetParamValue("xnl", "suffsoc", STR(0));
    case 11:
      return MyConfig.GetParamValue("xnl", "suffrange", STR(0));
    case 15:
    {
      int bits =
        ( MyConfig.GetParamValueBool("xnl", "canwrite",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
  }
}

class OvmsVehicleNissanLeafInit
  {
  public: OvmsVehicleNissanLeafInit();
  } MyOvmsVehicleNissanLeafInit  __attribute__ ((init_priority (9000)));

OvmsVehicleNissanLeafInit::OvmsVehicleNissanLeafInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Nissan Leaf (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleNissanLeaf>("NL","Nissan Leaf");
  }
