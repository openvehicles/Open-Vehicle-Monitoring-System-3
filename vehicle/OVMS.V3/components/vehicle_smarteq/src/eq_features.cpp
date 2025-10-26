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

// Set TPMS pressure and alert values
void OvmsVehicleSmartEQ::setTPMSValue(int index, int indexcar) {
  if (index < 0 || index > 3 || indexcar < 0 || indexcar > 3) {
    ESP_LOGE(TAG, "Invalid TPMS index: %d or indexcar: %d", index, indexcar);
    return;
  }

  float _pressure = m_tpms_pressure[index];
  float _temp = m_tpms_temperature[index];
  bool _lowbatt = m_tpms_lowbatt[index];
  bool _missing_tx = m_tpms_missing_tx[index];
  
  // Validate pressure value
  if (_pressure < 0.0f || _pressure > 500.0f) {
    ESP_LOGE(TAG, "Invalid TPMS pressure value: %f", _pressure);
    return;
  }

  StdMetrics.ms_v_tpms_pressure->SetElemValue(indexcar, _pressure);
  if (m_tpms_temp_enable) StdMetrics.ms_v_tpms_temp->SetElemValue(indexcar, _temp);
  
  mt_tpms_pressure->SetElemValue(indexcar, _pressure);
  mt_tpms_temp->SetElemValue(indexcar, _temp);
  mt_tpms_low_batt->SetElemValue(indexcar, _lowbatt);
  mt_tpms_missing_tx->SetElemValue(indexcar, _missing_tx);

  // Skip alert processing if pressure is too low or alerts are disabled
  if (_pressure < 10.0f || !m_tpms_alert_enable) {
    StdMetrics.ms_v_tpms_alert->SetElemValue(indexcar, 0);
    return;
  }

  // Get reference pressure based on front/rear position
  float reference_pressure = (indexcar < 2) ? m_front_pressure : m_rear_pressure;

  // Calculate deviation from reference pressure
  int _alert = 0;
  float deviation = _pressure - reference_pressure;
  float abs_deviation = std::abs(deviation);

  if (_lowbatt) 
    {
    _alert = 1;
    MyNotify.NotifyStringf("alert", "tpms.lowbatt", "TPMS low battery on wheel %d", indexcar + 1);
    }
  else if (_missing_tx) 
    {
    _alert = 2;
    MyNotify.NotifyStringf("alert", "tpms.missing_tx", "TPMS missing transmission on wheel %d", indexcar + 1);
    }
  else if (abs_deviation > m_pressure_alert) 
    {
    _alert = 2;
    }
  else if 
    (abs_deviation > m_pressure_warning) 
    {
    _alert = 1;
    }

  StdMetrics.ms_v_tpms_alert->SetElemValue(indexcar, _alert);
}

// Set TPMS value at boot, cached values are not available
// and we need to set dummy values to avoid alert messages
void OvmsVehicleSmartEQ::setTPMSValueBoot() {
  for (int i = 0; i < 4; i++) {
    StdMetrics.ms_v_tpms_pressure->SetElemValue(i, 0.0f);
    StdMetrics.ms_v_tpms_temp->SetElemValue(i, 0.0f);
    StdMetrics.ms_v_tpms_alert->SetElemValue(i, 0);
  }
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

// Task to check the time periodically
void OvmsVehicleSmartEQ::TimeCheckTask() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  // Check if the current time is the climate time
  if (timeinfo.tm_hour == mt_climate_h->AsInt() && timeinfo.tm_min == mt_climate_m->AsInt() && mt_climate_on->AsBool() && !StdMetrics.ms_v_env_hvac->AsBool()) 
    {
    CommandHomelink(mt_climate_1to3->AsInt());
    }

  // Check if the current day is within the climate days range
  int current_day = timeinfo.tm_wday;
  
  if (current_day == mt_climate_ds->AsInt() && mt_climate_weekly->AsBool() && !m_climate_start_day) 
    {
    m_climate_start_day = true;
    mt_climate_on->SetValue(true);
    }
  if (current_day == mt_climate_de->AsInt() && mt_climate_weekly->AsBool() && m_climate_start_day) 
    {
    m_climate_start_day = false;
    mt_climate_on->SetValue(false);
    }
  if ((!mt_climate_weekly->AsBool() || !mt_climate_on->AsBool()) && m_climate_start_day) 
    {
    m_climate_start_day = false;
    }
}

void OvmsVehicleSmartEQ::TimeBasedClimateData() {
  std::string _oldtime = mt_climate_time->AsString();
  std::string _newdata = mt_climate_data->AsString();
  std::vector<int> _data;
  std::stringstream _ss(_newdata);
  std::string _item, _climate_on, _climate_weekly, _climate_time;
  int _climate_ds, _climate_de, _climate_h, _climate_m;

  while (std::getline(_ss, _item, ',')) 
    {
    _data.push_back(atoi(_item.c_str()));
    }

  // Need at least 7 fields: [trigger,on,weekly,time,ds,de,btn]
  if (_data.size() < 7) 
    {
    ESP_LOGE(TAG, "Invalid climate data payload, need 7 ints, got %u", (unsigned)_data.size());
    mt_climate_data->SetValue("0,0,0,0,-1,-1,-1");             // reset the data
    return;
    }

  if(_data[0]>0 || m_climate_init) 
  {
    m_climate_init = false;
    mt_climate_data->SetValue("0,0,0,0,-1,-1,-1");              // reset the data
    _climate_on = _data[1] == 1 ? "yes" : "no";
    _climate_weekly = _data[2] == 1 ? "yes" : "no";
    if(_data[1]>0) { mt_climate_on->SetValue(_data[1] == 1);}
    if(_data[2]>0) { mt_climate_weekly->SetValue(_data[2] == 1);}
    if(_data[4]>-1) { _climate_ds = _data[4] > 6 ? 0 : _data[4]; mt_climate_ds->SetValue(_climate_ds);}
    if(_data[5]>-1) { _climate_de = _data[5] <= 5 ? _data[5]+1 : 0; mt_climate_de->SetValue(_climate_de);}
    if(_data[6]>-1) { mt_climate_1to3->SetValue(_data[6]);}

    if(_data[3]>0) 
      { 
      if (_data[3] > 2359 || (_data[3] % 100) > 59) 
        {
        ESP_LOGE(TAG,"Invalid HHMM time %d", _data[3]);
        return;
        } 
      else 
        {
        _climate_h = (_data[3] / 100) % 24;                      // Extract hours and ensure 24h format
        _climate_m = _data[3] % 100;                             // Extract minutes
        
        if(_climate_m >= 60) 
          {                                   // Handle invalid minutes
            _climate_h = (_climate_h + _climate_m / 60) % 24;
            _climate_m = _climate_m % 60;
          }

        if (_data[3] >= 0 && _data[3] <= 2359)
          {
          std::ostringstream oss;
          oss << std::setfill('0') << std::setw(4) << _data[3];
          mt_climate_time->SetValue(oss.str());
          } 
        else 
          {
          ESP_LOGE(TAG, "Invalid time value: %d", _data[3]);
          return;
          }

        mt_climate_h->SetValue(_climate_h);
        mt_climate_m->SetValue(_climate_m);
        }
      } 
    else    
      {
      mt_climate_time->SetValue(_oldtime);    
      }     
    
    // booster;no;no;0515;1;6;0
    char buf[64];
    snprintf(buf, sizeof(buf), "booster,%s,%s,%s,%d,%d,%d", _climate_on.c_str(), _climate_weekly.c_str(), mt_climate_time->AsString().c_str(), mt_climate_ds->AsInt(), mt_climate_de->AsInt(), mt_climate_1to3->AsInt());
    StdMetrics.ms_v_gen_mode->SetValue(std::string(buf));
    StdMetrics.ms_v_gen_current->SetValue(3);
    if(m_climate_notify) NotifyClimateTimer();
    }
}

// check the 12V alert periodically and charge the 12V battery if needed
void OvmsVehicleSmartEQ::Check12vState() {
  static const float MIN_VOLTAGE = 10.0f;
  static const int ALERT_THRESHOLD_TICKS = 10;
  
  float mref = StdMetrics.ms_v_bat_12v_voltage_ref->AsFloat();
  float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
  bool alert_on = StdMetrics.ms_v_bat_12v_voltage_alert->AsBool();
  float volt = StdMetrics.ms_v_bat_12v_voltage->AsFloat();
  
  // Validate and update reference voltage
  if (mref > dref) {
      ESP_LOGI(TAG, "Adjusting 12V reference voltage from %.1fV to %.1fV", mref, dref);
      StdMetrics.ms_v_bat_12v_voltage_ref->SetValue(dref);
  }
  
  // Handle alert conditions
  if (alert_on && (volt > MIN_VOLTAGE)) {
      m_12v_ticker++;
      ESP_LOGI(TAG, "12V alert active for %d ticks, voltage: %.1fV", m_12v_ticker, volt);
      
      if (m_12v_ticker > ALERT_THRESHOLD_TICKS) {
          m_12v_ticker = 0;
          ESP_LOGI(TAG, "Initiating climate control due to 12V alert");
          m_12v_charge_state = true;
          m_climate_ticker = 3;
          CommandClimateControl(true);
          m_climate_ticker = 3;
      }
  } else if (m_12v_ticker > 0) {
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

    const int NUM_SAMPLES = MyConfig.GetParamValueInt("xsq", "calc.adcfactor.samples", 10);
    uint16_t samples_adc[NUM_SAMPLES];
    
    // Collect samples
    for (int i = 0; i < NUM_SAMPLES; i++) 
      {
      vTaskDelay(5 / portTICK_PERIOD_MS);
      samples_adc[i] = adc1_get_raw(ADC1_CHANNEL_0);
      }

    // Bubble sort for median calculation
    for (int i = 0; i < NUM_SAMPLES - 1; i++) 
      {
      for (int j = 0; j < NUM_SAMPLES - i - 1; j++) 
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
    if (NUM_SAMPLES % 2 == 0) 
      {
      median_raw = (samples_adc[(NUM_SAMPLES / 2) - 1] + samples_adc[NUM_SAMPLES / 2]) / 2.0f;
      } 
      else 
      {
      median_raw = samples_adc[NUM_SAMPLES / 2];
      }

    // Calculate average of ADC samples
    uint32_t summid = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) 
      {
      summid += samples_adc[i];
      }
    float avg_raw = summid / (float)NUM_SAMPLES;    

    // After collecting samples, don't sort, instead:
    uint16_t min_val = samples_adc[0];          // Smallest value (after sorting)
    uint16_t max_val = samples_adc[NUM_SAMPLES - 1];  // Largest value (after sorting)
    // Trimmed Mean: Remove Min + Max
    float trimmed_avg = (summid - min_val - max_val) / (float)(NUM_SAMPLES - 2);
    
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
      char buf[10];
      sprintf(buf, "1,%d,0,0,-1,-1,-1", bits);
      mt_climate_data->SetValue(std::string(buf));
      return true;
    }
    case 5:
    {
      int bits = atoi(value);
      if(bits < 0) bits = 0;
      if(bits > 2359) bits = 0;
      
      char buf[4];
      snprintf(buf, sizeof(buf), "1,1,0,%04d,-1,-1,-1", bits);
      mt_climate_data->SetValue(std::string(buf));
      return true;
    }
    case 6:
    {
      int bits = atoi(value);
      if(bits < 0) bits = 0;
      if(bits > 2) bits = 2;
      char buf[64];
      snprintf(buf, sizeof(buf), "1,0,0,0,-1,-1,%d", bits);
      mt_climate_data->SetValue(std::string(buf));
      return true;
    }
    case 7:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "resettotal",  (bits& 1)!=0);
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
      MyConfig.SetParamValue("xsq", "suffrange", value);
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
    case 16:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "unlock.warning",  (bits& 1)!=0);
      return true;
    }
    case 17:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xsq", "door.warning",  (bits& 1)!=0);
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
      if(mt_climate_on->AsBool()){return std::string("1");}else{ return std::string("2");};
    case 5:
    {
      return mt_climate_time->AsString();
    }
    case 6:
    {
      int bits = mt_climate_1to3->AsInt();
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 7:
    {
      int bits = m_resettotal?  1 : 0;
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
      int bits = m_suffrange;
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
    case 16:
    {
      int bits = m_enable_lock_state ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 17:
    {
      int bits = m_enable_door_state ?  1 : 0;
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
  }
}
