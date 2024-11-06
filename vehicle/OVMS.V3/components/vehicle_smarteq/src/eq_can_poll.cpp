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
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"

#include "vehicle_smarteq.h"


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
  ESP_LOGV(TAG, "IncomingPollReply: PID %02X: len=%d %s", job.pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());
  
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
        case 0x3025: // rqDCDC_Amps
          PollReply_EVC_DCDC_Amps(m_rxbuf.data(), m_rxbuf.size());
          break;
        case 0x3494: // rqDCDC_Power
          PollReply_EVC_DCDC_Power(m_rxbuf.data(), m_rxbuf.size());
          break;
      }
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
          PollReply_BMS_BattTemps(m_rxbuf.data(), 31);
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
        case 0x504A: // rqJB2AC_Power
          PollReply_OBL_JB2AC_Power(m_rxbuf.data(), m_rxbuf.size());
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
      }
      break;
    case 0x765:
      switch (job.pid) {
        case 0x81: // req.VIN
          PollReply_VIN(m_rxbuf.data(), m_rxbuf.size());
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
      ESP_LOGE(TAG, "IncomingPollError: PID %02X: err=%02X", job.pid, code);
      break;
  }
}

void OvmsVehicleSmartEQ::PollReply_BMS_BattVolts(const char* data, uint16_t reply_len, uint16_t start) {
  float CV;
  static bool cellstat = false;
  
  CV = CAN_UINT(0);
  if (CV == 5120 && start == 0) cellstat = false;
  if (CV != 5120 && start == 0) cellstat = true;
  
  if (cellstat) {
    for(int n = 0; n < CELLCOUNT; n = n + 2){
      CV = CAN_UINT(n);
      CV = CV / 1024.0;
      BmsSetCellVoltage((n/2) + start, CV);
      
      ESP_LOGV(TAG, "CellVoltage: id=%d volt=%F", (n/2)+start, CV);
    }
  }
}

void OvmsVehicleSmartEQ::PollReply_BMS_BattTemps(const char* data, uint16_t reply_len) {
  int16_t Temps[31];
  float BMStemps[31];
  
  for (uint16_t n = 0; n < (reply_len * 2); n = n + 2) {
    int16_t value = CAN_UINT(n);
    if (n > 4) {
      //Test for negative min. module temperature and apply offset
      if (Temps[1] & 0x8000) {
        value = value - 0xA00; //minus offset
      }
    }
    Temps[n / 2] = value;
    BMStemps[n / 2] = value / 64.0;
    mt_bms_temps->SetElemValues(0, 31, BMStemps);
  }
}

void OvmsVehicleSmartEQ::PollReply_BMS_BattState(const char* data, uint16_t reply_len) {
  mt_bms_CV_Range_min->SetValue( CAN_UINT(0) / 1024.0 );
  mt_bms_CV_Range_max->SetValue( CAN_UINT(2) / 1024.0 );
  mt_bms_CV_Range_mean->SetValue( (mt_bms_CV_Range_max->AsFloat() + mt_bms_CV_Range_min->AsFloat()) / 2.0 );
  mt_bms_BattLinkVoltage->SetValue( CAN_UINT(4) / 64.0 );
  mt_bms_BattCV_Sum->SetValue( CAN_UINT(6) / 64.0 );
  mt_bms_BattPower_voltage->SetValue( CAN_UINT(8) );
  int16_t value = CAN_UINT(10);
  if (value > 0x1fff) {
    mt_bms_BattPower_current->SetValue( (float) value - 0xffff );
  } else {
    mt_bms_BattPower_current->SetValue( value );
  }
  mt_bms_BattPower_power->SetValue( mt_bms_BattPower_voltage->AsFloat() / 64.0 * mt_bms_BattPower_current->AsFloat() / 32.0 / 1000.0 );

  mt_bms_HVcontactState->SetValue( CAN_BYTE(12) );
  mt_bms_HV->SetValue( mt_bms_BattCV_Sum->AsFloat() );
  mt_bms_EVmode->SetValue( CAN_BYTE(21) );
  mt_bms_LV->SetValue( CAN_BYTE(22) / 8.0 );

  mt_bms_Amps->SetValue( mt_bms_BattPower_current->AsFloat() );
  mt_bms_Amps2->SetValue( mt_bms_BattPower_current->AsFloat() / 32.0 );
  mt_bms_Power->SetValue( mt_bms_HV->AsFloat() * mt_bms_Amps2->AsFloat() / 1000.0 );
  StandardMetrics.ms_v_bat_current->SetValue(mt_bms_Amps2->AsFloat(0));
  StandardMetrics.ms_v_bat_power->SetValue( mt_bms_Power->AsFloat(0) );
}

void OvmsVehicleSmartEQ::PollReply_HVAC(const char* data, uint16_t reply_len) {
  //StandardMetrics.ms_v_env_cabintemp->SetValue( (((reply_data[3] << 8) | reply_data[4]) - 400) * 0.1);
}

void OvmsVehicleSmartEQ::PollReply_TDB(const char* data, uint16_t reply_len) {
  StandardMetrics.ms_v_env_temp->SetValue( (CAN_UINT(2) - 400) * 0.1 );
  if (m_ios_tpms_fix) {
    float temp = (float) (CAN_UINT(2) - 400) * 0.1;
    if (temp < 1.0f) temp = 1.0;
    StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, temp);
    StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, temp);
    StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, temp);
    StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, temp);
  }
}

void OvmsVehicleSmartEQ::PollReply_VIN(const char* data, uint16_t reply_len) {
  std::string vin = data;
  StandardMetrics.ms_v_vin->SetValue(vin.substr(0, vin.length() - 2));
}

void OvmsVehicleSmartEQ::PollReply_EVC_HV_Energy(const char* data, uint16_t reply_len) {
  mt_evc_hv_energy->SetValue( CAN_UINT(0) / 200.0 );
  StandardMetrics.ms_v_inv_power->SetValue(mt_evc_hv_energy->AsFloat());
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_State(const char* data, uint16_t reply_len) {
  mt_evc_LV_DCDC_amps->SetValue( CAN_BYTE(0) );
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Load(const char* data, uint16_t reply_len) {
  mt_evc_LV_DCDC_load->SetValue( CAN_BYTE(0) == 0xFE ? 0 : CAN_BYTE(0) );
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Amps(const char* data, uint16_t reply_len) {
  mt_evc_LV_DCDC_power->SetValue( CAN_BYTE(0) );
}

void OvmsVehicleSmartEQ::PollReply_EVC_DCDC_Power(const char* data, uint16_t reply_len) {
  mt_evc_LV_DCDC_state->SetValue( CAN_UINT(0) );
}

void OvmsVehicleSmartEQ::PollReply_OBL_ChargerAC(const char* data, uint16_t reply_len) {
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
  if (mt_obl_main_amps->GetElemValue(0) > 0 || mt_obl_main_amps->GetElemValue(1) > 0) {
    mt_obl_main_freq->SetValue( CAN_BYTE(11) );
  } else {
    mt_obl_main_freq->SetValue(0);
  }

  //Get AC Power
  value = CAN_UINT(12);
  if (value < 0xEA00) {  //OBL showing only valid data while charging
    mt_obl_main_CHGpower->SetElemValue(0, value / 2000.0);
  } else {
    mt_obl_main_CHGpower->SetElemValue(0, 0);
  }
  value = CAN_UINT(14);
  if (value < 0xEA00) {  //OBL showing only valid data while charging
    mt_obl_main_CHGpower->SetElemValue(1, value / 2000.0);
  } else {
    mt_obl_main_CHGpower->SetElemValue(1, 0);
  }
  UpdateChargeMetrics();
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph1_RMS_A(const char* data, uint16_t reply_len) {
  float value = ((CAN_UINT(0) * 0.625) - 2000) / 10.0;
  mt_obl_main_amps->SetElemValue(0, value);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph2_RMS_A(const char* data, uint16_t reply_len) {
  float value = ((CAN_UINT(0) * 0.625) - 2000) / 10.0;
  mt_obl_main_amps->SetElemValue(1, value);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph3_RMS_A(const char* data, uint16_t reply_len) {
  float value = ((CAN_UINT(0) * 0.625) - 2000) / 10.0;
  mt_obl_main_amps->SetElemValue(2, value);
  UpdateChargeMetrics();
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph12_RMS_V(const char* data, uint16_t reply_len) {
  mt_obl_main_volts->SetElemValue(0, CAN_UINT(0) / 2.0);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph23_RMS_V(const char* data, uint16_t reply_len) {
  mt_obl_main_volts->SetElemValue(1, CAN_UINT(0) / 2.0);
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Ph31_RMS_V(const char* data, uint16_t reply_len) {
  mt_obl_main_volts->SetElemValue(2, CAN_UINT(0) / 2.0);
  UpdateChargeMetrics();
}

void OvmsVehicleSmartEQ::PollReply_OBL_JB2AC_Power(const char* data, uint16_t reply_len) {
  mt_obl_main_CHGpower->SetElemValue(0, (CAN_UINT(0) - 20000) / 1000.0);
  UpdateChargeMetrics();
}
