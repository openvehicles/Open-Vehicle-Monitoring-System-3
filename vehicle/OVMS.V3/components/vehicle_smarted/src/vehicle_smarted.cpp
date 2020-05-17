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
#include "ovms_events.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"
#include "ovms_boot.h"

#include "vehicle_smarted.h"

#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))

OvmsVehicleSmartED* OvmsVehicleSmartED::GetInstance(OvmsWriter* writer /*=NULL*/) {
  OvmsVehicleSmartED* smart = (OvmsVehicleSmartED*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();
  if (!smart || type != "SE") {
    if (writer)
      writer->puts("Error: SmartED vehicle module not selected");
    return NULL;
  }
  return smart;
}

/**
 * Constructor & destructor
 */
OvmsVehicleSmartED::OvmsVehicleSmartED() {
  ESP_LOGI(TAG, "Start Smart ED vehicle module");
  
  //memset(m_vin, 0, sizeof(m_vin));
  
  // init metrics:
  mt_vehicle_time          = MyMetrics.InitInt("xse.v.display.time", SM_STALE_MIN, 0, Minutes);
  mt_trip_start            = MyMetrics.InitFloat("xse.v.display.trip.start", SM_STALE_MID, 0, Kilometers);
  mt_trip_reset            = MyMetrics.InitFloat("xse.v.display.trip.reset", SM_STALE_MID, 0, Kilometers);
  mt_hv_active             = MyMetrics.InitBool("xse.v.b.hv.active", SM_STALE_MIN, false);
  mt_c_active              = MyMetrics.InitBool("xse.v.c.active", SM_STALE_MIN, false);
  mt_bus_awake             = MyMetrics.InitBool("xse.v.bus.awake", SM_STALE_MIN, false);
  mt_bat_energy_used_start = MyMetrics.InitFloat("xse.v.b.energy.used.start", SM_STALE_MID, 0, kWh);
  mt_bat_energy_used_reset = MyMetrics.InitFloat("xse.v.b.energy.used.reset", SM_STALE_MID, 0, kWh);
  mt_pos_odometer_start    = MyMetrics.InitFloat("xse.v.pos.odometer.start", SM_STALE_MID, 0, Kilometers);
  mt_real_soc              = MyMetrics.InitFloat("xse.v.b.real.soc", SM_STALE_MID, 0, Percentage);

  mt_ed_eco_accel          = MyMetrics.InitInt("xse.v.display.accel", SM_STALE_MIN, 50, Percentage);
  mt_ed_eco_const          = MyMetrics.InitInt("xse.v.display.const", SM_STALE_MIN, 50, Percentage);
  mt_ed_eco_coast          = MyMetrics.InitInt("xse.v.display.coast", SM_STALE_MIN, 50, Percentage);
  mt_ed_eco_score          = MyMetrics.InitInt("xse.v.display.ecoscore", SM_STALE_MIN, 50, Percentage);
  
  mt_ed_hv_off_time        = MyMetrics.InitInt("xse.hv.off.time", SM_STALE_MID, 0, Seconds);
  
  mt_12v_batt_voltage      = MyMetrics.InitFloat("xse.v.b.12v.voltage", SM_STALE_MID, 0, Volts);

  m_candata_timer     = 0;
  m_candata_poll      = 0;
  m_egpio_timer       = 0;
  
  m_shutdown_ticker   = 15*60;
  
  // init commands:
  cmd_xse = MyCommandApp.RegisterCommand("xse","SmartED 451 Gen.3");
  cmd_xse->RegisterCommand("recu","Set recu..", xse_recu, "<up/down>",1,1);
  cmd_xse->RegisterCommand("charge","Set charging Timer..", xse_chargetimer, "<hour> <minutes> <on/off>", 3, 3);
  cmd_xse->RegisterCommand("trip", "Show vehicle trip", xse_trip);
  cmd_xse->RegisterCommand("bmsdiag", "Show BMS diagnostic", xse_bmsdiag);
  cmd_xse->RegisterCommand("rptdata", "Show BMS RPTdata", xse_RPTdata);
  cmd_xse->RegisterCommand("obd2", "Send OBD2 request", shell_obd_request, "<txid> <rxid> <request>", 3, 3);
  cmd_xse->RegisterCommand("getvolts", "Send OBD2 request to get Cell Volts", shell_obd_request_volts);
  
  MyConfig.RegisterParam("xse", "Smart ED", true, true);
  ConfigChanged(NULL);
  
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  
  // init OBD2 poller:
  ObdInitPoll();

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
  
  ESP_LOGD(TAG, "Smart ED reload configuration");
  
  m_doorlock_port   = MyConfig.GetParamValueInt("xse", "doorlock.port", 9);
  m_doorunlock_port = MyConfig.GetParamValueInt("xse", "doorunlock.port", 8);
  m_ignition_port   = MyConfig.GetParamValueInt("xse", "ignition.port", 7);
  m_doorstatus_port = MyConfig.GetParamValueInt("xse", "doorstatus.port", 6);
  
  m_range_ideal     = MyConfig.GetParamValueInt("xse", "rangeideal", 135);
  m_egpio_timout    = MyConfig.GetParamValueInt("xse", "egpio_timout", 5);
  m_soc_rsoc        = MyConfig.GetParamValueBool("xse", "soc_rsoc", false);
  
  m_enable_write    = MyConfig.GetParamValueBool("xse", "canwrite", false);
  m_lock_state      = MyConfig.GetParamValueBool("xse", "lockstate", false);
  m_reset_trip      = MyConfig.GetParamValueBool("xse", "reset.trip.charge", false);
  m_notify_trip     = MyConfig.GetParamValueBool("xse", "notify.trip", true);
  m_gpio_highlow    = MyConfig.GetParamValueBool("xse", "gpio_highlow", false);
  
  m_preclima_time   = MyConfig.GetParamValueInt("xse", "preclimatime", 15);
  
  m_reboot_time     = MyConfig.GetParamValueInt("xse", "reboot", 0);
  m_reboot          = MyConfig.GetParamValueBool("xse", "doreboot", false);
  
  m_auto_set_recu   = MyConfig.GetParamValueBool("xse", "autosetrecu", false);
  
  StandardMetrics.ms_v_charge_limit_soc->SetValue((float) MyConfig.GetParamValueInt("xse", "suffsoc", 0), Percentage );
  StandardMetrics.ms_v_charge_limit_range->SetValue((float) MyConfig.GetParamValueInt("xse", "suffrange", 0), Kilometers );
  
  if (!m_enable_write) PollSetState(0);
  
#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(m_doorlock_port, (m_gpio_highlow ? 1 : 0));
  MyPeripherals->m_max7317->Output(m_doorunlock_port, (m_gpio_highlow ? 1 : 0));
  MyPeripherals->m_max7317->Output(m_ignition_port, 0);
#endif
}

void OvmsVehicleSmartED::vehicle_smarted_car_on(bool isOn) {
  if (isOn && !StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned on
    ESP_LOGI(TAG,"CAR IS ON");
    StandardMetrics.ms_v_env_awake->SetValue(isOn);
    StandardMetrics.ms_v_env_valet->SetValue(isOn);
    
    if (m_enable_write) PollSetState(2);

    // Reset trip values
    if (!m_reset_trip) {
      StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
      StandardMetrics.ms_v_bat_energy_used->SetValue(0);
      mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
      StandardMetrics.ms_v_pos_trip->SetValue(0);
    }
  }
  else if (!isOn && StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned off
    ESP_LOGI(TAG,"CAR IS OFF");
    StandardMetrics.ms_v_env_awake->SetValue(isOn);
    StandardMetrics.ms_v_env_valet->SetValue(isOn);
    
    if (mt_c_active->AsBool()) {
      if (m_enable_write) PollSetState(3);
    } else {
      if (m_enable_write) PollSetState(1);
    }
    if (StandardMetrics.ms_v_pos_trip->AsFloat(0) > 0.1 && m_notify_trip)
			NotifyTrip();
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
      if (!StandardMetrics.ms_v_charge_inprogress->AsBool()) {
        if (!isCharging) {
          isCharging = true;
          // Reset charge kWh
          StandardMetrics.ms_v_charge_kwh->SetValue(0);
          // Reset trip values
          if (m_reset_trip) {
            StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
            StandardMetrics.ms_v_bat_energy_used->SetValue(0);
            mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
            StandardMetrics.ms_v_pos_trip->SetValue(0);
          }
        }
        // set Poll state charging
        if (m_enable_write) PollSetState(3);
        
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
        if (m_enable_write) PollSetState(1);
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_type->SetValue("type2");
        // charging 12v stop
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
    }
  } else if (isCharging) {
    isCharging = false;
    StandardMetrics.ms_v_charge_duration_full->SetValue(0);
    StandardMetrics.ms_v_charge_duration_soc->SetValue(0);
    StandardMetrics.ms_v_charge_duration_range->SetValue(0);
  }
}

void OvmsVehicleSmartED::IncomingFrameCan1(CAN_frame_t* p_frame) {
  if (m_candata_poll != 1) {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    mt_bus_awake->SetValue(true);
    //StandardMetrics.ms_v_env_awake->SetValue(true);
    m_candata_poll = 1;
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
        mt_real_soc->SetValue((float) (d[7]/2.0));
      } else {
        StandardMetrics.ms_v_bat_soc->SetValue((float) (d[7]/2.0));
      }
      StandardMetrics.ms_v_charge_climit->SetValue(d[1]/2.0);
      switch(d[2]) {
        case 0x20: // D-
          StandardMetrics.ms_v_env_drivemode->SetValue(0);
          break;
        case 0x40: // D
          StandardMetrics.ms_v_env_drivemode->SetValue(1);
          break;
        case 0x60: // D+
          StandardMetrics.ms_v_env_drivemode->SetValue(2);
          break;
      }
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
        mt_real_soc->SetValue(rsoc, Percentage);
      }
      // StandardMetrics.ms_v_bat_cac->SetValue((DEFAULT_BATTERY_AMPHOURS * rsoc) / 100, AmpHours);
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
      mt_12v_batt_voltage->SetValue(LV, Volts);
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
      if(d[3] != 0xFF) {
        time = (d[2] * 60 + (d[3] & 0x40 ? d[3]-0x40 : d[3]) * 60);
        StandardMetrics.ms_v_charge_timerstart->SetValue(time, Seconds);
      }
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
      // StandardMetrics.ms_v_env_cabintemp->SetValue(d[0]*0.5);
      break;
    }
    case 0x3F2: //Eco display
    {
      mt_ed_eco_accel->SetValue(d[0] >> 1);
      mt_ed_eco_const->SetValue(d[1] >> 1);
      mt_ed_eco_coast->SetValue(d[2] >> 1);
      mt_ed_eco_score->SetValue(d[3] >> 1);
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
      float value;
      value = d[1] * 256 + d[2];
      if (value != 65535)
        mt_trip_start->SetValue((float)value/10);
      value = d[4] * 256 + d[5];
      if (value != 65535)
        mt_trip_reset->SetValue((float)value/10);
      break;
    }
    case 0x3D7: //HV Status
    {   //HV active
      mt_hv_active->SetValue(d[0]);
      StandardMetrics.ms_v_env_charging12v->SetValue(d[0]);
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
    
    // Polling IDs
    case 0x7a3:
    {
      // 7a3 8 04 61 12 64 00 00 00 00
      if (d[0] == 0x04 && d[1] == 0x61 && d[2] == 0x12) {
        StandardMetrics.ms_v_env_cabintemp->SetValue(d[3]/10.0);
      }
      // ESP_LOGD(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
    default: {
      // ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
  }
}

void OvmsVehicleSmartED::IncomingFrameCan2(CAN_frame_t* p_frame) {
  
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

void OvmsVehicleSmartED::CalculateEfficiency() {
  float consumption = 0;
  if (StdMetrics.ms_v_pos_speed->AsFloat() >= 5)
    consumption = ABS(StdMetrics.ms_v_bat_power->AsFloat(0, Watts)) / StdMetrics.ms_v_pos_speed->AsFloat();
  StdMetrics.ms_v_bat_consumption->SetValue((StdMetrics.ms_v_bat_consumption->AsFloat() * 4 + consumption) / 5);
}

void OvmsVehicleSmartED::Ticker1(uint32_t ticker) {
  if (m_candata_timer > 0) {
    if (--m_candata_timer == 0) {
      // Car has gone to sleep
      ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
      mt_bus_awake->SetValue(false);
      //StandardMetrics.ms_v_env_awake->SetValue(false);
      StandardMetrics.ms_v_env_hvac->SetValue(false);
      PollSetState(0);
      m_candata_poll = 0;
      // save metrics for crash reboot
      SaveStatus();
    }
  }
  HandleEnergy();
  if (mt_bus_awake->AsBool())
    HandleChargingStatus();
  if (mt_hv_active->AsBool())
    mt_ed_hv_off_time->SetValue(0);
  else
    mt_ed_hv_off_time->SetValue(mt_ed_hv_off_time->AsInt() + 1);
  
  // Handle Tripcounter
  if (mt_pos_odometer_start->AsFloat(0) == 0 && StandardMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0) {
    mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
  }
  if (StandardMetrics.ms_v_env_on->AsBool() && StandardMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0 && mt_pos_odometer_start->AsFloat(0) > 0.0) {
    StandardMetrics.ms_v_pos_trip->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start->AsFloat(0));
  }
  
  RestartNetwork();
  ShutDown();
}

void OvmsVehicleSmartED::Ticker10(uint32_t ticker) {
  HandleCharging();
  AutoSetRecu();
#ifdef CONFIG_OVMS_COMP_MAX7317
  if(m_lock_state) {
    MyPeripherals->m_max7317->Output((uint8_t)m_doorstatus_port,(uint8_t)1); // set port to input
    int level = MyPeripherals->m_max7317->Input((uint8_t)m_doorstatus_port);
    StandardMetrics.ms_v_env_locked->SetValue(level == 1 ? false : true);
  }
#endif
}

void OvmsVehicleSmartED::Ticker60(uint32_t ticker) {
  if (StandardMetrics.ms_v_bat_soc->AsFloat(0) == 0) RestoreStatus();
#ifdef CONFIG_OVMS_COMP_MAX7317
  if (m_egpio_timer > 0) {
    if (--m_egpio_timer == 0) {
      ESP_LOGI(TAG,"Ignition EGPIO off port: %d", m_ignition_port);
      MyPeripherals->m_max7317->Output(m_ignition_port, 0);
      StandardMetrics.ms_v_env_valet->SetValue(false);
      MyNotify.NotifyString("info", "valet.disabled", "Ignition off");
    }
  }
  if (StandardMetrics.ms_v_env_valet->AsBool() && StandardMetrics.ms_v_bat_soc->AsFloat(0) < 20) {
    MyPeripherals->m_max7317->Output(m_ignition_port, 0);
    StandardMetrics.ms_v_env_valet->SetValue(false);
    MyNotify.NotifyString("info", "valet.disabled", "Ignition off");
    m_egpio_timer = 0;
  }
#endif
}

void OvmsVehicleSmartED::RestartNetwork() {
  // Handle v2Server connection
  if (StandardMetrics.ms_s_v2_connected->AsBool() || m_reboot_time == 0) {
    m_reboot_ticker = m_reboot_time * 60; // set reboot ticker
  }
  else if (m_reboot_ticker > 0 && --m_reboot_ticker == 0) {
    SaveStatus();
    if (m_reboot) {
      MyBoot.Restart();
    } else {
      MyNetManager.RestartNetwork();
    }
    m_reboot_ticker = m_reboot_time * 60;
  }
}

void OvmsVehicleSmartED::AutoSetRecu() {
  if (StandardMetrics.ms_v_env_on->AsBool() && m_auto_set_recu && m_enable_write) {
    if (StandardMetrics.ms_v_env_drivemode->AsInt(1) != 2) {
      while(StandardMetrics.ms_v_env_drivemode->AsInt(1) != 2) {
        CommandSetRecu(true);
        vTaskDelay(50 / portTICK_PERIOD_MS);
      }
      MyEvents.SignalEvent("v-smarted.xse.recu.up",NULL);
    }
  }
}

void OvmsVehicleSmartED::ShutDown() {
  float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  
  if (volt > 0 && volt > 10.5) {
    m_shutdown_ticker = 15 * 60;
  } 
  else if (volt > 0 && volt < 10.5 && --m_shutdown_ticker == 0) {
    ESP_LOGW(TAG,"Powering off");
    MyEvents.SignalEvent("v-smarted.power.off",NULL);
    MyNotify.NotifyString("alert", "v-smarted.power.off", "Shutting down OVMS to prevent Battery drain");
    vTaskDelay(500 / portTICK_PERIOD_MS); // make sure all notifications all transmitted before powerring down
#ifdef CONFIG_OVMS_COMP_WIFI
    MyPeripherals->m_esp32wifi->SetPowerMode(Off);
#endif
#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
    MyPeripherals->m_simcom->SetPowerMode(Off);
#endif
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
    MyPeripherals->m_mcp2515_swcan->SetPowerMode(Off);
#endif
#ifdef CONFIG_OVMS_COMP_ESP32CAN
    MyPeripherals->m_esp32can->SetPowerMode(Off);
#endif
#ifdef CONFIG_OVMS_COMP_EXT12V
    MyPeripherals->m_ext12v->SetPowerMode(Off);
#endif
    MyPeripherals->m_esp32->SetPowerMode(Off);
  }
}

/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleSmartED::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 0:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xse", "doreboot",  (bits& 1)!=0);
      return true;
    }
    case 1:
      MyConfig.SetParamValue("xse", "reboot", value);
      return true;
    case 2:
      MyConfig.SetParamValue("xse", "preclimatime", value);
      return true;
    case 3:
      MyConfig.SetParamValue("xse", "egpio_timout", value);
      return true;
    case 4:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xse", "autosetrecu",  (bits& 1)!=0);
      return true;
    }
    case 10:
      MyConfig.SetParamValue("xse", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("xse", "suffrange", value);
      return true;
    case 12:
      MyConfig.SetParamValue("xse", "rangeideal", value);
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
    case 0:
    {
      int bits = ( MyConfig.GetParamValueBool("xse", "doreboot",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 1:
      return MyConfig.GetParamValue("xse", "reboot", STR(0));
    case 2:
      return MyConfig.GetParamValue("xse", "preclimatime", STR(15));
    case 3:
      return MyConfig.GetParamValue("xse", "egpio_timout", STR(5));
    case 4:
    {
      int bits = ( MyConfig.GetParamValueBool("xse", "autosetrecu",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 10:
      return MyConfig.GetParamValue("xse", "suffsoc", STR(0));
    case 11:
      return MyConfig.GetParamValue("xse", "suffrange", STR(0));
    case 12:
      return MyConfig.GetParamValue("xse", "rangeideal", STR(135));
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

class OvmsVehicleSmartEDInit {
  public:
  OvmsVehicleSmartEDInit();
} MyOvmsVehicleSmartEDInit __attribute__ ((init_priority (9000)));

OvmsVehicleSmartEDInit::OvmsVehicleSmartEDInit() {
  ESP_LOGI(TAG, "Registering Vehicle: SMART ED (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleSmartED>("SE", "Smart ED 3.Gen");
}
