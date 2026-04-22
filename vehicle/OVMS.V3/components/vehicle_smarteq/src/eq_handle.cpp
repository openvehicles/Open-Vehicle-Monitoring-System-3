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
#include "eq_poller.h"

void OvmsVehicleSmartEQ::HandlePollState() {  

  static const char* state_names[] = {"Off", "Awake", "Running", "Charging"};
  static const char* state_disabled = "Pollstate Off (write disabled)";
  if (!IsCANwrite()) 
    {
    if (m_poll_state != POLLSTATE_OFF) 
      {
      PollSetState(POLLSTATE_OFF);
      ESP_LOGD(TAG, "Pollstate Off (write disabled)");
      }
    mt_poll_state->SetValue(state_disabled);
    return;
    }
  
  int desired_state = m_poll_state; // OvmsVehicle::m_poll_state

  if (IsChargingEQ())
    {
    desired_state = POLLSTATE_CHARGING;   //- car is charging
    }
  else if (IsOnEQ())
    {
    desired_state = POLLSTATE_ON;    //- car is on
    }
  else if (IsAwakeEQ())
    {
    desired_state = POLLSTATE_AWAKE;         //- car is awake (but not on)
    }    
  else if (!IsAwakeEQ() && !IsChargingEQ())
    {
    desired_state = POLLSTATE_OFF;        //- car is asleep
    }

  if (desired_state != m_poll_state) 
    {
    PollSetState(desired_state);
    ESP_LOGD(TAG, "Pollstate %s", state_names[desired_state]);
    mt_poll_state->SetValue(state_names[desired_state]);
    if (desired_state == POLLSTATE_OFF)
      {
      smartSleep();                      // switch to liten mode OBDII polling immediately
      }
    else if (desired_state == POLLSTATE_AWAKE)
      {
      smartAwake();                       // switch to active mode OBDII polling immediately
      }
    else if (desired_state == POLLSTATE_ON)
      {
      smartOn();                          // switch to active mode OBDII polling immediately
      }
    else if (desired_state == POLLSTATE_CHARGING)
      {
      smartChargeStart();                 // switch to active mode OBDII polling immediately
      }
    }
}

void OvmsVehicleSmartEQ::HandleOBDpolling() {
  PollSetPidList(m_can1, NULL);
  PollSetThrottling(5);
  PollSetResponseSeparationTime(20);
  HandlePollState();

  // modify Poller..
  m_poll_vector.clear();
  if (!m_can_active)
    {
    ESP_LOGD(TAG, "HandleOBDpolling(): OBD polling disabled (CAN bus not active)");
    return;
    }
  // Pre-allocate capacity to avoid reallocs during insert operations
  // obdii_polls + slow/fast_charger_polls + 2 cell polls + terminator = ~60 entries max
  m_poll_vector.reserve(60);

  // Add PIDs to poll list:
  
  if (m_obdii_79b) // 79b non-cell PIDs
    m_poll_vector.insert(m_poll_vector.end(), obdii_79b_polls, endof_array(obdii_79b_polls));

  if (m_obdii_745) // 745 PIDs Doorlock and VIN
    m_poll_vector.insert(m_poll_vector.end(), obdii_745_polls, endof_array(obdii_745_polls));
       
  if (m_obdii_745_tpms) // full TPMS mode with individual pressure/temp/alert status for each wheel
    m_poll_vector.insert(m_poll_vector.end(), obdii_745_tpms_polls, endof_array(obdii_745_tpms_polls));
  
  if (m_obdii_79b_cell) // 79b PIDs cell V/R/T values with configurable intervals
    {
    for (const auto& p79bcell : obdii_79b_cell_vrt_polls) 
      {
      OvmsPoller::poll_pid_t p79bcell_mod = p79bcell;
      p79bcell_mod.polltime[2] = m_cfg_cell_interval_drv;
      p79bcell_mod.polltime[3] = m_cfg_cell_interval_chg;
      m_poll_vector.push_back(p79bcell_mod);
      }
    }
    
  if (m_obdii_7e4_dcdc) // 7e4 PIDs DCDC 
    m_poll_vector.insert(m_poll_vector.end(), obdii_7e4_dcdc_polls, endof_array(obdii_7e4_dcdc_polls));

  if (m_obdii_7e4) // 7e4 PIDs charging plug, 12V system, and misc
    m_poll_vector.insert(m_poll_vector.end(), obdii_7e4_polls, endof_array(obdii_7e4_polls));
  
  if (m_obdii_743) // 743 PIDs Maintenance data, OBD trip counters
    m_poll_vector.insert(m_poll_vector.end(), obdii_743_polls, endof_array(obdii_743_polls));
  
  if (m_poll_on_charge)
    { 
    if (mt_obl_fastchg->AsBool(false)) 
      {
      m_poll_vector.insert(m_poll_vector.end(), fast_charger_polls, endof_array(fast_charger_polls));
      }
    else 
      {
      m_poll_vector.insert(m_poll_vector.end(), slow_charger_polls, endof_array(slow_charger_polls));
      }
    }

  // Terminate poll list:
  m_poll_vector.push_back(POLL_LIST_END);
  // Release excess capacity to free unused heap memory
  m_poll_vector.shrink_to_fit();
  ESP_LOGD(TAG, "HandleOBDpolling(): Poll vector: size=%d cap=%d", m_poll_vector.size(), m_poll_vector.capacity());
  PollSetPidList(m_can1, m_poll_vector.data());
}


/**
 * Update derived energy metrics while driving
 * Called once per second from Ticker1
 */
void OvmsVehicleSmartEQ::HandleEnergy() {
  float power = StdMetrics.ms_v_bat_power->AsFloat(0.0f); // in kW
  ESP_LOGD(TAG, "HandleEnergy(): power=%.2f kW", power);
  if (power != 0.0f)
    {
    // Update energy used and recovered   
    float energy = fabs(power / 3600.0f);       // 1 second worth of energy in kwh's
    float current_Ah = fabs(StdMetrics.ms_v_bat_current->AsFloat(0.0f) / 3600.0f);   // 1 second worth of current in Ah
    if (power < 0.0f)
      {
      float energy_used = StdMetrics.ms_v_bat_energy_used->AsFloat(0.0f) + energy;
      float energy_used_total = StdMetrics.ms_v_bat_energy_used_total->AsFloat(0.0f) + energy;
      StdMetrics.ms_v_bat_energy_used->SetValue(energy_used);
      StdMetrics.ms_v_bat_energy_used_total->SetValue(energy_used_total);
      float coulomb_used = StdMetrics.ms_v_bat_coulomb_used->AsFloat(0.0f) + current_Ah;
      float coulomb_used_total = StdMetrics.ms_v_bat_coulomb_used_total->AsFloat(0.0f) + current_Ah;
      StdMetrics.ms_v_bat_coulomb_used->SetValue(coulomb_used);
      StdMetrics.ms_v_bat_coulomb_used_total->SetValue(coulomb_used_total);
      }
    else if (power > 0.0f)
      {
      float energy_recd = StdMetrics.ms_v_bat_energy_recd->AsFloat(0.0f) + energy;
      float energy_recd_total = StdMetrics.ms_v_bat_energy_recd_total->AsFloat(0.0f) + energy;
      StdMetrics.ms_v_bat_energy_recd->SetValue(energy_recd);
      StdMetrics.ms_v_bat_energy_recd_total->SetValue(energy_recd_total);
      float coulomb_recd = StdMetrics.ms_v_bat_coulomb_recd->AsFloat(0.0f) + current_Ah;
      float coulomb_recd_total = StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat(0.0f) + current_Ah;
      StdMetrics.ms_v_bat_coulomb_recd->SetValue(coulomb_recd);
      StdMetrics.ms_v_bat_coulomb_recd_total->SetValue(coulomb_recd_total);
      }
    }
}

void OvmsVehicleSmartEQ::HandleTripcounter(){
  ESP_LOGD(TAG, "HandleTripcounter()");
  if (mt_pos_odometer_start->AsFloat(0) == 0 && StdMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0) 
    {
    mt_pos_odometer_start->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat());
    }
  if (IsOnEQ() && StdMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0 && mt_pos_odometer_start->AsFloat(0) > 0.0) 
    {
    StdMetrics.ms_v_pos_trip->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start->AsFloat(0));
    }

  if (mt_pos_odometer_start_total->AsFloat(0) == 0 && StdMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0) 
    {
    mt_pos_odometer_start_total->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat());
    }
  if (IsOnEQ() && StdMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0 && mt_pos_odometer_start_total->AsFloat(0) > 0.0) 
    {
    mt_pos_odometer_trip_total->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start_total->AsFloat(0));
    }
}

void OvmsVehicleSmartEQ::Handlev2Server(){
  // Handle v2Server connection
  if (StdMetrics.ms_s_v2_connected->AsBool()) {
    m_reboot_ticker = m_reboot_time; // set reboot ticker
  }
  else if (m_reboot_ticker > 0 && --m_reboot_ticker == 0) {
    MyNetManager.RestartNetwork();
    m_reboot_ticker = m_reboot_time;
  }
}

/**
 * Update derived metrics when charging
 * Called once per 10 seconds from Ticker10
 */
void OvmsVehicleSmartEQ::HandleCharging() {
  ESP_LOGD(TAG, "HandleCharging()");
  float act_soc         = StdMetrics.ms_v_bat_soc->AsFloat(0.0f);
  float limit_soc       = StdMetrics.ms_v_charge_limit_soc->AsFloat(0.0f);
  float limit_range     = StdMetrics.ms_v_charge_limit_range->AsFloat(0.0f);
  float max_range       = StdMetrics.ms_v_bat_range_full->AsFloat(0.0f);
  float charge_voltage  = StdMetrics.ms_v_bat_voltage->AsFloat(0.0f);  
  float charge_current  = fabs(StdMetrics.ms_v_bat_current->AsFloat(0.0f));
  float energy = fabs(StdMetrics.ms_v_bat_power->AsFloat(0.0f) / 3600.0f);
  float charged = StdMetrics.ms_v_charge_kwh->AsFloat(0.0f);

  // Check if we have what is needed to calculate energy and remaining minutes
  if (charge_voltage > 0.0f && charge_current > 0.0f) 
    {
    // Update energy taken
    StdMetrics.ms_v_charge_kwh->SetValue( charged + energy);
    // If no limits are set, then calculate remaining time to full charge
    if (limit_soc <= 0.0f && limit_range <= 0.0f) 
      {
      // If the charge power is above 12kW, then use the OBD duration value
      if(StdMetrics.ms_v_charge_power->AsFloat(0.0f) > 12.0f)
        {
        StdMetrics.ms_v_charge_duration_full->SetValue(mt_obd_duration->AsInt(0));
        ESP_LOGV(TAG, "Time remaining: %d mins to 100%% soc", mt_obd_duration->AsInt(0));
        } 
      else 
        {
        float soc100 = 100.0f;
        int remaining_soc = calcMinutesRemaining(soc100, charge_voltage, charge_current);
        StdMetrics.ms_v_charge_duration_full->SetValue(remaining_soc, Minutes);
        ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", remaining_soc, soc100);
        }
      }    
    // if limit_soc is set, then calculate remaining time to limit_soc
    if (limit_soc > 0.0f) 
      {
      int minsremaining_soc = calcMinutesRemaining(limit_soc, charge_voltage, charge_current);

      StdMetrics.ms_v_charge_duration_soc->SetValue(minsremaining_soc, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", minsremaining_soc, limit_soc);
      if (act_soc >= limit_soc && !m_notifySOClimit) 
        {
        m_notifySOClimit = true;
        StdMetrics.ms_v_charge_duration_soc->SetValue(0, Minutes);
        ESP_LOGV(TAG, "Time remaining: 0 mins to %0.0f%% soc (already above limit)", limit_soc);
        NotifySOClimit();
        }
      }
    // If limit_range is set, then calculate remaining time to that range
    if (limit_range > 0.0f && max_range > 0.0f) 
      {
      float range_soc           = limit_range / max_range * 100.0f;
      int   minsremaining_range = calcMinutesRemaining(range_soc, charge_voltage, charge_current);

      StdMetrics.ms_v_charge_duration_range->SetValue(minsremaining_range, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins for %0.0f km (%0.0f%% soc)", minsremaining_range, limit_range, range_soc);
      }
    }
}
