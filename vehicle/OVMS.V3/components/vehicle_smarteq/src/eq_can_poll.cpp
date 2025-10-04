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
 
const PROGMEM uint32_t rqID_BMS                   = 0x79B;
const PROGMEM uint32_t respID_BMS                 = 0x7BB; 
const PROGMEM byte rqPartNo[2]                    = {0x21, 0xEF};
const PROGMEM byte rqIDpart[2]                    = {0x21, 0x80};
const PROGMEM byte rqBattHealth[2]                = {0x21, 0x61}; 
const PROGMEM byte rqBattLimits[2]                = {0x21, 0x01}; 
const PROGMEM byte rqBattHVContactorCycles[2]     = {0x21, 0x02}; 
const PROGMEM byte rqBattState[2]                 = {0x21, 0x07}; 
const PROGMEM byte rqBattSOC[2]                   = {0x21, 0x08}; 
const PROGMEM byte rqBattSOCrecal[2]              = {0x21, 0x25}; 
const PROGMEM byte rqBattTemperatures[2]          = {0x21, 0x04}; 
const PROGMEM byte rqBattVoltages_P1[2]           = {0x21, 0x41}; 
const PROGMEM byte rqBattVoltages_P2[2]           = {0x21, 0x42}; 
const PROGMEM byte rqBattOCV_Cal[2]               = {0x21, 0x05};
const PROGMEM byte rqBattCellResistance_P1[2]     = {0x21, 0x10};
const PROGMEM byte rqBattCellResistance_P2[2]     = {0x21, 0x11};
const PROGMEM byte rqBattIsolation[2]             = {0x21, 0x29}; 
const PROGMEM byte rqBattMeas_Capacity[2]         = {0x21, 0x0B}; 
const PROGMEM byte rqBattSN[2]                    = {0x21, 0xA0};
const PROGMEM byte rqBattCapacity_dSOC_P1[2]      = {0x21, 0x12}; 
const PROGMEM byte rqBattCapacity_dSOC_P2[2]      = {0x21, 0x13}; 
const PROGMEM byte rqBattCapacity_P1[2]           = {0x21, 0x14};
const PROGMEM byte rqBattCapacity_P2[2]           = {0x21, 0x15};
const PROGMEM byte rqBattBalancing[2]             = {0x21, 0x16}; 
const PROGMEM byte rqBattLogData_P1[2]            = {0x21, 0x30};
const PROGMEM byte rqBattProdDate[3]              = {0x22, 0x03, 0x04};
 */

#include "ovms_log.h"
static const char *TAG = "v-smarteq";

#include <stdio.h>
#include <string>
#include <iomanip>
#include <cmath>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"

#include "vehicle_smarteq.h"
#include <iostream>
#include <iomanip>
#include <sstream>

std::string SecondsToHHmm(int totalSeconds) {
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;

    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << hours << ":"
        << std::setw(2) << std::setfill('0') << minutes;

    return oss.str();
}

// Central reply length guard for poll parsers (enhanced):
// Logs only when reply_len changes to avoid log spam on repeated truncated frames.
#define REQUIRE_LEN(minlen)                                                    \
  do {                                                                         \
    if (reply_len < (minlen)) {                                                \
      static uint16_t __last_short_len = 0xFFFF;                               \
      if (__last_short_len != reply_len) {                                     \
        ESP_LOGV(TAG, "%s: short reply (%u < %u)",                             \
          __FUNCTION__, (unsigned)reply_len, (unsigned)(minlen));              \
        __last_short_len = reply_len;                                          \
      }                                                                        \
      return;                                                                  \
    }                                                                          \
  } while(0)

/**
 * Incoming poll reply messages
 */
void OvmsVehicleSmartEQ::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) {
  
  // init / fill rx buffer:
  if (job.mlframe == 0) {
    m_rxbuf.clear();
    m_rxbuf.reserve(length + job.mlremain);
  }
  m_rxbuf.append((const char*)data, length);
  if (job.mlremain)
    return;

  // response complete:
  ESP_LOGV(TAG, "IncomingPollReply: PID %04X: len=%u %s", (unsigned)job.pid, (unsigned)m_rxbuf.size(), hexencode(m_rxbuf).c_str());
  
  switch (job.moduleid_rec) {
    case 0x7EC:
      switch (job.pid) {        
        case 0x320C: // rqHV_Energy
          PollReply_EVC_HV_Energy(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x302A: // rqDCDC_State
          PollReply_EVC_DCDC_State(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x3495: // rqDCDC_Load
          PollReply_EVC_DCDC_Load(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x3023: // 14V DCDC voltage request
          PollReply_EVC_DCDC_VoltReq(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x3024: // 14V DCDC voltage measure
          PollReply_EVC_DCDC_Volt(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x3025: // 14V DCDC current measure
          PollReply_EVC_DCDC_Amps(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x3494: // rqDCDC_Power
          PollReply_EVC_DCDC_Power(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x33BA: // indicates ext power supply
          PollReply_EVC_ext_power(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x339D: // charging plug present
          PollReply_EVC_plug_present(m_rxbuf.data(), m_rxbuf.size());
          break;
      }
      break;  // FIX: prevent fallthrough into 0x7BB
    case 0x7BB:
      switch (job.pid) {
        case 0x07: // rqBattState
          PollReply_BMS_BattState(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x41: // rqBattVoltages_P1
          PollReply_BMS_BattVolts(m_rxbuf.data(), m_rxbuf.size(), 0);
          break;
        case 0x42: // rqBattVoltages_P2
          PollReply_BMS_BattVolts(m_rxbuf.data(), m_rxbuf.size(), 48);
          break;
        case 0x04: // rqBattTemperatures
          PollReply_BMS_BattTemps(m_rxbuf.data(), m_rxbuf.size());
          break;
      }
      break;
    case 0x793:
      switch (job.pid) {
        case 0x7303: // rqChargerAC
          PollReply_OBL_ChargerAC(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x503F: // rqJB2AC_Ph12_RMS_V
          PollReply_OBL_JB2AC_Ph12_RMS_V(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5041: // rqJB2AC_Ph23_RMS_V
          PollReply_OBL_JB2AC_Ph23_RMS_V(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5042: // rqJB2AC_Ph31_RMS_V
          PollReply_OBL_JB2AC_Ph31_RMS_V(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x2001: // rqJB2AC_Ph1_RMS_A
          PollReply_OBL_JB2AC_Ph1_RMS_A(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x503A: // rqJB2AC_Ph2_RMS_A
          PollReply_OBL_JB2AC_Ph2_RMS_A(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x503B: // rqJB2AC_Ph3_RMS_A
          PollReply_OBL_JB2AC_Ph3_RMS_A(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x504A: // Mains active power consumed (W)
          PollReply_OBL_JB2AC_Power(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5062: // Mains ground resistance (Ohm)
          PollReply_OBL_JB2AC_GroundResistance(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5064: // leakage current diagnostics
          PollReply_OBL_JB2AC_LeakageDiag(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5065: // Leakage DC current saved indicator after failure (mA)
          PollReply_OBL_JB2AC_DCCurrent(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5066: // Leakage HF 10kHz current saved indicator after failure (mA)
          PollReply_OBL_JB2AC_HF10kHz(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5067: // Leakage HF current saved indicator after failure (mA) 
          PollReply_OBL_JB2AC_HFCurrent(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5068: // Leakage LF current saved indicator after failure (mA)
          PollReply_OBL_JB2AC_LFCurrent(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x5070: // max AC current limitation configuration (A)
          PollReply_OBL_JB2AC_MaxCurrent(m_rxbuf.data(), m_rxbuf.size());
          break;
      }
      break;
    case 0x764:
      switch (job.pid) {
        case 0x52: // temperature sensor values
          PollReply_HVAC(m_rxbuf.data(), m_rxbuf.size());
          break;
      }
      break;
    case 0x763:
      switch (job.pid) {
        case 0x200c: // temperature sensor values
          PollReply_TDB(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x2101: // obd Trip Distance km
          PollReply_obd_trip(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x2104: // obd Trip time s
          PollReply_obd_time(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x01A0: // obd Start Trip Distance km
          PollReply_obd_start_trip(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x01A2: // obd Start Trip time s
          PollReply_obd_start_time(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x0188: // maintenance level
          PollReply_obd_mt_level(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x0204: // maintenance data duration days
          PollReply_obd_mt_day(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x0203: // maintenance data usual km
          PollReply_obd_mt_km(m_rxbuf.data(), m_rxbuf.size());
          break;
      }
      break;
    case 0x765:
      switch (job.pid) {
        case 0x81: // req.VIN
          PollReply_VIN(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x8003: // rq_VehicleState
          PollReply_VehicleState(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x605e: // rq_UNDERHOOD_OPENED
          PollReply_DoorUnderhoodOpened(m_rxbuf.data(), m_rxbuf.size());
          break;
      }
      break;
    default:
      ESP_LOGW(TAG, "IncomingPollReply: unhandled PID %02X: len=%d %s", job.pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());
      break;
  }
}

void OvmsVehicleSmartEQ::IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code) {
  switch (job.moduleid_rec) {
    case 0x793:
      switch (job.pid) {
        case 0x7303: // rqChargerAC
          if (code == 0x12) {
            mt_obl_fastchg->SetValue(true);
            ObdModifyPoll();
          }
          break;
      }
      break;
    default:
      ESP_LOGE(TAG, "IncomingPollError: PID %04X: err=%02X", (unsigned)job.pid, code);
      break;
  }
}

void OvmsVehicleSmartEQ::PollReply_BMS_BattVolts(const char* data, uint16_t reply_len, uint16_t start)
{
  REQUIRE_LEN(2);
  uint16_t max_bytes = reply_len & 0xFFFE;
  for (uint16_t off = 0; off < max_bytes; off += 2)
    {
      if (off+1 >= reply_len)
        break;
      uint16_t cell_index = (off / 2) + start;
      if (cell_index >= CELLCOUNT)   // safety bound
        break;
      float CV = CAN_UINT(off) / 1024.0f;
      if (CV >= 0.5f && CV <= 5.5f)  // simple sanity window
        BmsSetCellVoltage(cell_index, CV);
    }
}

void OvmsVehicleSmartEQ::PollReply_BMS_BattTemps(const char* data, uint16_t reply_len)
{
  // Each temperature = 2 bytes. reply_len is number of bytes.
  REQUIRE_LEN(6);

  // We will not read beyond reply_len:
  uint16_t max_bytes = reply_len & 0xFFFE; // even boundary
  if (max_bytes > 62) max_bytes = 62;      // max 31 pairs
  // Each pair = 2 bytes:
  uint16_t max_pairs = max_bytes / 2;
  if (max_pairs > 31) max_pairs = 31;

  int16_t Temps[31];
  float   BMStemps[31];

  for (uint16_t idx = 0; idx < max_pairs; idx++)
    {
    uint16_t byte_off = idx * 2;
    // Bounds safe due to max_bytes calculation
    int16_t value = CAN_UINT(byte_off);

    // For indices beyond first two (= after original header area) adjust negative base using second value:
    if (idx > 2 && (Temps[1] & 0x8000))
      value -= 0x0A00;

    Temps[idx] = value;
    BMStemps[idx] = value / 64.0f;
    }

  // Fill remaining (if any) with NAN to avoid stale data:
  for (uint16_t i = max_pairs; i < 31; i++)
    {
    Temps[i] = 0;
    BMStemps[i] = NAN;
    }

  mt_bms_temps->SetElemValues(0, 31, BMStemps);

  // Cell temperatures start at index 3 (as in original), ensure bounds:
  uint16_t cellcount = (max_pairs > 3) ? (max_pairs - 3) : 0;
  if (cellcount > 27) cellcount = 27; // 31 total - 3 offset = 28 max, but original loop used <30 -> 27 cells
  for (uint16_t i = 0; i < cellcount; i++)
    {
      if (!std::isnan(BMStemps[i+3]) && i < 27) // ensure bound 0..26
        BmsSetCellTemperature(i, BMStemps[i+3]);
    }
}

void OvmsVehicleSmartEQ::PollReply_BMS_BattState(const char* data, uint16_t reply_len)
{
  // New layout per “ReadData.Actual Data Read” spec:
  // Byte 0 : 0x61 (positive response to 0x21)
  // Byte 1 : 0x07 (identifier)
  // Byte 2 : status / unused (observed 0x00)
  // Byte 3-4  Minimum Cell Voltage        /1024 (V)
  // Byte 5-6  Maximum Cell Voltage        /1024 (V)
  // Byte 7-8  Traction (Link) Voltage     /64   (V)
  // Byte 9-10 Sum of Cell Voltages        (raw total HV = BattCV_Sum/64)
  // Byte 11-12 Battery Voltage            /64   (V)
  // Byte 13-14 Battery Current            /32 (A)
  // Byte 15   State Contactor             enum (0=open,1=precharge,2=closed,3=SNA)
  // Byte 16   Interlock HV Plug           0=disable,1=enable
  // Byte 17   Interlock Service Disc      0=disable,1=enable
  // Byte 18-19 LBC FUSI Mode              enum (0=No Error,4=HVBatLevel1 Failure,6=HVBatLevel2 Failure)
  // Byte 20-21 MG Output Revolution       /2 (rpm)
  // Byte 22   SafetyMode1Flag             enum (0 Not Used,1 No Safety Mode,2 Safety Mode 1 Requested,3 Unavailable)
  // Byte 23   Bitfield:
  //            bits4-5 High voltage relay status flag (0 not active,1 active,2 undefined)
  //            bits6-7 Relay on Permit Flag          (0 not active,1 active,2 undefined)
  // Byte 24   Vehicle Mode Request        enum (0..7)
  // Byte 25   Clamp 30 Voltage            /8 (V) (+ optional offset)
  //
  // Need at least up to byte 22 => length >= 23
  REQUIRE_LEN(23);

  float v_cell_min = (float)CAN_UINT(0) / 1024.0f;
  float v_cell_max = (float)CAN_UINT(2) / 1024.0f;
  float v_link     = (float)CAN_UINT(4) / 64.0f;
  float cv_sum_raw = (float)CAN_UINT(6) / 64.0f;
  float v_power_raw = (float)CAN_UINT(8);  // raw scaled*64
  float v_pack_term = (float)CAN_UINT(9) / 64.0f; // battery terminal voltage (may equal link voltage depending on contactor)
  uint16_t cur_u = CAN_UINT(10);
  int16_t  cur_s = (int16_t)cur_u;
  float    cur_raw = (float)cur_s;      // raw signed counts
  float i_pack = cur_raw / 32.0f;

  mt_bms_CV_Range_min->SetValue(v_cell_min);
  mt_bms_CV_Range_max->SetValue(v_cell_max);
  mt_bms_CV_Range_mean->SetValue((v_cell_min + v_cell_max) / 2.0f);

  mt_bms_BattLinkVoltage->SetValue(v_link);
  mt_bms_BattContactorVoltage->SetValue(v_pack_term);
  mt_bms_BattCV_Sum->SetValue(cv_sum_raw);
  mt_bms_BattPower_voltage->SetValue(v_power_raw); // store raw scaled*64 so existing power calc logic can adapt if needed
  mt_bms_BattPower_current->SetValue(cur_raw);        // raw current counts

  // Compute power (re-using prior scaling: voltage_raw/64 * current_raw/32)
  float pack_power_kw = ((v_power_raw  / 64.0f) * ( (float)cur_raw / 32.0f )) / 1000.0f;
  mt_bms_BattPower_power->SetValue(pack_power_kw);

  int contactor_code = CAN_BYTE(12);
  const char* contactor_txt;
  switch (contactor_code)
  {
    case 0: contactor_txt="open"; break;
    case 1: contactor_txt="precharge"; break;
    case 2: contactor_txt="closed"; break;
    case 3: contactor_txt="SNA"; break;
    default: contactor_txt="Unknown"; break;
  }
  mt_bms_HVcontactStateCode->SetValue(contactor_code);
  mt_bms_HVcontactStateTXT->SetValue(contactor_txt);

  // HV (sum of cell voltages) – keep legacy assignment:
  mt_bms_HV->SetValue(mt_bms_BattCV_Sum->AsFloat());

  int veh_mode = CAN_BYTE(21);
  const char* veh_mode_txt;
  switch (veh_mode)
  {
    case 0: veh_mode_txt="No Request"; break;
    case 1: veh_mode_txt="Slow Charging(Isolated Charging)"; break;
    case 2: veh_mode_txt="Fast Charging"; break;
    case 3: veh_mode_txt="Normal"; break;
    case 4: veh_mode_txt="Quick Drop"; break;
    case 5: veh_mode_txt="Cameleon (Non-Isolated Charging)"; break;
    case 6: veh_mode_txt="Not Used"; break;
    case 7: veh_mode_txt="Unavailable Value"; break;
    default: veh_mode_txt="Unknown"; break;
  }
  mt_bms_EVmode->SetValue(veh_mode);
  mt_bms_EVmode_txt->SetValue(veh_mode_txt);

  // Clamp 30 Voltage:
  float clamp30 = ((float)(uint8_t)CAN_BYTE(22)) / 8.0f + m_12v_measured_BMS_offset;
  if (clamp30 > 5 && clamp30 < 20)  // plausible 12V system window
    mt_bms_12v->SetValue(clamp30);

  // Publish derived standard metrics (sign convention: discharge negative):
  StdMetrics.ms_v_bat_current->SetValue(i_pack * -1.0f);
  StdMetrics.ms_v_bat_power->SetValue(pack_power_kw);

  mt_bms_interlock_hvplug->SetValue(((int)CAN_BYTE(13)) == 1);
  mt_bms_interlock_service->SetValue(((int)CAN_BYTE(14)) == 1);

  int16_t flags23 = (int16_t)CAN_UINT(20);
  uint8_t hv_relay_flag = (flags23 >> 4) & 0x03;
  uint8_t relay_permit_flag = (flags23 >> 6) & 0x03;

  mt_bms_relay_hv_status->SetValue(hv_relay_flag);
  const char* hv_relay_txt;
  switch (hv_relay_flag)
  {
    case 0: hv_relay_txt="not active"; break;
    case 1: hv_relay_txt="active"; break;
    default: hv_relay_txt="undefined"; break;
  }
  mt_bms_relay_hv_status_txt->SetValue(hv_relay_txt);

  mt_bms_relay_permit_flag->SetValue(relay_permit_flag);
  const char* relay_permit_txt;
  switch (relay_permit_flag)
  {
    case 0: relay_permit_txt="not active"; break;
    case 1: relay_permit_txt="active"; break;
    default: relay_permit_txt="undefined"; break;
  }
  mt_bms_relay_permit_flag_txt->SetValue(relay_permit_txt);

  int fusi = CAN_BYTE(15);
  const char* fusi_txt;
  if (fusi == 0) fusi_txt = "No Error";
  switch (fusi)
  {
    case 0: fusi_txt="No Error"; break;
    case 4: fusi_txt="HVBatLevel1 Failure"; break;
    case 6: fusi_txt="HVBatLevel2 Failure"; break;
    default: fusi_txt="Unavailable Value"; break;
  }
  mt_bms_fusi_mode->SetValue((int)fusi);
  mt_bms_fusi_mode_txt->SetValue(fusi_txt);

  float mg_rpm_raw = CAN_UINT(17);
  float mg_rpm = mg_rpm_raw / 2.0f;
  mt_bms_mg_rpm->SetValue(mg_rpm);

  int safety = CAN_BYTE(19);
  const char* safety_txt;
  switch (safety)
  {
    case 0: safety_txt="Not Used"; break;
    case 1: safety_txt="No Safety Mode"; break;
    case 2: safety_txt="Safety Mode 1 Requested"; break;
    default: safety_txt="Unavailable Value"; break;
  }
  mt_bms_safety_mode->SetValue(safety);
  mt_bms_safety_mode_txt->SetValue(safety_txt);
}

void OvmsVehicleSmartEQ::PollReply_HVAC(const char* data, uint16_t reply_len) {
  //StdMetrics.ms_v_env_cabintemp->SetValue( (((reply_data[3] << 8) | reply_data[4]) - 400) * 0.1);
}

void OvmsVehicleSmartEQ::PollReply_TDB(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(5);             // minimal sanity
  float temp = (float)(CAN_UINT(2)) > 400.0f ? 
    (float)(CAN_UINT(2) - 400.0f) * 0.1f : 
    (float)(400.0f - CAN_UINT(2)) * -0.1f;

  static const float MAX_VALID_TEMP = 100.0f;
  static const float MIN_VALID_TEMP = -50.0f;
  if (temp > MIN_VALID_TEMP && temp < MAX_VALID_TEMP) {
      StdMetrics.ms_v_env_temp->SetValue(temp);
      ESP_LOGV(TAG, "Ambient temperature updated: %.1f°C", temp);
  } else {
      ESP_LOGW(TAG, "Invalid temperature reading ignored: %.1f°C", temp);
  }
  float temptpms = temp > 1.0f && temp < MAX_VALID_TEMP ? temp : 1.1f;
  if (m_ios_tpms_fix) {
    StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, temptpms);
    StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, temptpms);
    StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, temptpms);
    StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, temptpms);
  }
}

void OvmsVehicleSmartEQ::PollReply_VIN(const char* data, uint16_t reply_len) {
  // Expect at least 17 VIN chars; responses often carry +2 bytes (CRC / filler)
  if (reply_len < 17) {
    ESP_LOGW(TAG,"VIN reply too short (%u)", reply_len);
    return;
  }
  size_t vinlen = (reply_len >= 19) ? 17 : reply_len; // if only 17, take all
  std::string vin(data, vinlen);
  // Basic sanity: only allow [A-Z0-9]
  for (char &c: vin) {
    if (!( (c>='A'&&c<='Z') || (c>='0'&&c<='9') ))
    { ESP_LOGW(TAG,"Discarding VIN (invalid char)"); return; }
  }
  StdMetrics.ms_v_vin->SetValue(vin);
}

void OvmsVehicleSmartEQ::PollReply_VehicleState(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  int code = CAN_BYTE(0);
  std::string msgtxt = "";
  switch(code) {
    case 0: msgtxt = "SLEEPING"; break; 
    case 1: msgtxt = "TECHNICAL WAKE UP"; break;
    case 2: msgtxt = "CUT OFF PENDING"; break;
    case 3: msgtxt = "BAT TEMPO LEVEL"; break;
    case 4: msgtxt = "BAT TEMPO LEVEL"; break;
    case 5: msgtxt = "ACCESSORY LEVEL"; break;
    case 6: msgtxt = "IGNITION LEVEL"; break;
    case 7: msgtxt = "STARTING IN PROGRESS"; break;
    case 8: msgtxt = "ENGINE RUNNING"; break;
    case 9: msgtxt = "AUTOSTART"; break;
    case 10: msgtxt = "ENGINE SYSTEM STOP"; break;
    default: msgtxt = "Unknown code"; break;
  }
  mt_vehicle_state->SetValue(msgtxt);
  mt_vehicle_state_code->SetValue(code);
  //mt_bus_awake->SetValue(code > 0);
}

void OvmsVehicleSmartEQ::PollReply_DoorUnderhoodOpened(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  int code = CAN_BYTE(0);
  StdMetrics.ms_v_door_hood->SetValue(code==1);
}

void OvmsVehicleSmartEQ::PollReply_EVC_HV_Energy(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_evc_hv_energy->SetValue( CAN_UINT(0) / 200.0f );
  StdMetrics.ms_v_bat_capacity->SetValue(mt_evc_hv_energy->AsFloat());
  StdMetrics.ms_v_bat_cac->SetValue(mt_evc_hv_energy->AsFloat() * 1000.0f / mt_bms_HV->AsFloat());
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_State(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  mt_evc_LV_DCDC_amps->SetValue( CAN_BYTE(0) );
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Load(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  mt_evc_LV_DCDC_load->SetValue( CAN_BYTE(0) == 0xFE ? 0 : CAN_BYTE(0) );
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_VoltReq(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  float value = CAN_BYTE(0);
  mt_evc_LV_DCDC_volt_req->SetValue( value - 12.0f );
}
void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Volt(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);  
  float value = CAN_BYTE(0);
  mt_evc_LV_DCDC_volt->SetValue( value - 4.0f);
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Amps(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);    
  float value = CAN_BYTE(0);
  mt_evc_LV_DCDC_power->SetValue( value );
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Power(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_evc_LV_DCDC_state->SetValue( CAN_UINT(0) );
}

void OvmsVehicleSmartEQ::PollReply_EVC_ext_power(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_evc_ext_power->SetValue(CAN_UINT(0)>0);
}

void OvmsVehicleSmartEQ::PollReply_EVC_plug_present(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_evc_plug_present->SetValue(CAN_UINT(0)>0);
}

void OvmsVehicleSmartEQ::PollReply_OBL_ChargerAC(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(16);
  int32_t value;
  //Get AC Currents (two rails from >= 20A, sum up for total current)
  value = CAN_UINT(6);
  if (value < 0xEA00) {  //OBL showing only valid data while charging
    mt_obl_main_amps->SetElemValue(0, value / 10.0);
  } else {
    mt_obl_main_amps->SetElemValue(0, 0);
  }
  value = CAN_UINT(8);
  if (value < 0xEA00) {  //OBL showing only valid data while charging
    mt_obl_main_amps->SetElemValue(1, value / 10.0);
  } else {
    mt_obl_main_amps->SetElemValue(1, 0);
  }
  mt_obl_main_amps->SetElemValue(2, 0);

  //Get AC Voltages
  value = CAN_UINT(0);
  if (value < 0xEA00) {  //OBL showing only valid data while charging
    mt_obl_main_volts->SetElemValue(0, value / 10.0);
  } else {
    mt_obl_main_volts->SetElemValue(0, 0);
  }
  mt_obl_main_volts->SetElemValue(1, 0); mt_obl_main_volts->SetElemValue(2, 0);
  //Get AC Frequency
  if (mt_obl_main_amps->GetElemValue(0) > 0 || mt_obl_main_amps->GetElemValue(1) > 0) {
    float value = CAN_BYTE(11);
    if (value < 0xF0)  //OBL showing only valid data while charging
      mt_obl_main_freq->SetValue( CAN_BYTE(11) + 10.0f);
  } else {
    mt_obl_main_freq->SetValue(0);
  }
  //Get AC Power
  value = CAN_UINT(12);
  if (value < 0xEA00) {  //OBL showing only valid data while charging
    mt_obl_main_CHGpower->SetElemValue(0, value / 2000.0f);
  } else {
    mt_obl_main_CHGpower->SetElemValue(0, 0);
  }
  value = CAN_UINT(14);
  if (value < 0xEA00) {  //OBL showing only valid data while charging
    mt_obl_main_CHGpower->SetElemValue(1, value / 2000.0f);
  } else {
    mt_obl_main_CHGpower->SetElemValue(1, 0);
  }
  UpdateChargeMetrics();
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph1_RMS_A(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  float value = ((CAN_UINT(0) * 0.625) - 2000) / 10.0;
  mt_obl_main_amps->SetElemValue(0, value);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph2_RMS_A(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  float value = ((CAN_UINT(0) * 0.625) - 2000) / 10.0;
  mt_obl_main_amps->SetElemValue(1, value);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph3_RMS_A(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  float value = ((CAN_UINT(0) * 0.625) - 2000) / 10.0;
  mt_obl_main_amps->SetElemValue(2, value);
  UpdateChargeMetrics();
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph12_RMS_V(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_obl_main_volts->SetElemValue(0, CAN_UINT(0) / 2.0);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph23_RMS_V(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_obl_main_volts->SetElemValue(1, CAN_UINT(0) / 2.0);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph31_RMS_V(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_obl_main_volts->SetElemValue(2, CAN_UINT(0) / 2.0);
  UpdateChargeMetrics();
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Power(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  if(CAN_UINT(0) > 20000) {
    mt_obl_main_CHGpower->SetElemValue(0, (CAN_UINT(0) - 20000.0f) / 1000.0f);
  }else{
    mt_obl_main_CHGpower->SetElemValue(0, ((20000.0f - CAN_UINT(0)) / 1000.0f) * -1.0f);
  }
  // mt_obl_main_CHGpower->SetElemValue(0, (CAN_UINT(0) - 20000) / 1000.0f);
  UpdateChargeMetrics();
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_GroundResistance(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_obl_main_ground_resistance->SetValue((float) CAN_UINT(0));
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_LeakageDiag(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  int code = CAN_BYTE(0);
  std::string msgtxt = "";
  switch(code) {
    case 0: msgtxt = "init"; break;
    case 1: msgtxt = "HF10"; break;
    case 3: msgtxt = "Mains Ground Default"; break;
    case 97: msgtxt = "Means Leakage LF+HF"; break;
    case 5: msgtxt = "Earth Current default"; break;
    case 81: msgtxt = "Means Leakage DC+HF"; break;
    case 65: msgtxt = "Means Leakage HF"; break;
    case 9: msgtxt = "Ground Default"; break;
    case 49: msgtxt = "Means Leakage DC+LF"; break;
    case 17: msgtxt = "Means Leakage DC"; break;
    case 113: msgtxt = "Means Leakage DC+LF+HF"; break;
    case 33: msgtxt = "Means Leakage LF"; break;
    default: msgtxt = "Unknown code"; break;
  }
  mt_obl_main_leakage_diag->SetValue(msgtxt);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_DCCurrent(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  float value1 = (float) CAN_UINT(0);
  float value2 = value1 -32768.0f >= 0.0f ? value1 -32768.0f : (-32768.0f - value1) * -1.0f;
  mt_obl_main_current_leakage_dc->SetValue(value2 / 1000.0f);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_HF10kHz(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  float value1 = (float) CAN_UINT(0);
  float value2 = value1 -32768.0f >= 0.0f ? value1 -32768.0f : (-32768.0f - value1) * -1.0f;
  mt_obl_main_current_leakage_hf_10khz->SetValue(value2 / 1000.0f);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_HFCurrent(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  float value1 = (float) CAN_UINT(0);
  float value2 = value1 -32768.0f >= 0.0f ? value1 -32768.0f : (-32768.0f - value1) * -1.0f;
  mt_obl_main_current_leakage_hf->SetValue(value2 / 1000.0f);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_LFCurrent(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  float value1 = (float) CAN_UINT(0);
  float value2 = value1 -32768.0f >= 0.0f ? value1 -32768.0f : (-32768.0f - value1) * -1.0f;
  mt_obl_main_current_leakage_lf->SetValue(value2 / 1000.0f);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_MaxCurrent(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  mt_obl_main_max_current->SetValue((float) CAN_BYTE(0));
}


void OvmsVehicleSmartEQ::PollReply_obd_trip(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_obd_trip_km->SetValue((float) CAN_UINT(0));
}

void OvmsVehicleSmartEQ::PollReply_obd_time(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(3);
  int value_1 = CAN_UINT24(0);
  std::string timeStr = SecondsToHHmm(value_1);
  mt_obd_trip_time->SetValue( timeStr );
}

void OvmsVehicleSmartEQ::PollReply_obd_start_trip(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_obd_start_trip_km->SetValue((float) CAN_UINT(0));
}

void OvmsVehicleSmartEQ::PollReply_obd_start_time(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(3);
  int value_1 = CAN_UINT24(0);
  std::string timeStr = SecondsToHHmm(value_1);
  mt_obd_start_trip_time->SetValue( timeStr );
}

void OvmsVehicleSmartEQ::PollReply_obd_mt_day(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  int value = CAN_UINT(0);
  if (value > 0) { // excluding value of 0 seems to be necessary for now
    // Send notification?
    int now = StdMetrics.ms_m_timeutc->AsInt();
    int threshold = mt_obd_mt_day_prewarn->AsInt();
    int old_value = ROUNDPREC((StdMetrics.ms_v_env_service_time->AsInt() - now) / 86400, 0);
    if (old_value > threshold && value <= threshold) {
      //MyNotify.NotifyStringf("info", "serv.time", "Service time left: %d days!", value);
      NotifyMaintenance();
      // send notification only once per threshold, todo: make configurable???
      mt_obd_mt_day_prewarn->SetValue(threshold - 7);
    }
    // reset prewarn value after service time has been increased
    if (value > 100 && threshold < 100) {
      mt_obd_mt_day_prewarn->SetValue(45);  // default value 45 days, todo: make configurable???
    }
    mt_obd_mt_day_usual->SetValue(value); // set next service in days
    StdMetrics.ms_v_env_service_time->SetValue(StdMetrics.ms_m_timeutc->AsInt() + (value * 86400));  // set next service at time
    if(MyConfig.GetParamValueInt("xsq", "service.time", -1) > -1) {
      MyConfig.SetParamValueInt("xsq", "service.time", -1);
    }
  } else {
    // reset service time to current time if service time is 0
    if(MyConfig.GetParamValueInt("xsq", "service.time", -1) == -1) {
      StdMetrics.ms_v_env_service_time->SetValue(StdMetrics.ms_m_timeutc->AsInt());
      MyConfig.SetParamValueInt("xsq", "service.time", StdMetrics.ms_m_timeutc->AsInt());
    } else {
      // get service time from config if service time is < 0 Days
      StdMetrics.ms_v_env_service_time->SetValue(MyConfig.GetParamValueInt("xsq", "service.time", -1));
    }
    int now = StdMetrics.ms_m_timeutc->AsInt();
    int old_value = ROUNDPREC(( now - StdMetrics.ms_v_env_service_time->AsInt()) / 86400, 0);
    mt_obd_mt_day_usual->SetValue(old_value * -1);
  }

}

void OvmsVehicleSmartEQ::PollReply_obd_mt_km(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  int value = CAN_UINT(0);
  StdMetrics.ms_v_env_service_range->SetValue(value); // set next service in km
  mt_obd_mt_km_usual->SetValue(value);
}

void OvmsVehicleSmartEQ::PollReply_obd_mt_level(const char* data, uint16_t reply_len) {
  if (reply_len < 1) return;
  int value = CAN_UINT(0);
  std::string txt;
  if (value == 0) 
    {
    txt = "Service A";
    }
  else if (value == 1) 
    {
    txt = "Service B";
    } 
  else 
    {
    std::ostringstream oss;
    oss << "Unknown " << value;
    txt = oss.str();
    }
  mt_obd_mt_level->SetValue(txt.c_str());
  StdMetrics.ms_v_gen_substate->SetValue(txt.c_str());
}