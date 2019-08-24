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
void OvmsVehicleSmartEQ::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
  static int last_pid = -1;
  static int last_remain = -1;
  static uint8_t buf[MAX_POLL_DATA_LEN];
  static int bufpos = 0;

  int i;
  
  //if (pid == 0xF111) ESP_LOGD(TAG, "IncomingPollReply: pid=%#x len=%d remain=%d", pid, length, remain);
  
  if ( pid != last_pid || remain >= last_remain ) {
    // must be a new reply, so reset to the beginning
    last_pid=pid;
    last_remain=remain;
    bufpos=0;
  }
  
  if (bufpos == MAX_POLL_DATA_LEN) { 
    remain=0;
    m_poll_ml_remain=0;
  } else {
    for (i=0; i<length; i++) {
      if ( bufpos < sizeof(buf) ) buf[bufpos++] = data[i];
    }
  }
  
  if (remain==0) {
    uint32_t id_pid = m_poll_moduleid_low<<16 | pid;
    switch (m_poll_moduleid_low) {
      case 0x7BB:
        switch (pid) {
          case 0x41: // rqBattVoltages_P1
            PollReply_BMS_BattVolts(buf, bufpos, 0);
            break;
          case 0x42: // rqBattVoltages_P2
            PollReply_BMS_BattVolts(buf, bufpos, 48);
            break;
          case 0x04: // rqBattTemperatures
            PollReply_BMS_BattTemps(buf, 31);
            break;
        }
        break;
      case 0x793:
        switch (pid) {
          case 0x80: // rqIDpart OBL_7KW_Installed
            //PollReply_BMS_BattVolts(buf, bufpos, 0);
            break;
        }
        break;
      default:
        ESP_LOGI(TAG, "IncomingPollReply: unknown reply module|pid=%#x len=%d", id_pid, bufpos);
        break;
    }
    last_pid=pid;
    last_remain=remain;
    bufpos=0;
    memset(buf, 0, sizeof(buf));
  }
}

void OvmsVehicleSmartEQ::PollReply_BMS_BattVolts(uint8_t* reply_data, uint16_t reply_len, uint16_t start) {
  float CV;
  static bool cellstat = false;
  
  CV = (reply_data[0] * 256 + reply_data[1]);
  if (CV == 5120 && start == 0) cellstat = false;
  if (CV != 5120 && start == 0) cellstat = true;
  
  if (cellstat) {
    for(uint16_t n = 0; n < CELLCOUNT; n = n + 2){
      CV = (reply_data[n] * 256 + reply_data[n + 1]);
      CV = CV / 1024.0 * 1000;
      BmsSetCellVoltage((n/2)+start, CV);
      //ESP_LOGV(TAG, "CellVoltage: id=%d len=%F", (n/2)+start, CV);
    }
  }
}

void OvmsVehicleSmartEQ::PollReply_BMS_BattTemps(uint8_t* reply_data, uint16_t reply_len) {
  int16_t Temps[31];
  float BMStemps[31];
  
  for (uint16_t n = 0; n < (reply_len * 2); n = n + 2) {
    int16_t value = reply_data[n] * 256 + reply_data[n + 1];
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

