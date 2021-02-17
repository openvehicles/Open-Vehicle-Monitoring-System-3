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
 
const PROGMEM byte rqBattHWrev[4]                 = {0x03, 0x22, 0xF1, 0x50};
const PROGMEM byte rqBattSWrev[4]                 = {0x03, 0x22, 0xF1, 0x51};
const PROGMEM byte rqBattVIN[4]                   = {0x03, 0x22, 0xF1, 0x90};
const PROGMEM byte rqBattTemperatures[4]          = {0x03, 0x22, 0x02, 0x01}; 
const PROGMEM byte rqBattModuleTemperatures[4]    = {0x03, 0x22, 0x02, 0x02};
const PROGMEM byte rqBattHVstatus[4]              = {0x03, 0x22, 0x02, 0x04};
const PROGMEM byte rqBattADCref[4]                = {0x03, 0x22, 0x02, 0x07};
const PROGMEM byte rqBattVolts[6]                 = {0x03, 0x22, 0x02, 0x08, 28, 57};
const PROGMEM byte rqBattIsolation[4]             = {0x03, 0x22, 0x02, 0x09};
const PROGMEM byte rqBattAmps[4]                  = {0x03, 0x22, 0x02, 0x03};
const PROGMEM byte rqBattDate[4]                  = {0x03, 0x22, 0x03, 0x04};
const PROGMEM byte rqBattProdDate[4]              = {0x03, 0x22, 0xF1, 0x8C};
const PROGMEM byte rqBattCapacity[6]              = {0x03, 0x22, 0x03, 0x10, 31, 59};
const PROGMEM byte rqBattHVContactorCyclesLeft[4] = {0x03, 0x22, 0x03, 0x0B};
const PROGMEM byte rqBattHVContactorMax[4]        = {0x03, 0x22, 0x03, 0x0C};
const PROGMEM byte rqBattHVContactorState[4]      = {0x03, 0x22, 0xD0, 0x00};

//Experimental readouts
const PROGMEM byte rqBattCapInit[4]               = {0x03, 0x22, 0x03, 0x05};
const PROGMEM byte rqBattCapLoss[4]               = {0x03, 0x22, 0x03, 0x09};
const PROGMEM byte rqBattUnknownCounter[4]        = {0x03, 0x22, 0x01, 0x01};

// NLG6-Charger module
const PROGMEM byte rqChargerPN_HW[4]              = {0x03, 0x22, 0xF1, 0x11};
const PROGMEM byte rqChargerSWrev[4]              = {0x03, 0x22, 0xF1, 0x21};
const PROGMEM byte rqChargerVoltages[4]           = {0x03, 0x22, 0x02, 0x26};
const PROGMEM byte rqChargerAmps[4]               = {0x03, 0x22, 0x02, 0x25};
const PROGMEM byte rqChargerSelCurrent[4]         = {0x03, 0x22, 0x02, 0x2A};
const PROGMEM byte rqChargerTemperatures[4]       = {0x03, 0x22, 0x02, 0x23};
 */

#include "ovms_log.h"
static const char *TAG = "v-smarted";

#include <stdio.h>
#include <algorithm>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_events.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_utils.h"

#include "vehicle_smarted.h"

#undef SQR
#define SQR(n) ((n)*(n))
#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))
#undef LIMIT_MIN
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#undef LIMIT_MAX
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))

#undef ROUNDPREC
#define ROUNDPREC(fval,prec) (round((fval) * pow(10,(prec))) / pow(10,(prec)))

static const OvmsVehicle::poll_pid_t smarted_polls[] =
{
  { 0x7E5, 0x7ED, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x1001, {  0,3600,3600,3600 }, 0, ISOTP_STD }, // Wippen...
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF111, {  0,300,600,600 }, 0, ISOTP_STD }, // rqChargerPN_HW
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0226, {  0,300,0,60 }, 0, ISOTP_STD }, // rqChargerVoltages
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0225, {  0,300,0,60 }, 0, ISOTP_STD }, // rqChargerAmps
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x022A, {  0,300,0,60 }, 0, ISOTP_STD }, // rqChargerSelCurrent
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0223, {  0,300,0,60 }, 0, ISOTP_STD }, // rqChargerTemperatures
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF190, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattVIN
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0208, {  0,300,600,60 }, 0, ISOTP_STD }, // rqBattVolts
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0310, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattCapacity
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0203, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattAmps
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0207, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattADCref
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0304, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattDate
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF18C, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattProdDate
  //getBatteryRevision
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF150, {  0,300,600,0 }, 0, ISOTP_STD }, //rqBattHWrev
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF151, {  0,300,600,0 }, 0, ISOTP_STD }, //rqBattSWrev
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0201, {  0,300,600,120 }, 0, ISOTP_STD }, // rqBattTemperatures
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0202, {  0,300,600,120 }, 0, ISOTP_STD }, // rqBattModuleTemperatures
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x030B, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattHVContactorCyclesLeft
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x030C, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattHVContactorMax
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xD000, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattHVContactorState
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0204, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattHVstatus
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0209, {  0,300,600,0 }, 0, ISOTP_STD }, // rqBattIsolation
  POLL_LIST_END
};

void OvmsVehicleSmartED::ObdInitPoll() {

  m_bms_capacitys = NULL;
  m_bms_cmins = NULL;
  m_bms_cmaxs = NULL;
  m_bms_cdevmaxs = NULL;
  m_bms_calerts = NULL;
  m_bms_calerts_new = 0;
  m_bms_has_capacitys = false;

  m_bms_bitset_c.clear();
  m_bms_bitset_cc = 0;
  m_bms_readings_c = 0;
  m_bms_readingspermodule_c = 0;

  m_bms_limit_cmin = 1000;
  m_bms_limit_cmax = 22000;

  mt_v_bat_pack_cmin = new OvmsMetricFloat("xse.v.b.p.capacity.min", SM_STALE_HIGH, Other);
  mt_v_bat_pack_cmax = new OvmsMetricFloat("xse.v.b.p.capacity.max", SM_STALE_HIGH, Other);
  mt_v_bat_pack_cavg = new OvmsMetricFloat("xse.v.b.p.capacity.avg", SM_STALE_HIGH, Other);
  mt_v_bat_pack_cstddev = new OvmsMetricFloat("xse.v.b.p.capacity.stddev", SM_STALE_HIGH, Other);
  mt_v_bat_pack_cstddev_max = new OvmsMetricFloat("xse.v.b.p.capacity.stddev.max", SM_STALE_HIGH, Other);
  mt_v_bat_pack_cmin_cell = new OvmsMetricInt("xse.v.b.p.capacity.min.cell", SM_STALE_HIGH, Other);
  mt_v_bat_pack_cmax_cell = new OvmsMetricInt("xse.v.b.p.capacity.max.cell", SM_STALE_HIGH, Other);
  mt_v_bat_pack_cmin_cell_volt = new OvmsMetricInt("xse.v.b.p.voltage.min.cell", SM_STALE_HIGH, Other);
  mt_v_bat_pack_cmax_cell_volt = new OvmsMetricInt("xse.v.b.p.voltage.max.cell", SM_STALE_HIGH, Other);

  mt_v_bat_cell_capacity = new OvmsMetricVector<float>("xse.v.b.c.capacity", SM_STALE_HIGH, Other);
  mt_v_bat_cell_cmin = new OvmsMetricVector<float>("xse.v.b.c.capacity.min", SM_STALE_HIGH, Other);
  mt_v_bat_cell_cmax = new OvmsMetricVector<float>("xse.v.b.c.capacity.max", SM_STALE_HIGH, Other);
  mt_v_bat_cell_cdevmax = new OvmsMetricVector<float>("xse.v.b.c.capacity.dev.max", SM_STALE_HIGH, Other);

  mt_v_bat_HVoff_time = new OvmsMetricInt("xse.v.b.p.hv.off.time", SM_STALE_HIGH, Seconds);
  mt_v_bat_HV_lowcurrent = new OvmsMetricInt("xse.v.b.p.hv.lowcurrent", SM_STALE_HIGH, Seconds);
  mt_v_bat_OCVtimer = new OvmsMetricInt("xse.v.b.p.ocv.timer", SM_STALE_HIGH, Seconds);
  mt_v_bat_LastMeas_days = new OvmsMetricInt("xse.v.b.p.last.meas.days", SM_STALE_HIGH, Other);
  mt_v_bat_Cap_meas_quality = new OvmsMetricFloat("xse.v.b.p.capacity.quality", SM_STALE_HIGH, Other);
  mt_v_bat_Cap_combined_quality = new OvmsMetricFloat("xse.v.b.p.capacity.combined.quality", SM_STALE_HIGH, Other);
  mt_v_bat_Cap_As_min = new OvmsMetricInt("xse.v.b.p.capacity.as.minimum", SM_STALE_HIGH, Other);
  mt_v_bat_Cap_As_max = new OvmsMetricInt("xse.v.b.p.capacity.as.maximum", SM_STALE_HIGH, Other);
  mt_v_bat_Cap_As_avg = new OvmsMetricInt("xse.v.b.p.capacity.as.average", SM_STALE_HIGH, Other);

  mt_nlg6_present             = MyMetrics.InitBool("xse.v.nlg6.present", SM_STALE_MIN, false);
  mt_nlg6_main_volts          = new OvmsMetricVector<float>("xse.v.nlg6.main.volts", SM_STALE_HIGH, Volts);
  mt_nlg6_main_amps           = new OvmsMetricVector<float>("xse.v.nlg6.main.amps", SM_STALE_HIGH, Amps);
  mt_nlg6_amps_setpoint       = MyMetrics.InitFloat("xse.v.nlg6.amps.setpoint", SM_STALE_MIN, 0, Amps);
  mt_nlg6_amps_cablecode      = MyMetrics.InitFloat("xse.v.nlg6.amps.cablecode", SM_STALE_MIN, 0, Amps);
  mt_nlg6_amps_chargingpoint  = MyMetrics.InitFloat("xse.v.nlg6.amps.chargingpoint", SM_STALE_MIN, 0, Amps);
  mt_nlg6_dc_current          = MyMetrics.InitFloat("xse.v.nlg6.dc.current", SM_STALE_MIN, 0, Amps);
  mt_nlg6_dc_hv               = MyMetrics.InitFloat("xse.v.nlg6.dc.hv", SM_STALE_MIN, 0, Volts);
  mt_nlg6_dc_lv               = MyMetrics.InitFloat("xse.v.nlg6.dc.lv", SM_STALE_MIN, 0, Volts);
  mt_nlg6_temps               = new OvmsMetricVector<float>("xse.v.nlg6.temps", SM_STALE_HIGH, Celcius);
  mt_nlg6_temp_reported       = MyMetrics.InitFloat("xse.v.nlg6.temp.reported", SM_STALE_MIN, 0, Celcius);
  mt_nlg6_temp_socket         = MyMetrics.InitFloat("xse.v.nlg6.temp.socket", SM_STALE_MIN, 0, Celcius);
  mt_nlg6_temp_coolingplate   = MyMetrics.InitFloat("xse.v.nlg6.temp.coolingplate", SM_STALE_MIN, 0, Celcius);
  mt_nlg6_pn_hw               = MyMetrics.InitString("xse.v.nlg6.pn.hw", SM_STALE_MIN, 0);

  mt_myBMS_Amps                = new OvmsMetricFloat("xse.mybms.amps", SM_STALE_HIGH, Amps);
  mt_myBMS_Amps2               = new OvmsMetricFloat("xse.mybms.amps2", SM_STALE_HIGH, Amps);
  mt_myBMS_Power               = new OvmsMetricFloat("xse.mybms.power", SM_STALE_HIGH, kW);
  mt_myBMS_HV                  = new OvmsMetricFloat("xse.mybms.hv", SM_STALE_HIGH, Volts);
  mt_myBMS_HVcontactCyclesLeft = new OvmsMetricInt("xse.mybms.hv.contact.cycles.left", SM_STALE_HIGH, Other);
  mt_myBMS_HVcontactCyclesMax  = new OvmsMetricInt("xse.mybms.hv.contact.cycles.max", SM_STALE_HIGH, Other);
  mt_myBMS_ADCCvolts_mean      = new OvmsMetricInt("xse.mybms.adc.cvolts.mean", SM_STALE_HIGH, Other);
  mt_myBMS_ADCCvolts_min       = new OvmsMetricInt("xse.mybms.adc.cvolts.min", SM_STALE_HIGH, Other);
  mt_myBMS_ADCCvolts_max       = new OvmsMetricInt("xse.mybms.adc.cvolts.max", SM_STALE_HIGH, Other);
  mt_myBMS_ADCvoltsOffset      = new OvmsMetricInt("xse.mybms.adc.volts.offset", SM_STALE_HIGH, Other);
  mt_myBMS_Isolation           = new OvmsMetricInt("xse.mybms.isolation", SM_STALE_HIGH, Other);
  mt_myBMS_DCfault             = new OvmsMetricInt("xse.mybms.dc.fault", SM_STALE_HIGH, Other);
  mt_myBMS_BattVIN             = new OvmsMetricString("xse.mybms.batt.vin");
  mt_myBMS_HWrev               = new OvmsMetricVector<int>("xse.mybms.HW.rev", SM_STALE_HIGH, Other);
  mt_myBMS_SWrev               = new OvmsMetricVector<int>("xse.mybms.SW.rev", SM_STALE_HIGH, Other);

  mt_CEPC_Wippen               = new OvmsMetricBool("xse.cepc.wippen", SM_STALE_MID);

  // BMS configuration:
  BmsSetCellArrangementCapacity(93, 1);
  BmsSetCellArrangementVoltage(93, 1);
  BmsSetCellArrangementTemperature(9, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.030, 0.050);
  BmsSetCellDefaultThresholdsTemperature(4.0, 5.0);

  // init poller:
  PollSetPidList(m_can1, smarted_polls);
  PollSetState(0);
  PollSetThrottling(5);
  PollSetResponseSeparationTime(10);

  // init commands:
  OvmsCommand* cmd;
  OvmsCommand* obd;
  obd = cmd_xse->RegisterCommand("obd", "OBD2 tools");
  cmd = obd->RegisterCommand("request", "Send OBD2 request, output response");
  cmd->RegisterCommand("device", "Send OBD2 request to a device", shell_obd_request, "<txid> <rxid> <request>", 3, 3);
  cmd->RegisterCommand("broadcast", "Send OBD2 request as broadcast", shell_obd_request, "<request>", 1, 1);
  cmd->RegisterCommand("bms", "Send OBD2 request to BMS", shell_obd_request, "<request>", 1, 1);
  cmd->RegisterCommand("getvolts", "Send OBD2 request to get Cell Volts", shell_obd_request);
}

/**
 * Incoming poll reply messages
 */
void OvmsVehicleSmartED::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
  string& rxbuf = smarted_obd_rxbuf;
  static uint16_t last_pid = -1;
  
  if (pid != last_pid) {
    //ESP_LOGD(TAG, "pid: %04x length: %d m_poll_ml_remain: %d m_poll_ml_frame: %d", pid, length, m_poll_ml_remain, m_poll_ml_frame);
    last_pid = pid;
    m_poll_ml_frame=0;
  }
  
  // init / fill rx buffer:
  if (m_poll_ml_frame == 0) {
    rxbuf.clear();
    rxbuf.reserve(length + remain);
  }
  rxbuf.append((char*)data, length);
  
  if (remain)
    return;
  
  // char *buf = NULL;
  // size_t rlen = rxbuf.size(), offset = 0;
  // do {
    // rlen = FormatHexDump(&buf, rxbuf.data() + offset, rlen, 16);
    // offset += 16;
    // ESP_LOGW(TAG, "OBD2: reply [%02x %02x]: %s", type, pid, buf ? buf : "-");
  // } while (rlen);
  // if (buf)
    // free(buf);
  
  // complete:
  switch (pid) {
    case 0x0201: // rqBattTemperatures
      PollReply_BMS_BattTemp(rxbuf.data(), rxbuf.size());
      break;
    case 0x0202: // rqBattModuleTemperatures
      PollReply_BMS_ModuleTemp(rxbuf.data(), rxbuf.size());
      break;
    case 0x0203: //rqBattAmps
      PollReply_BMS_BattAmps(rxbuf.data(), rxbuf.size());
      break;
    case 0x0204: //rqBattHVstatus
      PollReply_BMS_BattHVstatus(rxbuf.data(), rxbuf.size());
      break;
    case 0x0207: //rqBattADCref
      PollReply_BMS_BattADCref(rxbuf.data(), rxbuf.size());
      break;
    case 0x0208: // rqBattVolts
      PollReply_BMS_BattVolts(rxbuf.data(), rxbuf.size());
      break;
    case 0x0209: // rqBattIsolation
      PollReply_BMS_BattIsolation(rxbuf.data(), rxbuf.size());
      break;
    case 0x0310: // rqBattCapacity
      PollReply_BMS_BattCapacity(rxbuf.data(), rxbuf.size());
      break;
    case 0x030B: // rqBattHVContactorCyclesLeft
      PollReply_BMS_BattHVContactorCyclesLeft(rxbuf.data(), rxbuf.size());
      break;
    case 0x030C: // rqBattHVContactorMax
      PollReply_BMS_BattHVContactorMax(rxbuf.data(), rxbuf.size());
      break;
    case 0xD000: // rqBattHVContactorState
      PollReply_BMS_BattHVContactorState(rxbuf.data(), rxbuf.size());
      break;
    case 0x0304: // rqBattDate
      PollReply_BMS_BattDate(rxbuf.data(), rxbuf.size());
      break;
    case 0xF18C: // rqBattProdDate
      PollReply_BMS_BattProdDate(rxbuf.data(), rxbuf.size());
      break;
    case 0xF150: //rqBattHWrev
      PollReply_BMS_BattHWrev(rxbuf.data(), rxbuf.size());
      break;
    case 0xF151: //rqBattSWrev
      PollReply_BMS_BattSWrev(rxbuf.data(), rxbuf.size());
      break;
    case 0xF190: // rqBattVIN
      PollReply_BMS_BattVIN(rxbuf.data(), rxbuf.size());
      break;
    case 0xF111: // rqChargerPN_HW
      PollReply_NLG6_ChargerPN_HW(rxbuf.data(), rxbuf.size());
      break;
    case 0x0226: // rqChargerVoltages
      PollReply_NLG6_ChargerVoltages(rxbuf.data(), rxbuf.size());
      break;
    case 0x0225: // rqChargerAmps
      PollReply_NLG6_ChargerAmps(rxbuf.data(), rxbuf.size());
      break;
    case 0x022A: // rqChargerSelCurrent
      PollReply_NLG6_ChargerSelCurrent(rxbuf.data(), rxbuf.size());
      break;
    case 0x0223: // rqChargerTemperatures
      PollReply_NLG6_ChargerTemperatures(rxbuf.data(), rxbuf.size());
      break;
    case 0x1001:
      PollReply_CEPC(rxbuf.data(), rxbuf.size());
      break;
    // Unknown: output
    default: {
      char *buf = NULL;
      size_t rlen = rxbuf.size(), offset = 0;
      do {
        rlen = FormatHexDump(&buf, rxbuf.data() + offset, rlen, 16);
        offset += 16;
        ESP_LOGW(TAG, "OBD2: unhandled reply [%02x %02x]: %s", type, pid, buf ? buf : "-");
      } while (rlen);
      if (buf)
        free(buf);
      break;
    }
  }
  
  // single poll?
  if (!smarted_obd_rxwait.IsAvail()) {
    // yes: stop poller & signal response
    PollSetPidList(m_can1, NULL);
    smarted_obd_rxerr = 0;
    smarted_obd_rxwait.Give();
  }
}

void OvmsVehicleSmartED::IncomingPollError(canbus* bus, uint16_t type, uint16_t pid, uint16_t code)
{
  // single poll?
  if (!smarted_obd_rxwait.IsAvail()) {
    // yes: stop poller & signal response
    PollSetPidList(m_can1, NULL);
    smarted_obd_rxerr = code;
    smarted_obd_rxwait.Give();
  }
}

int OvmsVehicleSmartED::ObdRequest(uint16_t txid, uint16_t rxid, string request, string& response, int timeout_ms /*=3000*/) {
  OvmsMutexLock lock(&smarted_obd_request);

  // prepare single poll:
  OvmsVehicle::poll_pid_t poll[] = {
    { txid, rxid, 0, 0, { 1, 1, 1, 1 }, 0, ISOTP_STD },
    POLL_LIST_END
  };

  assert(request.size() > 0);
  poll[0].type = request[0];

  if (POLL_TYPE_HAS_16BIT_PID(poll[0].type)) {
    assert(request.size() >= 3);
    poll[0].args.pid = request[1] << 8 | request[2];
    poll[0].args.datalen = LIMIT_MAX(request.size()-3, sizeof(poll[0].args.data));
    memcpy(poll[0].args.data, request.data()+3, poll[0].args.datalen);
  }
  else if (POLL_TYPE_HAS_8BIT_PID(poll[0].type)) {
    assert(request.size() >= 2);
    poll[0].args.pid = request.at(1);
    poll[0].args.datalen = LIMIT_MAX(request.size()-2, sizeof(poll[0].args.data));
    memcpy(poll[0].args.data, request.data()+2, poll[0].args.datalen);
  }
  else {
    poll[0].args.pid = 0;
    poll[0].args.datalen = LIMIT_MAX(request.size()-1, sizeof(poll[0].args.data));
    memcpy(poll[0].args.data, request.data()+1, poll[0].args.datalen);
  }

  // stop default polling:
  PollSetPidList(m_can1, NULL);
  vTaskDelay(pdMS_TO_TICKS(100));

  // clear rx semaphore, start single poll:
  smarted_obd_rxwait.Take(0);
  PollSetPidList(m_can1, poll);

  // wait for response:
  bool rxok = smarted_obd_rxwait.Take(pdMS_TO_TICKS(timeout_ms));
  if (rxok == pdTRUE)
    response = smarted_obd_rxbuf;

  // restore default polling:
  smarted_obd_rxwait.Give();
  PollSetPidList(m_can1, smarted_polls);

  return (rxok == pdFALSE) ? -1 : (int)smarted_obd_rxerr;
}

void OvmsVehicleSmartED::PollReply_BMS_BattAmps(const char* reply_data, uint16_t reply_len) {
  float value = reply_data[0] * 256 + reply_data[1];
  mt_myBMS_Amps->SetValue(value);
  mt_myBMS_Amps2->SetValue(value / 32.0);
  mt_myBMS_Power->SetValue(StdMetrics.ms_v_bat_voltage->AsFloat() * mt_myBMS_Amps2->AsFloat()  / 1000.0);
}

void OvmsVehicleSmartED::PollReply_BMS_BattHVstatus(const char* reply_data, uint16_t reply_len) {
  float value = reply_data[8] * 256 + reply_data[9];
  if(HVcontactState != 0x02) {
    mt_myBMS_HV->SetValue((float) value/64.0);
  }
}

void OvmsVehicleSmartED::PollReply_BMS_BattHVContactorCyclesLeft(const char* reply_data, uint16_t reply_len) {
  mt_myBMS_HVcontactCyclesLeft->SetValue((unsigned long) reply_data[1] * 65536 + (uint16_t) reply_data[2] * 256 + reply_data[3]); 
}

void OvmsVehicleSmartED::PollReply_BMS_BattHVContactorMax(const char* reply_data, uint16_t reply_len) {
  mt_myBMS_HVcontactCyclesMax->SetValue((unsigned long) reply_data[1] * 65536 + (uint16_t) reply_data[2] * 256 + reply_data[3]); 
}

void OvmsVehicleSmartED::PollReply_BMS_BattHVContactorState(const char* reply_data, uint16_t reply_len) {
  HVcontactState = (uint16_t) reply_data[0]; 
}

void OvmsVehicleSmartED::PollReply_BMS_BattADCref(const char* reply_data, uint16_t reply_len) {
  mt_myBMS_ADCCvolts_mean->SetValue(reply_data[4] * 256 + reply_data[5]);
  mt_myBMS_ADCCvolts_min->SetValue(reply_data[2] * 256 + reply_data[3] + 1500);
  mt_myBMS_ADCCvolts_max->SetValue(reply_data[0] * 256 + reply_data[1] + 1500);
  
  if (RAW_VOLTAGES) {
    mt_myBMS_ADCvoltsOffset->SetValue(0);
  } else {
    mt_myBMS_ADCvoltsOffset->SetValue(StdMetrics.ms_v_bat_pack_vavg->AsFloat()*1000 - mt_myBMS_ADCCvolts_mean->AsInt());
  }
}

void OvmsVehicleSmartED::PollReply_BMS_BattDate(const char* reply_data, uint16_t reply_len) {
  myBMS_Year = reply_data[0];
  myBMS_Month = reply_data[1];
  myBMS_Day = reply_data[2];
}

void OvmsVehicleSmartED::PollReply_BMS_BattProdDate(const char* reply_data, uint16_t reply_len) {
  myBMS_ProdYear = (reply_data[0] - 48) * 10 + (reply_data[1] - 48);
  myBMS_ProdMonth = (reply_data[2] - 48) * 10 + (reply_data[3] - 48);
  myBMS_ProdDay = (reply_data[4] - 48) * 10 + (reply_data[5] - 48);
}

void OvmsVehicleSmartED::PollReply_BMS_BattHWrev(const char* reply_data, uint16_t reply_len) {
  for(int n = 0; n < 3; n++) {
    mt_myBMS_HWrev->SetElemValue(n, reply_data[n]);
  }
}

void OvmsVehicleSmartED::PollReply_BMS_BattSWrev(const char* reply_data, uint16_t reply_len) {
  for(int n = 3; n < 6; n++) {
    mt_myBMS_SWrev->SetElemValue(n-3, reply_data[n]);
  }
}

void OvmsVehicleSmartED::PollReply_BMS_BattIsolation(const char* reply_data, uint16_t reply_len) {
  uint16_t value = reply_data[0] * 256 + reply_data[1];//this->ReadDiagWord(&value,data,4,1);
  if (value != 65535) {
    mt_myBMS_Isolation->SetValue((signed) value);
    mt_myBMS_DCfault->SetValue(reply_data[2]);
  }
}

void OvmsVehicleSmartED::PollReply_BMS_BattVIN(const char* reply_data, uint16_t reply_len) {
  char vin[18];
  memset(vin, 0, sizeof(vin));
  for(int n = 0; n < 17; n++) {
    vin[n] = reply_data[n];
  }
  mt_myBMS_BattVIN->SetValue(vin);
}

void OvmsVehicleSmartED::PollReply_BMS_BattTemp(const char* reply_data, uint16_t reply_len) {
  for(uint16_t n = 0; n < (7 * 2); n = n + 2){
    myBMS_Temps[n/2] = ((reply_data[n] * 256 + reply_data[n + 1]));
  }
  //Copy max, min, mean and coolant-in temp to end of array
  for(uint16_t n = 0; n < 4; n++) {
    myBMS_Temps[n + 9] = myBMS_Temps[n];
  }
  if (myBMS_Temps[0]/64 != -512)
    StandardMetrics.ms_v_bat_temp->SetValue((float) myBMS_Temps[0]/64);
}

void OvmsVehicleSmartED::PollReply_BMS_ModuleTemp(const char* reply_data, uint16_t reply_len) {
  for(uint16_t n = 0; n < (9 * 2); n = n + 2){
    myBMS_Temps[n/2] = ((reply_data[n] * 256 + reply_data[n + 1]));
    BmsSetCellTemperature(n/2, (float) myBMS_Temps[n/2]/64);
  }
}

void OvmsVehicleSmartED::PollReply_BMS_BattVolts(const char* reply_data, uint16_t reply_len) {
  for(uint16_t n = 0; n < (CELLCOUNT * 2); n = n + 2){
    float Cells = (reply_data[n] * 256 + reply_data[n + 1]);
    BmsSetCellVoltage(n/2, Cells/1000);
  }
  float min=0, max=0;
  int cmin=0, cmax=0;
  for(int i = 0; i < StdMetrics.ms_v_bat_cell_voltage->GetSize(); i++) {
    if (min==0 || StdMetrics.ms_v_bat_cell_voltage->GetElemValue(i)<min) {
      min = StdMetrics.ms_v_bat_cell_voltage->GetElemValue(i);
      cmin = i+1;
    }
    if (max==0 || StdMetrics.ms_v_bat_cell_voltage->GetElemValue(i)>max) {
      max = StdMetrics.ms_v_bat_cell_voltage->GetElemValue(i);
      cmax = i+1;
    }
  }
  mt_v_bat_pack_cmin_cell_volt->SetValue(cmin);
  mt_v_bat_pack_cmax_cell_volt->SetValue(cmax);
}

void OvmsVehicleSmartED::PollReply_BMS_BattCapacity(const char* reply_data, uint16_t reply_len) {
  for(uint16_t n = 0; n < (CELLCOUNT * 2); n = n + 2){
    BmsSetCellCapacity(n/2, (reply_data[n + 21] * 256 + reply_data[n + 22]));
  }
/*
  myBMS->Ccap_As.min = CellCapacity.minimum(&myBMS->CAP_min_at);
  myBMS->Ccap_As.max = CellCapacity.maximum(&myBMS->CAP_max_at);
  myBMS->Ccap_As.mean = CellCapacity.mean();
*/
  mt_v_bat_HVoff_time->SetValue( reply_data[1] * 65535 + (uint16_t) reply_data[2] * 256 + reply_data[3] );
  mt_v_bat_HV_lowcurrent->SetValue( (unsigned long) reply_data[5] * 65535 + (uint16_t) reply_data[6] * 256 + reply_data[7] );
  mt_v_bat_OCVtimer->SetValue( (uint16_t) reply_data[8] * 256 + reply_data[9] );
  myBMS_SOH = reply_data[10];
  mt_v_bat_Cap_As_min->SetValue( reply_data[17] * 256 + reply_data[18] );
  mt_v_bat_Cap_As_avg->SetValue( reply_data[19] * 256 + reply_data[20] );
  mt_v_bat_Cap_As_max->SetValue( reply_data[13] * 256 + reply_data[14] );
  mt_v_bat_LastMeas_days->SetValue( reply_data[423] * 256 + reply_data[424] );
  uint16_t value;
  value = reply_data[425] * 256 + reply_data[426];
  mt_v_bat_Cap_meas_quality->SetValue( value / 65535.0 );
  value = reply_data[421] * 256 + reply_data[422];
  mt_v_bat_Cap_combined_quality->SetValue( value / 65535.0 );
  
  StandardMetrics.ms_v_bat_cac->SetValue(mt_v_bat_Cap_As_avg->AsFloat()/360.0, AmpHours);
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerPN_HW(const char* reply_data, uint16_t reply_len) {
  int n;
  int comp = 0;
  char NLG6PN_HW[11];
  memset(NLG6PN_HW, 0, sizeof(NLG6PN_HW));
  
  for (n = 0; n < 10; n++) {
    NLG6PN_HW[n] = reply_data[n];
    if (reply_data[n] == NLG6_PN_HW[n]) {
      comp++;
    }
  }
  mt_nlg6_pn_hw->SetValue(NLG6PN_HW);
  
  if (comp == 10){
    mt_nlg6_present->SetValue(true);
  } else {
    mt_nlg6_present->SetValue(false);
  }
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerVoltages(const char* reply_data, uint16_t reply_len) {
  float NLG6MainsVoltage[3];
  
  if (mt_nlg6_present->AsBool()){
    mt_nlg6_dc_lv->SetValue(reply_data[4]/10.0);
    mt_nlg6_dc_hv->SetValue((reply_data[5] * 256 + reply_data[6])/10.0);
    NLG6MainsVoltage[0] = (reply_data[7] * 256 + reply_data[8])/10.0;
    NLG6MainsVoltage[1] = (reply_data[9] * 256 + reply_data[10])/10.0;
    NLG6MainsVoltage[2] = (reply_data[11] * 256 + reply_data[12])/10.0;
  } else {
    mt_nlg6_dc_lv->SetValue(reply_data[2]/10.0);
    if ((reply_data[5] * 256 + reply_data[6]) != 8190) {  //OBL showing only valid data while charging
      mt_nlg6_dc_hv->SetValue((reply_data[5] * 256 + reply_data[6])/10.0);
    } else {
      mt_nlg6_dc_hv->SetValue(0);
    }
    if ((reply_data[7] * 256 + reply_data[8]) != 8190) {  //OBL showing only valid data while charging
      NLG6MainsVoltage[0] = (reply_data[7] * 256 + reply_data[8])/10.0;
    } else {
      NLG6MainsVoltage[0] = 0;
    }
    NLG6MainsVoltage[1] = 0;
    NLG6MainsVoltage[2] = 0;
  }
  mt_nlg6_main_volts->SetElemValues(0, 3, NLG6MainsVoltage);
  if (NLG6MainsVoltage[0] != 0) StandardMetrics.ms_v_charge_voltage->SetValue(NLG6MainsVoltage[0]);
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerAmps(const char* reply_data, uint16_t reply_len) {
  float NLG6MainsAmps[3]; 
  
  if (mt_nlg6_present->AsBool()){
    mt_nlg6_dc_current->SetValue((reply_data[0] * 256 + reply_data[1])/10.0); 
    NLG6MainsAmps[0] = (reply_data[2] * 256 + reply_data[3])/10.0;  
    NLG6MainsAmps[1] = (reply_data[4] * 256 + reply_data[5])/10.0;
    NLG6MainsAmps[2] = (reply_data[6] * 256 + reply_data[7])/10.0; 
    mt_nlg6_amps_chargingpoint->SetValue((reply_data[12] * 256 + reply_data[13])/10.0);
    mt_nlg6_amps_cablecode->SetValue((reply_data[14] * 256 + reply_data[15])/10.0);
  } else {
    if ((reply_data[2] * 256 + reply_data[3]) != 2047) {  //OBL showing only valid data while charging
      NLG6MainsAmps[0] = (reply_data[2] * 256 + reply_data[3])/10.0;
    } else {
      NLG6MainsAmps[0] = 0;
    }
    NLG6MainsAmps[1] = 0;
    NLG6MainsAmps[2] = 0;
    mt_nlg6_amps_chargingpoint->SetValue(0);
    if ((reply_data[14] * 256 + reply_data[15]) != 2047) {  //OBL showing only valid data while charging
      mt_nlg6_dc_current->SetValue((reply_data[14] * 256 + reply_data[15])/10.0);
    } else {
      mt_nlg6_dc_current->SetValue(0);
    }
    //Usable AmpsCode from Cable seem to be also a word with OBL as with NLG6?!
    mt_nlg6_amps_cablecode->SetValue((reply_data[4] * 256 + reply_data[5])/10.0);
    //NLG6AmpsCableCode = data[12]; //12
  }
  mt_nlg6_main_amps->SetElemValues(0, 3, NLG6MainsAmps);
  if (NLG6MainsAmps[0] != 0) StandardMetrics.ms_v_charge_current->SetValue(NLG6MainsAmps[0]);
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerSelCurrent(const char* reply_data, uint16_t reply_len) {
  if(mt_nlg6_present->AsBool()){
    mt_nlg6_amps_setpoint->SetValue(reply_data[4]); //Get data for NLG6 fast charger
  } else {
    mt_nlg6_amps_setpoint->SetValue(reply_data[3]); //7 //Get data for standard OBL
  }
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerTemperatures(const char* reply_data, uint16_t reply_len) {
  float NLG6Temps[9];
  int n;
  
  if (mt_nlg6_present->AsBool()){
    mt_nlg6_temp_coolingplate->SetValue((reply_data[0] < 0xFF) ? reply_data[0]-40 : 0);
    for(n = 0; n < 8; n++) {
      NLG6Temps[n] = (reply_data[n + 1] < 0xFF) ? reply_data[n + 1]-40 : 0;
    }
    mt_nlg6_temp_reported->SetValue((reply_data[8] < 0xFF) ? reply_data[8]-40 : 0);
    mt_nlg6_temp_socket->SetValue((reply_data[9] < 0xFF) ? reply_data[9]-40 : 0);
    mt_nlg6_temps->SetElemValues(0, 8, NLG6Temps);
  } else {
    mt_nlg6_temp_coolingplate->SetValue((reply_data[1] < 0xFF) ? reply_data[1]-40 : 0); //5
    mt_nlg6_temp_reported->SetValue((reply_data[3] < 0xFF) ? reply_data[3]-40 : 0); //7
    mt_nlg6_temp_socket->SetValue((reply_data[5] < 0xFF) ? reply_data[5]-40 : 0); //9
  }
  if (mt_nlg6_temp_reported->AsFloat() != 0) StandardMetrics.ms_v_charge_temp->SetValue(mt_nlg6_temp_reported->AsFloat());
}

void OvmsVehicleSmartED::PollReply_CEPC(const char* reply_data, uint16_t reply_len) {
  mt_CEPC_Wippen->SetValue(reply_data[16]&0x04);
}

// BMS helpers
void OvmsVehicleSmartED::BmsSetCellArrangementCapacity(int readings, int readingspermodule) {
  if (m_bms_capacitys != NULL) delete m_bms_capacitys;
  m_bms_capacitys = new float[readings];
  if (m_bms_cmins != NULL) delete m_bms_cmins;
  m_bms_cmins = new float[readings];
  if (m_bms_cmaxs != NULL) delete m_bms_cmaxs;
  m_bms_cmaxs = new float[readings];
  if (m_bms_cdevmaxs != NULL) delete m_bms_cdevmaxs;
  m_bms_cdevmaxs = new float[readings];

  m_bms_bitset_c.clear();
  m_bms_bitset_c.reserve(readings);

  m_bms_readings_c = readings;
  m_bms_readingspermodule_c = readingspermodule;

  BmsResetCellCapacitys();
}
  
void OvmsVehicleSmartED::BmsSetCellCapacity(int index, float value) {
  // ESP_LOGI(TAG,"BmsSetCellCapacity(%d,%f) c=%d", index, value, m_bms_bitset_cc);
  if ((index<0)||(index>=m_bms_readings_c)) return;
  if ((value<m_bms_limit_cmin)||(value>m_bms_limit_cmax)) return;
  m_bms_capacitys[index] = value;

  if (! m_bms_has_capacitys)
    {
    m_bms_cmins[index] = value;
    m_bms_cmaxs[index] = value;
    }
  else if (m_bms_cmins[index] > value)
    m_bms_cmins[index] = value;
  else if (m_bms_cmaxs[index] < value)
    m_bms_cmaxs[index] = value;

  if (m_bms_bitset_c[index] == false) m_bms_bitset_cc++;
  if (m_bms_bitset_cc == m_bms_readings_c) {
    // get min, max, avg & standard deviation:
    double sum=0, sqrsum=0, avg, stddev=0;
    float min=0, max=0;
    int cmin=0, cmax=0;
    for (int i=0; i<m_bms_readings_c; i++) {
      sum += m_bms_capacitys[i];
      sqrsum += SQR(m_bms_capacitys[i]);
      if (min==0 || m_bms_capacitys[i]<min) {
        min = m_bms_capacitys[i];
        cmin = i+1;
      }
      if (max==0 || m_bms_capacitys[i]>max) {
        max = m_bms_capacitys[i];
        cmax = i+1;
      }
    }
    avg = sum / m_bms_readings_c;
    stddev = sqrt(LIMIT_MIN((sqrsum / m_bms_readings_c) - SQR(avg), 0));
    // check cell deviations:
    float dev;
    for (int i=0; i<m_bms_readings_c; i++) {
      dev = ROUNDPREC(m_bms_capacitys[i] - avg, 1);
      if (ABS(dev) > ABS(m_bms_cdevmaxs[i]))
        m_bms_cdevmaxs[i] = dev;
    }
    // publish to metrics:
    avg = ROUNDPREC(avg, 1);
    stddev = ROUNDPREC(stddev, 1);
    mt_v_bat_pack_cmin->SetValue(min);
    mt_v_bat_pack_cmax->SetValue(max);
    mt_v_bat_pack_cavg->SetValue(avg);
    mt_v_bat_pack_cstddev->SetValue(stddev);
    mt_v_bat_pack_cmin_cell->SetValue(cmin);
    mt_v_bat_pack_cmax_cell->SetValue(cmax);
    if (stddev > mt_v_bat_pack_cstddev_max->AsFloat())
      mt_v_bat_pack_cstddev_max->SetValue(stddev);
    mt_v_bat_cell_capacity->SetElemValues(0, m_bms_readings_c, m_bms_capacitys);
    mt_v_bat_cell_cmin->SetElemValues(0, m_bms_readings_c, m_bms_cmins);
    mt_v_bat_cell_cmax->SetElemValues(0, m_bms_readings_c, m_bms_cmaxs);
    mt_v_bat_cell_cdevmax->SetElemValues(0, m_bms_readings_c, m_bms_cdevmaxs);
    StandardMetrics.ms_v_bat_soh->SetValue((min/360.0)*1.9230769);
    // complete:
    m_bms_has_capacitys = true;
    m_bms_bitset_c.clear();
    m_bms_bitset_c.resize(m_bms_readings_c);
    m_bms_bitset_cc = 0;
  }
  else {
    m_bms_bitset_c[index] = true;
  }
}

void OvmsVehicleSmartED::BmsRestartCellCapacitys() {
  m_bms_bitset_c.clear();
  m_bms_bitset_c.resize(m_bms_readings_c);
  m_bms_bitset_cc = 0;
}

void OvmsVehicleSmartED::BmsResetCellCapacitys() {
  if (m_bms_readings_c > 0) {
    m_bms_bitset_c.clear();
    m_bms_bitset_c.resize(m_bms_readings_c);
    m_bms_bitset_cc = 0;
    m_bms_has_capacitys = false;
    for (int k=0; k<m_bms_readings_c; k++) {
      m_bms_cmins[k] = 0;
      m_bms_cmaxs[k] = 0;
      m_bms_cdevmaxs[k] = 0;
    }
    mt_v_bat_cell_cmin->ClearValue();
    mt_v_bat_cell_cmax->ClearValue();
    mt_v_bat_cell_cdevmax->ClearValue();
    mt_v_bat_pack_cstddev_max->SetValue(mt_v_bat_pack_cstddev->AsFloat());
  }
}

void OvmsVehicleSmartED::BmsResetCellStats() {
  BmsResetCellVoltages();
  BmsResetCellTemperatures();
  BmsResetCellCapacitys();
}

void OvmsVehicleSmartED::BmsDiag(int verbosity, OvmsWriter* writer) {
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

  if (!m_bms_has_capacitys) {
    writer->puts("No BMS status data available");
    return;
  }
  
  writer->puts("-------------------------------------------");
  writer->puts("---- ED Battery Management Diagnostics ----");
  writer->puts("----         OVMS Version 1.1          ----");
  writer->puts("-------------------------------------------");
  
  writer->printf("Battery VIN: %s\n", (char*) mt_myBMS_BattVIN->AsString().c_str());
  writer->printf("Time [hh:mm]: %02d:%02d", mt_vehicle_time->AsInt(0, Hours), mt_vehicle_time->AsInt(0, Minutes)-(mt_vehicle_time->AsInt(0, Hours)*60));
  writer->printf(",   ODO : %s\n", (char*) StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 0).c_str());
  
  writer->puts("-------------------------------------------");
  
  writer->printf("Battery Status   : ");
  if (myBMS_SOH == 0xFF) {
    writer->puts("OK");
  } else if (myBMS_SOH == 0) {
    writer->puts("FAULT");
  } else {
    writer->printf("DEGRADED: ");
    for(uint8_t mask = 0x80; mask; mask >>= 1) {
      if(mask & myBMS_SOH) {
        writer->printf("1");
      } else {
        writer->printf("0");
      }
    }
    writer->puts("");
  }
  writer->puts("");
  writer->printf("Battery Production [Y/M/D]: %d/%d/%d\n", 2000 + myBMS_ProdYear, myBMS_ProdMonth, myBMS_ProdDay);  
  writer->printf("Battery-FAT date   [Y/M/D]: %d/%d/%d\n", 2000 + myBMS_Year, myBMS_Month, myBMS_Day);
  writer->printf("Rev.[Y/WK/PL] HW:%d/%d/%d, SW:%d/%d/%d\n",
    2000 + mt_myBMS_HWrev->GetElemValue(0), mt_myBMS_HWrev->GetElemValue(1), mt_myBMS_HWrev->GetElemValue(2),
    2000 + mt_myBMS_SWrev->GetElemValue(0), mt_myBMS_SWrev->GetElemValue(1), mt_myBMS_SWrev->GetElemValue(2));
  
  writer->puts("-------------------------------------------");
  
  writer->printf("SOC : %s, realSOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str(), (char*) mt_real_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("HV  : %3.1f V, %.2f A, %0.2f kW\n", StdMetrics.ms_v_bat_voltage->AsFloat(), StdMetrics.ms_v_bat_current->AsFloat(), StdMetrics.ms_v_bat_power->AsFloat());
  writer->printf("LV  : %2.1f V\n", StdMetrics.ms_v_bat_12v_voltage->AsFloat());
  
  writer->puts("-------------------------------------------");
  
  writer->printf("CV mean : %5.0f mV, dV = %.2f mV\n", mt_myBMS_ADCCvolts_mean->AsFloat(), mt_myBMS_ADCCvolts_max->AsFloat() - mt_myBMS_ADCCvolts_min->AsFloat());
  writer->printf("CV min  : %5.0f mV\n", mt_myBMS_ADCCvolts_min->AsFloat());
  writer->printf("CV max  : %5.0f mV\n", mt_myBMS_ADCCvolts_max->AsFloat());
  writer->printf("OCVtimer: %d s\n", mt_v_bat_OCVtimer->AsInt());
  
  writer->puts("-------------------------------------------");
  
  writer->printf("Last measurement      : %d day(s)\n", mt_v_bat_LastMeas_days->AsInt());
  writer->printf("Measurement estimation: %.3f\n", mt_v_bat_Cap_meas_quality->AsFloat());
  writer->printf("Actual estimation     : %.3f\n", mt_v_bat_Cap_combined_quality->AsFloat());
  writer->printf("CAP mean: %5.0f As/10, %2.1f Ah\n", mt_v_bat_Cap_As_avg->AsFloat(), mt_v_bat_Cap_As_avg->AsFloat()/360.0);
  writer->printf("CAP min : %5.0f As/10, %2.1f Ah\n", mt_v_bat_Cap_As_min->AsFloat(), mt_v_bat_Cap_As_min->AsFloat()/360.0);
  writer->printf("CAP max : %5.0f As/10, %2.1f Ah\n", mt_v_bat_Cap_As_max->AsFloat(), mt_v_bat_Cap_As_max->AsFloat()/360.0);
  
  writer->puts("-------------------------------------------");
  
  writer->printf("HV contactor ");
  if (HVcontactState == 0x02) {
    writer->printf("state ON");
    writer->printf(", low current: %d s\n", mt_v_bat_HV_lowcurrent->AsInt());
  } else if (HVcontactState == 0x00) {
    writer->printf("state OFF");
    writer->printf(", for: %d s\n", mt_v_bat_HVoff_time->AsInt());
  }
  writer->printf("Cycles left   : %d\n", mt_myBMS_HVcontactCyclesLeft->AsInt());
  writer->printf("of max. cycles: %d\n", mt_myBMS_HVcontactCyclesMax->AsInt());
  writer->printf("DC isolation  : %d kOhm, ", mt_myBMS_Isolation->AsInt());
  if (mt_myBMS_DCfault->AsInt() == 0) {
    writer->puts("OK");
  } else {
    writer->puts("DC FAULT");
  }
  
  writer->puts("-------------------------------------------");
  
  writer->puts("Temperatures Battery-Unit /degC: ");
  for (uint16_t n = 0; n < 9; n = n + 3) {
    writer->printf("module %d: ", (n / 3) + 1);
    for (uint16_t i = 0; i < 3; i++) {
      writer->printf("%.1f", (float) myBMS_Temps[n + i] / 64);
      if ( i < 2) {
        writer->printf(", ");
      } else {
        writer->puts("");
      }
    }
  }
  writer->printf("   mean : %.1f", (float) myBMS_Temps[11] / 64);
  writer->printf(", min : %.1f", (float) myBMS_Temps[10] / 64);
  writer->printf(", max : %.1f\n", (float) myBMS_Temps[9] / 64);
  writer->printf("coolant : %.1f\n", (float) myBMS_Temps[12] / 64);
  
  writer->puts("-------------------------------------------");
  
  writer->puts(" # ;mV   ;As/10");
  for(int16_t n = 0; n < CELLCOUNT; n++){
    writer->printf("%3d; %4.0f; %5.0f\n", n+1, m_bms_voltages[n]*1000 - mt_myBMS_ADCvoltsOffset->AsInt(), m_bms_capacitys[n]);
  }
  writer->puts("-------------------------------------------");
  writer->puts("Individual Cell Statistics:");
  writer->puts("-------------------------------------------");
  writer->printf("CV mean : %4.0f mV", StdMetrics.ms_v_bat_pack_vavg->AsFloat()*1000 - mt_myBMS_ADCvoltsOffset->AsInt());
  writer->printf(", dV= %.0f mV", StdMetrics.ms_v_bat_pack_vmax->AsFloat()*1000 - StdMetrics.ms_v_bat_pack_vmin->AsFloat()*1000);
  writer->printf(", s= %.2f mV\n", StdMetrics.ms_v_bat_pack_vstddev->AsFloat()*1000);
  writer->printf("CV min  : %4.0f mV, # %d\n", StdMetrics.ms_v_bat_pack_vmin->AsFloat()*1000 - mt_myBMS_ADCvoltsOffset->AsInt(), mt_v_bat_pack_cmin_cell_volt->AsInt());
  writer->printf("CV max  : %4.0f mV, # %d\n", StdMetrics.ms_v_bat_pack_vmax->AsFloat()*1000 - mt_myBMS_ADCvoltsOffset->AsInt(), mt_v_bat_pack_cmax_cell_volt->AsInt());
  writer->puts("-------------------------------------------");
  writer->printf("CAP mean: %5.0f As/10, %2.1f Ah\n", mt_v_bat_pack_cavg->AsFloat(), mt_v_bat_pack_cavg->AsFloat() / 360.0);
  writer->printf("CAP min : %5.0f As/10, %2.1f Ah, # %d \n", mt_v_bat_pack_cmin->AsFloat(), mt_v_bat_pack_cmin->AsFloat() / 360.0, mt_v_bat_pack_cmin_cell->AsInt());
  writer->printf("CAP max : %5.0f As/10, %2.1f Ah, # %d \n", mt_v_bat_pack_cmax->AsFloat(), mt_v_bat_pack_cmax->AsFloat() / 360.0, mt_v_bat_pack_cmax_cell->AsInt());
  writer->puts("-------------------------------------------");
}

//--------------------------------------------------------------------------------
//! \brief   Output BMS dataset
//--------------------------------------------------------------------------------
void OvmsVehicleSmartED::printRPTdata(int verbosity, OvmsWriter* writer) {
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
  
  if (!m_bms_has_capacitys) {
    writer->puts("No BMS status data available");
    return;
  }
  
  writer->puts("-----------------------------------------");
  writer->puts("---       Battery Status Report       ---");
  writer->puts("---         OVMS Version 1.0          ---");
  writer->puts("-----------------------------------------");
  
  writer->printf("Battery VIN: %s\n", (char*) mt_myBMS_BattVIN->AsString().c_str());
  writer->printf("Time [hh:mm]: %02d:%02d", mt_vehicle_time->AsInt(0, Hours), mt_vehicle_time->AsInt(0, Minutes)-(mt_vehicle_time->AsInt(0, Hours)*60));
  writer->printf(",   ODO : %s\n", (char*) StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 0).c_str());
  
  writer->printf("Battery Status   : ");
  if (myBMS_SOH == 0xFF) {
    writer->puts("OK");
  } else if (myBMS_SOH == 0) {
    writer->puts("FAULT");
  } else {
    writer->printf("DEGRADED: ");
    for(uint8_t mask = 0x80; mask; mask >>= 1) {
      if(mask & myBMS_SOH) {
        writer->printf("1");
      } else {
        writer->printf("0");
      }
    }
    writer->puts("");
  }
  writer->puts("");
  writer->printf("Battery Production [Y/M/D]: %d/%d/%d\n", 2000 + myBMS_ProdYear, myBMS_ProdMonth, myBMS_ProdDay);
  writer->printf("Battery verified   [Y/M/D]: %d/%d/%d\n", 2000 + myBMS_Year, myBMS_Month, myBMS_Day);  
  writer->printf("Rev.[Y/WK/PL] HW:%d/%d/%d, SW:%d/%d/%d\n",
    2000 + mt_myBMS_HWrev->GetElemValue(0), mt_myBMS_HWrev->GetElemValue(1), mt_myBMS_HWrev->GetElemValue(2),
    2000 + mt_myBMS_SWrev->GetElemValue(0), mt_myBMS_SWrev->GetElemValue(1), mt_myBMS_SWrev->GetElemValue(2));
  
  writer->puts("");
  writer->printf("realSOC          : %s\n", (char*) mt_real_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("SOC              : %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("Charged capacity : %2.1f Ah, min. Cell# %d\n", mt_v_bat_pack_cmin->AsFloat()/360.0, mt_v_bat_pack_cmin_cell->AsInt());
  writer->printf("BMS estimate     : %2.1f Ah, value @25degC\n", mt_v_bat_Cap_As_min->AsFloat()/360.0);
  writer->printf("Measurement done : %d day(s) ago\n", mt_v_bat_LastMeas_days->AsInt());
  if (mt_v_bat_LastMeas_days->AsInt() > 21) writer->puts("* Watch timer for OCV state, redo test! *");
  writer->printf("Estimation factor: %.3f", mt_v_bat_Cap_meas_quality->AsFloat());
  if (mt_v_bat_Cap_meas_quality->AsFloat() >= 0.85) {
    writer->puts(", excellent");
  } else if (mt_v_bat_Cap_meas_quality->AsFloat() >= 0.82) {
    writer->puts(", very good");
  } else if (mt_v_bat_Cap_meas_quality->AsFloat() >= 0.80) {
    writer->puts(", good");
  } else if (mt_v_bat_Cap_meas_quality->AsFloat() >= 0.78) {
    writer->puts(", fair");
    writer->puts("* need >>dSOC, start test below 20% SOC *");
  } else {
    writer->puts(", poor!");
    writer->puts("*** Don't trust and redo measurement! ***");
  }
  writer->puts(" ");
  writer->printf("OCV timer        : %d s, ", mt_v_bat_OCVtimer->AsInt());
  writer->printf("@%.1f degC\n", (float) myBMS_Temps[11] / 64);
  writer->printf("OCV state        : ");
  
  if (HVcontactState == 0x02) {
    writer->puts("HV ON, can't meas. OCV");
  } else if (HVcontactState == 0x00) {  
    if (mt_v_bat_HVoff_time->AsInt() < mt_v_bat_OCVtimer->AsInt()) {
      writer->printf("wait for ");
      uint16_t dTime = mt_v_bat_OCVtimer->AsInt() - mt_v_bat_HVoff_time->AsInt();
      if (dTime > 3600) {
        if ((dTime / 3600) < 10) writer->printf("0");
        writer->printf("%d", dTime / 3600); writer->printf(":"); 
        if (((dTime % 3600) / 60) < 10) writer->printf("0");
        writer->printf("%d", (dTime % 3600) / 60); writer->puts(" [hh:mm]");
      } else if (dTime > 60) {
        if ((dTime / 60) < 10) writer->printf("0");
        writer->printf("%d", dTime / 60); writer->printf(":"); 
        if ((dTime % 60) < 10) writer->printf("0");
        writer->printf("%d", dTime % 60); writer->puts(" [mm:ss]");
      } else {
        writer->printf("%d", dTime); writer->puts(" s");
      }
    } else {
      if (StdMetrics.ms_v_bat_soc->AsFloat() <= 20) {
        writer->puts("Ready for cap. meas. !");
      } else {
        writer->puts("OK");
      }
    }
  }
  writer->puts(" ");
  writer->printf("DC isolation     : %d kOhm ", mt_myBMS_Isolation->AsInt());
  if (mt_myBMS_DCfault->AsInt() == 0) {
    writer->puts("OK");
  } else {
    writer->puts("DC FAULT");
  }
  writer->puts(" ");
  //DiagCAN.getBatteryVoltageDist(&BMS);  //Sort cell voltages rising up and calc. quartiles
                                        //!!! after sorting track of individual cells is lost -> redo ".getBatteryVoltages" !!!
  //printVoltageDistribution();           //Print statistic data as boxplot
  //PrintSPACER();
  
}
