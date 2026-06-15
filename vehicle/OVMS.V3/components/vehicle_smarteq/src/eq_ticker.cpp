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
    --m_ddt4all_exec;

  if (can_350_ticker > 0 &&--can_350_ticker == 0) 
    {
    ESP_LOGD(TAG, "CAN 0x350 timeout reached");
    can_350_ticker = -1;
    can_awake = false;
    can_env_on = false;
    can_battery_on = false;
    smartCAN2Metrics();
    }
    
  if(IsAwakeEQ())
    smartCAN2Metrics();

  if(IsChargingEQ()) 
    HandleCharging();
  
  if(IsOnEQ())
    HandleEnergy();
  
  if (m_cmd_locked && DoorOpen()) 
    {
    m_cmd_locked = false; // reset command lock when car is opened
    }
  } // Ticker 1

void OvmsVehicleSmartEQ::Ticker10(uint32_t ticker) 
  {
  // if awake state has changed, then update metric to prevent desync
  if (IsAwakeEQ() != StdMetrics.ms_v_env_awake->AsBool(false))
    {
    StdMetrics.ms_v_env_awake->SetValue(IsAwakeEQ());
    }
  // if 12V charging state has changed, then update metric to prevent desync
  if (Is12VchargeEQ() != StdMetrics.ms_v_env_charging12v->AsBool(false))
    {
    StdMetrics.ms_v_env_charging12v->SetValue(Is12VchargeEQ());    
    } 
  // reactivate door lock warning if the car is parked and unlocked
  if( m_enable_lock_state && 
        m_warning_unlocked &&
        DoorOpen()) 
    {
    StdMetrics.ms_v_env_parktime->SetValue(0); // reset parking time
    m_warning_unlocked = false;
    }

  if(IsOnEQ())
    HandleTripcounter();

  if(m_enable_LED_state) 
    OnlineState();

  // if HVAC is on, then modify polling to get the DCDC data (reboot prevention)
  if (IsOnHVACEQ() && IsAwakeEQ() && IsCANwrite() && !m_can_active)
    {
    smartOBDpolling(true);
    }
  // if charging is in progress, then modify polling to get the DCDC/Charging data (reboot prevention)
  if (IsChargingEQ() && !m_poll_on_charge)
    {
    smartChargeStart();
    }
  } // Ticker 10

void OvmsVehicleSmartEQ::Ticker60(uint32_t ticker) 
  {  
  if(m_12v_charge && !IsOnEQ()) 
    Check12vState();  
  if(m_enable_lock_state && !m_warning_unlocked && StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10) 
    DoorLockState();
  if(m_enable_door_state && !m_warning_dooropen && StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10) 
    DoorOpenState();
  if(IsOnEQ())
    setTPMSValue();   // update TPMS metrics
  if(!Is12VchargeEQ() && StdMetrics.ms_v_charge_12v_voltage->AsFloat(0.0f) > 0.1f)
    StdMetrics.ms_v_charge_12v_voltage->SetValue(0.0f); // reset 12V voltage when not charging to prevent desync

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
  if((IsOnEQ() || IsChargingEQ()) || Is12VchargeEQ())
    {
    float can12V = StdMetrics.ms_v_charge_12v_voltage->AsFloat(0.0f);   // DCDC voltage = mt_evc_dcdc->GetElemValue(1)
    // check for 12V voltage difference between CAN and ADC when the car is rebooted, to detect if ADC factor recalibration is needed
    if(m_check12vadc && can12V >= 13.1f)
      {
      float adc12V = StdMetrics.ms_v_bat_12v_voltage->AsFloat(0.0f);
      float diff = fabs(can12V - adc12V);      
      if (diff > 0.1f && !m_ADCfactor_recalc && Is12VchargeEQ())      
        {
        ESP_LOGW(TAG, "12V voltage difference detected: CAN=%.2fV, ADC=%.2fV, diff=%.2fV", can12V, adc12V, diff);
        m_ADCfactor_recalc_timer = 2;   // wait at least 2 min. before recalculation
        m_enable_calcADCfactor = true;
        m_ADCfactor_recalc = true;      // recalculate ADC factor when 12V voltage difference detected
        }
      m_check12vadc = false;
      }
    // if ADC factor recalculation is enabled, then check if it's time to recalculate the factor
    if (m_enable_calcADCfactor && m_ADCfactor_recalc && can12V >= 13.1f) 
      {
      if (--m_ADCfactor_recalc_timer == 0) 
        {
        m_ADCfactor_recalc = false;
        m_ADCfactor_recalc_timer = 2;
        // calculate new ADC factor         
        if (Is12VchargeEQ())
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
    }
  #endif
  } // Ticker 60

void OvmsVehicleSmartEQ::Ticker3600(uint32_t ticker) 
  { 
  if (mt_12v_trickle_charge_count->AsInt(0) > 0)
    {
    // remove timestamps older than 24h
    time_t now = time(NULL);
    while (!m_12v_trickle_charge_times.empty() && (now - m_12v_trickle_charge_times.front()) > 24 * 3600)
      m_12v_trickle_charge_times.pop_front();
    mt_12v_trickle_charge_count->SetValue((int)m_12v_trickle_charge_times.size());
    }
  } // Ticker 3600

/**
 * PollerStateTicker: check for state changes
 *  This is called by VehicleTicker1() just before the next PollerSend().
 */
void OvmsVehicleSmartEQ::PollerStateTicker(canbus *bus) 
  {
  if (IsAwakeEQ())
    {
    m_candata_poll = true;
    m_candata_timer = SQ_CANDATA_TIMEOUT;
    }
  
  if (m_candata_timer > 0 && --m_candata_timer == 0 && m_candata_poll)
    {
    // Car has gone to sleep
    ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
    m_candata_poll = false;
    m_candata_timer = -1;
    }
  
  // - base system is awake if we've got a fresh lv_pwrstate:
  // use CAN 0x350 state for 12V aux state, because it seems to be more reliable than the 12V voltage for detecting if the car is in accessory mode or not, which is needed for the powermgmt system
  StdMetrics.ms_v_env_aux12v->SetValue(Is12VchargeEQ());
  HandlePollState();
  }
