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

// can can1 tx st 634 40 01 72 00
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandClimateControl(bool enable) {
  if(!m_enable_write)
    {
    ESP_LOGE(TAG, "CommandClimateControl failed / no write access");
    m_climate_restart_ticker = 0;
    m_climate_restart = false; 
    return Fail;
    }

  if(!enable) // HVAC off not implemented by vehicle
    {
    m_climate_restart_ticker = 0;
    m_climate_restart = false; 
    return NotImplemented;
    }

  if (StandardMetrics.ms_v_bat_soc->AsInt(0) < 31)
    {    
    char msg[100];
    snprintf(msg, sizeof(msg), "Scheduled precondition skipped: Battery SOC too low (%d%%)",
             StandardMetrics.ms_v_bat_soc->AsInt(0));
    ESP_LOGW(TAG, "%s", msg);
    MyNotify.NotifyString("alert", "climatecontrol.schedule", msg);
    m_climate_restart_ticker = 0;
    m_climate_restart = false; 
    return Fail;
    }

  if (StdMetrics.ms_v_env_hvac->AsBool()) 
    {
    MyNotify.NotifyString("info", "hvac.enabled", "Climate already on");
    ESP_LOGI(TAG, "CommandClimateControl already on");
    return Success;
    }
  ESP_LOGI(TAG, "CommandClimateControl %s", enable ? "ON" : "OFF");

  OvmsVehicle::vehicle_command_t res;

  if (enable) 
    {
    uint8_t data[4] = {0x40, 0x01, 0x00, 0x00};
    canbus *obd;
    obd = m_can1;

    res = CommandWakeup();
    if (res == Success) 
      {
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      for (int i = 0; i < 10; i++) 
        {
        obd->WriteStandard(0x634, 4, data);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        if (StdMetrics.ms_v_env_hvac->AsBool(false)) 
          {
          ESP_LOGI(TAG, "Climate control started");
          break;
          }
        }     
      res = Success;
      }
    else
      {
      res = Fail;
      }
    }
  else
    {
    res = NotImplemented;
    }

  // fallback to default implementation?
  if (res == NotImplemented) 
    {
    res = OvmsVehicle::CommandClimateControl(enable);
    }
  return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandHomelink(int button, int durationms) {
  // This is needed to enable climate control via Homelink for the iOS app
  ESP_LOGI(TAG, "CommandHomelink button=%d durationms=%d", button, durationms);
  OvmsVehicle::vehicle_command_t res = NotImplemented;

  switch (button)
  {
    case 0:
    {
      res = CommandClimateControl(true);

      if (res == Success)
        {
          m_climate_restart = false;
          m_climate_restart_ticker = 0; // 5 minutes default runtime, no ticker required
          ESP_LOGI(TAG, "Precondition activated successfully");
          MyNotify.NotifyString("info", "climatecontrol.schedule",
                                "5 minutes precondition started");
        }
      else
        {
          ESP_LOGE(TAG, "Failed to activate precondition (result=%d)", res);
          MyNotify.NotifyString("error", "climatecontrol.schedule",
                                "precondition failed to start");
        }
      break;
    }
    case 1:
    {
      res = CommandClimateControl(true);

      if (res == Success)
        {
          m_climate_restart = true;
          m_climate_restart_ticker = 8; // 10 minutes
          ESP_LOGI(TAG, "Precondition activated successfully");
          MyNotify.NotifyString("info", "climatecontrol.schedule",
                                "10 minutes precondition started");
        }
      else
        {
          ESP_LOGE(TAG, "Failed to activate precondition (result=%d)", res);
          MyNotify.NotifyString("error", "climatecontrol.schedule",
                                "precondition failed to start");
        }
      break;
    }
    case 2:
    { 
      res = CommandClimateControl(true);

      if (res == Success)
        {
          m_climate_restart = true;
          m_climate_restart_ticker = 12; // 15 minutes
          ESP_LOGI(TAG, "Precondition activated successfully");
          MyNotify.NotifyString("info", "climatecontrol.schedule",
                                "15 minutes precondition started");
        }
      else
        {
          ESP_LOGE(TAG, "Failed to activate precondition (result=%d)", res);
          MyNotify.NotifyString("error", "climatecontrol.schedule",
                                "precondition failed to start");
        }
      break;
    }
    default:
      res = OvmsVehicle::CommandHomelink(button, durationms);
      break;
  }

  return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandWakeup() {
  if(!m_enable_write) 
    {
    ESP_LOGE(TAG, "CommandWakeup failed: no write access!");
    return Fail;
    }

  ESP_LOGI(TAG, "Send Wakeup Command");

  OvmsVehicle::vehicle_command_t res = Fail;

  if(!mt_bus_awake->AsBool(false)) 
    {
    uint8_t data[4] = {0x40, 0x00, 0x00, 0x00};
    canbus *obd;
    obd = m_can1;

    for (int i = 0; i < 15; i++) 
      {
      obd->WriteStandard(0x634, 4, data);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      if (mt_bus_awake->AsBool(false)) 
        {
        res = Success;
        break;
        }
      }
    mt_bus_awake->SetValue(true);
    res = Success;
    ESP_LOGI(TAG, "Vehicle is now awake");
    } 
  else 
    {
    res = Success;
    ESP_LOGI(TAG, "Vehicle is awake");
    }
  return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandWakeup2() {
  if(!m_enable_write) 
    {
    ESP_LOGE(TAG, "CommandWakeup2 failed: no write access!");
    return Fail;
    }

  ESP_LOGI(TAG, "Send Wakeup Command 2");  

  OvmsVehicle::vehicle_command_t res = Fail;

  if(!mt_bus_awake->AsBool(false)) 
    {
    ESP_LOGI(TAG, "Send Wakeup CommandWakeup2");
    uint8_t data[8] = {0xc3, 0x11, 0x96, 0xef, 0x14, 0x10, 0x96, 0x85};
    canbus *obd;
    obd = m_can1;
    for (int i = 0; i < 10; i++) 
      {
      obd->WriteStandard(0x350, 8, data);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      if (mt_bus_awake->AsBool(false)) 
        {
        res = Success;
        break;
        }
      }
    mt_bus_awake->SetValue(true);    
    res = Success;
    ESP_LOGI(TAG, "Vehicle is awake");
    } 
  else 
    {
    ESP_LOGI(TAG, "Vehicle is awake");
    res = Success;
    }
  return res;
}

// lock: can can1 tx st 745 04 30 01 00 00
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandLock(const char* pin) {
  if(!m_enable_write) {
    ESP_LOGE(TAG, "CommandLock failed / no write access");
    return Fail;
  }
  ESP_LOGI(TAG, "CommandLock");  

  OvmsVehicle::vehicle_command_t res = Fail;

  if(m_indicator) 
    {
    res = CommandCanVector(0x745, 0x765, {"30010000","30010000","30010000","30010000","30010000","30082002"}, false, true);
    }
  else 
    {
    res = CommandCanVector(0x745, 0x765, {"30010000","30010000","30010000","30010000","30010000"}, false, true);
    }
    
  if(res == Success)  
    {
    ESP_LOGI(TAG, "Lock successful");
    StdMetrics.ms_v_env_locked->SetValue(true);
    if (DoorOpen())
        MyNotify.NotifyString("alert", "door.lock", "Vehicle locked, but one or more doors are open!");
    else
        MyNotify.NotifyString("info", "door.lock", "Vehicle locked");
    }
  else
    {
    ESP_LOGE(TAG, "Lock failed");
    }
  return res;
}

// unlock: can can1 tx st 745 04 30 01 00 01
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandUnlock(const char* pin) {

  if(!m_enable_write) {
    ESP_LOGE(TAG, "CommandUnlock failed / no write access");
    return Fail;
  }
  ESP_LOGI(TAG, "CommandUnlock");

  OvmsVehicle::vehicle_command_t res = Fail;

  if(m_indicator) 
    {
    res = CommandCanVector(0x745, 0x765, {"30010001","30010001","30010001","30010001","30010001","30082002"}, false, true);
    }
  else 
    {
    res = CommandCanVector(0x745, 0x765, {"30010001","30010001","30010001","30010001","30010001"}, false, true);
    }

  if(res == Success) 
    {
    ESP_LOGI(TAG, "Unlock successful");
    StdMetrics.ms_v_env_locked->SetValue(false);
    MyNotify.NotifyString("info", "door.unlock", "Vehicle unlocked");
    }
  else
    {
    ESP_LOGE(TAG, "Unlock failed");
    }    
  return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandStat(int verbosity, OvmsWriter* writer) {

  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  std::string charge_state = StdMetrics.ms_v_charge_state->AsString();
  if (chargeport_open && charge_state != "")
    {
    std::string charge_mode = StdMetrics.ms_v_charge_mode->AsString();
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
    else if (charge_state == "timerwait")
      charge_state = "Charge Stopped, Timer On";

    if (charge_mode != "")
      writer->printf("%s - ", charge_mode.c_str());
    writer->printf("%s\n", charge_state.c_str());

    if (show_details)
      {
      // Voltage & current:
      bool show_vc = (StdMetrics.ms_v_charge_voltage->AsFloat() > 0 || StdMetrics.ms_v_charge_current->AsFloat() > 0);
      if (show_vc)
        {
        writer->printf("%s/%s ",
          (char*) StdMetrics.ms_v_charge_voltage->AsUnitString("-", Native, 1).c_str(),
          (char*) StdMetrics.ms_v_charge_current->AsUnitString("-", Native, 1).c_str());
        }

      // Charge speed:
      if (StdMetrics.ms_v_bat_range_speed->IsDefined() && StdMetrics.ms_v_bat_range_speed->AsFloat() != 0)
        {
        writer->printf("%s\n", StdMetrics.ms_v_bat_range_speed->AsUnitString("-", ToUser, 1).c_str());
        }
      else if (show_vc)
        {
        writer->puts("");
        }

      // Estimated time(s) remaining:
      int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
      if (duration_full > 0)
        writer->printf("Full: %d:%02dh\n", duration_full / 60, duration_full % 60);

      int duration_soc = StdMetrics.ms_v_charge_duration_soc->AsInt();
      if (duration_soc > 0)
        writer->printf("%s: %d:%02dh\n",
          (char*) StdMetrics.ms_v_charge_limit_soc->AsUnitString("SOC", ToUser, 0).c_str(),
          duration_soc / 60, duration_soc % 60);

      int duration_range = StdMetrics.ms_v_charge_duration_range->AsInt();
      if (duration_range > 0)
        writer->printf("%s: %d:%02dh\n",
          (char*) StdMetrics.ms_v_charge_limit_range->AsUnitString("Range", ToUser, 0).c_str(),
          duration_range / 60, duration_range % 60);
      }

    // Energy sums:
    /*
    if (StdMetrics.ms_v_charge_kwh_grid->IsDefined())
      {
      writer->printf("Drawn: %s\n",
        StdMetrics.ms_v_charge_kwh_grid->AsUnitString("-", ToUser, 1).c_str());
      }
    */
    if (StdMetrics.ms_v_charge_kwh->IsDefined())
      {
      writer->printf("Charged: %s\n",
        StdMetrics.ms_v_charge_kwh->AsUnitString("-", ToUser, 1).c_str());
      }
    }
  else
    {
    writer->puts("Not charging");
    }

  writer->printf("SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", ToUser, 1).c_str());
/*
  if (StdMetrics.ms_v_bat_range_ideal->IsDefined())
    {
    const std::string& range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("-", ToUser, 0);
    writer->printf("Ideal range: %s\n", range_ideal.c_str());
    }
*/
  if (StdMetrics.ms_v_bat_range_est->IsDefined())
    {
    const std::string& range_est = StdMetrics.ms_v_bat_range_est->AsUnitString("-", ToUser, 0);
    writer->printf("Est. range: %s\n", range_est.c_str());
    }

  if (StdMetrics.ms_v_pos_odometer->IsDefined())
    {
    const std::string& odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", ToUser, 1);
    writer->printf("ODO: %s\n", odometer.c_str());
    }

  if (StdMetrics.ms_v_bat_cac->IsDefined())
    {
    const std::string& cac = StdMetrics.ms_v_bat_cac->AsUnitString("-", ToUser, 1);
    writer->printf("CAC: %s\n", cac.c_str());
    }

  if (StdMetrics.ms_v_bat_soh->IsDefined())
    {
    const std::string& soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0);
    const std::string& health = StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0);
    writer->printf("SOH: %s %s\n", soh.c_str(), health.c_str());
    }
  if (StdMetrics.ms_v_bat_capacity->IsDefined())
    {
    const std::string& hv_energy = StdMetrics.ms_v_bat_capacity->AsUnitString("-", ToUser, 3);
    writer->printf("usable Energy: %s\n", hv_energy.c_str());
    }
  
  if (m_extendedStats)
  { 
    if (mt_reset_consumption->IsDefined())
      {
      const std::string& reset_used = mt_reset_consumption->AsUnitString("-", ToUser, 1);
      writer->printf("RESET kWh/100km: %s\n", reset_used.c_str());
      }
    
    if (mt_reset_distance->IsDefined())
      {
      const std::string& reset_trip_km = mt_reset_distance->AsUnitString("-", ToUser, 1);
      writer->printf("RESET Trip km: %s\n", reset_trip_km.c_str());
      }
    if (mt_reset_time->IsDefined())
      {
      const std::string& reset_time = mt_reset_time->AsUnitString("-", ToUser, 1);
      writer->printf("RESET Trip time: %s\n", reset_time.c_str());
      }
    if (mt_reset_consumption->IsDefined())
      {
      const std::string& reset_consumption = mt_reset_consumption->AsUnitString("-", ToUser, 1);
      writer->printf("START kWh: %s\n", reset_consumption.c_str());
      }
    if (mt_start_distance->IsDefined())
      {
      const std::string& obd_trip_km = mt_start_distance->AsUnitString("-", ToUser, 1);
      writer->printf("START Trip km: %s\n", obd_trip_km.c_str());
      }
    if (mt_start_time->IsDefined())
      {
      const std::string& obd_trip_time = mt_start_time->AsUnitString("-", ToUser, 1);
      writer->printf("START Trip time: %s\n", obd_trip_time.c_str());
      }
    if (StdMetrics.ms_v_env_service_time->IsDefined())
      {
      time_t service_time = StdMetrics.ms_v_env_service_time->AsInt();
      char service_date[32];
      struct tm service_tm;
      localtime_r(&service_time, &service_tm);
      strftime(service_date, sizeof(service_date), "%d-%m-%Y", &service_tm);
      writer->printf("Maintenance Date: %s\n", service_date);
      }
    if (mt_obd_mt_day_usual->IsDefined())
      {
      const std::string& service_at_days = mt_obd_mt_day_usual->AsUnitString("-", ToUser, 1);
      writer->printf("Maintenance in days: %s\n", service_at_days.c_str());
      }
    if (mt_obd_mt_km_usual->IsDefined())
      {
      const std::string& service_at_km = mt_obd_mt_km_usual->AsUnitString("-", ToUser, 1);
      writer->printf("Maintenance in km: %s\n", service_at_km.c_str());
      }
    if (mt_obd_mt_level->IsDefined())
      {
      const std::string& service_at_lvl = mt_obd_mt_level->AsUnitString("-", ToUser, 1);
      writer->printf("Maintenance level: %s\n", service_at_lvl.c_str());
      }
  }
  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandActivateValet(const char* pin) {
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandDeactivateValet(const char* pin) {
  return NotImplemented;
}
 
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandStartCharge()
  {
  return NotImplemented;
  //CommandCanVector(0x7aa, 0x7ad, {"2F90020301","2F90020301"}, false, true);
  //return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandStopCharge()
  {
  return NotImplemented;
  //CommandCanVector(0x7aa, 0x7ad, {"2F90020300","2F90020300"}, false, true);
  //return Success;
  }
