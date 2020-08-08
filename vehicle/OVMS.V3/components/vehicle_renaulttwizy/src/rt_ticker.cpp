/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: timer/ticker based tasks
 * 
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ovms_log.h"
static const char *TAG = "v-twizy";

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

#include "vehicle_renaulttwizy.h"

using namespace std;


/**
 * Ticker1: per second ticker
 */

void OvmsVehicleRenaultTwizy::Ticker1(uint32_t ticker)
{
  // --------------------------------------------------------------------------
  // Update standard metrics:
  // 
  
  *StdMetrics.ms_v_pos_odometer = (float) twizy_odometer / 100;
  
  if (twizy_odometer >= twizy_odometer_tripstart)
    *StdMetrics.ms_v_pos_trip = (float) (twizy_odometer - twizy_odometer_tripstart) / 100;
  else
    *StdMetrics.ms_v_pos_trip = (float) 0;
  
  *StdMetrics.ms_v_mot_temp = (float) twizy_tmotor;
  
  
  // --------------------------------------------------------------------------
  // STATUS:
  //
  
  if (StdMetrics.ms_v_charge_voltage->AsFloat() > 0)
    twizy_flags.PilotSignal = 1;
  else
    twizy_flags.PilotSignal = 0;
  
  if ((twizy_status & 0x60) == 0x20)
  {
    twizy_flags.ChargePort = 1;
    twizy_flags.Charging = 1;
  }
  else
  {
    twizy_flags.Charging = 0;
    // Port will be closed on next use start
    // 12V charging will be stopped by DC converter status check
  }
  
  
  // Ignition status:
  twizy_flags.CarON = (twizy_status & CAN_STATUS_GO) ? true : false;
  
  
  // Power status change?
  if ((twizy_status & CAN_STATUS_KEYON)
    && !(twizy_status & CAN_STATUS_OFFLINE)
    && !twizy_flags.CarAwake)
  {
    // CAR has just been turned ON & CAN bus is online
    ESP_LOGI(TAG, "turned on");
    twizy_flags.CarAwake = 1;
    
    // set trip references:
    twizy_soc_tripstart = twizy_soc;
    twizy_odometer_tripstart = twizy_odometer;
    twizy_accel_avg = 0;
    twizy_accel_min = 0;
    twizy_accel_max = 0;
    
    // reset battery subsystem:
    m_batt_doreset = true;
    
    // reset power subsystem:
    PowerReset();
    
    // update sevcon subsystem:
    if (m_sevcon)
      m_sevcon->SetStatus(twizy_flags.CarAwake);
    
    twizy_button_cnt = 0;
  }
  else if (!(twizy_status & CAN_STATUS_KEYON) && twizy_flags.CarAwake)
  {
    // CAR has just been turned OFF
    ESP_LOGI(TAG, "turned off");
    twizy_flags.CarAwake = 0;
    
    // set trip references:
    twizy_soc_tripend = twizy_soc;
    
    // send power statistics if 25+ Wh used:
    if ((twizy_speedpwr[CAN_SPEED_CONST].use
      +twizy_speedpwr[CAN_SPEED_ACCEL].use
      +twizy_speedpwr[CAN_SPEED_DECEL].use) > (WH_DIV * 25))
    {
      RequestNotify(SEND_PowerNotify | SEND_TripLog);
    }
    
    // update sevcon subsystem:
    if (m_sevcon)
      m_sevcon->SetStatus(twizy_flags.CarAwake);
    
    *StdMetrics.ms_v_env_gear = (int) 0;
    *StdMetrics.ms_v_env_throttle = (float) 0;
    *StdMetrics.ms_v_env_footbrake = (float) 0;
    *StdMetrics.ms_v_pos_speed = (float) 0;
    *StdMetrics.ms_v_mot_rpm = (int) 0;
    *StdMetrics.ms_v_bat_power = (float) 0;
  }
  
  
  // --------------------------------------------------------------------------
  // Subsystem updates:
  
  PowerUpdate();
  ObdTicker1();
  if (m_sevcon)
    m_sevcon->Ticker1(ticker);
  
  
  // --------------------------------------------------------------------------
  // Charge notification + alerts:
  // 
  //  - twizy_chargestate: 0="" (none), 1=charging, 2=top off, 4=done, 21=stopped charging
  //  - twizy_chg_stop_request: 1=stop
  // 

  if (twizy_flags.Charging)
  {
    // --------------------------------------------------------------------------
    // CHARGING
    // 
    
    twizy_chargeduration++;
    
    *StdMetrics.ms_v_charge_kwh = (float) twizy_speedpwr[CAN_SPEED_CONST].rec / WH_DIV / 1000;
    
    *StdMetrics.ms_v_charge_current = (float) -twizy_current / 4;

    // Calculate range during charging:
    // scale twizy_soc_min_range to twizy_soc
    if ((twizy_soc_min_range > 0) && (twizy_soc > 0) && (twizy_soc_min > 0))
    {
      // Update twizy_range_est:
      twizy_range_est = (((float) twizy_soc_min_range) / twizy_soc_min) * twizy_soc;

      if (twizy_maxrange > 0)
        twizy_range_ideal = (((float) twizy_maxrange) * twizy_soc) / 10000;
      else
        twizy_range_ideal = twizy_range_est;
    }


    // If we've not been charging before...
    if (twizy_chargestate != 1 && twizy_chargestate != 2)
    {
      ESP_LOGI(TAG, "charge start");
      
      // reset SOC max:
      twizy_soc_max = twizy_soc;

      // reset SOC min & power sums if not resuming from charge interruption:
      if (twizy_chargestate != 21)
      {
        twizy_soc_min = twizy_soc;
        twizy_soc_min_range = twizy_range_est;
        PowerReset();
      }

      // reset battery monitor:
      m_batt_doreset = true;

      // ...enter state 1=charging or 2=topping-off depending on the
      // charge power request level we're starting with (7=full power):
      twizy_chargestate = (twizy_chg_power_request == 7) ? 1 : 2;
      *StdMetrics.ms_v_charge_substate = (string) "";
    }

    else
    {
      // We've already been charging:
      
      // check for crossing "sufficient SOC/Range" thresholds:
      if ((cfg_chargemode == TWIZY_CHARGEMODE_AUTOSTOP) &&
              (((cfg_suffsoc > 0) && (twizy_soc >= cfg_suffsoc*100)) ||
              ((cfg_suffrange > 0) && (twizy_range_ideal >= cfg_suffrange))))
      {
        // set charge stop request:
        ESP_LOGI(TAG, "requesting charge stop (sufficient charge)");
        MyEvents.SignalEvent("vehicle.charge.substate.scheduledstop", NULL);
        *StdMetrics.ms_v_charge_substate = (string) "scheduledstop";
        twizy_chg_stop_request = true;
      }

      else if (
              ((cfg_suffsoc > 0) && (twizy_soc >= cfg_suffsoc*100) && (twizy_last_soc < cfg_suffsoc*100))
              ||
              ((cfg_suffrange > 0) && (twizy_range_ideal >= cfg_suffrange) && (twizy_last_idealrange < cfg_suffrange))
              )
      {
        // ...send sufficient charge alert:
        RequestNotify(SEND_SuffCharge);
      }
      
      // Battery capacity estimation: detect end of CC phase
      // by monitoring the charge power level requested by the BMS;
      // if it drops, we're entering the CV phase
      // (Note: depending on battery temperature, this may happen at rather
      // low SOC and without having reached the nominal top voltage)
      if (twizy_chg_power_request >= twizy_cc_power_level)
      {
        // still in CC phase:
        twizy_cc_power_level = twizy_chg_power_request;
        twizy_cc_soc = twizy_soc;
        twizy_cc_charge = twizy_charge_rec;
      }
      else if (twizy_chargestate != 2)
      {
        // entering CV phase, set state 2=topping off:
        twizy_chargestate = 2;
      }

    }

    // update "sufficient" threshold helpers:
    twizy_last_soc = twizy_soc;
    twizy_last_idealrange = twizy_range_ideal;
    
    // Switch on additional charger if not in / short before CV phase
    // and charge power level has not been limited by the user:
    if (cfg_aux_charger_port) {
      if ((twizy_chargestate == 1) && (twizy_soc < 9400) && (cfg_chargelevel == 0))
        MyPeripherals->m_max7317->Output(cfg_aux_charger_port, 1);
      else
        MyPeripherals->m_max7317->Output(cfg_aux_charger_port, 0);
    }
    
    // END OF STATE: CHARGING
  }

  else
  {
    // --------------------------------------------------------------------------
    // NOT CHARGING
    // 

    // Switch off additional charger:
    if (cfg_aux_charger_port) {
      MyPeripherals->m_max7317->Output(cfg_aux_charger_port, 0);
    }

    // Calculate range:
    if (twizy_range_est > 0)
    {
      if (twizy_maxrange > 0)
        twizy_range_ideal = (((float) twizy_maxrange) * twizy_soc) / 10000;
      else
        twizy_range_ideal = twizy_range_est;
    }


    // Check if we've been charging before:
    if (twizy_chargestate == 1 || twizy_chargestate == 2)
    {
      ESP_LOGI(TAG, "charge stop");
      
      // yes, check if charging has been finished by the BMS
      // by checking if we've reached charge power level 0
      // (this is more reliable than checking for SOC 100% as some Twizy will
      // not reach 100% while others will still top off in 100% for some time)
      if (twizy_chg_power_request == 0)
      {
        float charge;
        float new_cap_prc;
        
        // yes, means "done"
        twizy_chargestate = 4;
        
        // calculate battery capacity if charge started below 40% SOC:
        if (twizy_soc_min < 4000 && twizy_cc_soc > twizy_soc_min)
        {
          // scale CC charge part by SOC range:
          charge = (twizy_cc_charge / (twizy_cc_soc - twizy_soc_min)) * twizy_cc_soc;
          // add CV charge part:
          charge += (twizy_charge_rec - twizy_cc_charge);
          
          // convert to Ah:
          charge = charge / 400 / 3600;
          
          // convert to percent:
          new_cap_prc = charge / cfg_bat_cap_nominal_ah * 100;
          
          // smooth over 10 samples:
          if (cfg_bat_cap_actual_prc > 0)
          {
            new_cap_prc = (cfg_bat_cap_actual_prc * 9 + new_cap_prc) / 10;
          }
          cfg_bat_cap_actual_prc = new_cap_prc;
          
          // store in config flash:
          MyConfig.SetParamValueFloat("xrt", "cap_act_prc", cfg_bat_cap_actual_prc);
        }

      }
      else
      {
        // no, means "stopped"
        twizy_chargestate = 21;
      }

    }

    else if (twizy_flags.CarAwake && twizy_flags.ChargePort)
    {
      // Car awake, not charging, charge port open:
      // beginning the next car usage cycle:

      // Close charging port:
      twizy_flags.ChargePort = 0;
      twizy_chargeduration = 0;

      // Clear charge state:
      twizy_chargestate = 0;
      *StdMetrics.ms_v_charge_substate = (string) "";

      // reset SOC minimum:
      twizy_soc_min = twizy_soc;
      twizy_soc_min_range = twizy_range_est;
    }
    
    // clear charge stop request:
    twizy_chg_stop_request = false;

    // END OF STATE: NOT CHARGING
  }
  
  
  // convert battery capacity percent to framework Ah:
  *StdMetrics.ms_v_bat_cac = (float) cfg_bat_cap_actual_prc / 100 * cfg_bat_cap_nominal_ah;


  // --------------------------------------------------------------------------
  // Notifications:
  // 
  // Note: these are distributed within a full minute to minimize
  // current load on the modem and match GPS requests
  // 

  int i = ticker % 60;
  
  // Update charge ETR once per minute:
  if (i == 0) {
    UpdateChargeTimes();
  }

  // Send RT-PWR-Stats update once per minute:
  if (i == 21) {
    RequestNotify(SEND_PowerStats);
  }
  
  // Send RT-BAT-* update once per minute:
  if (i == 42) {
    RequestNotify(SEND_BatteryStats);
  }

  // Send GPS-Log updates:
  if (twizy_speed > 0 && cfg_gpslog_interval > 0) {
    // … streaming mode:
    if (ticker - twizy_last_gpslog >= cfg_gpslog_interval) {
      RequestNotify(SEND_GPSLog);
      twizy_last_gpslog = ticker;
    }
  }
  else if (i == 21) {
    // … parking mode:
    RequestNotify(SEND_GPSLog);
  }
  

  // --------------------------------------------------------------------------
  // Publish metrics:
  
  *m_batt_use_soc_min = (float) twizy_soc_min / 100;
  *m_batt_use_soc_max = (float) twizy_soc_max / 100;
  
  *StdMetrics.ms_v_charge_mode = (string)
    ((cfg_chargemode == TWIZY_CHARGEMODE_AUTOSTOP) ? "storage" : "standard");
  *StdMetrics.ms_v_charge_climit = (float) cfg_chargelevel * 5;
  
  *StdMetrics.ms_v_bat_range_ideal = (float) twizy_range_ideal;
  *StdMetrics.ms_v_bat_range_est = (float) twizy_range_est;
  
  *StdMetrics.ms_v_env_awake = (bool) twizy_flags.CarAwake;
  *StdMetrics.ms_v_env_on = (bool) twizy_flags.CarON;
  
  *StdMetrics.ms_v_charge_pilot = (bool) twizy_flags.PilotSignal;
  *StdMetrics.ms_v_door_chargeport = (bool) twizy_flags.ChargePort;
  *StdMetrics.ms_v_charge_inprogress = (bool) twizy_flags.Charging;
  *StdMetrics.ms_v_env_charging12v = (bool) twizy_flags.Charging12V;
  
  // ms_v_charge_state triggers the notification, so needs to be last:
  *StdMetrics.ms_v_charge_state = (string) chargestate_code(twizy_chargestate);
  
  
  // --------------------------------------------------------------------------
  // Send notifications:
  
  DoNotify();
  
}


/**
 * Ticker10: per 10 second ticker
 */

void OvmsVehicleRenaultTwizy::Ticker10(uint32_t ticker)
{
  // Check if CAN-Bus has turned offline:
  if (twizy_status & CAN_STATUS_ONLINE)
  {
    // Clear online flag to test for CAN activity:
    twizy_status &= ~CAN_STATUS_ONLINE;
  }
  else
  {
    // CAN offline: implies...
    twizy_status = CAN_STATUS_OFFLINE;
    *StdMetrics.ms_v_charge_voltage = (float) 0;
    twizy_speed = 0;
    twizy_power = 0;
    twizy_power_min = twizy_current_min = 32767;
    twizy_power_max = twizy_current_max = -32768;
  }
  
  
  // --------------------------------------------------------------------------
  // Control additional charger fan:
  // Elips 2000W specified operation range is -20..+50 °C
  // => switch on additional fan above 45 °C, off below 45 °C
  // As we don't get temperature updates after switching the
  // Twizy off, we need the timer to keep the fan ON for a while.

  if (cfg_aux_fan_port) {
    if (twizy_flags.CarAwake || twizy_flags.Charging) {
      if (StdMetrics.ms_v_charge_temp->AsFloat() > TWIZY_FAN_THRESHOLD) {
        MyPeripherals->m_max7317->Output(cfg_aux_fan_port, 1);
        if (twizy_fan_timer == 0)
          ESP_LOGW(TAG, "charger temperature %.1f celcius; extra fan switched ON",
            StdMetrics.ms_v_charge_temp->AsFloat());
        twizy_fan_timer = TWIZY_FAN_OVERSHOOT * 6;
      }
      else if (StdMetrics.ms_v_charge_temp->AsFloat() < TWIZY_FAN_THRESHOLD) {
        MyPeripherals->m_max7317->Output(cfg_aux_fan_port, 0);
        if (twizy_fan_timer != 0)
          ESP_LOGI(TAG, "charger temperature %.1f celcius; extra fan switched OFF",
            StdMetrics.ms_v_charge_temp->AsFloat());
        twizy_fan_timer = 0;
      }
    }
    else {
      if (twizy_fan_timer > 0 && --twizy_fan_timer == 0) {
        MyPeripherals->m_max7317->Output(cfg_aux_fan_port, 0);
        if (twizy_fan_timer != 0)
          ESP_LOGI(TAG, "charger extra fan overshoot end; switched OFF");
      }
    }
  }
  

  // --------------------------------------------------------------------------
  // Valet mode: lock speed to 6 kph if valet max odometer reached:
  
  if (ValetMode() && !CarLocked() && twizy_odometer > twizy_valet_odo) {
    ESP_LOGI(TAG, "valet mode: odo limit %.2f reached; locking car", twizy_valet_odo / 100.0f);
    string buf;
    MsgCommandRestrict(buf, CMD_Lock, NULL);
    MyNotify.NotifyString("alert", "valetmode.odolimit", buf.c_str());
  }


  // --------------------------------------------------------------------------
  // Subsystem updates:
  ObdTicker10();

}
