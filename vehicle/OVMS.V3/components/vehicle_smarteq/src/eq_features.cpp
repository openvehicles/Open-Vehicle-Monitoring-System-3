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
  std::vector<float> tpms_pressure(count);
  std::vector<float> tpms_temp(count);
  std::vector<short> tpms_alert(count);

  float _threshold_front = m_front_pressure;
  float _threshold_rear = m_rear_pressure;
  float _threshold_warn = m_pressure_warning;
  float _threshold_alert = m_pressure_alert;

  // Pressure validation limits
  static const float PRESSURE_MIN = 10.0f;   // Below this = sensor not working
  static const float PRESSURE_MAX = 500.0f;  // Above this = invalid reading
  static const float TEMP_MIN = -40.0f;
  static const float TEMP_MAX = 150.0f;

  for (int i=0; i < count; i++) 
    {
    int indexcar = m_tpms_index[i];
    
    // Bounds check for indexcar
    if (indexcar < 0 || indexcar >= count)
      {
      ESP_LOGW(TAG, "Invalid TPMS index mapping: %d -> %d (max: %d)", i, indexcar, count-1);
      tpms_pressure[i] = 0.0f;
      tpms_temp[i] = 0.0f;
      tpms_alert[i] = 0;
      continue;
      }

    float _pressure = m_tpms_pressure[indexcar];
    float _temp = m_tpms_temperature[indexcar];
    bool _lowbatt = m_tpms_lowbatt[indexcar];
    bool _missing_tx = m_tpms_missing_tx[indexcar];
    
    short _alert = 0;
    bool _flag = false;

    // Validate pressure value and check if sensor is active
    bool pressure_valid = (_pressure >= PRESSURE_MIN && _pressure < PRESSURE_MAX);
    bool alerts_enabled = m_tpms_alert_enable;
    
    if (pressure_valid)
      {
      tpms_pressure[i] = _pressure;
      _flag = true;
      }
    else
      {
      tpms_pressure[i] = 0.0f;
      }
    
    // Validate and set temperature
    if (m_tpms_temp_enable && _temp >= TEMP_MIN && _temp < TEMP_MAX) 
      {
      tpms_temp[i] = _temp;
      }
    else
      {
      tpms_temp[i] = 0.0f;
      }
    
    // Handle alert conditions only if sensor is working and alerts are enabled
    if (!pressure_valid || !alerts_enabled)
      {
      // Sensor not working or alerts disabled - clear all alerts
      _lowbatt = false;
      _missing_tx = false;
      tpms_alert[i] = 0;
      _flag = false;
      
      // Clear stored alert states
      mt_tpms_low_batt->SetElemValue(indexcar, 0);
      mt_tpms_missing_tx->SetElemValue(indexcar, 0);
      }
    else
      {
      // Sensor working and alerts enabled - update alert states
      mt_tpms_low_batt->SetElemValue(indexcar, _lowbatt);
      mt_tpms_missing_tx->SetElemValue(indexcar, _missing_tx);
      }
    
    // Calculate pressure deviation alerts
    if (alerts_enabled && _flag)
      {
      // Get reference pressure based on front/rear position
      float reference_pressure = (indexcar < (count / 2)) ? _threshold_front : _threshold_rear;
      
      // Calculate deviation from reference pressure      
      float deviation = _pressure - reference_pressure;
      float abs_deviation = std::abs(deviation);
      
      // Priority: low battery > missing transmission > pressure deviation
      if (_lowbatt) 
        {
        _alert = 1;
        MyNotify.NotifyStringf("alert", "tpms.lowbatt", 
                               "TPMS low battery on wheel %s", 
                               tpms_layout[i].c_str());
        }
      else if (_missing_tx) 
        {
        _alert = 2;
        MyNotify.NotifyStringf("alert", "tpms.missing_tx", 
                               "TPMS missing transmission on wheel %s", 
                               tpms_layout[i].c_str());
        }
      else if (abs_deviation > _threshold_alert) 
        {
        _alert = 2;
        MyNotify.NotifyStringf("alert", "tpms.alert", 
                               "TPMS pressure alert on wheel %s: %.1f kPa (ref: %.1f kPa)", 
                               tpms_layout[i].c_str(), _pressure, reference_pressure);
        }
      else if (abs_deviation > _threshold_warn) 
        {
        _alert = 1;
        MyNotify.NotifyStringf("alert", "tpms.warning", 
                               "TPMS pressure warning on wheel %s: %.1f kPa (ref: %.1f kPa)", 
                               tpms_layout[i].c_str(), _pressure, reference_pressure);
        }
      
      tpms_alert[i] = _alert;
      }
    else
      {
      tpms_alert[i] = 0;
      }
    } // end for loop
    
  // Set the metrics  
  StdMetrics.ms_v_tpms_pressure->SetValue(tpms_pressure);
  mt_tpms_pressure->SetValue(tpms_pressure);
  
  if (m_tpms_temp_enable)
    {
    StdMetrics.ms_v_tpms_temp->SetValue(tpms_temp);
    mt_tpms_temp->SetValue(tpms_temp);    
    }
    
  if (m_tpms_alert_enable)
    {
    StdMetrics.ms_v_tpms_alert->SetValue(tpms_alert);
    mt_tpms_alert->SetValue(tpms_alert);    
    }
}

// Set TPMS value at boot, cached values are not available
// and we need to set dummy values to avoid alert messages
void OvmsVehicleSmartEQ::setTPMSValueBoot() {
  // Get actual TPMS layout size (same as in setTPMSValue)
  std::vector<std::string> tpms_layout = GetTpmsLayout();
  int count = (int)tpms_layout.size();
  
  // Initialize with actual count, not hardcoded 4
  std::vector<float> tpms_pressure(count, 0.0f);
  std::vector<float> tpms_temp(count, 0.0f);
  std::vector<short> tpms_alert(count, -1); // -1 indicates unknown at boot
  
  StdMetrics.ms_v_tpms_pressure->SetValue(tpms_pressure);
  StdMetrics.ms_v_tpms_temp->SetValue(tpms_temp);
  StdMetrics.ms_v_tpms_alert->SetValue(tpms_alert);
}

void OvmsVehicleSmartEQ::DisablePlugin(const char* plugin) {
  #ifdef CONFIG_OVMS_COMP_PLUGINS
      if (!ExecuteCommand("plugin disable " + std::string(plugin))) {
          ESP_LOGE(TAG, "Failed to disable plugin: %s", plugin);
          return;
      }
      if (!ExecuteCommand("vfs rm /store/plugins/" + std::string(plugin) + "/*.*")) {
          ESP_LOGE(TAG, "Failed to remove plugin files: %s", plugin);
          return;
      }
  #else
      ESP_LOGW(TAG, "Plugin system not enabled");
  #endif
}

bool OvmsVehicleSmartEQ::ExecuteCommand(const std::string& command) {
  if (command.empty()) {
    ESP_LOGE(TAG,"ExecuteCommand: empty command");
    return Fail;
  }

  std::string result = BufferedShell::ExecuteCommand(command, true);
  bool success = !result.empty();  // Consider command successful if output is not empty
  
  if (!success) {
      ESP_LOGE(TAG, "Failed to execute command: %s", command.c_str());
      return Fail;
  }

  return Success;
}

void OvmsVehicleSmartEQ::ResetChargingValues() {
  m_charge_finished = false;
  m_notifySOClimit = false;
  StdMetrics.ms_v_charge_kwh->SetValue(0); // charged Energy
  //StdMetrics.ms_v_charge_kwh_grid->SetValue(0);
}

void OvmsVehicleSmartEQ::ResetTripCounters() {
  if (m_tripnotify) {
    NotifyTripCounters();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Give notification time
  }
  StdMetrics.ms_v_bat_energy_recd->SetValue(0);
  StdMetrics.ms_v_bat_energy_used->SetValue(0);
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
  mt_pos_odometer_trip_total->SetValue(0);
  mt_pos_odometer_start_total->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat());
  StdMetrics.ms_v_charge_kwh_grid_total->SetValue(0);
  MyConfig.SetParamValueBool("xsq", "resettotal", false);
}

// check the 12V alert periodically and charge the 12V battery if needed
void OvmsVehicleSmartEQ::Check12vState() {
  OvmsVehicle::vehicle_command_t res = NotImplemented;
  static const float MIN_VOLTAGE = 10.0f;
  static const int ALERT_THRESHOLD_TICKS = 10;
  
  float mref = StdMetrics.ms_v_bat_12v_voltage_ref->AsFloat();
  float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
  bool alert_on = StdMetrics.ms_v_bat_12v_voltage_alert->AsBool();
  float volt = StdMetrics.ms_v_bat_12v_voltage->AsFloat();
  
  // Validate and update reference voltage
  if (mref > dref) 
    {
    ESP_LOGI(TAG, "Adjusting 12V reference voltage from %.1fV to %.1fV", mref, dref);
    StdMetrics.ms_v_bat_12v_voltage_ref->SetValue(dref);
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
        res = CommandClimateControl(true);

      if (res == Success)
        {
        m_climate_restart = true;
        m_climate_restart_ticker = 12; // 15 minutes
        ESP_LOGI(TAG, "activated 12V charging successfully");
        Notify12Vcharge();
        }
      else 
        {
        ESP_LOGE(TAG, "Failed to activate 12V charging");
        }
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
    if (can12V < 12.0f || can12V > 15.0f) 
      {
      ESP_LOGW(TAG, "Skip ADC factor recalculation (invalid 12V input %.2f)", can12V);
      if (writer) writer->printf("Skip ADC factor recalculation (invalid 12V input %.2f)\n", can12V);
      return;
      }
      
    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    uint16_t samples_adc[m_adc_samples];
    
    // Collect samples
    for (int i = 0; i < m_adc_samples; i++) 
      {
      vTaskDelay(2 / portTICK_PERIOD_MS);
      samples_adc[i] = adc1_get_raw(ADC1_CHANNEL_0);
      }

    // Bubble sort for median calculation
    for (int i = 0; i < m_adc_samples - 1; i++) 
      {
      for (int j = 0; j < m_adc_samples - i - 1; j++) 
        {
        if (samples_adc[j] > samples_adc[j + 1]) 
          {
          uint16_t temp = samples_adc[j];
          samples_adc[j] = samples_adc[j + 1];
          samples_adc[j + 1] = temp;
          }
        }
      }

    // Calculate median
    float median_raw;
    if (m_adc_samples % 2 == 0) 
      {
      median_raw = (samples_adc[(m_adc_samples / 2) - 1] + samples_adc[m_adc_samples / 2]) / 2.0f;
      } 
      else 
      {
      median_raw = samples_adc[m_adc_samples / 2];
      }

    // Calculate average of ADC samples
    uint32_t summid = 0;
    for (int i = 0; i < m_adc_samples; i++) 
      {
      summid += samples_adc[i];
      }
    float avg_raw = summid / (float)m_adc_samples;    

    // After collecting samples, don't sort, instead:
    uint16_t min_val = samples_adc[0];          // Smallest value (after sorting)
    uint16_t max_val = samples_adc[m_adc_samples - 1];  // Largest value (after sorting)
    // Trimmed Mean: Remove Min + Max
    float trimmed_avg = (summid - min_val - max_val) / (float)(m_adc_samples - 2);
    
    // Calculate average of USM voltage samples
    float V_batt = can12V;

    float adc_factor_median = (V_batt > 12.0f) ? (median_raw / V_batt) : 0.0f;
    float adc_factor_avg = (V_batt > 12.0f) ? (avg_raw / V_batt) : 0.0f;
    float adc_factor_trimmed = (V_batt > 12.0f) ? (trimmed_avg / V_batt) : 0.0f;

    float adc_factor_new = adc_factor_median;
    float adc_factor_prev = MyConfig.GetParamValueFloat("system.adc", "factor12v", 195.7f);

    if (writer) 
      {
      writer->printf("ADC samples (sorted): min=%u max=%u median=%.2f avg=%.2f trimmed=%.2f\n",
                      min_val, max_val, median_raw, avg_raw, trimmed_avg);
      writer->printf("     factor_median=%.3f factor_avg=%.3f factor_trimmed=%.3f\n",
                      adc_factor_median, adc_factor_avg, adc_factor_trimmed);
      }

    if (writer) writer->printf("ADC recalculation: avg_raw=%.2f  V_batt=%.3f  factor=%.3f\n", avg_raw, V_batt, adc_factor_new);
    if (adc_factor_new < 180.0f || adc_factor_new > 210.0f) 
      {
      if (writer) writer->printf("Reject ADC factor %.3f (out of bounds, keeping %.3f)\n", adc_factor_new, adc_factor_prev);
      return;
      }    
    
    m_adc_factor_history.push_back(adc_factor_prev);
    if (m_adc_factor_history.size() > 10)
      m_adc_factor_history.pop_front();
    float hist[10];
    size_t n = m_adc_factor_history.size();
    for (size_t i=0; i<n; ++i)
      hist[i] = m_adc_factor_history[i];
    if (n > 0)
      mt_adc_factor_history->SetElemValues(0, n, hist);
    mt_adc_factor->SetValue(adc_factor_new);
    MyConfig.SetParamValueFloat("system.adc", "factor12v", adc_factor_new);
    if (writer) writer->printf("New ADC factor stored: %.3f (prev %.3f, history size %u)\n", adc_factor_new, adc_factor_prev, (unsigned)n);
  #else
    ESP_LOGD(TAG, "ADC support not enabled");
    if (writer) writer->puts("ADC support not enabled");
  #endif
}

void OvmsVehicleSmartEQ::DoorLockState() {
  bool warning_unlocked = (StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs &&
                          !StdMetrics.ms_v_env_on->AsBool() &&
                          !StdMetrics.ms_v_env_locked->AsBool() &&
                          !m_warning_unlocked);
  
  if (warning_unlocked) {
      m_warning_unlocked = true;
      ESP_LOGI(TAG, "Warning: Vehicle is unlocked and parked for more than 10 minutes");
      MyNotify.NotifyString("alert", "vehicle.unlocked", "The vehicle is unlocked and parked for more than 10 minutes.");
  } else if (StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10 && !warning_unlocked){
      m_warning_unlocked = true; // prevent warning if the vehicle is parked locked for more than 10 minutes
  }
}

void OvmsVehicleSmartEQ::DoorOpenState() {
  bool open_doors = (StdMetrics.ms_v_door_fl->AsBool() ||
                     StdMetrics.ms_v_door_fr->AsBool() ||
                     StdMetrics.ms_v_door_rl->AsBool() ||
                     StdMetrics.ms_v_door_rr->AsBool() ||
                     StdMetrics.ms_v_door_trunk->AsBool() ||
                     StdMetrics.ms_v_door_hood->AsBool()) &&
                     !m_warning_dooropen;

  if (open_doors) {
      m_warning_dooropen = true;
      ESP_LOGI(TAG, "Warning: Vehicle has open doors");
      MyNotify.NotifyString("alert", "vehicle.open_doors", "The vehicle has open doors.");
  } else if (StdMetrics.ms_v_env_parktime->AsInt() > m_park_timeout_secs +10 && !open_doors){
      m_warning_dooropen = true; // prevent warning if the vehicle is parked locked for more than 10 minutes
  }
}

void OvmsVehicleSmartEQ::WifiRestart() {
  #ifdef CONFIG_OVMS_COMP_WIFI
    if (MyPeripherals && MyPeripherals->m_esp32wifi) {
        MyPeripherals->m_esp32wifi->Restart();
        ESP_LOGI(TAG, "WiFi restart initiated");
    } else {
        ESP_LOGE(TAG, "WiFi restart failed - WiFi not available");
    }
  #else
    ESP_LOGE(TAG, "WiFi support not enabled");
  #endif
}


void OvmsVehicleSmartEQ::ModemRestart() {
  #ifdef CONFIG_OVMS_COMP_CELLULAR
    m_modem_ticker = 0; // Reset modem ticker on restart

    if (MyPeripherals && MyPeripherals->m_cellular_modem) {
        MyPeripherals->m_cellular_modem->Restart();
        ESP_LOGI(TAG, "Cellular modem restart initiated");
    } else {
        ESP_LOGE(TAG, "Cellular modem restart failed - modem not available");
    }
  #else
    ESP_LOGE(TAG, "Cellular support not enabled");
  #endif
}

void OvmsVehicleSmartEQ::ModemEventRestart(std::string event, void* data) {
  if (!m_modem_check) {
    // ESP_LOGE(TAG, "Modem auto restart is disabled");
    return;
  }

  ModemRestart();
  ESP_LOGI(TAG, "Modem event '%s' triggered a modem restart", event.c_str());
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
