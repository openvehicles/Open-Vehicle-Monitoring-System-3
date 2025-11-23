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
 ; https://github.com/MyLab-odyssey/ED4scan
 
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

static const char *TAG = "v-smarteq";

#include "vehicle_smarteq.h"

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
        case 0x3495: // rqDCDC_Load
          PollReply_EVC_DCDC_Load(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x3022: // DCDC activation request
          PollReply_EVC_DCDC_ActReq(m_rxbuf.data(), m_rxbuf.size());
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
        case 0x3301: // USM 14V Voltage measure (CAN)
          PollReply_EVC_USM14VVoltage(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x3433: // Battery voltage request (internal SCH)
          PollReply_EVC_14VBatteryVoltageReq(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x34CB: // Cabin blower command
          PollReply_EVC_CabinBlower(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x2005: // Battery voltage 14V
          PollReply_EVC_14VBatteryVoltage(m_rxbuf.data(), m_rxbuf.size());
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
        case 0x5049: // Mains phase frequency
          PollReply_OBL_JB2AC_PhaseFreq(m_rxbuf.data(), m_rxbuf.size());
          break;
      }
      break;
    case 0x763:
      switch (job.pid) {
        case 0x200c: // temperature sensor values
          PollReply_TDB(m_rxbuf.data(), m_rxbuf.size());
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
    case 0x765:  // BCM
      switch (job.pid) {
        case 0x81: // req.VIN
          PollReply_BCM_VIN(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x74: // TPMS input capture actual
          PollReply_BCM_TPMS_InputCapt(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x79: // TPMS counters/status (missing transmitters)
          PollReply_BCM_TPMS_Status(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x8003: // rq_VehicleState
          PollReply_BCM_VehicleState(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x605e: // rq_UNDERHOOD_OPENED
          PollReply_BCM_DoorUnderhoodOpened(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x8079: // Generator mode
          PollReply_BCM_GenMode(m_rxbuf.data(), m_rxbuf.size());
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

  // Byte 11-12: Battery Voltage (Terminal Voltage) /64 (V)
  float v_pack_term = (float)CAN_UINT(8) / 64.0f;
  
  // Byte 13-14: Battery Current /32 (A), signed
  uint16_t cur_u = CAN_UINT(10);  
  int16_t  cur_s = (int16_t)cur_u > 0x1fff ? (int16_t)(cur_u - 0xffff) : (int16_t)cur_u;
  float i_pack = (float)cur_s / 32.0f;

  mt_bms_CV_Range_min->SetValue(v_cell_min);
  mt_bms_CV_Range_max->SetValue(v_cell_max);
  mt_bms_CV_Range_mean->SetValue((v_cell_min + v_cell_max) / 2.0f);

  mt_bms_BattLinkVoltage->SetValue(v_link);
  mt_bms_BattContactorVoltage->SetValue(v_pack_term);
  mt_bms_BattCV_Sum->SetValue(cv_sum_raw);

  // P = V × I (in kW)
  float pack_power_kw = (v_pack_term * i_pack) / 1000.0f;
  mt_bms_BattPower_power->SetValue(pack_power_kw);
  StdMetrics.ms_v_bat_voltage->SetValue(v_pack_term);
  StdMetrics.ms_v_bat_current->SetValue(i_pack * -1.0f); // invert to charge(+)/discharge(-) convention
  StdMetrics.ms_v_bat_power->SetValue(pack_power_kw);

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
  mt_bms_EVmode_txt->SetValue(veh_mode_txt);

  // Clamp 30 Voltage:
  float clamp30 = ((float)(uint8_t)CAN_BYTE(22)) / 8.0f;
  if (clamp30 > 5 && clamp30 < 20)  // plausible 12V system window
    mt_bms_12v->SetValue(clamp30);

  mt_bms_interlock_hvplug->SetValue(((int)CAN_BYTE(13)) == 1);
  mt_bms_interlock_service->SetValue(((int)CAN_BYTE(14)) == 1);

  int fusi = CAN_UINT(15);
  const char* fusi_txt;
  switch (fusi)
  {
    case 0: fusi_txt="No Error"; break;
    case 4: fusi_txt="HVBatLevel1 Failure"; break;
    case 6: fusi_txt="HVBatLevel2 Failure"; break;
    default: fusi_txt="Unavailable Value"; break;
  }
  mt_bms_fusi_mode_txt->SetValue(fusi_txt);

  float mg_rpm_raw = CAN_UINT(17); // motor rpm
  float mg_rpm = mg_rpm_raw / 2.0f;
  mt_bms_mg_rpm->SetValue(mg_rpm);
  StdMetrics.ms_v_mot_rpm->SetValue(mg_rpm);

  int safety = CAN_BYTE(19);
  const char* safety_txt;
  switch (safety)
  {
    case 0: safety_txt="Not Used"; break;
    case 1: safety_txt="No Safety Mode"; break;
    case 2: safety_txt="Safety Mode 1 Requested"; break;
    default: safety_txt="Unavailable Value"; break;
  }
  mt_bms_safety_mode_txt->SetValue(safety_txt);
}

void OvmsVehicleSmartEQ::PollReply_TDB(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(4);
  const int raw = static_cast<int>(CAN_UINT(2));
  const float temp = (raw - 400) * 0.1f;

  constexpr float MAX_VALID_TEMP = 100.0f;
  constexpr float MIN_VALID_TEMP = -50.0f;
  if (temp > MIN_VALID_TEMP && temp < MAX_VALID_TEMP) {
    StdMetrics.ms_v_env_temp->SetValue(temp);
  }
}

void OvmsVehicleSmartEQ::PollReply_BCM_VIN(const char* data, uint16_t reply_len) {
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

void OvmsVehicleSmartEQ::PollReply_BCM_TPMS_InputCapt(const char* data, uint16_t reply_len) {
  // so require >=22 bytes of payload (indices 0..21).
  REQUIRE_LEN(22);

  // Index mapping (payload):
  //  0..3   RF event types (wheel 1..4)
  //  4..7  Motion status (wheel 1..4)
  //  8-9 / 10-11 / 12-13 / 14-15 pressures raw16
  //  16-19 temperatures raw8 (offset -30.0 °C)
  //  20 bits 0..3 temp incoh (w1..w4), bits 4..7 internal incoh (w1..w4)
  //  21 bits 0..3 low battery (w1..w4)

  uint8_t lowbatt_bits = (uint8_t)CAN_BYTE(21);

  for (int i=0;i<4;i++) {
    // Pressure: big-endian 16 bit *0.75 kPa
    uint16_t praw = CAN_UINT(8 + (i*2));
    m_tpms_pressure[i] = (float)praw != 0xffff ? (float)praw * 0.75f : 0.0f;
    // Temperature: raw byte + offset -30.0
    uint16_t traw = (uint16_t)(uint8_t)CAN_BYTE(16 + i);
    m_tpms_temperature[i] = traw != 0xffff ? (float)traw - 30.0f : 0.0f;
    m_tpms_lowbatt[i]     = (lowbatt_bits >> i) & 0x01;
  }
}

void OvmsVehicleSmartEQ::PollReply_BCM_TPMS_Status(const char* data, uint16_t reply_len) {
  // Byte 26 (1-based) → index 28 (0-based): bits 0..3 = missing transmitter flags for wheels 1..4
  REQUIRE_LEN(25);
  uint8_t raw = CAN_BYTE(25);
  for (int i = 0; i < 4; i++) {
    bool missing = ((raw >> i) & 0x01) != 0;
    m_tpms_missing_tx[i] = missing;
  }
}

void OvmsVehicleSmartEQ::PollReply_BCM_VehicleState(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  int code = CAN_BYTE(0);
  std::string msgtxt = "";
  switch(code) {
    case 0: msgtxt = "SLEEPING"; break; 
    case 1: msgtxt = "TECHNICAL WAKE UP"; break;
    case 2: msgtxt = "CUT OFF PENDING"; break;
    case 3: msgtxt = "BAT TEMPO LEVEL"; break;
    case 4: msgtxt = "ACCESSORY LEVEL"; break;
    case 5: msgtxt = "IGNITION LEVEL"; break;
    case 6: msgtxt = "STARTING IN PROGRESS"; break;
    case 7: msgtxt = "ENGINE RUNNING"; break;
    case 8: msgtxt = "AUTOSTART"; break;
    case 9: msgtxt = "ENGINE SYSTEM STOP"; break;
    default: msgtxt = "Unknown code"; break;
  }
  mt_bcm_vehicle_state->SetValue(msgtxt);
  StdMetrics.ms_v_env_awake->SetValue(code > 0);
}

void OvmsVehicleSmartEQ::PollReply_BCM_DoorUnderhoodOpened(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  int code = CAN_BYTE(0);
  StdMetrics.ms_v_door_hood->SetValue(code==1);
}

void OvmsVehicleSmartEQ::PollReply_BCM_GenMode(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 80 79 <Byte>
  // Spec: firstbyte 4, lists 0-15
  REQUIRE_LEN(1);
  uint8_t mode = CAN_BYTE(0);
  
  const char* txt;
  switch (mode) {
    case 0:  txt = "LIMIT_REQ_BY_ENGINE"; break;
    case 1:  txt = "NOMINAL_STRATEGY_MODE"; break;
    case 2:  txt = "ESM_INTERMEDIATE_MODE"; break;
    case 3:  txt = "BATTERY_PROTECTION"; break;
    case 4:  txt = "ESM_DISCHARGE_MODE"; break;
    case 5:  txt = "ESM_REGENERATION_MODE"; break;
    case 6:  txt = "ESM_INHIBITED_BY_WIPER"; break;
    case 7:  txt = "ESM_INHIBITED_BY_ACTUATOR"; break;
    case 8:  txt = "MAX_VOLTAGE_BY_ENDUR"; break;
    case 9:  txt = "MIN_VOLTAGE_BY_ECM"; break;
    case 10: txt = "FLOATING_MODE"; break;
    case 11: txt = "NOT_USED1"; break;
    case 12: txt = "NOT_USED2"; break;
    case 13: txt = "NOT_USED3"; break;
    case 14: txt = "NOT_USED4"; break;
    default: txt = "UNAVAILABLE"; break;
  }
  mt_bcm_gen_mode->SetValue(txt);
  StdMetrics.ms_v_gen_state->SetValue(txt);
}

void OvmsVehicleSmartEQ::PollReply_EVC_HV_Energy(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 32 0C <Byte> <Byte>
  REQUIRE_LEN(2);
  uint16_t raw = CAN_UINT(0);
  // step 0.005 kWh
  float value = raw * 0.005f;
  mt_evc_hv_energy->SetValue(value);
  // (Optional: keep legacy standard metrics updates)
  if (mt_bms_HV->IsDefined() && mt_bms_HV->AsFloat() > 0) {
    StdMetrics.ms_v_bat_capacity->SetValue(value);
    StdMetrics.ms_v_bat_cac->SetValue(value * 1000.0f / mt_bms_HV->AsFloat());
  }
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Load(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 34 95 <Byte>
  REQUIRE_LEN(1);
  uint8_t raw = CAN_BYTE(0);
  if (raw == 0xFE || raw == 0xFF) {
    // 0xFE/0xFF often used as invalid
    mt_evc_LV_DCDC_load->SetValue(0);
    return;
  }
  // step 0.390625 %
  float pct = raw * 0.390625f;
  mt_evc_LV_DCDC_load->SetValue(pct);
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_ActReq(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 30 22 <Byte>
  // Spec: bitoffset 7, bitscount 1 (MSB of byte 0 in payload)
  REQUIRE_LEN(1);
  uint8_t raw = CAN_BYTE(0);
  bool active = raw != 0;  // Bit 7
  mt_evc_LV_DCDC_act_req->SetValue(active);
  StdMetrics.ms_v_env_charging12v->SetValue(active);
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_VoltReq(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 30 23 <Byte>
  REQUIRE_LEN(1);
  uint8_t raw = CAN_BYTE(0);
  float value = 12.0f + raw * 0.05f;   // offset 12.0, step 0.05
  mt_evc_LV_DCDC_volt_req->SetValue(value);
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Volt(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 30 24 <Byte>
  REQUIRE_LEN(1);
  uint8_t raw = CAN_BYTE(0);
  float value = 4.0f + raw * 0.1f;     // offset 4.0, step 0.1
  mt_evc_LV_DCDC_volt->SetValue(value);
  StdMetrics.ms_v_charge_12v_voltage->SetValue(value);
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Amps(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 30 25 <Byte>
  REQUIRE_LEN(1);
  uint8_t raw = CAN_BYTE(0);
  mt_evc_LV_DCDC_amps->SetValue((float)raw);
  StdMetrics.ms_v_charge_12v_current->SetValue((float)raw);
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Power(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 34 94 <Byte> <Byte>
  REQUIRE_LEN(2);
  uint16_t raw = CAN_UINT(0);
  // step 10 W
  float watts = raw * 10.0f;
  mt_evc_LV_DCDC_power->SetValue(watts);
  StdMetrics.ms_v_charge_12v_power->SetValue(watts);
}

void OvmsVehicleSmartEQ::PollReply_EVC_USM14VVoltage(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 33 01 <Byte>
  REQUIRE_LEN(1);
  uint8_t raw = CAN_BYTE(0);
  float value = 6.0f + raw * 0.06f;
  mt_evc_LV_USM_volt->SetValue(value);
}

void OvmsVehicleSmartEQ::PollReply_EVC_14VBatteryVoltage(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 20 05  <Byte> <Byte>
  REQUIRE_LEN(2);
  uint16_t raw = CAN_UINT(0);
  float value = raw * 0.01f;
  mt_evc_LV_batt_voltage_can->SetValue(value);
}

void OvmsVehicleSmartEQ::PollReply_EVC_14VBatteryVoltageReq(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 34 33 <Byte>
  REQUIRE_LEN(1);
  uint8_t raw = CAN_BYTE(0);
  float value = 12.0f + raw * 0.05f;
  mt_evc_LV_batt_voltage_req->SetValue(value);
}

void OvmsVehicleSmartEQ::PollReply_EVC_CabinBlower(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 34 CB <Byte>
  // Spec: step 2.0, unit %
  REQUIRE_LEN(1);
  uint8_t raw = CAN_BYTE(0);
  float pct = raw * 2.0f;
  
  if (pct <= 100.0f) {  // sanity check
    StdMetrics.ms_v_env_cabinfan->SetValue(pct);
  }
}

void OvmsVehicleSmartEQ::PollReply_OBL_ChargerAC(const char* data, uint16_t reply_len) {
  // slow charge (OBL) - 1 phase charger data
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
  // POSITIVE RESPONSE FORMAT: 62 50 4A <Byte> <Byte>
  // Spec: offset -20000.0, bytescount 2, unit W
  REQUIRE_LEN(2);
  uint16_t raw = CAN_UINT(0);
  float power_kw = (raw - 20000.0f) / 1000.0f;
  
  mt_obl_main_CHGpower->SetElemValue(0, power_kw);
  mt_obl_main_CHGpower->SetElemValue(1, 0);
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
  int16_t raw = (int16_t)(CAN_UINT(0) - 32768);
  mt_obl_main_current_leakage_dc->SetValue((float)raw / 1000.0f);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_HF10kHz(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  int16_t raw = (int16_t)(CAN_UINT(0) - 32768);
  mt_obl_main_current_leakage_hf_10khz->SetValue((float)raw / 1000.0f);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_HFCurrent(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  int16_t raw = (int16_t)(CAN_UINT(0) - 32768);
  mt_obl_main_current_leakage_hf->SetValue((float)raw / 1000.0f);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_LFCurrent(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  int16_t raw = (int16_t)(CAN_UINT(0) - 32768);
  mt_obl_main_current_leakage_lf->SetValue((float)raw / 1000.0f);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_MaxCurrent(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(1);
  mt_obl_main_max_current->SetValue((float) CAN_BYTE(0));
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_PhaseFreq(const char* data, uint16_t reply_len) {
  // POSITIVE RESPONSE FORMAT: 62 50 49 <Byte> <Byte>
  // Spec: offset 10.0, step 0.0078125, bytescount 2
  REQUIRE_LEN(2);
  uint16_t raw = CAN_UINT(0);
  float freq = 10.0f + (raw * 0.0078125f);
  if (freq > 40.0f) // plausibility check
    mt_obl_main_freq->SetValue(freq);
}

void OvmsVehicleSmartEQ::PollReply_obd_time(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(3);
  int value_1 = CAN_UINT24(0);
  std::string timeStr = SecondsToHHmm(value_1);
  mt_reset_time->SetValue( timeStr );
}

void OvmsVehicleSmartEQ::PollReply_obd_start_trip(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(2);
  mt_start_distance->SetValue((float) CAN_UINT(0));
}

void OvmsVehicleSmartEQ::PollReply_obd_start_time(const char* data, uint16_t reply_len) {
  REQUIRE_LEN(3);
  int value_1 = CAN_UINT24(0);
  std::string timeStr = SecondsToHHmm(value_1);
  mt_start_time->SetValue( timeStr );
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
  else
    {
    txt = "Service B";
    }
  mt_obd_mt_level->SetValue(txt.c_str());
  StdMetrics.ms_v_gen_substate->SetValue(txt.c_str());
}