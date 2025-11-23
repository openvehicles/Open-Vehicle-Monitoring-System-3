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
  if (m_ddt4all_exec >= 1) 
    { 
    --m_ddt4all_exec;
    }

  HandleCharging();
  HandleChargeport();
  
  if (StdMetrics.ms_v_env_on->AsBool(false))
    {
    HandleEnergy();
    HandleTripcounter();
    }
  
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
    } // end every 10 seconds
  }

void OvmsVehicleSmartEQ::Ticker60(uint32_t ticker) {  
  if(m_12v_charge && !StdMetrics.ms_v_env_on->AsBool()) 
    Check12vState();
  if(m_enable_lock_state && !m_warning_unlocked && StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10) 
    DoorLockState();
  if(m_enable_door_state && !m_warning_dooropen && StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10) 
    DoorOpenState();
  if(StdMetrics.ms_v_env_on->AsBool(false)) 
    setTPMSValue();   // update TPMS metrics

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
      float can12V = mt_evc_LV_DCDC_volt->AsFloat(0.0f);
      if (mt_evc_LV_DCDC_act_req->AsBool(false))
        {
        ReCalcADCfactor(can12V, nullptr);  // nullptr = no Log-Output
        ESP_LOGI(TAG, "Auto ADC recalibration started (%.2fV)", can12V);
        } 
      else 
        {
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
  HandlePollState();
  }
