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
 */

static const char *TAG = "v-smarteq";

#include "vehicle_smarteq.h"


void OvmsVehicleSmartEQ::Ticker1(uint32_t ticker) 
  {
  // climate start 2-3 times when Homelink 2 or 3
  if (m_climate_ticker >= 1 && !StdMetrics.ms_v_env_hvac->AsBool() && !m_climate_start)
      CommandClimateControl(true);

  if (m_climate_start && StdMetrics.ms_v_env_hvac->AsBool()) 
    {
    m_climate_start = false;
    if (m_12v_charge_state) 
      {
      Notify12Vcharge();
      } 
    else 
      {
      NotifyClimate();
      }
    if (m_climate_ticker >= 1) 
      { 
      --m_climate_ticker;
      ESP_LOGI(TAG,"Climate ticker: %d", m_climate_ticker);
      }
    }

  if (m_ddt4all_exec >= 1) 
    { 
    --m_ddt4all_exec;
    }

  if (StdMetrics.ms_v_env_on->AsBool(false)) 
    HandleEnergy();
  if (StdMetrics.ms_v_env_on->AsBool(false))
    HandleTripcounter();
  if (StdMetrics.ms_v_charge_pilot->AsBool(false))
    HandleCharging();
  if (StdMetrics.ms_v_env_on->AsBool(false))
    HandleChargeport();

  // reactivate door lock warning if the car is parked and unlocked
  if( m_enable_lock_state && 
        m_warning_unlocked &&
        (StdMetrics.ms_v_door_fl->AsBool()  || 
          StdMetrics.ms_v_door_fr->AsBool() ||
          StdMetrics.ms_v_door_rl->AsBool() ||
          StdMetrics.ms_v_door_rr->AsBool() ||
          StdMetrics.ms_v_door_trunk->AsBool() ||
          StdMetrics.ms_v_door_hood->AsBool())) 
    {
    StdMetrics.ms_v_env_parktime->SetValue(0); // reset parking time
    m_warning_unlocked = false;
    }
  
  if (ticker % 10 == 0) // Every 10 seconds
    {
    if(m_enable_LED_state) 
      OnlineState();
    if(m_climate_system) 
      TimeBasedClimateData();
    }
  }

void OvmsVehicleSmartEQ::Ticker60(uint32_t ticker) {
  if(mt_climate_on->AsBool()) 
    TimeCheckTask();
  if(m_12v_charge && !StdMetrics.ms_v_env_on->AsBool()) 
    Check12vState();
  if(m_enable_lock_state && !m_warning_unlocked && StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10) 
    DoorLockState();
  if(m_enable_door_state && !m_warning_dooropen && StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10) 
    DoorOpenState();

  #if defined(CONFIG_OVMS_COMP_WIFI) || defined(CONFIG_OVMS_COMP_CELLULAR)
    if(m_reboot_time > 0) 
      Handlev2Server();
  #endif

  // DDT4ALL session timeout on 5 minutes
  if (m_ddt4all) 
    {
    m_ddt4all_ticker++;
    if (m_ddt4all_ticker >= 5) 
      {
      m_ddt4all = false;
      m_ddt4all_ticker = 0;
      ESP_LOGI(TAG, "DDT4ALL session timeout reached");
      MyNotify.NotifyString("info", "xsq.ddt4all", "DDT4ALL session timeout reached");
      }
    }

  #ifdef CONFIG_OVMS_COMP_ADC
  if (m_enable_calcADCfactor && m_ADCfactor_recalc) 
    {
    if (--m_ADCfactor_recalc_timer == 0) 
      {
      m_ADCfactor_recalc = false;
      m_ADCfactor_recalc_timer = 4;
      // calculate new ADC factor
      float can12V = mt_bms_12v->AsFloat(0.0f) + 0.25f;   // BMS 12V voltage + offset
      if (can12V >= 13.10f) {
        ReCalcADCfactor(can12V, nullptr);  // nullptr = no Log-Output
        ESP_LOGI(TAG, "Auto ADC recalibration started (%.2fV)", can12V);
      } else {
        ESP_LOGW(TAG, "Error: Auto ADC recalibration, 12V voltage is not stable for ADC calibration! (%.2fV)", can12V);
      }
      }
    }
  #endif
}

/**
 * PollerStateTicker: check for state changes
 *  This is called by VehicleTicker1() just before the next PollerSend().
 */
void OvmsVehicleSmartEQ::PollerStateTicker(canbus *bus) 
  {
  bool lv_pwrstate =  mt_evc_LV_DCDC_act_req->AsBool(false); //(StdMetrics.ms_v_bat_12v_voltage->AsFloat(0) > 13.4f);  
  bool car_online = StdMetrics.ms_v_env_awake->AsBool(false);  
  bool car_bus = mt_bus_awake->AsBool(false);

  if (car_online != car_bus && !m_candata_poll) 
    {
    m_candata_poll = true;
    m_candata_timer = SQ_CANDATA_TIMEOUT;
    }
  
  if (m_candata_timer > 0 && --m_candata_timer == 0 && m_candata_poll) 
    {
    // Car has gone to sleep
    ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
    mt_bus_awake->SetValue(false);
    m_candata_poll = false;
    m_candata_timer = -1;
    }
  
  // - base system is awake if we've got a fresh lv_pwrstate:
  StdMetrics.ms_v_env_aux12v->SetValue(car_online);

  // - charging / trickle charging 12V battery is active when lv_pwrstate is true:
  StdMetrics.ms_v_env_charging12v->SetValue(lv_pwrstate);
  
  HandlePollState();
  }
