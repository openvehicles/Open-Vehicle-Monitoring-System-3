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
#include "vehicle_smarted.h"

static const char *TAG = "v-smarted";

#include <stdio.h>
#include "ovms_metrics.h"
#include "ovms_peripherals.h"


static const OvmsVehicle::poll_pid_t obdii_polls[] =
{
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0201, {  0,300,600 } }, // rqBattTemperatures
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0202, {  0,300,600 } }, // rqBattModuleTemperatures
  { 0x7E7, 0x7EF, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0208, {  0,300,600 } }, // rqBattVolts
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF111, {  0,120,999 } }, // rqChargerPN_HW
//  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF121, {  0,300,300 } }, // rqChargerSWrev
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0226, {  0,120,999 } }, // rqChargerVoltages
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0225, {  0,120,999 } }, // rqChargerAmps
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x022A, {  0,120,999 } }, // rqChargerSelCurrent
  { 0x61A, 0x483, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0223, {  0,120,999 } }, // rqChargerTemperatures
  { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
};

/**
 * Constructor & destructor
 */

OvmsVehicleSmartED::OvmsVehicleSmartED() {
    ESP_LOGI(TAG, "Start Smart ED vehicle module");
    
    memset(m_vin, 0, sizeof(m_vin));
    
    // init metrics:
    mt_vehicle_time = MyMetrics.InitInt("v.display.time", SM_STALE_MIN, 0);
    mt_trip_start = MyMetrics.InitInt("v.display.trip.start", SM_STALE_MIN, 0);
    mt_trip_reset = MyMetrics.InitInt("v.display.trip.reset", SM_STALE_MIN, 0);
    mt_hv_active = MyMetrics.InitBool("v.b.hv.active", SM_STALE_MIN, false);

    m_doorlock_port     = 9;
    m_doorunlock_port   = 8;
    m_ignition_port     = 7;
    m_range_ideal       = 135;
    m_egpio_timout      = 5;
    m_soc_rsoc          = false;
    
    m_candata_timer     = 0;
    m_candata_poll      = 0;
    m_egpio_timer       = 0;
    
    StandardMetrics.ms_v_charge_mode->SetValue("standard");
    StandardMetrics.ms_v_charge_type->SetValue("type2");
    StandardMetrics.ms_v_charge_limit_soc->SetValue(80);
    
    // BMS configuration:
    BmsSetCellArrangementVoltage(93, 1);
    BmsSetCellArrangementTemperature(3, 1);
    BmsSetCellLimitsVoltage(2.0, 5.0);
    BmsSetCellLimitsTemperature(-39, 200);
    BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
    BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
    
    RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
    PollSetPidList(m_can1,obdii_polls);
    PollSetState(0);
    
    MyConfig.RegisterParam("xse", "Smart ED", true, true);
    ConfigChanged(NULL);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
#endif
}

OvmsVehicleSmartED::~OvmsVehicleSmartED() {
    ESP_LOGI(TAG, "Stop Smart ED vehicle module");
    
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebDeInit();
#endif
}

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleSmartED::ConfigChanged(OvmsConfigParam* param) {
    if (param && param->GetName() != "xse")
        return;

    ESP_LOGI(TAG, "Smart ED reload configuration");
    
    m_doorlock_port = MyConfig.GetParamValueInt("xse", "doorlock.port", 9);
    m_doorunlock_port = MyConfig.GetParamValueInt("xse", "doorunlock.port", 8);
    m_ignition_port = MyConfig.GetParamValueInt("xse", "ignition.port", 7);
    
    m_range_ideal = MyConfig.GetParamValueInt("xse", "rangeideal", 135);
    m_egpio_timout = MyConfig.GetParamValueInt("xse", "egpio_timout", 5);
    m_soc_rsoc = MyConfig.GetParamValueBool("xse", "soc_rsoc", false);

#ifdef CONFIG_OVMS_COMP_MAX7317
    MyPeripherals->m_max7317->Output(m_doorlock_port, 0);
    MyPeripherals->m_max7317->Output(m_doorunlock_port, 0);
    MyPeripherals->m_max7317->Output(m_ignition_port, 0);
#endif
}

void OvmsVehicleSmartED::vehicle_smarted_car_on(bool isOn) {
  if (isOn && !StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned on
    ESP_LOGI(TAG,"CAR IS ON");
    PollSetState(2);

    // Reset trip values
    StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
    StandardMetrics.ms_v_bat_energy_used->SetValue(0);
  }
  else if (!isOn && StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned off
    ESP_LOGI(TAG,"CAR IS OFF");
    PollSetState(1);
  }

  // Always set this value to prevent it from going stale
  StandardMetrics.ms_v_env_on->SetValue(isOn);
}

/**
 * Update derived metrics when charging
 * Called once per 10 seconds from Ticker10
 */
void OvmsVehicleSmartED::HandleCharging() {
  float limit_soc       = StandardMetrics.ms_v_charge_limit_soc->AsFloat(0);
  float charge_current  = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);
  float charge_voltage  = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  
  // Are we charging?
  if (!StandardMetrics.ms_v_charge_pilot->AsBool()      ||
      !StandardMetrics.ms_v_charge_inprogress->AsBool() ||
      (charge_current <= 0.0) ) {
    return;
  }
  
  // Check if we have what is needed to calculate energy and remaining minutes
  if (charge_voltage > 0 && charge_current > 0) {
    // Update energy taken
    // Value is reset to 0 when a new charging session starts...
    float power  = charge_voltage * charge_current / 1000.0;     // power in kw
    float energy = power / 3600.0 * 10.0;                        // 10 second worth of energy in kwh's
    StandardMetrics.ms_v_charge_kwh->SetValue( StandardMetrics.ms_v_charge_kwh->AsFloat() + energy);

    // always calculate remaining charge time to full
    float full_soc           = 100.0;     // 100%
    int   minsremaining_full = calcMinutesRemaining(full_soc, charge_voltage, charge_current);

    StandardMetrics.ms_v_charge_duration_full->SetValue(minsremaining_full, Minutes);
    ESP_LOGV(TAG, "Time remaining: %d mins to full", minsremaining_full);
    
    if (limit_soc > 0) {
      // if limit_soc is set, then calculate remaining time to limit_soc
      int minsremaining_soc = calcMinutesRemaining(limit_soc, charge_voltage, charge_current);

      StandardMetrics.ms_v_charge_duration_soc->SetValue(minsremaining_soc, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", minsremaining_soc, limit_soc);
    }
  }
}

/**
 * Calculates minutes remaining before target is reached. Based on current charge speed.
 * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
 */
int OvmsVehicleSmartED::calcMinutesRemaining(float target_soc, float charge_voltage, float charge_current) {
  float bat_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
  if (bat_soc > target_soc) {
    return 0;   // Done!
  }
  float remaining_wh    = DEFAULT_BATTERY_CAPACITY * (target_soc - bat_soc) / 100.0;
  float remaining_hours = remaining_wh / (charge_current * charge_voltage);
  float remaining_mins  = remaining_hours * 60.0;

  return MIN( 1440, (int)remaining_mins );
}

/**
 * Update charging status
 */
void OvmsVehicleSmartED::HandleChargingStatus(bool status) {
  float charge_current  = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);
  float charge_voltage  = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  
  if (StandardMetrics.ms_v_door_chargeport->AsBool()) {
    if (status && charge_voltage > 50 && charge_current > 0) {
      if (!StandardMetrics.ms_v_charge_inprogress->AsBool()) {
        StandardMetrics.ms_v_charge_kwh->SetValue(0); // Reset charge kWh
      }
      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
    }
    else if (!status && !(charge_voltage > 50) && !(charge_current > 0)) {
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
    }
    else if (status && !(charge_voltage > 50) && !(charge_current > 0)) {
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
    }
  } else if (!(charge_voltage > 50) && !(charge_current > 0)) {
    StandardMetrics.ms_v_charge_pilot->SetValue(false);
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_state->SetValue("done");
    StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
  }
}

void OvmsVehicleSmartED::IncomingFrameCan1(CAN_frame_t* p_frame) {
    
  if (m_candata_poll != 1) {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    StandardMetrics.ms_v_env_awake->SetValue(true);
    m_candata_poll = 1;
    PollSetState(1);
  }
  m_candata_timer = SE_CANDATA_TIMEOUT;
  
  uint8_t *d = p_frame->data.u8;
  
  switch (p_frame->MsgID) {
    case 0x6FA: // Vehicle identification number
    {
      // CarVIN
      if (d[0]==1) for (int k = 0; k < 7; k++) m_vin[k] = d[k+1];
      else if (d[0] == 2) for (int k = 0; k < 7; k++) m_vin[k+7] = d[k+1];
      else if (d[0] == 3) {
        m_vin[14] = d[1];
        m_vin[15] = d[2];
        m_vin[16] = d[3];
        m_vin[17] = 0;
      }
      if ( (m_vin[0] != 0) && (m_vin[7] != 0) && (m_vin[14] != 0) ) {
        StandardMetrics.ms_v_vin->SetValue(m_vin);
      }
      break;
    }
    case 0x518: // displayed SOC
    {
      ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      /*
      ID:518 Nibble 15,16 (Wert halbieren)
      Beispiel:
      ID: 518  Data: 01 24 00 00 F8 7F C8 A7
      = A7 (in HEX)
      = 167 (in base10) und das nun halbieren
      = 83,5%
      
      0x518 [01] 4E 00 00 F8 7F C8 B4 //Pilotsignal von der Säule - Valide Ja/Nein
      Pilot valide = 01
      Pilot wird gemessen != 01 
      
      0x518 01 [4E] 00 00 F8 7F C8 B4 //Pilotsignal von der Säule - max lieferbarer Strom 
      4E = 78 Faktor 0,5 = 39 Ampere 
      
      0x518 01 4e 00 00 [f8 7f] c8 b4 // Lade dauer? 
      */
      if (m_soc_rsoc) {
        StandardMetrics.ms_v_bat_soh->SetValue((float) (d[7]/2));
      } else {
        StandardMetrics.ms_v_bat_soc->SetValue((float) (d[7]/2));
      }
      StandardMetrics.ms_v_charge_climit->SetValue(d[1]/2);
      HandleChargingStatus(d[1]!=0);
      break;
    }
    case 0x2D5: //realSOC
    {
      /*ID:2D5 Nibble 10,11,12 (Wert durch zehn)
       Beispiel:
       ID: 2D5  Data: 00 00 4F 14 03 40 00 00
       = 340 (in HEX)
       = 832 (in base10) und das nun durch zehn
       = 83,2% */
      if (m_soc_rsoc) {
        StandardMetrics.ms_v_bat_soc->SetValue(((float) ((d[4] & 0x03) * 256 + d[5])) / 10, Percentage);
      } else {
        StandardMetrics.ms_v_bat_soh->SetValue(((float) ((d[4] & 0x03) * 256 + d[5])) / 10, Percentage);
      }
      break;
    }
    case 0x508: //HV ampere and charging yes/no
    {
      //ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      float HVA = 0;
      HVA = (float) (d[2] & 0x3F) * 256 + (float) d[3];
      HVA = (HVA - 0x2000) / 10.0;
      StandardMetrics.ms_v_bat_current->SetValue(HVA, Amps);
      StandardMetrics.ms_v_door_chargeport->SetValue(d[2]&0x40);
      break;
    }
    case 0x448: //HV Voltage
    {
      float HV;
      float HVA = StandardMetrics.ms_v_bat_current->AsFloat();
      HV = ((float) d[6] * 256 + (float) d[7]);
      HV = HV / 10.0;
      float HVP = (HV * HVA) / 1000;
      StandardMetrics.ms_v_bat_voltage->SetValue(HV, Volts);
      StandardMetrics.ms_v_bat_power->SetValue(HVP);
      break;
    }
    case 0x3D5: //LV Voltage
    {
      float LV;
      LV = ((float) d[3]);
      LV = LV / 10.0;
      StandardMetrics.ms_v_bat_12v_voltage->SetValue(LV, Volts);
      break;
    }
    case 0x412: //ODO
    {
      /*ID:412 Nibble 7,8,9,10
       Beispiel:
       ID: 412  Data: 3B FF 00 63 5D 80 00 10
       = 635D (in HEX)
       = 25437 (in base10) Kilometer*/
      unsigned long ODO = (unsigned long) (d[2] * 65535 + (uint16_t) d[3] * 256
              + (uint16_t) d[4]);
      StandardMetrics.ms_v_pos_odometer->SetValue(ODO, Kilometers);
      break;
    }
    case 0x512: // system time? and departure time [0] = hour, [1]= minutes
    {
      uint32_t time = (d[0] * 60 + d[1]) * 60;
      mt_vehicle_time->SetValue(time, Seconds);
      time = (d[2] * 60 + d[3]) * 60;
      StandardMetrics.ms_v_charge_timerstart->SetValue(time, Seconds);
      break;
    }
    case 0x423: // Zündung,Türen,Fenster,Schloss, Licht, Blinker, Heckscheibenheizung
    {
      /*D0 is the state of the Ignition Key
      No Key is 0x40 in D0, Key on is 0x01

       D1 state of the Turn Lights
       D2 are the doors (0x00 all close, 0x01 driver open, 0x02 passenger open, etc)
       D3 state of the Headlamps, etc*/

      vehicle_smarted_car_on((d[0] & 0x01) > 0);
      //StandardMetrics.ms_v_env_on->SetValue((d[0] & 0x01) > 0);
      StandardMetrics.ms_v_door_fl->SetValue((d[2] & 0x01) > 0);
      StandardMetrics.ms_v_door_fr->SetValue((d[2] & 0x02) > 0);
      StandardMetrics.ms_v_door_trunk->SetValue((d[2] & 0x04) > 0);
      StandardMetrics.ms_v_env_headlights->SetValue((d[3] & 0x02) > 0);
      //StandardMetrics.ms_v_env_locked->SetBValue();
      break;
    }
    case 0x200: // hand brake and speed
    {
      StandardMetrics.ms_v_env_handbrake->SetValue(d[0]);
      float velocity = (d[2] * 256 + d[3]) / 18;
      StandardMetrics.ms_v_pos_speed->SetValue(velocity, Kph);
      CalculateAcceleration();
      break;
    }
    case 0x236: // paddels recu up and down
    {
      break;
    }
    case 0x318: // range and powerbar
    {
      float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
      if(soc > 0) {
          float smart_range_ideal = (m_range_ideal * soc) / 100;
          StandardMetrics.ms_v_bat_range_ideal->SetValue(smart_range_ideal); // ToDo
      }
      StandardMetrics.ms_v_bat_range_est->SetValue(d[7], Kilometers);
      StandardMetrics.ms_v_env_throttle->SetValue(d[5]);
      break;
    }
    case 0x443: //air condition and fan
    {
      StandardMetrics.ms_v_env_hvac->SetValue(d[2] & 0xC8);
      break;
    }
    case 0x3F2: //Eco display
    {
      /*myMetrics.->ECO_accel                             = rxBuf[0] >> 1;
       myMetrics.->ECO_const                               = rxBuf[1] >> 1;
       myMetrics.->ECO_coast                               = rxBuf[2] >> 1;
       myMetrics.->ECO_total                               = rxBuf[3] >> 1;
       */
      break;
    }
    case 0x418: //gear shift
    {
      switch(d[4]) {
        case 0xDD: // Parking
          StandardMetrics.ms_v_env_gear->SetValue(0);
          break;
        case 0xBB: // Rear
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          break;
        case 0x00: // Neutral
          StandardMetrics.ms_v_env_gear->SetValue(0);
          break;
        case 0x11: // Drive
          StandardMetrics.ms_v_env_gear->SetValue(1);
          break;
      }
      break;
    }
    case 0x408: //temp outdoor
    {
      float TEMPERATUR = ((d[4] - 50 ) - 32 ) / 1.8; 
      StandardMetrics.ms_v_env_temp->SetValue(TEMPERATUR);
      break;
    }
    case 0x3CE: //Verbrauch ab Start und ab Reset
    {
      float energy_used = (d[0] * 256 + d[1]) / 100;
      float energy_recd = (d[2] * 256 + d[3]) / 100;
      
      StandardMetrics.ms_v_bat_energy_used->SetValue(energy_used);
      StandardMetrics.ms_v_bat_energy_recd->SetValue(energy_recd);
      break;
    }
    case 0x504: //Strecke ab Start und ab Reset
    {
      uint16_t value = d[1] * 256 + d[2];
      if (value != 254) {
        mt_trip_start->SetValue(value);
      }
      value = d[4] * 256 + d[5];
      if (value != 254) {
        mt_trip_reset->SetValue(value);
      }
      break;
    }
    case 0x3D7: //HV Status
    {   //HV active
      mt_hv_active->SetValue(d[0]);
      break;
    }
    case 0x312: //Powerflow von/zum Motor 
    {
      /*
      0x312 08 21 00 00 07 D0 07 D0
      Benötigt wird Byte 0 und 1, interpretiert als eine Zahl.
      (0x0821 - 2000) * 0.2 = 14,6% 
      */
      break;
    }
  }
}

void OvmsVehicleSmartED::Ticker1(uint32_t ticker) {
  if (m_candata_timer > 0) {
    if (--m_candata_timer == 0) {
      // Car has gone to sleep
      ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
      StandardMetrics.ms_v_env_awake->SetValue(false);
      PollSetState(0);
      m_candata_poll = 0;
    }
  }
}

void OvmsVehicleSmartED::Ticker10(uint32_t ticker) {
  HandleCharging();
}

void OvmsVehicleSmartED::Ticker60(uint32_t ticker) {
#ifdef CONFIG_OVMS_COMP_MAX7317
  if (m_egpio_timer > 0) {
    if (--m_egpio_timer == 0) {
      ESP_LOGI(TAG,"Ignition EGPIO off port: %d", m_ignition_port);
      MyPeripherals->m_max7317->Output(m_ignition_port, 0);
      StandardMetrics.ms_v_env_valet->SetValue(false);
    }
  }
#endif
}


OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandClimateControl(bool enable) {
    return CommandSetChargeTimer(enable, mt_vehicle_time->AsInt());
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandSetChargeTimer(bool timeron, uint32_t timerstart) {
    //Set the charge start time as seconds since midnight or 8 Bit hour and 8 bit minutes?
    /*Mit
     0x512 00 00 12 1E 00 00 00 00
     setzt man z.B. die Uhrzeit auf 18:30. Maskiert man nun Byte 3 (0x12) mit 0x40 (und setzt so dort das zweite Bit auf High) wird die A/C Funktion mit aktiviert.
    */
    if(timerstart == 0) { 
        return Fail;
    }
    if(!StandardMetrics.ms_v_env_awake->AsBool()) {
        return Fail;
    }
    int t = timerstart + 600; // mt_vehicle_time + 10 min
    //int days = (t / 86400);
    //t = t - (days * 86400);
    int hours = (t / 3600);
    t = t - (hours * 3600);
    int minutes = (t / 60);
    minutes = (minutes - (minutes % 5));
    
    ESP_LOGI(TAG,"%d:%d", hours, minutes);
    
    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 8;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x512;
    frame.data.u8[0] = 0x00;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = (hours & 0xff);
    if (timeron) {
        frame.data.u8[3] = (minutes & 0xff) + 0x40; //(timerstart >> 8) & 0xff;
    } else {
        frame.data.u8[3] = (minutes & 0xff); //((timerstart >> 8) & 0xff) + 0x40;
    }
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = 0x00;
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    m_can1->Write(&frame);
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandSetChargeCurrent(uint16_t limit) {
    /*Den Ladestrom ändert man mit
     0x512 00 00 1F FF 00 7C 00 00 auf 12A.
     8A = 0x74, 12A = 0x7C and 32A = 0xA4 (als max. bei 22kW).
     Also einen Offset berechnen 0xA4 = d164: z.B. 12A = 0xA4 - 0x7C = 0x28 = d40 -> vom Maximum (0xA4 = 32A) abziehen
     und dann durch zwei dividieren, um auf den Wert für 20A zu kommen (mit 0,5A Auflösung).
    */

    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 8;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x512;
    frame.data.u8[0] = 0x00;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = 0x1F;
    frame.data.u8[3] = 0xFF;
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = (uint8_t) (100 + 2 * limit);
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    m_can1->Write(&frame);

    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandHomelink(int button, int durationms) {
    bool enable;
    if (button == 0) {
        enable = true;
        return CommandSetChargeTimer(enable, mt_vehicle_time->AsInt());
    }
    if (button == 1) {
        enable = false;
        return CommandSetChargeTimer(enable, mt_vehicle_time->AsInt());
    }
    return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandWakeup() {
    /*So we still have to get the car to wake up. Can someone please test these two queries when the car is asleep:
    0x218 00 00 00 00 00 00 00 00 or
    0x210 00 00 00 01 00 00 00 00
    Both patterns should comply with the Bosch CAN bus spec. for waking up a sleeping bus with recessive bits.
    */

    ESP_LOGI(TAG, "Send Wakeup Command");
    
    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 4;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x423;
    frame.data.u8[0] = 0x01;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = 0x00;
    frame.data.u8[3] = 0x00;
    m_can1->Write(&frame);

    return Success;
}

#ifdef CONFIG_OVMS_COMP_MAX7317
void SmartEDLockingTimer(TimerHandle_t timer) {
    xTimerStop(timer, 0);
    xTimerDelete(timer, 0);
    //reset GEP 1 + 2
    int m_doorlock_port   = MyConfig.GetParamValueInt("xse", "doorlock.port", 9);
    int m_doorunlock_port = MyConfig.GetParamValueInt("xse", "doorunlock.port", 8);
    
    MyPeripherals->m_max7317->Output(m_doorlock_port, 0);
    MyPeripherals->m_max7317->Output(m_doorunlock_port, 0);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandLock(const char* pin) {
    //switch 12v to GEP 1
    MyPeripherals->m_max7317->Output(m_doorlock_port, 1);
    m_locking_timer = xTimerCreate("Smart ED Locking Timer", 500 / portTICK_PERIOD_MS, pdTRUE, this, SmartEDLockingTimer);
    xTimerStart(m_locking_timer, 0);
    StandardMetrics.ms_v_env_locked->SetValue(true);
    return Success;
    //return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandUnlock(const char* pin) {
    //switch 12v to GEP 2 
    MyPeripherals->m_max7317->Output(m_doorunlock_port, 1);
    m_locking_timer = xTimerCreate("Smart ED Locking Timer", 500 / portTICK_PERIOD_MS, pdTRUE, this, SmartEDLockingTimer);
    xTimerStart(m_locking_timer, 0);
    StandardMetrics.ms_v_env_locked->SetValue(false);
    return Success;
    //return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandActivateValet(const char* pin) {
    ESP_LOGI(TAG,"Ignition EGPIO on port: %d", m_ignition_port);
    MyPeripherals->m_max7317->Output(m_ignition_port, 1);
    m_egpio_timer = m_egpio_timout;
    StandardMetrics.ms_v_env_valet->SetValue(true);
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandDeactivateValet(const char* pin) {
    ESP_LOGI(TAG,"Ignition EGPIO off port: %d", m_ignition_port);
    MyPeripherals->m_max7317->Output(m_ignition_port, 0);
    m_egpio_timer = 0;
    StandardMetrics.ms_v_env_valet->SetValue(false);
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandStat(int verbosity, OvmsWriter* writer) {
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  if (chargeport_open)
    {
    std::string charge_mode = StdMetrics.ms_v_charge_mode->AsString();
    std::string charge_state = StdMetrics.ms_v_charge_state->AsString();
    bool show_details = !(charge_state == "done" || charge_state == "stopped");

    // Translate mode codes:
    if (charge_mode == "standard")
      charge_mode = "Standard";
    else if (charge_mode == "storage")
      charge_mode = "Storage";
    else if (charge_mode == "range")
      charge_mode = "Range";
    else if (charge_mode == "performance")
      charge_mode = "Performance";

    // Translate state codes:
    if (charge_state == "charging")
      charge_state = "Charging";
    else if (charge_state == "topoff")
      charge_state = "Topping off";
    else if (charge_state == "done")
      charge_state = "Charge Done";
    else if (charge_state == "preparing")
      charge_state = "Preparing";
    else if (charge_state == "heating")
      charge_state = "Charging, Heating";
    else if (charge_state == "stopped")
      charge_state = "Charge Stopped";

    writer->printf("%s - %s\n", charge_mode.c_str(), charge_state.c_str());

    if (show_details)
      {
      writer->printf("%s/%s\n",
        (char*) StdMetrics.ms_v_charge_voltage->AsUnitString("-", Native, 1).c_str(),
        (char*) StdMetrics.ms_v_charge_current->AsUnitString("-", Native, 1).c_str());

      int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
      if (duration_full > 0)
        writer->printf("Full: %d mins\n", duration_full);

      int duration_soc = StdMetrics.ms_v_charge_duration_soc->AsInt();
      if (duration_soc > 0)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_soc->AsUnitString("SOC", Native, 0).c_str(),
          duration_soc);

      int duration_range = StdMetrics.ms_v_charge_duration_range->AsInt();
      if (duration_range > 0)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_range->AsUnitString("Range", rangeUnit, 0).c_str(),
          duration_range);
      }
    }
  else
    {
    writer->puts("Not charging");
    }

  writer->printf("SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());

  const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_ideal != '-')
    writer->printf("Ideal range: %s\n", range_ideal);

  const char* range_est = StdMetrics.ms_v_bat_range_est->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_est != '-')
    writer->printf("Est. range: %s\n", range_est);

  const char* chargedkwh = StdMetrics.ms_v_charge_kwh->AsUnitString("-", Native, 2).c_str();
  if (*chargedkwh != '-')
    writer->printf("Energy charged: %s\n", chargedkwh);

  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf("ODO: %s\n", odometer);

  const char* cac = StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str();
  if (*cac != '-')
    writer->printf("CAC: %s\n", cac);

  const char* soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 0).c_str();
  if (*soh != '-')
    writer->printf("SOH: %s\n", soh);

  return Success;
}
#endif

class OvmsVehicleSmartEDInit {
    public:
    OvmsVehicleSmartEDInit();
} MyOvmsVehicleSmartEDInit __attribute__ ((init_priority (9000)));

OvmsVehicleSmartEDInit::OvmsVehicleSmartEDInit() {
    ESP_LOGI(TAG, "Registering Vehicle: SMART ED (9000)");
    MyVehicleFactory.RegisterVehicle<OvmsVehicleSmartED>("SE", "Smart ED");
}

