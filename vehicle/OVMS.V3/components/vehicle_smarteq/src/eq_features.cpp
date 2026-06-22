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
#include "buffered_shell.h"
#include "pcp.h"

// Set TPMS pressure, temperature and alert values
void OvmsVehicleSmartEQ::setTPMSValue() {
  std::vector<string> tpms_layout = OvmsVehicle::GetTpmsLayout();
  int count = (int)tpms_layout.size();
  std::vector<float> tpms_pressure(count, 0.0f);
  std::vector<float> tpms_temp(count, 0.0f);
  std::vector<short> tpms_alert(count, 0);

  static const float PRESSURE_MIN = 10.0f;   // Below this = sensor not working
  static const float PRESSURE_MAX = 500.0f;  // Above this = invalid reading
  static const float TEMP_MIN = -40.0f;
  static const float TEMP_MAX = 90.0f;

  for (int i = 0; i < count; i++)
    {
    int   indexcar      = m_tpms_index[i];
    float pressure      = m_tpms_pressure[indexcar];
    float temp          = m_tpms_temperature[indexcar];
    bool  lowbatt       = m_tpms_lowbatt[indexcar];
    bool  missing_tx    = m_tpms_missing_tx[indexcar];
    bool  pressure_valid = (pressure >= PRESSURE_MIN && pressure < PRESSURE_MAX);

    if (pressure_valid)
      tpms_pressure[i] = pressure;

    if (m_tpms_temp_enable && temp >= TEMP_MIN && temp < TEMP_MAX)
      tpms_temp[i] = temp;

    if (pressure_valid && m_tpms_alert_enable)
      {
      // Sensor working and alerts enabled: update states and compute alert level
      mt_tpms_low_batt->SetElemValue(i, lowbatt);
      mt_tpms_missing_tx->SetElemValue(i, missing_tx);

      float ref_pressure  = (i < (count / 2)) ? m_front_pressure : m_rear_pressure;
      float abs_deviation = std::abs(pressure - ref_pressure);
      short alert         = 0;

      if (lowbatt)
        {
        alert = 1;
        MyNotify.NotifyStringf("alert", "tpms.lowbatt",
                               "TPMS low battery on wheel %s",
                               tpms_layout[i].c_str());
        }
      else if (missing_tx)
        {
        alert = 2;
        MyNotify.NotifyStringf("alert", "tpms.missing_tx",
                               "TPMS missing transmission on wheel %s",
                               tpms_layout[i].c_str());
        }
      else if (abs_deviation > m_pressure_alert)
        {
        alert = 2;
        MyNotify.NotifyStringf("alert", "tpms.alert",
                               "TPMS pressure alert on wheel %s: %.1f kPa (ref: %.1f kPa)",
                               tpms_layout[i].c_str(), pressure, ref_pressure);
        }
      else if (abs_deviation > m_pressure_warning)
        {
        alert = 1;
        MyNotify.NotifyStringf("alert", "tpms.warning",
                               "TPMS pressure warning on wheel %s: %.1f kPa (ref: %.1f kPa)",
                               tpms_layout[i].c_str(), pressure, ref_pressure);
        }
      tpms_alert[i] = alert;
      }
    else
      {
      // Sensor not working or alerts disabled: clear stored alert states
      mt_tpms_low_batt->SetElemValue(i, 0);
      mt_tpms_missing_tx->SetElemValue(i, 0);
      // tpms_alert[i] remains 0 (initialized above)
      }
    } // end for loop

  StdMetrics.ms_v_tpms_pressure->SetValue(tpms_pressure);
  if (m_tpms_temp_enable)
    StdMetrics.ms_v_tpms_temp->SetValue(tpms_temp);
  if (m_tpms_alert_enable)
    StdMetrics.ms_v_tpms_alert->SetValue(tpms_alert);
}

void OvmsVehicleSmartEQ::ResetChargingValues() {
  m_charge_finished = false;
  m_notifySOClimit = false;
  StdMetrics.ms_v_charge_kwh->SetValue(0); // charged Energy
}

void OvmsVehicleSmartEQ::ResetTripCounters() {
  if (m_tripnotify) {
    NotifyTripCounters();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Give notification time
  }
  StdMetrics.ms_v_bat_energy_recd->SetValue(0);
  StdMetrics.ms_v_bat_energy_used->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_recd->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_used->SetValue(0);
  mt_pos_odometer_start->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat());
  StdMetrics.ms_v_pos_trip->SetValue(0);
  StdMetrics.ms_v_charge_kwh_grid->SetValue(0);
}

void OvmsVehicleSmartEQ::ResetTotalCounters() {
  if (m_tripnotify) {
    NotifyTotalCounters();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Give notification time to complete
  }
  StdMetrics.ms_v_bat_energy_recd_total->SetValue(0);
  StdMetrics.ms_v_bat_energy_used_total->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_recd_total->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_used_total->SetValue(0);
  mt_pos_odometer_trip_total->SetValue(0);
  mt_pos_odometer_start_total->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat());
  StdMetrics.ms_v_charge_kwh_grid_total->SetValue(0);
  if (m_resettotal)
    MyConfig.SetParamValueBool("xsq", "resettotal", false);
}

// check the 12V alert periodically and charge the 12V battery if needed
void OvmsVehicleSmartEQ::Check12vState() {
  static const float MIN_VOLTAGE = 10.0f;
  static const int ALERT_THRESHOLD_TICKS = 10;
  
  float mref = StdMetrics.ms_v_bat_12v_voltage_ref->AsFloat();
  bool alert_on = StdMetrics.ms_v_bat_12v_voltage_alert->AsBool();
  float volt = StdMetrics.ms_v_bat_12v_voltage->AsFloat();
  
  // Validate and update reference voltage
  if (mref > m_ref12V) 
    {
    ESP_LOGI(TAG, "Adjusting 12V reference voltage from %.1fV to %.1fV", mref, m_ref12V);
    StdMetrics.ms_v_bat_12v_voltage_ref->SetValue(m_ref12V);
    }
  
  // Handle alert conditions
  if (alert_on && (volt > MIN_VOLTAGE)) 
    {
    m_12v_ticker++;
    ESP_LOGI(TAG, "12V alert active for %d ticks, voltage: %.1fV", m_12v_ticker, volt);
    
    if (m_12v_ticker > ALERT_THRESHOLD_TICKS) 
      {
        m_12v_ticker = 0;
        ESP_LOGI(TAG, "Initiating climate control due to 12V alert");
        CommandClimateControlEQ(true,true,8,true); // Start climate control with restart and 10 minutes duration
      }
    } 
  else if (m_12v_ticker > 0) 
    {
    ESP_LOGI(TAG, "12V alert cleared, resetting ticker");
    m_12v_ticker = 0;
    }
}

void OvmsVehicleSmartEQ::ReCalcADCfactor(float can12V, OvmsWriter* writer) {
  #ifdef CONFIG_OVMS_COMP_ADC
    if (can12V < 11.0f || can12V > 16.0f)
      {
      ESP_LOGW(TAG, "Skip ADC factor recalculation (invalid 12V input %.2f)", can12V);
      if (writer) writer->printf("Skip ADC factor recalculation (invalid 12V input %.2f)\n", can12V);
      return;
      }

    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    uint16_t samples_adc[m_adc_samples];

    // Collect samples and accumulate sum in one pass (sort doesn't affect sum)
    uint32_t summid = 0;
    for (int i = 0; i < m_adc_samples; i++)
      {
      vTaskDelay(5 / portTICK_PERIOD_MS);
      samples_adc[i] = adc1_get_raw(ADC1_CHANNEL_0);
      summid += samples_adc[i];
      }

    // Bubble sort (needed for median)
    for (int i = 0; i < m_adc_samples - 1; i++)
      {
      for (int j = 0; j < m_adc_samples - i - 1; j++)
        {
        if (samples_adc[j] > samples_adc[j + 1])
          {
          uint16_t tmp   = samples_adc[j];
          samples_adc[j] = samples_adc[j + 1];
          samples_adc[j + 1] = tmp;
          }
        }
      }

    // Median (used as the new factor; can12V >= 11.0f is guaranteed above)
    float median_raw = (m_adc_samples % 2 == 0)
      ? (samples_adc[(m_adc_samples / 2) - 1] + samples_adc[m_adc_samples / 2]) / 2.0f
      : samples_adc[m_adc_samples / 2];

    float adc_factor_new  = median_raw / can12V;
    float adc_factor_prev = MyConfig.GetParamValueFloat("system.adc", "factor12v", 195.7f);

    // Diagnostic output (only computed when a writer is attached)
    if (writer)
      {
      uint16_t min_val  = samples_adc[0];
      uint16_t max_val  = samples_adc[m_adc_samples - 1];
      float avg_raw     = summid / (float)m_adc_samples;
      float trimmed_avg = (summid - min_val - max_val) / (float)(m_adc_samples - 2);
      writer->printf("ADC samples (sorted): min=%u max=%u median=%.2f avg=%.2f trimmed=%.2f\n",
                      min_val, max_val, median_raw, avg_raw, trimmed_avg);
      writer->printf("     factor_median=%.3f factor_avg=%.3f factor_trimmed=%.3f\n",
                      adc_factor_new, avg_raw / can12V, trimmed_avg / can12V);
      writer->printf("ADC recalculation: avg_raw=%.2f  V_batt=%.3f  factor=%.3f\n",
                      avg_raw, can12V, adc_factor_new);
      }

    if (adc_factor_new < 160.0f || adc_factor_new > 230.0f)
      {
      if (writer) writer->printf("Reject ADC factor %.3f (out of bounds, keeping %.3f)\n", adc_factor_new, adc_factor_prev);
      return;
      }

    m_adc_factor_history.push_back(adc_factor_prev);
    if (m_adc_factor_history.size() > 10)
      m_adc_factor_history.pop_front();
    float hist[10];
    size_t n = m_adc_factor_history.size();
    for (size_t i = 0; i < n; ++i)
      hist[i] = m_adc_factor_history[i];
    if (n > 0)
      mt_adc_factor_history->SetElemValues(0, n, hist);
    mt_adc_factor->SetValue(adc_factor_new);
    {
      auto lock = MyConfig.Lock();  // single flash write for both keys
      MyConfig.SetParamValueFloat("system.adc", "factor12v", adc_factor_new);
      MyConfig.SetParamValueBool("xsq", "calc.adcfactor", false);
    }
    if (writer) writer->printf("New ADC factor stored: %.3f (prev %.3f, history size %u)\n", adc_factor_new, adc_factor_prev, (unsigned)n);
  #else
    ESP_LOGD(TAG, "ADC support not enabled");
    if (writer) writer->puts("ADC support not enabled");
  #endif
}

void OvmsVehicleSmartEQ::DoorLockState() {
  bool warning_unlocked = (StdMetrics.ms_v_env_parktime->AsInt(0) > m_park_timeout_secs &&
                          !IsOnEQ() &&
                          !StdMetrics.ms_v_env_locked->AsBool(false) &&
                          !m_cmd_locked &&
                          !m_warning_unlocked);
  
  if (warning_unlocked) {
      m_warning_unlocked = true;
      ESP_LOGI(TAG, "Warning: Vehicle is unlocked and parked for more than 10 minutes");
      MyNotify.NotifyString("alert", "vehicle.unlocked", "The vehicle is unlocked and parked for more than 10 minutes.");
  } else if (StdMetrics.ms_v_env_parktime->AsInt(0) > m_park_timeout_secs +10 && !warning_unlocked){
      m_warning_unlocked = true; // prevent warning if the vehicle is parked locked for more than 10 minutes
  }
}

bool OvmsVehicleSmartEQ::DoorOpen() {
  return (StdMetrics.ms_v_door_fl->AsBool(false) ||
          StdMetrics.ms_v_door_fr->AsBool(false) ||
          StdMetrics.ms_v_door_rl->AsBool(false) ||
          StdMetrics.ms_v_door_rr->AsBool(false) ||
          StdMetrics.ms_v_door_trunk->AsBool(false) ||
          StdMetrics.ms_v_door_hood->AsBool(false));
}

void OvmsVehicleSmartEQ::DoorOpenState() {
  bool open_doors = !m_warning_dooropen && DoorOpen();
  if (open_doors) {
      m_warning_dooropen = true;
      ESP_LOGI(TAG, "Warning: Vehicle has open doors");
      MyNotify.NotifyString("alert", "vehicle.open_doors", "The vehicle has open doors.");
  } else if (StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10 && !DoorOpen()){
      m_warning_dooropen = true; // prevent warning if the vehicle is parked locked for more than 10 minutes
  }
}

void OvmsVehicleSmartEQ::smart12VHistory()
{
  // On first call after reboot: restore previous entries from persistent vec metric
  if (m_12v_undervolt_history.empty())
    {
    size_t vn = mt_12v_undervolt_history_vec->GetSize();
    for (size_t i = 0; i < vn && i < 10; ++i)
      {
      float v = mt_12v_undervolt_history_vec->GetElemValue(i);
      if (v > 0.0f)
        {
        char entry[16];
        snprintf(entry, sizeof(entry), "reboot=%.2fV", v);
        m_12v_undervolt_history.push_back(entry);
        }
      }
    }
  float volt = StdMetrics.ms_v_bat_12v_voltage->AsFloat(0.0f);
  time_t ts = (time_t)StdMetrics.ms_m_timeutc->AsInt();
  if (ts < 86400)  // UTC not yet synced (still at/near 1970-01-01)
    {
    ESP_LOGD(TAG, "smart12VHistory: UTC time not synced, skipping entry");
    m_12v_alerted = false;
    return;
    }
  struct tm t;
  localtime_r(&ts, &t);
  char buf[32];
  strftime(buf, sizeof(buf)-8, "%Y-%m-%dT%H:%M:%S", &t);
  snprintf(buf + 19, sizeof(buf) - 19, "=%.2fV", volt);
  m_12v_undervolt_history.push_back(buf);
  if (m_12v_undervolt_history.size() > 10)
    m_12v_undervolt_history.pop_front();
  std::string hist_str;
  float hist_vec[10];
  size_t n = m_12v_undervolt_history.size();
  for (size_t i=0; i<n; ++i)
    {
    if (i > 0) hist_str += '|';
    hist_str += m_12v_undervolt_history[i];
    hist_vec[i] = atof(strchr(m_12v_undervolt_history[i].c_str(), '=') + 1);
    }
  mt_12v_undervolt_history->SetValue(hist_str);
  mt_12v_undervolt_history_vec->SetElemValues(0, n, hist_vec);
}

void OvmsVehicleSmartEQ::smartOn()
{
  // Reset trip values
  if (!m_resettrip)
    ResetTripCounters();
  // Reset kWh/100km values
  if (m_resettotal)
    ResetTotalCounters();
  // Reset warning flags
  m_warning_unlocked = false;
  m_warning_dooropen = false;
  // Reset 12V and climate control tickers
  m_12v_ticker = 0;
  m_climate_restart = false;
  m_climate_restart_ticker = 0;
  // reset idle ticker when vehicle turned on to prevent trigger every 60 sec.
  m_idle_ticker = 15 * 60;
  // canwrite enable write access, only when car is on
  if(IsCANwrite()) 
    {
    smartCoolDownPolling(5);
    smartOBDpolling(true);
    }
  ESP_LOGD(TAG, "smartOn()");
}

void OvmsVehicleSmartEQ::smartOff()
{
  // Reset gear
  StdMetrics.ms_v_env_gear->SetValue(0);
  smartCoolDownPolling();
}

void OvmsVehicleSmartEQ::smartAwake()
{
  smartCoolDownPolling();
  // enable active polling when car wakes up (canwrite only)
  if(m_enable_write)
    smartOBDpolling(true);
  else if (m_enable_write_caron && m_can_active)
    smartOBDpolling(false); // only enable when car is on and CAN write access #2 is enabled
}

void OvmsVehicleSmartEQ::smartSleep()
{  
  smartCoolDownPolling(20);
  // disable active polling when car goes to sleep
  if((m_enable_write_caron && m_can_active) || (m_enable_write_sleep && m_can_active))
    smartOBDpolling(false);
  ESP_LOGD(TAG, "smartSleep()");
}

void OvmsVehicleSmartEQ::smartChargeStart()
{
  smartCoolDownPolling(20);
  if (m_charge_finished)
    {
    ResetChargingValues();
    if (m_resettrip)
      ResetTripCounters();
    }
  m_poll_on_charge = true;
  // Set charging metrics
  StdMetrics.ms_v_charge_pilot->SetValue(true);
  StdMetrics.ms_v_charge_mode->SetValue("standard");
  StdMetrics.ms_v_charge_type->SetValue("type2");
  StdMetrics.ms_v_charge_state->SetValue("charging");
  StdMetrics.ms_v_charge_substate->SetValue("onrequest");
  StdMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
  // trigger ADC factor recalculation when HV charging started
  if(m_enable_calcADCfactor && !m_ADCfactor_recalc) 
    {
    m_ADCfactor_recalc_timer = 2;   // wait at least 2 min. before recalculation
    m_ADCfactor_recalc = true;      // recalculate ADC factor when HV charging
    }
  smartOBDpolling(true);  
  ESP_LOGD(TAG, "smartChargeStart()");
}

void OvmsVehicleSmartEQ::smartChargeStop()
{
  smartCoolDownPolling(20);
  StdMetrics.ms_v_charge_pilot->SetValue(false);
  StdMetrics.ms_v_charge_mode->SetValue("standard");
  StdMetrics.ms_v_charge_type->SetValue("type2");
  StdMetrics.ms_v_charge_duration_full->SetValue(0);
  StdMetrics.ms_v_charge_duration_soc->SetValue(0);
  StdMetrics.ms_v_charge_duration_range->SetValue(0);
  StdMetrics.ms_v_charge_power->SetValue(0);
  StdMetrics.ms_v_charge_current->SetValue(0);
  StdMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());

  if (StdMetrics.ms_v_bat_soc->AsInt() < 95)
    {
    // Assume the charge was interrupted
    ESP_LOGI(TAG,"charge session was interrupted");
    StdMetrics.ms_v_charge_state->SetValue("stopped");
    StdMetrics.ms_v_charge_substate->SetValue("interrupted");
    }
  else 
    {
    // Assume the charge completed normally
    ESP_LOGI(TAG,"charge session completed");
    StdMetrics.ms_v_charge_state->SetValue("done");
    StdMetrics.ms_v_charge_substate->SetValue("onrequest");
    }
  // stop recalculation when HV charging stopped
  m_ADCfactor_recalc_timer = 2;
  m_ADCfactor_recalc = false;
  ESP_LOGD(TAG, "smartChargeStop()");
}

void OvmsVehicleSmartEQ::smartChargePrepare()
{
  ESP_LOGD(TAG, "smartChargePrepare()");
}

void OvmsVehicleSmartEQ::smartChargeFinish()
{
  m_charge_finished = true;
  m_poll_on_charge = false;
  StdMetrics.ms_v_charge_power->SetValue(0);
  ESP_LOGD(TAG, "smartChargeFinish()");
  smartAwake(); // polling cooldown and reload polling list
}

void OvmsVehicleSmartEQ::smartCoolDownPolling(int delay_sec)
{
  m_poll_cooldown = true;
  m_cooldown_ticker = delay_sec;
  PollSetState(POLLSTATE_OFF);
  mt_poll_state->SetValue("Off");
}

void OvmsVehicleSmartEQ::smartOBDpolling(bool activate)
{
  if(!IsCANwrite())
    {
    PollSetPidList(m_can1, NULL);
    m_can_active = false;
    m_poll_on_charge = false;
    ESP_LOGD(TAG, "smartOBDpolling(): CAN bus polling list cleared (write access disabled)");
    return;
    }
    
  m_can_active = activate;
  if (activate)
    ESP_LOGD(TAG, "smartOBDpolling(): CAN bus polling list will be updated");
  else
    ESP_LOGD(TAG, "smartOBDpolling(): CAN bus polling list cleared");
  HandleOBDpolling();
}

/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleSmartEQ::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 1:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "led",  (bits& 1)!=0);
      return true;
    }
    case 2:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "tpms.temp",  (bits& 1)!=0);
      return true;
    }
    case 3:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "resettrip",  (bits& 1)!=0);
      return true;
    }
    case 4:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "resettotal",  (bits& 1)!=0);
      return true;
    }
    case 5:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "12v.charge",  (bits& 1)!=0);
      return true;
    }  
    case 6:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "unlock.warning",  (bits& 1)!=0);
      return true;
    }
    case 7:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "door.warning",  (bits& 1)!=0);
      return true;
    }
    // case 8 -> Vehicle.cpp GPS stream
    // case 9 -> Vehicle.cpp minsoc
    case 10:
    {
      MyConfig.SetParamValue("xsq", "suffsoc", value);
      return true;
    }
    case 11:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "calc.adcfactor",  (bits& 1)!=0);
      return true;
    }
    case 12:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "bcvalue",  (bits& 1)!=0);
      return true;
    }
    case 13:
    {
      MyConfig.SetParamValue("xsq", "full.km", value);
      return true;
    }
    // case 14 -> Vehicle.cpp carbits
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "canwrite",  (bits& 1)!=0);
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
const std::string OvmsVehicleSmartEQ::GetFeature(int key)
{
  switch (key)
  {
    case 1:
    {
      int bits = m_enable_LED_state ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 2:
    {
      int bits = m_tpms_temp_enable ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 3:
    {
      int bits = m_resettrip ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }    
    case 4:
    {
      int bits = m_resettotal?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 5:
    {
      int bits = m_12v_charge ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }   
    case 6:
    {
      int bits = m_enable_lock_state ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 7:
    {
      int bits = m_enable_door_state ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    // case 8 -> Vehicle.cpp stream
    // case 9 -> Vehicle.cpp minsoc
    case 10:
    {
      int bits = m_suffsoc;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 11:
    {
      int bits = m_enable_calcADCfactor ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 12:
    {
      int bits = m_bcvalue ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 13:
    {
      int bits = m_full_km;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    // case 14 -> Vehicle.cpp carbits
    case 15:
    {
      int bits = m_enable_write ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
  }
}
