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
static const char *TAG = "v-smarted";

#define VERSION "1.0.0"

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

#include "vehicle_smarted.h"


static const OvmsVehicle::poll_pid_t obdii_polls[] =
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

/**
 * Constructor & destructor
 */

OvmsVehicleSmartED::OvmsVehicleSmartED() {
    ESP_LOGI(TAG, "Start Smart ED vehicle module");
    
    memset(m_vin, 0, sizeof(m_vin));
    
    // init metrics:
    mt_vehicle_time          = MyMetrics.InitInt("xse.v.display.time", SM_STALE_MIN, 0);
    mt_trip_start            = MyMetrics.InitInt("xse.v.display.trip.start", SM_STALE_MIN, 0);
    mt_trip_reset            = MyMetrics.InitInt("xse.v.display.trip.reset", SM_STALE_MIN, 0);
    mt_hv_active             = MyMetrics.InitBool("xse.v.b.hv.active", SM_STALE_MIN, false);
    mt_c_active              = MyMetrics.InitBool("xse.v.c.active", SM_STALE_MIN, false);
    mt_bat_energy_used_start = MyMetrics.InitFloat("xse.v.b.energy.used.start", SM_STALE_MID, kWh);
    mt_bat_energy_used_reset = MyMetrics.InitFloat("xse.v.b.energy.used.reset", SM_STALE_MID, kWh);
    mt_pos_odometer_start    = MyMetrics.InitFloat("xse.v.pos.odometer.start", SM_STALE_MID, Kilometers);

    mt_nlg6_present             = MyMetrics.InitBool("xse.v.nlg6.present", SM_STALE_MIN, false);
    mt_nlg6_main_volts          = new OvmsMetricVector<float>("xse.v.nlg6.main.volts", SM_STALE_HIGH, Volts);
    mt_nlg6_main_amps           = new OvmsMetricVector<float>("xse.v.nlg6.main.amps", SM_STALE_HIGH, Amps);
    mt_nlg6_amps_setpoint       = MyMetrics.InitFloat("xse.v.nlg6.amps.setpoint", SM_STALE_MIN, Amps);
    mt_nlg6_amps_cablecode      = MyMetrics.InitFloat("xse.v.nlg6.amps.cablecode", SM_STALE_MIN, Amps);
    mt_nlg6_amps_chargingpoint  = MyMetrics.InitFloat("xse.v.nlg6.amps.chargingpoint", SM_STALE_MIN, Amps);
    mt_nlg6_dc_current          = MyMetrics.InitFloat("xse.v.nlg6.dc.current", SM_STALE_MIN, Volts);
    mt_nlg6_dc_hv               = MyMetrics.InitFloat("xse.v.nlg6.dc.hv", SM_STALE_MIN, Volts);
    mt_nlg6_dc_lv               = MyMetrics.InitFloat("xse.v.nlg6.dc.lv", SM_STALE_MIN, Volts);
    mt_nlg6_temps               = new OvmsMetricVector<float>("xse.v.nlg6.temps", SM_STALE_HIGH, Celcius);
    mt_nlg6_temp_reported       = MyMetrics.InitFloat("xse.v.nlg6.temp.reported", SM_STALE_MIN, Celcius);
    mt_nlg6_temp_socket         = MyMetrics.InitFloat("xse.v.nlg6.temp.socket", SM_STALE_MIN, Celcius);
    mt_nlg6_temp_coolingplate   = MyMetrics.InitFloat("xse.v.nlg6.temp.coolingplate", SM_STALE_MIN, Celcius);
    mt_nlg6_pn_hw               = MyMetrics.InitString("xse.v.nlg6.pn.hw", SM_STALE_MIN, 0);
    
    mt_pos_odometer_start->SetValue(0);

    m_doorlock_port     = 9;
    m_doorunlock_port   = 8;
    m_ignition_port     = 7;
    m_doorstatus_port   = 6;
    m_range_ideal       = 135;
    m_egpio_timout      = 5;
    m_soc_rsoc          = false;
    
    m_candata_timer     = 0;
    m_candata_poll      = 0;
    m_egpio_timer       = 0;
    
    // init commands:
    cmd_xse = MyCommandApp.RegisterCommand("xse","SmartED 451 Gen.3");
    cmd_xse->RegisterCommand("recu","Set recu..", xse_recu, "<up/down>",1,1);
    cmd_xse->RegisterCommand("charge","Set charging Timer..", xse_chargetimer, "<hour> <minutes>", 2, 2);
    
    // BMS configuration:
    BmsSetCellArrangementVoltage(93, 1);
    BmsSetCellArrangementTemperature(3, 1);
    BmsSetCellLimitsVoltage(2.0, 5.0);
    BmsSetCellLimitsTemperature(-39, 200);
    BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
    BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
    
    RestoreStatus();
    
    RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
    RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
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
    
    m_doorlock_port   = MyConfig.GetParamValueInt("xse", "doorlock.port", 9);
    m_doorunlock_port = MyConfig.GetParamValueInt("xse", "doorunlock.port", 8);
    m_ignition_port   = MyConfig.GetParamValueInt("xse", "ignition.port", 7);
    m_doorstatus_port = MyConfig.GetParamValueInt("xse", "doorstatus.port", 6);
    
    m_range_ideal     = MyConfig.GetParamValueInt("xse", "rangeideal", 135);
    m_egpio_timout    = MyConfig.GetParamValueInt("xse", "egpio_timout", 5);
    m_soc_rsoc        = MyConfig.GetParamValueBool("xse", "soc_rsoc", false);
    
    m_enable_write    = MyConfig.GetParamValueBool("xse", "canwrite", false);
    m_lock_state      = MyConfig.GetParamValueBool("xse", "lockstate", false);
    
    StandardMetrics.ms_v_charge_limit_soc->SetValue((float) MyConfig.GetParamValueInt("xse", "suffsoc", 0), Percentage );
    StandardMetrics.ms_v_charge_limit_range->SetValue((float) MyConfig.GetParamValueInt("xse", "suffrange", 0), Kilometers );

#ifdef CONFIG_OVMS_COMP_MAX7317
    MyPeripherals->m_max7317->Output(m_doorlock_port, 0);
    MyPeripherals->m_max7317->Output(m_doorunlock_port, 0);
    MyPeripherals->m_max7317->Output(m_ignition_port, 0);
    MyPeripherals->m_max7317->Output((uint8_t)m_doorstatus_port,(uint8_t)1); // set port to input
#endif
}

void OvmsVehicleSmartED::vehicle_smarted_car_on(bool isOn) {
  if (isOn && !StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned on
    ESP_LOGI(TAG,"CAR IS ON");
    if (m_enable_write) PollSetState(2);

    // Reset trip values
    //StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
    //StandardMetrics.ms_v_bat_energy_used->SetValue(0);
  }
  else if (!isOn && StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned off
    ESP_LOGI(TAG,"CAR IS OFF");
    if (m_enable_write) PollSetState(1);
    SaveStatus();
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
  float limit_range     = StandardMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers);
  float max_range       = StandardMetrics.ms_v_bat_range_full->AsFloat(0, Kilometers);
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
    if (limit_range > 0 && max_range > 0.0) {
      // if range limit is set, then compute required soc and then calculate remaining time to that soc
      float range_soc           = limit_range / max_range * 100.0;
      int   minsremaining_range = calcMinutesRemaining(range_soc, charge_voltage, charge_current);

      StandardMetrics.ms_v_charge_duration_range->SetValue(minsremaining_range, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins for %0.0f km (%0.0f%% soc)", minsremaining_range, limit_range, range_soc);
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

void OvmsVehicleSmartED::calcBusAktivity(bool state, uint8_t pos) {
  static uint8_t counter_a = 0;
  static uint8_t counter_b = 0;
  
  switch (pos) {
    case 1:
      if (state != StandardMetrics.ms_v_door_chargeport->AsBool()) {
        counter_a++;
      } else {
        counter_a = 0;
      }
      if (counter_a == 5) {
        StandardMetrics.ms_v_door_chargeport->SetValue(state);
        counter_a = 0;
      }
    break;
    case 2:
      if (state != mt_c_active->AsBool()) {
        counter_b++;
      } else {
        counter_b = 0;
      }
      if (counter_b == 5) {
        mt_c_active->SetValue(state);
        counter_b = 0;
      }
    break;
  }
}

/**
 * Update charging status
 */
void OvmsVehicleSmartED::HandleChargingStatus() {
  bool port   = StandardMetrics.ms_v_door_chargeport->AsBool();
  bool status = mt_c_active->AsBool();
  static bool isCharging = false;
  
  if (port) {
    if (status) {
      // The car is charging
      //StandardMetrics.ms_v_env_charging12v->SetValue(true);
      if (!StandardMetrics.ms_v_charge_inprogress->AsBool()) {
        if (!isCharging) {
          isCharging = true;
          // Reset charge kWh
          StandardMetrics.ms_v_charge_kwh->SetValue(0);
          // Reset trip values
          StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
          StandardMetrics.ms_v_bat_energy_used->SetValue(0);
          mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
          StandardMetrics.ms_v_pos_trip->SetValue(0);
        }
        
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_type->SetValue("type2");
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      }
    } else {
      // The car is not charging
      if (StandardMetrics.ms_v_charge_inprogress->AsBool()) {
        // The charge has completed/stopped
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_type->SetValue("type2");
        if (StandardMetrics.ms_v_bat_soc->AsInt() < 95) {
          // Assume the charge was interrupted
          ESP_LOGI(TAG,"Car charge session was interrupted");
          StandardMetrics.ms_v_charge_state->SetValue("stopped");
          StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
        } else {
          // Assume the charge completed normally
          ESP_LOGI(TAG,"Car charge session completed");
          StandardMetrics.ms_v_charge_state->SetValue("done");
          StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        }
      }
      //StandardMetrics.ms_v_env_charging12v->SetValue(false);
    }
  } else if (isCharging) {
    isCharging = false;
    StandardMetrics.ms_v_charge_state->SetValue("done");
  }
}

void OvmsVehicleSmartED::IncomingFrameCan1(CAN_frame_t* p_frame) {
    
  if (m_candata_poll != 1 && mt_hv_active) {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    StandardMetrics.ms_v_env_awake->SetValue(true);
    m_candata_poll = 1;
    //PollSetState(1);
    if (m_enable_write) PollSetState(1);
  }
  
  m_candata_timer = SE_CANDATA_TIMEOUT;
  
  uint8_t *d = p_frame->data.u8;
  
  switch (p_frame->MsgID) {
    case 0x168:
    {
      bool active;
      if (d[7] == 0x20 || d[7] == 0x40)
        active = true;
      else
        active = false;
      calcBusAktivity(active, 2);
      break;
    }
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
      //ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      /*
      ID:518 Nibble 15,16 (Cut the value in half)
      example:
      ID: 518  Data: 01 24 00 00 F8 7F C8 A7
      = A7 (in HEX)
      = 167 (in base10) and halve that now
      = 83,5%
      
      0x518 [01] 4E 00 00 F8 7F C8 B4 //Pilot signal from the column - valid Yes / No
      Pilot valid = 01
      Pilot is measured != 01 
      
      0x518 01 [4E] 00 00 F8 7F C8 B4 //Pilot signal from the column - max. Supply current
      4E = 78 factor 0,5 = 39 Ampere 
      
      0x518 01 4e 00 00 [f8 7f] c8 b4 // Charge duration? 
      */
      if (m_soc_rsoc) {
        StandardMetrics.ms_v_bat_soh->SetValue((float) (d[7]/2.0));
      } else {
        StandardMetrics.ms_v_bat_soc->SetValue((float) (d[7]/2.0));
      }
      StandardMetrics.ms_v_charge_climit->SetValue(d[1]/2.0);
      break;
    }
    case 0x2D5: //realSOC
    {
      /*ID:2D5 Nibble 10,11,12 (Value by ten)
       example:
       ID: 2D5  Data: 00 00 4F 14 03 40 00 00
       = 340 (in HEX)
       = 832 (in base10) and now by ten
       = 83,2% */
      float rsoc = ((d[4] & 0x03) * 256 + d[5]) / 10.0;
      if (m_soc_rsoc) {
        StandardMetrics.ms_v_bat_soc->SetValue(rsoc, Percentage);
      } else {
        StandardMetrics.ms_v_bat_soh->SetValue(rsoc, Percentage);
      }
      StandardMetrics.ms_v_bat_cac->SetValue((DEFAULT_BATTERY_AMPHOURS * rsoc) / 100, AmpHours);
      break;
    }
    case 0x508: //HV ampere and charging yes/no
    {
      //ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      float HVA = 0;
      HVA = (float) (d[2] & 0x3F) * 256 + (float) d[3];
      HVA = (HVA - 0x2000) / 10.0;
      StandardMetrics.ms_v_bat_current->SetValue(HVA, Amps);
      
      calcBusAktivity((d[2]&0x40), 1);
      //StandardMetrics.ms_v_door_chargeport->SetValue((d[2]&0x40));
      break;
    }
    case 0x448: //HV Voltage
    {
      float HV;
      float HVA = StandardMetrics.ms_v_bat_current->AsFloat();
      HV = ((float) d[6] * 256 + (float) d[7]);
      HV = HV / 10.0;
      float HVP = (HV * HVA) / 1000.0;
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
       Example:
       ID: 412  Data: 3B FF 00 63 5D 80 00 10
       = 635D (in HEX)
       = 25437 (in base10) kilometre*/
      float ODO = (float) (d[2] * 65535 + (uint16_t) d[3] * 256 + (uint16_t) d[4]);
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
    case 0x423: // Ignition, doors, windows, lock, lights, turn signals, rear window heating
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
      //StandardMetrics.ms_v_env_locked->SetValue();
      break;
    }
    case 0x200: // hand brake and speed
    {
      StandardMetrics.ms_v_env_handbrake->SetValue(d[0]);
      float velocity = (d[2] * 256 + d[3]) / 18.0;
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
          float smart_range_ideal = (m_range_ideal * soc) / 100.0;
          StandardMetrics.ms_v_bat_range_ideal->SetValue(smart_range_ideal); // ToDo
          StandardMetrics.ms_v_bat_range_full->SetValue((float) (d[7] / soc) * 100.0, Kilometers); // ToDo
      }
      StandardMetrics.ms_v_bat_range_est->SetValue(d[7], Kilometers);
      StandardMetrics.ms_v_env_throttle->SetValue(d[5]);
      break;
    }
    case 0x443: //air condition and fan
    {
      if(StandardMetrics.ms_v_env_cooling->AsBool() || d[2] > 0)
        StandardMetrics.ms_v_env_hvac->SetValue(true);
      else if(StandardMetrics.ms_v_env_on->AsBool() && (d[0] & 0x80))
        StandardMetrics.ms_v_env_hvac->SetValue(true);
      else StandardMetrics.ms_v_env_hvac->SetValue(false);
      StandardMetrics.ms_v_env_heating->SetValue(d[2] > 0);
      break;
    }
    case 0x452: // Temp Airflow
    {
      StandardMetrics.ms_v_env_cooling->SetValue(d[0] > 0);
      StandardMetrics.ms_v_env_cabintemp->SetValue(d[0]*0.5);
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
    case 0x3CE: //Consumption from start and from reset
    {
      float energy_start = (d[0] * 256 + d[1]) / 100.0;
      float energy_reset = (d[2] * 256 + d[3]) / 100.0;
      
      mt_bat_energy_used_start->SetValue(energy_start);
      mt_bat_energy_used_reset->SetValue(energy_reset);
      break;
    }
    case 0x504: //Distance from start and from reset
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
    case 0x312: //Powerflow from/to Engine 
    {
      /*
      0x312 08 21 00 00 07 D0 07 D0
      Requires bytes 0 and 1, interpreted as a number.
      (0x0821 - 2000) * 0.2 = 14,6% 
      */
      break;
    }
  }
}

void OvmsVehicleSmartED::IncomingFrameCan2(CAN_frame_t* p_frame) {
  //uint8_t *d = p_frame->data.u8;
  
  switch (p_frame->MsgID) {
    
  }
}
/**
 * Update derived energy metrics while driving
 * Called once per second from Ticker1
 */
void OvmsVehicleSmartED::HandleEnergy() {
  float voltage  = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float current  = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);

  // Power (in kw) resulting from voltage and current
  float power = voltage * current / 1000.0;

  // Are we driving?
  if (power != 0.0 && StandardMetrics.ms_v_env_on->AsBool()) {
    // Update energy used and recovered
    float energy = power / 3600.0;    // 1 second worth of energy in kwh's
    if (energy < 0.0f)
      StandardMetrics.ms_v_bat_energy_used->SetValue( StandardMetrics.ms_v_bat_energy_used->AsFloat() - energy);
    else // (energy > 0.0f)
      StandardMetrics.ms_v_bat_energy_recd->SetValue( StandardMetrics.ms_v_bat_energy_recd->AsFloat() + energy);
  }
}

void OvmsVehicleSmartED::Ticker1(uint32_t ticker) {
  if (m_candata_timer > 0) {
    if (--m_candata_timer == 0) {
      // Car has gone to sleep
      ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
      StandardMetrics.ms_v_env_awake->SetValue(false);
      StandardMetrics.ms_v_env_hvac->SetValue(false);
      if (m_enable_write) PollSetState(0);
      m_candata_poll = 0;
    }
  }
  HandleEnergy();
  if (StandardMetrics.ms_v_env_awake->AsBool())
    HandleChargingStatus();
  
  // Handle Tripcounter
  if (mt_pos_odometer_start->AsFloat(0) == 0 && StandardMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0) {
    mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
  }
  if (StandardMetrics.ms_v_env_on->AsBool() && StandardMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0 && mt_pos_odometer_start->AsFloat(0) > 0.0) {
    StandardMetrics.ms_v_pos_trip->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start->AsFloat(0));
  }
}

void OvmsVehicleSmartED::Ticker10(uint32_t ticker) {
  HandleCharging();
#ifdef CONFIG_OVMS_COMP_MAX7317
  if(m_lock_state) {
    int level = MyPeripherals->m_max7317->Input((uint8_t)m_doorstatus_port);
    StandardMetrics.ms_v_env_locked->SetValue(level == 1 ? false : true);
  }
  if (m_egpio_timer > 0 && StandardMetrics.ms_v_door_fl->AsBool()) {
    MyPeripherals->m_max7317->Output(m_ignition_port, 0);
    StandardMetrics.ms_v_env_valet->SetValue(false);
  }
#endif
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

/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleSmartED::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 10:
      MyConfig.SetParamValue("xse", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("xse", "suffrange", value);
      return true;
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xse", "canwrite",  (bits& 1)!=0);
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
const std::string OvmsVehicleSmartED::GetFeature(int key)
{
  switch (key)
  {
    case 10:
      return MyConfig.GetParamValue("xse", "suffsoc", STR(0));
    case 11:
      return MyConfig.GetParamValue("xse", "suffrange", STR(0));
    case 15:
    {
      int bits = ( MyConfig.GetParamValueBool("xse", "canwrite",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
  }
}

bool OvmsVehicleSmartED::CommandSetRecu(bool on) {
  if(!m_enable_write)
    return false;
  
  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x236;
  frame.data.u8[0] = (on == true ? 0x02 : 0x04);
  m_can1->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can1->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can1->Write(&frame);

  return true;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandSetChargeCurrent(uint16_t limit) {
    /*The charging current changes with
     0x512 00 00 1F FF 00 7C 00 00 auf 12A.
     8A = 0x74, 12A = 0x7C and 32A = 0xA4 (as max. at 22kW).
     So calculate an offset 0xA4 = d164: e.g. 12A = 0xA4 - 0x7C = 0x28 = d40 -> subtract from the maximum (0xA4 = 32A)
      then divide by two to get the value for 20A (with 0.5A resolution).
    */
    if(!m_enable_write)
      return Fail;
    
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

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandWakeup() {
    /*So we still have to get the car to wake up. Can someone please test these two queries when the car is asleep:
    0x218 00 00 00 00 00 00 00 00 or
    0x210 00 00 00 01 00 00 00 00
    Both patterns should comply with the Bosch CAN bus spec. for waking up a sleeping bus with recessive bits.
    0x423 01 00 00 00 will wake up the CAN bus but not the right on.
    */
    if(!m_enable_write)
      return Fail;
    
    ESP_LOGI(TAG, "Send Wakeup Command");
    
    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can2;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 4;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x210;
    frame.data.u8[0] = 0x00;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = 0x00;
    frame.data.u8[3] = 0x00;
    m_can2->Write(&frame);

    return Success;
}

#ifdef CONFIG_OVMS_COMP_MAX7317
OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandClimateControl(bool enable) {
  time_t rawtime;
  time ( &rawtime );
  struct tm* tmu = localtime(&rawtime);
  int hours = tmu->tm_hour;
  int minutes = tmu->tm_min;
  minutes = (minutes - (minutes % 5));
  return CommandSetChargeTimer(enable, hours, minutes);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandSetChargeTimer(bool timeron, int hours, int minutes) {
    //Set the charge start time as seconds since midnight or 8 Bit hour and 8 bit minutes?
    /*With
     0x512 00 00 12 1E 00 00 00 00
     if one sets e.g. the time at 18:30. If you now mask byte 3 (0x12) with 0x40 (and set the second bit to high there), the A / C function is also activated.
    */
    if(!m_enable_write)
      return Fail;
    
    // Try first Wakeup can if car sleep
    if(!StandardMetrics.ms_v_env_awake->AsBool()) {
      CommandWakeup();
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    // Try unlock/lock doors to Wakeup Can
    if(!StandardMetrics.ms_v_env_awake->AsBool()) {
      if (!MyConfig.IsDefined("password","pin")) return Fail;
      
      std::string vpin = MyConfig.GetParamValue("password","pin");
      CommandUnlock(vpin.c_str());
      vTaskDelay(600 / portTICK_PERIOD_MS);
      CommandLock(vpin.c_str());
      vTaskDelay(600 / portTICK_PERIOD_MS);
      if(!StandardMetrics.ms_v_env_awake->AsBool()) return Fail;
    }
    
    ESP_LOGI(TAG,"ClimaStartTime: %d:%d", hours, minutes);
    
    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 8;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x512;
    frame.data.u8[0] = 0x00;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = ((uint8_t) hours & 0xff);
    if (timeron) {
        frame.data.u8[3] = ((uint8_t) minutes & 0xff) + 0x40; //(timerstart >> 8) & 0xff;
    } else {
        frame.data.u8[3] = ((uint8_t) minutes & 0xff); //((timerstart >> 8) & 0xff) + 0x40;
    }
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = 0x00;
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    m_can1->Write(&frame);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    m_can1->Write(&frame);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    m_can1->Write(&frame);
    
    ESP_LOGI(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", frame.MsgID, frame.data.u8[0], frame.data.u8[1], frame.data.u8[2], frame.data.u8[3], frame.data.u8[4], frame.data.u8[5], frame.data.u8[6], frame.data.u8[7]);
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandHomelink(int button, int durationms) {
    bool enable;
    time_t rawtime;
    time ( &rawtime );
    struct tm* tmu = localtime(&rawtime);
    int hours = tmu->tm_hour;
    int minutes = tmu->tm_min;
    minutes = (minutes - (minutes % 5));
    if (button == 0) {
        enable = true;
        return CommandSetChargeTimer(enable, hours, minutes);
    }
    if (button == 1) {
        enable = false;
        return CommandSetChargeTimer(enable, hours, minutes);
    }
    return NotImplemented;
}

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
#endif

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

  const char* chargedkwh = StdMetrics.ms_v_charge_kwh->AsUnitString("-", Native, 3).c_str();
  if (*chargedkwh != '-')
    writer->printf("Energy charged: %s\n", chargedkwh);

  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf("ODO: %s\n", odometer);

  const char* cac = StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str();
  if (*cac != '-')
    writer->printf("CAC: %s\n", cac);

  const char* soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 1).c_str();
  if (*soh != '-')
    writer->printf("SOH: %s\n", soh);

  return Success;
}

class OvmsVehicleSmartEDInit {
    public:
    OvmsVehicleSmartEDInit();
} MyOvmsVehicleSmartEDInit __attribute__ ((init_priority (9000)));

OvmsVehicleSmartEDInit::OvmsVehicleSmartEDInit() {
    ESP_LOGI(TAG, "Registering Vehicle: SMART ED (9000)");
    MyVehicleFactory.RegisterVehicle<OvmsVehicleSmartED>("SE", "Smart ED 3.Gen");
}
