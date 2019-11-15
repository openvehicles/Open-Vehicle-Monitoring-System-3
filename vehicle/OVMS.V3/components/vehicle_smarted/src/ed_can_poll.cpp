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

const PROGMEM byte rqCarVIN[4]                   = {0x02, 0x09, 0x02, 0x00};

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
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_utils.h"

#include "vehicle_smarted.h"

static const OvmsVehicle::poll_pid_t smarted_polls[] =
{
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF111, {  0,120,999 } }, // rqChargerPN_HW
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0226, {  0,120,999 } }, // rqChargerVoltages
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0225, {  0,120,999 } }, // rqChargerAmps
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x022A, {  0,120,999 } }, // rqChargerSelCurrent
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0223, {  0,120,999 } }, // rqChargerTemperatures
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0201, {  0,300,600 } }, // rqBattTemperatures
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0202, {  0,300,600 } }, // rqBattModuleTemperatures
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0208, {  0,300,600 } }, // rqBattVolts
  { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
};

void OvmsVehicleSmartED::ObdInitPoll() {
  // init poller:
  PollSetPidList(m_can1, smarted_polls);
  PollSetState(0);
}

/**
 * Incoming poll reply messages
 */
void OvmsVehicleSmartED::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
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
  
  if (pid == 0xF111 && remain <= 3) {
    remain=0;
    m_poll_ml_remain=0;
    for (i=0; i<length; i++) {
      if ( bufpos < sizeof(buf) ) buf[bufpos++] = data[i];
    }
  } else {
    if (bufpos == MAX_POLL_DATA_LEN) { 
      remain=0;
      m_poll_ml_remain=0;
    } else {
      for (i=0; i<length; i++) {
        if ( bufpos < sizeof(buf) ) buf[bufpos++] = data[i];
      }
    }
  }
  if (remain==0) {
    uint32_t id_pid = m_poll_moduleid_low<<16 | pid;
    switch (id_pid) {
      case 0x7EF0201: // rqBattTemperatures
        PollReply_BMS_BattTemp(buf, bufpos);
        break;
      case 0x7EF0202: // rqBattModuleTemperatures
        PollReply_BMS_ModuleTemp(buf, bufpos);
        break;
      case 0x7EF0208: // rqBattVolts
        PollReply_BMS_BattVolts(buf, bufpos);
        break;
      case 0x483F111: // rqChargerPN_HW
        PollReply_NLG6_ChargerPN_HW(buf, bufpos);
        break;
      case 0x4830226: // rqChargerVoltages
        PollReply_NLG6_ChargerVoltages(buf, bufpos);
        break;
      case 0x4830225: // rqChargerAmps
        PollReply_NLG6_ChargerAmps(buf, bufpos);
        break;
      case 0x483022A: // rqChargerSelCurrent
        PollReply_NLG6_ChargerSelCurrent(buf, bufpos);
        break;
      case 0x4830223: // rqChargerTemperatures
        PollReply_NLG6_ChargerTemperatures(buf, bufpos);
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

void OvmsVehicleSmartED::PollReply_BMS_BattTemp(uint8_t* reply_data, uint16_t reply_len) {
  int16_t Temps[7];
  int n;
  
  for(n = 0; n < (7 * 2); n = n + 2){
    Temps[n/2] = ((reply_data[n + 1] * 256 + reply_data[n + 2]));
  }
  if (Temps[0]/64 != -512)
    StandardMetrics.ms_v_bat_temp->SetValue(Temps[0]/64);
}

void OvmsVehicleSmartED::PollReply_BMS_ModuleTemp(uint8_t* reply_data, uint16_t reply_len) {
  int16_t Temps[13];
  int n;
  int i;

  for(n = 0; n < (9 * 2); n = n + 2){
    Temps[n/2] = ((reply_data[n + 1] * 256 + reply_data[n + 2]));
  }
  for (n = 0; n < 9; n = n + 3) {
    for (i = 0; i < 3; i++) {
      if (i==2) BmsSetCellTemperature(n/3, (float) Temps[n + i]/64);
    }
  }
}

void OvmsVehicleSmartED::PollReply_BMS_BattVolts(uint8_t* reply_data, uint16_t reply_len) {
  int n;
  
  for(n = 0; n < (CELLCOUNT * 2); n = n + 2){
    float Cells = (reply_data[n + 1] * 256 + reply_data[n + 2]);
    BmsSetCellVoltage(n/2, Cells/1000);
  } 
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerPN_HW(uint8_t* reply_data, uint16_t reply_len) {
  int n;
  int comp = 0;
  char NLG6PN_HW[11];
  memset(NLG6PN_HW, 0, sizeof(NLG6PN_HW));
  
  for (n = 1; n < 11; n++) {
    NLG6PN_HW[n - 1] = reply_data[n];
    if (reply_data[n] == NLG6_PN_HW[n - 1]) {
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

void OvmsVehicleSmartED::PollReply_NLG6_ChargerVoltages(uint8_t* reply_data, uint16_t reply_len) {
  float NLG6MainsVoltage[3];
  
  if (mt_nlg6_present->AsBool()){
    mt_nlg6_dc_lv->SetValue(reply_data[5]/10.0);
    mt_nlg6_dc_hv->SetValue((reply_data[6] * 256 + reply_data[7])/10.0);
    NLG6MainsVoltage[0] = (reply_data[8] * 256 + reply_data[9])/10.0;
    NLG6MainsVoltage[1] = (reply_data[10] * 256 + reply_data[11])/10.0;
    NLG6MainsVoltage[2] = (reply_data[12] * 256 + reply_data[13])/10.0;
  } else {
    mt_nlg6_dc_lv->SetValue(reply_data[3]/10.0);
    if ((reply_data[6] * 256 + reply_data[7]) != 8190) {  //OBL showing only valid data while charging
      mt_nlg6_dc_hv->SetValue((reply_data[6] * 256 + reply_data[7])/10.0);
    } else {
      mt_nlg6_dc_hv->SetValue(0);
    }
    if ((reply_data[8] * 256 + reply_data[9]) != 8190) {  //OBL showing only valid data while charging
      NLG6MainsVoltage[0] = (reply_data[8] * 256 + reply_data[9])/10.0;
    } else {
      NLG6MainsVoltage[0] = 0;
    }
    NLG6MainsVoltage[1] = 0;
    NLG6MainsVoltage[2] = 0;
  }
  mt_nlg6_main_volts->SetElemValues(0, 3, NLG6MainsVoltage);
  if (NLG6MainsVoltage[0] != 0) StandardMetrics.ms_v_charge_voltage->SetValue(NLG6MainsVoltage[0]);
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerAmps(uint8_t* reply_data, uint16_t reply_len) {
  float NLG6MainsAmps[3]; 
  
  if (mt_nlg6_present->AsBool()){
    mt_nlg6_dc_current->SetValue((reply_data[1] * 256 + reply_data[2])/10.0); 
    NLG6MainsAmps[0] = (reply_data[3] * 256 + reply_data[4])/10.0;  
    NLG6MainsAmps[1] = (reply_data[5] * 256 + reply_data[6])/10.0;
    NLG6MainsAmps[2] = (reply_data[7] * 256 + reply_data[8])/10.0; 
    mt_nlg6_amps_chargingpoint->SetValue((reply_data[13] * 256 + reply_data[14])/10.0);
    mt_nlg6_amps_cablecode->SetValue((reply_data[15] * 256 + reply_data[16])/10.0);
  } else {
    if ((reply_data[3] * 256 + reply_data[4]) != 2047) {  //OBL showing only valid data while charging
      NLG6MainsAmps[0] = (reply_data[3] * 256 + reply_data[4])/10.0;
    } else {
      NLG6MainsAmps[0] = 0;
    }
    NLG6MainsAmps[1] = 0;
    NLG6MainsAmps[2] = 0;
    mt_nlg6_amps_chargingpoint->SetValue(0);
    if ((reply_data[15] * 256 + reply_data[16]) != 2047) {  //OBL showing only valid data while charging
      mt_nlg6_dc_current->SetValue((reply_data[15] * 256 + reply_data[16])/10.0);
    } else {
      mt_nlg6_dc_current->SetValue(0);
    }
    //Usable AmpsCode from Cable seem to be also a word with OBL as with NLG6?!
    mt_nlg6_amps_cablecode->SetValue((reply_data[5] * 256 + reply_data[6])/10.0);
    //NLG6AmpsCableCode = data[12]; //12
  }
  mt_nlg6_main_amps->SetElemValues(0, 3, NLG6MainsAmps);
  if (NLG6MainsAmps[0] != 0) StandardMetrics.ms_v_charge_current->SetValue(NLG6MainsAmps[0]);
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerSelCurrent(uint8_t* reply_data, uint16_t reply_len) {
  if(mt_nlg6_present->AsBool()){
    mt_nlg6_amps_setpoint->SetValue(reply_data[5]); //Get data for NLG6 fast charger
  } else {
    mt_nlg6_amps_setpoint->SetValue(reply_data[4]); //7 //Get data for standard OBL
  }
}

void OvmsVehicleSmartED::PollReply_NLG6_ChargerTemperatures(uint8_t* reply_data, uint16_t reply_len) {
  float NLG6Temps[9];
  int n;
  
  if (mt_nlg6_present->AsBool()){
    mt_nlg6_temp_coolingplate->SetValue((reply_data[1] < 0xFF) ? reply_data[1]-40 : 0);
    for(n = 0; n < 8; n++) {
      NLG6Temps[n] = (reply_data[n + 2] < 0xFF) ? reply_data[n + 2]-40 : 0;
    }
    mt_nlg6_temp_reported->SetValue((reply_data[9] < 0xFF) ? reply_data[9]-40 : 0);
    mt_nlg6_temp_socket->SetValue((reply_data[10] < 0xFF) ? reply_data[10]-40 : 0);
    mt_nlg6_temps->SetElemValues(0, 8, NLG6Temps);
  } else {
    mt_nlg6_temp_coolingplate->SetValue((reply_data[2] < 0xFF) ? reply_data[2]-40 : 0); //5
    mt_nlg6_temp_reported->SetValue((reply_data[4] < 0xFF) ? reply_data[4]-40 : 0); //7
    mt_nlg6_temp_socket->SetValue((reply_data[6] < 0xFF) ? reply_data[6]-40 : 0); //9
  }
  if (mt_nlg6_temp_reported->AsFloat() != 0) StandardMetrics.ms_v_charge_temp->SetValue(mt_nlg6_temp_reported->AsFloat());
}

