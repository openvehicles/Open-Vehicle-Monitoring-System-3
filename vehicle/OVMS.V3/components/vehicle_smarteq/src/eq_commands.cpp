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

// CommandCanVector(txid, rxid, hexbytes = {"30010000","30082002"}, reset CAN = true/false, CommandWakeup = true/ CommandWakeup2 = false)
OvmsVehicle::vehicle_command_t  OvmsVehicleSmartEQ::CommandCanVector(uint32_t txid,uint32_t rxid, std::vector<std::string> hexbytes,bool reset,bool wakeup) {
  if(!m_enable_write) 
    {
    ESP_LOGE(TAG, "CommandCanVector failed / no write access");
    return Fail;
    }

  if(m_ddt4all_exec > 1) 
    {
    ESP_LOGE(TAG, "DDT4all command rejected - previous command still processing (%d seconds remaining)",m_ddt4all_exec);
    return Fail;
    }

  m_ddt4all_exec = 10; // 10 seconds delay for next DDT4ALL command execution

  ESP_LOGI(TAG, "CommandCanVector");

  std::string request;
  std::string response;

  int vecsize = hexbytes.size();
  if (vecsize == 0) 
    {
    ESP_LOGE(TAG, "Empty hexbytes vector");
    m_ddt4all_exec = 5; // reduce cooldown on error
    return Fail;
    }
  // Validate all hex strings before starting
  for (int i = 0; i < vecsize; i++) 
    {
    if (hexbytes[i].size() % 2 != 0) 
      {
      ESP_LOGE(TAG, "Invalid hex length at index %d", i);
      m_ddt4all_exec = 5; // reduce cooldown on error
      return Fail;
      }
    }

  mt_canbyte->SetValue(hexbytes[0].c_str());

  OvmsVehicle::vehicle_command_t res = Fail;
  res = wakeup ? CommandWakeup() : CommandWakeup2();
  
  if (res == Success)
    {
    PollSetState(POLLSTATE_ON);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if (!mt_bus_awake->AsBool(false)) 
      {
      ESP_LOGE(TAG, "vehicle not awake");
      m_ddt4all_exec = 5; // reduce cooldown on error
      return Fail;
      }

    uint8_t protocol = ISOTP_STD;
    int timeout_ms = 200;
    int err = 0;

    request = hexdecode("10C0");
    PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);
    
    vTaskDelay(200 / portTICK_PERIOD_MS);
    for (int i = 0; i < vecsize; i++) 
      {
      request = hexdecode(hexbytes[i]);
      err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);    
      vTaskDelay(200 / portTICK_PERIOD_MS);
      }

    if (reset) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      m_ddt4all_exec = 30; // 30 seconds delay for next DDT4ALL command execution
      request = hexdecode("1103");  // key on/off
      PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);
    }

    if (err == POLLSINGLE_TXFAILURE)
      {
      ESP_LOGD(TAG, "ERROR: transmission failure (CAN bus error)");
      return Fail;
      }
    else if (err < 0)
      {
      ESP_LOGD(TAG, "ERROR: timeout waiting for poller/response");
      return Fail;
      }
    else if (err)
      {
      ESP_LOGD(TAG, "ERROR: request failed with response error code %02X\n", err);
      return Fail;
      }
    return Success;
    } 
  else
    {
    m_ddt4all_exec = 5; // reduce cooldown on error
    MyNotify.NotifyString("error", "CommandCanVector.fail","Command failed during wakeup");
    return res;
    }
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
    }
  else
    {
    ESP_LOGE(TAG, "Unlock failed");
    }    
  return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandActivateValet(const char* pin) {
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandDeactivateValet(const char* pin) {
  return NotImplemented;
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

/**
 * ProcessMsgCommand: V2 compatibility protocol message command processing
 *  result: optional payload or message to return to the caller with the command response
 */
OvmsVehicleSmartEQ::vehicle_command_t OvmsVehicleSmartEQ::ProcessMsgCommand(string& result, int command, const char* args)
{
  switch (command)
  {
    case CMD_SetChargeAlerts:
      return MsgCommandCA(result, command, args);

    default:
      return NotImplemented;
  }
}

/**
 * MsgCommandCA:
 *  - CMD_SetChargeAlerts(<soc>)
 */
OvmsVehicleSmartEQ::vehicle_command_t OvmsVehicleSmartEQ::MsgCommandCA(std::string &result, int command, const char* args)
{
  if (command == CMD_SetChargeAlerts && args && *args)
  {
    std::istringstream sentence(args);
    std::string token;
    
    // CMD_SetChargeAlerts(<soc limit>)
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xsq", "suffsoc", atoi(token.c_str()));
    
    // Synchronize with config changes:
    ConfigChanged(NULL);
  }
  return Success;
}
  
/**
 * writer for command line interface
 */
void OvmsVehicleSmartEQ::xsq_wakeup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;

  smarteq->CommandWakeup2();
}
void OvmsVehicleSmartEQ::xsq_trip_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
	
  smarteq->CommandTripStart(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTripStart(int verbosity, OvmsWriter* writer) {
    metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
    writer->puts("Trip start values:");
    writer->printf("  distance: %s\n", (char*) mt_start_distance->AsUnitString("-", rangeUnit, 1).c_str());
    writer->printf("  time: %s\n", (char*) mt_start_time->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
    writer->printf("  CAC: %s\n", (char*) StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyTripStart() {
  StringWriter buf(200);
  CommandTripStart(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.trip.start",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_trip_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
  smarteq->CommandTripReset(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTripReset(int verbosity, OvmsWriter* writer) {
    metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
    writer->puts("Trip reset values:");
    writer->printf("  distance: %s\n", (char*) mt_start_distance->AsUnitString("-", rangeUnit, 1).c_str());
    writer->printf("  time: %s\n", (char*) mt_start_time->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
    writer->printf("  CAC: %s\n", (char*) StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyTripReset() {
  StringWriter buf(200);
  CommandTripReset(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.trip.reset",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_trip_counters(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
	
    smarteq->CommandTripCounters(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTripCounters(int verbosity, OvmsWriter* writer) {
    metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
    writer->puts("Trip counter values:");
    writer->printf("  distance: %s\n", (char*) StdMetrics.ms_v_pos_trip->AsUnitString("-", rangeUnit, 1).c_str());
	  writer->printf("  Energy used: %s\n", (char*) StdMetrics.ms_v_bat_energy_used->AsUnitString("-", Native, 3).c_str());
	  writer->printf("  Energy recd: %s\n", (char*) StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", Native, 3).c_str());
    writer->printf("  Energy grid: %s\n", (char*) StdMetrics.ms_v_charge_kwh_grid->AsUnitString("-", Native, 3).c_str());
    writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
    writer->printf("  CAC: %s\n", (char*) StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyTripCounters() {
  StringWriter buf(200);
  CommandTripCounters(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.trip.counters",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_trip_total(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
    smarteq->CommandTripTotal(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTripTotal(int verbosity, OvmsWriter* writer) {
    metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
    writer->puts("Trip total values:");
    writer->printf("  distance: %s\n", (char*) mt_pos_odometer_trip_total->AsUnitString("-", rangeUnit, 1).c_str());
	  writer->printf("  Energy used: %s\n", (char*) StdMetrics.ms_v_bat_energy_used_total->AsUnitString("-", Native, 3).c_str());
	  writer->printf("  Energy recd: %s\n", (char*) StdMetrics.ms_v_bat_energy_recd_total->AsUnitString("-", Native, 3).c_str());
    writer->printf("  Energy grid: %s\n", (char*) StdMetrics.ms_v_charge_kwh_grid_total->AsUnitString("-", Native, 3).c_str());
    writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
    writer->printf("  CAC: %s\n", (char*) StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyTotalCounters() {
  StringWriter buf(200);
  CommandTripTotal(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.trip.total",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_maintenance(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
    smarteq->CommandMaintenance(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandMaintenance(int verbosity, OvmsWriter* writer) {
    writer->puts("Maintenance values:");    
    if (StdMetrics.ms_v_env_service_time->IsDefined()) {
      time_t service_time = StdMetrics.ms_v_env_service_time->AsInt();
      char service_date[32];
      struct tm service_tm;
      localtime_r(&service_time, &service_tm);
      strftime(service_date, sizeof(service_date), "%d.%m.%Y", &service_tm);
      writer->printf("  service date: %s\n", service_date);
    }
    writer->printf("  days: %d\n", mt_obd_mt_day_usual->AsInt());
    writer->printf("  level: %s\n", mt_obd_mt_level->AsString().c_str());
    writer->printf("  km: %d\n", mt_obd_mt_km_usual->AsInt());    
    writer->printf("  remaining HV contactor cycles: %d / %d\n", 
                   mt_bms_contactor_cycles->GetElemValue(1), 
                   mt_bms_contactor_cycles->GetElemValue(0));
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyMaintenance() {
  StringWriter buf(200);
  CommandMaintenance(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.maintenance",buf.c_str());
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandSOClimit(int verbosity, OvmsWriter* writer) {
  writer->puts("SOC limit reached:");
  writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("  CAC: %s\n", (char*) StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
  writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
  return Success;
}
void OvmsVehicleSmartEQ::NotifySOClimit() {
  StringWriter buf(200);
  CommandSOClimit(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.soclimit",buf.c_str());
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::Command12Vcharge(int verbosity, OvmsWriter* writer) {
  writer->puts("12V charge on:");
 
  writer->printf("  12V: %s\n", (char*) StdMetrics.ms_v_bat_12v_voltage->AsUnitString("-", Native, 1).c_str());
  writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("  CAC: %s\n", (char*) StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
  writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
  return Success;
}
void OvmsVehicleSmartEQ::Notify12Vcharge() {
  StringWriter buf(200);
  Command12Vcharge(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.12v.charge",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_tpms_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
    smarteq->CommandTPMSset(verbosity, writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTPMSset(int verbosity, OvmsWriter* writer) {
  float dummy_pressure = mt_dummy_pressure->AsFloat();
  writer->printf("Setting dummy TPMS values (base pressure: %.1f kPa)\n", dummy_pressure);
  
  for (int i = 0; i < 4; i++) {
    m_tpms_pressure[i] = dummy_pressure + (i * 10); // kPa
    m_tpms_temperature[i] = 21 + i; // Celsius
    m_tpms_lowbatt[i] = false;
    m_tpms_missing_tx[i] = false;
    writer->printf("  Sensor %d: pressure=%.1f kPa, temp=%.1f°C\n", 
                   i, m_tpms_pressure[i], m_tpms_temperature[i]);
  }
  
  setTPMSValue();   // update TPMS metrics
  
  // Show what was actually set in metrics
  writer->puts("\nAfter setTPMSValue():");
  auto pressure = StdMetrics.ms_v_tpms_pressure->AsVector();
  auto temp = StdMetrics.ms_v_tpms_temp->AsVector();
  for (size_t i = 0; i < pressure.size(); i++) {
    writer->printf("  Wheel %d: pressure=%.1f kPa, temp=%.1f°C\n", 
                   (int)i, pressure[i], temp[i]);
  }
  
  writer->puts("TPMS dummy values set");
  return Success;
}

void OvmsVehicleSmartEQ::xsq_calc_adc(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  #ifdef CONFIG_OVMS_COMP_ADC
  if (argc == 1) 
    {
    char* endp = nullptr;
    double val = strtod(argv[0], &endp);
    if (endp == argv[0] || *endp != 0) 
      {
      writer->puts("Error: invalid number");
      return;
      }
    if (val < 12.0 || val > 15.0) 
      {
      writer->puts("Error: voltage out of plausible range (12.0–15.0)");
      return;
      }
    writer->printf("Recalculating ADC factor using override voltage %.2fV\n", val);
    smarteq->ReCalcADCfactor((float)val, writer);
    return;
    } 
  else if (argc == 0) 
    {
    if (!smarteq->m_enable_write) 
      {
      writer->puts("Error: no write access");
      return;
      }

    if (!StdMetrics.ms_v_env_charging12v->AsBool(false)) 
      {
      writer->puts("Error: vehicle 12V DC-DC converter not active");
      return;
      } 
        
    float can12V = smarteq->mt_evc_dcdc->GetElemValue(1);   // DCDC voltage
    if (can12V <= 13.10f) 
      {
      writer->puts("Error: vehicle 12V is not charging, 12V voltage is not stable for ADC calibration!");
      return;
      }
    writer->printf("Recalculating ADC factor using vehicle 12V voltage %.2fV\n", can12V);
    smarteq->ReCalcADCfactor(can12V, writer);
    return;
   }
  #else
    writer->puts("ADC component not enabled");
    return;
  #endif
}

void OvmsVehicleSmartEQ::xsq_preset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;

  smarteq->CommandPreset(verbosity, writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandPreset(int verbosity, OvmsWriter* writer) {
  auto lock = MyConfig.Lock();
  auto map_xsq = MyConfig.GetParamMap("xsq");

  // Update xsq preset version
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", PRESET_VERSION);
  map_xsq["cfg.preset.ver"] = buf;

  // Set default 12V alert threshold if not defined
  if (!MyConfig.IsDefined("vehicle", "12v.alert")) 
    {
    MyConfig.SetParamValueFloat("vehicle", "12v.alert", 0.8); // set default 12V alert threshold to 0.8V for Check12V System
    }

  if (!MyConfig.IsDefined("modem", "gps.parkpause")) 
    {
    MyConfig.SetParamValue("modem", "gps.parkpause", "600"); // default 10 min.
    MyConfig.SetParamValue("modem", "gps.parkreactivate", "50"); // default 50 min.
    MyConfig.SetParamValue("modem", "gps.parkreactlock", "5"); // default 5 min.
    MyConfig.SetParamValue("vehicle", "stream", "10"); // set stream to 10 sec.
    }
  
  if (!MyConfig.IsDefined("modem", "gps.parkreactawake")) 
    {
    MyConfig.SetParamValueBool("modem", "gps.parkreactawake", true);
    }

  if (!MyConfig.IsDefined("modem", "net.type")) 
    {
    MyConfig.SetParamValue("modem", "net.type", "4G");
    }

  if (!MyConfig.IsDefined("network", "wifi.ap2client.enable")) 
    {
    MyConfig.SetParamValueBool("network", "wifi.ap2client.enable", true);
    }

  if (MyConfig.IsDefined("network", "type")) 
    {
    MyConfig.DeleteInstance("network", "type");
    }

  if (MyConfig.IsDefined("xsq", "TPMS_FL")) 
    {
    MyConfig.SetParamValueInt("vehicle", "tpms.fl", MyConfig.GetParamValueInt("xsq", "TPMS_FL", 0));
    MyConfig.SetParamValueInt("vehicle", "tpms.fr", MyConfig.GetParamValueInt("xsq", "TPMS_FR", 1));
    MyConfig.SetParamValueInt("vehicle", "tpms.rl", MyConfig.GetParamValueInt("xsq", "TPMS_RL", 2));
    MyConfig.SetParamValueInt("vehicle", "tpms.rr", MyConfig.GetParamValueInt("xsq", "TPMS_RR", 3));
    }

  if (MyConfig.GetParamValue("ota", "server", "0") == "https://ovms.dimitrie.eu/firmware/ota" && 
      MyConfig.GetParamValue("ota", "tag", "0") == "smarteq")
    {
    MyConfig.SetParamValue("ota", "server", "https://ovms.dexters-web.de/firmware/ota");
    MyConfig.SetParamValue("ota", "tag", "edge");
    }

  // Remove deprecated preclimate section
  if (MyConfig.CachedParam("xsq.preclimate"))
    MyConfig.DeregisterParam("xsq.preclimate");

  // Delete old config entries - reuse existing map_xsq
  // List of deprecated keys to remove
  const char* deprecated_keys[] = {
    "ios_tpms_fix",
    "restart.wakeup",
    "v2.check",
    "12v.measured.offset",
    "12v.measured.BMS.offset",
    "modem.net.type",
    "booster.1to3",
    "booster.de",
    "booster.ds",
    "booster.h",
    "booster.m",
    "booster.on",
    "booster.system",
    "booster.weekly",
    "booster.time",
    "ddt4all",
    "gps.onoff",
    "gps.off",
    "gps.reactmin",
    "precondition",
    "climate.system",
    "climate.data",
    "climate.notify",
    "climate.data.store",
    "gps.deact",
    "modem.threshold",
    "modem.check",
    "TPMS_FL",
    "TPMS_FR",
    "TPMS_RL",
    "TPMS_RR",
    "lock.byte",
    "unlock.byte",
    "indicator",
    "adc.samples"
  };
  
  // Remove all deprecated keys from map
  int removed_count = 0;
  for (const char* key : deprecated_keys) {
    auto it = map_xsq.find(key);
    if (it != map_xsq.end()) 
      {
      map_xsq.erase(it);
      removed_count++;
      }
    }
  
  // Write all changes in transactions (one per config section)
  MyConfig.SetParamMap("xsq", map_xsq);

  if (writer) 
    {
    writer->printf("Config V%d preset activated", PRESET_VERSION);
    if (removed_count > 0) 
      {
      writer->printf("Removed %d deprecated config entries\n", removed_count);
      ESP_LOGI(TAG, "Removed %d deprecated config entries", removed_count);
      }
    }

  return Success;
}

void OvmsVehicleSmartEQ::xsq_setdefault(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;

  smarteq->CommandSetDefault(verbosity, writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandSetDefault(int verbosity, OvmsWriter* writer) {
  auto lock = MyConfig.Lock();
  
  // Clear all xsq config entries
  ConfigParamMap empty_map;
  MyConfig.SetParamMap("xsq", empty_map);

  MyConfig.SetParamValueBool("auto", "init", true);
  MyConfig.SetParamValueBool("auto", "ota", false);
  MyConfig.SetParamValueBool("auto", "modem", true);
  MyConfig.SetParamValueBool("auto", "server.v2", true);

  MyConfig.SetParamValue("modem", "net.type", "4G");
  MyConfig.SetParamValueBool("modem", "gps.parkreactawake", true);
  MyConfig.SetParamValue("modem", "gps.parkpause", "600"); // default 10 min.
  MyConfig.SetParamValue("modem", "gps.parkreactivate", "50"); // default 50 min.
  MyConfig.SetParamValue("modem", "gps.parkreactlock", "5"); // default 5 min.

  MyConfig.SetParamValue("vehicle", "stream", "10"); // set stream to 10 sec.
  MyConfig.SetParamValueFloat("vehicle", "12v.alert", 0.8); // set default 12V alert threshold to 0.8V for Check12V System
  
  MyConfig.SetParamValue("ota", "server", "https://ovms.dexters-web.de/firmware/ota");
  MyConfig.SetParamValue("ota", "tag", "main");
  MyConfig.SetParamValue("ota", "http.mru", "https://ovms.dexters-web.de/firmware/ota/edge/ovms.bin");
  
  MyConfig.SetParamValueBool("network", "wifi.ap2client.enable", true);
  MyConfig.DeleteInstance("network", "wifi.bad.reconnect");
  MyConfig.DeleteInstance("network", "wifi.sq.good");
  MyConfig.DeleteInstance("network", "wifi.sq.bad");
  MyConfig.DeleteInstance("network", "modem.sq.good");
  MyConfig.DeleteInstance("network", "modem.sq.bad");
  
  if (writer) 
    {
    writer->puts("SmartEQ config reset to defaults");
    ESP_LOGI(TAG, "SmartEQ config reset to defaults");
    }
  
  return Success;
}

void OvmsVehicleSmartEQ::xsq_ed4scan(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;

  smarteq->CommandED4scan(verbosity, writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandED4scan(int verbosity, OvmsWriter* writer) {
  writer->puts("=== ED4scan-like BMS/EVC/OBL Data Output ===\n");
  writer->printf("  HV Batt Serialnumber:    %s\n", mt_bat_serial->AsString().c_str());
  writer->printf("  BMS Serialnumber:        %s\n", mt_bms_prod_data->AsString().c_str());
  writer->printf("  EVC Traceability:        %s\n", mt_evc_traceability->AsString().c_str());
  writer->printf("  Battery Mileage:         %.0f km\n", mt_bms_mileage->AsFloat());
  writer->printf("  Odometer:                %.0f km\n", StdMetrics.ms_v_pos_odometer->AsFloat()); 

  writer->puts("--- Battery Health (PID 0x61) ---");
  writer->printf("  State of Health:         %.0f%%\n", mt_bms_soh->AsFloat());
  writer->printf("  Usable Capacity:         %.2f Ah\n", mt_bms_cap->GetElemValue(0));
  
  writer->puts("\n--- HV Contactor Cycles (PID 0x02) ---");
  writer->printf("  Max Cycles:              %d\n", mt_bms_contactor_cycles->GetElemValue(0));
  writer->printf("  remaining Cycles:        %d\n", mt_bms_contactor_cycles->GetElemValue(1));
  
  writer->puts("\n--- SOC Kernel Data (PID 0x08) ---");
  writer->printf("  Open Circuit Voltage:    %.2f V\n", mt_bms_voltages->GetElemValue(5));
  writer->printf("  Real SOC (Min):          %.2f%%\n", mt_bms_soc_values->GetElemValue(2));
  writer->printf("  Real SOC (Max):          %.2f%%\n", mt_bms_soc_values->GetElemValue(3));
  writer->printf("  Kernel SOC:              %.2f%%\n", mt_bms_soc_values->GetElemValue(0));
  writer->printf("  Capacity Loss:           %.2f%%\n", mt_bms_cap->GetElemValue(3));
  writer->printf("  Initial Capacity:        %.2f Ah\n", mt_bms_cap->GetElemValue(1));
  writer->printf("  Estimated Capacity:      %.2f Ah\n", mt_bms_cap->GetElemValue(2));
  writer->printf("  Voltage State:           %s\n", mt_bms_voltage_state->AsString().c_str());
  
  writer->puts("\n--- SOC Recalibration (PID 0x25) ---");
  writer->printf("  Recalibration State:     %s\n", mt_bms_soc_recal_state->AsString().c_str());
  writer->printf("  Display SOC:             %.2f%%\n", mt_bms_soc_values->GetElemValue(4));
  
  writer->puts("\n--- Battery State (PID 0x07) ---");
  writer->printf("  Cell Voltage Min:        %.3f V\n", mt_bms_voltages->GetElemValue(0));
  writer->printf("  Cell Voltage Max:        %.3f V\n", mt_bms_voltages->GetElemValue(1));
  writer->printf("  Cell Voltage Mean:       %.3f V\n", mt_bms_voltages->GetElemValue(2));
  writer->printf("  Link Voltage:            %.2f V\n", mt_bms_voltages->GetElemValue(3));
  writer->printf("  Pack Voltage:            %.2f V\n", mt_bms_voltages->GetElemValue(4));
  writer->printf("  Pack Current:            %.2f A\n", StdMetrics.ms_v_bat_current->AsFloat());
  writer->printf("  Pack Power:              %.2f kW\n", StdMetrics.ms_v_bat_power->AsFloat());
  writer->printf("  Contactor State:         %s\n", mt_bms_HVcontactStateTXT->AsString().c_str());
  writer->printf("  Vehicle Mode:            %s\n", mt_bms_EVmode_txt->AsString().c_str());
  writer->printf("  BMS 12V Voltage:         %.2f V\n", mt_bms_voltages->GetElemValue(6));

  writer->puts("\n--- EVC Data (0x7EC) ---");
  writer->printf("  HV Energy:               %.3f kWh\n", StdMetrics.ms_v_bat_capacity->AsFloat());
  writer->printf("  DCDC Active Request:     %s\n", StdMetrics.ms_v_env_charging12v->AsBool() ? "Yes" : "No");
  writer->printf("  DCDC Current:            %.2f A\n", mt_evc_dcdc->GetElemValue(6));
  writer->printf("  DCDC Power:              %.0f W\n", mt_evc_dcdc->GetElemValue(2));
  writer->printf("  DCDC Load:               %.2f%%\n", mt_evc_dcdc->GetElemValue(7));
  writer->printf("  Req (int) 12V Voltage:   %.2f V\n", mt_evc_dcdc->GetElemValue(5));
  writer->printf("  DCDC Voltage Request:    %.2f V\n", mt_evc_dcdc->GetElemValue(0));
  writer->printf("  DCDC 12V Voltage:        %.2f V\n", mt_evc_dcdc->GetElemValue(1));
  writer->printf("  USM 12V Voltage:         %.2f V\n", mt_evc_dcdc->GetElemValue(3));
  writer->printf("  Batt 12V Voltage:        %.2f V\n", mt_evc_dcdc->GetElemValue(4));

  writer->puts("\n--- OBL Charger Data (0x793) ---");
  writer->printf("  Fast Charge Active:      %s\n", mt_obl_fastchg->AsBool() ? "Yes" : "No");  
  writer->printf("  Max AC Current:          %.0f A\n", mt_obl_misc->GetElemValue(2));
  writer->printf("  Mains Frequency:         %.2f Hz\n", mt_obl_misc->GetElemValue(0));
  writer->printf("  Ground Resistance:       %.4f Ohm\n", mt_obl_misc->GetElemValue(1));
  writer->printf("  Leakage DC Current:      %.4f mA\n", mt_obl_misc->GetElemValue(3));
  writer->printf("  Leakage HF10kHz:         %.4f mA\n", mt_obl_misc->GetElemValue(4));
  writer->printf("  Leakage HF Current:      %.4f mA\n", mt_obl_misc->GetElemValue(5));
  writer->printf("  Leakage LF Current:      %.4f mA\n", mt_obl_misc->GetElemValue(6));
  writer->printf("  Leakage Diagnostic:      %s\n", mt_obl_main_leakage_diag->AsString().c_str());

  int show = mt_ed4_values->AsInt(10); // number of cells to show

  writer->puts("\n--- Cell Voltage Data (PID 0x41/0x42) ---");
  const std::vector<float> voltage_values = StdMetrics.ms_v_bat_cell_voltage->AsVector();
  if (voltage_values.size() >= show) {
    writer->printf("  Voltages (first %d sensors):\n", show);
    for (int i = 0; i < show && i < (int)voltage_values.size(); i++) {
      writer->printf("    Sensor %02d: %.4f V\n", i + 1, voltage_values[i]);
    }
    writer->printf("  ... (showing %d of 96 cells)\n", show);
  } else {
    writer->puts("  Voltage data not available");
  }
  
  writer->puts("\n--- Cell Temperature Data (PID 0x04) ---");
  const std::vector<float> temp_values = mt_bms_temps->AsVector();
  if (temp_values.size() >= show) {
    writer->printf("  Temperatures (first %d sensors):\n", show);
    for (int i = 0; i < show && i < (int)temp_values.size(); i++) {
      writer->printf("    Sensor %02d: %.1f°C\n", i + 1, temp_values[i]);
    }
    writer->printf("  ... (showing %d of 31 cells)\n", show);
  } else {
    writer->puts("  Temperature data not available");
  }
  
  writer->puts("\n--- Cell Resistance (PID 0x10/0x11) ---");
  const std::vector<float> resistance_values = mt_bms_cell_resistance->AsVector();
  if (resistance_values.size() >= (int)resistance_values.size()) {
    writer->printf("  Relative Resistance (first %d cells):\n", show);
    for (int i = 0; i < show && i < (int)resistance_values.size(); i++) {
      writer->printf("    Cell %02d: %.6f\n", i + 1, resistance_values[i]);
    }
    writer->printf("  ... (showing %d of 96 cells)\n", show);
  } else {
    writer->puts("  Cell resistance data not available");
  }
  
  writer->puts("\n=== End of ED4scan Data ===");
  return Success;
}

void OvmsVehicleSmartEQ::xsq_tpms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq) return;

  bool data_shown = false;
  const char* alertname[] = { "OK", "WARN", "ALERT" };  
  std::vector<std::string> tpms_layout = smarteq->GetTpmsLayout();  
  
  writer->printf("TPMS Data       : ");
  for (auto wheel : tpms_layout)
    writer->printf(" %8s", wheel.c_str());
  writer->puts("");

  if (StandardMetrics.ms_v_tpms_alert->IsDefined())
    {
    writer->printf("Alert level     : ");
    for (auto val : StandardMetrics.ms_v_tpms_alert->AsVector())
      {
      // Bounds check to prevent array overflow
      if (val >= 0 && val <= 2)
        writer->printf(" %8s", alertname[val]);
      else if (val < 0)
        writer->printf(" %8s", "UNKNOWN");
      else
        writer->printf(" %8s", "INVALID");
      }
    writer->puts(StandardMetrics.ms_v_tpms_alert->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (smarteq->mt_tpms_low_batt->IsDefined())
    {
    writer->printf("Battery         : ");
    for (auto val : smarteq->mt_tpms_low_batt->AsVector())
      {
      // Bounds check to prevent array overflow
      if (val >= 0 && val <= 2)
        writer->printf(" %8s", alertname[val]);
      else if (val < 0)
        writer->printf(" %8s", "UNKNOWN");
      else
        writer->printf(" %8s", "INVALID");
      }
    writer->puts(smarteq->mt_tpms_low_batt->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (smarteq->mt_tpms_missing_tx->IsDefined())
    {
    writer->printf("Missing Tx      : ");
    for (auto val : smarteq->mt_tpms_missing_tx->AsVector())
      {
      // Bounds check to prevent array overflow
      if (val >= 0 && val <= 2)
        writer->printf(" %8s", alertname[val]);
      else if (val < 0)
        writer->printf(" %8s", "UNKNOWN");
      else
        writer->printf(" %8s", "INVALID");
      }
    writer->puts(smarteq->mt_tpms_missing_tx->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (!smarteq->m_tpms_index.empty())
    {
    writer->printf("Sensor mapping  : ");
    for (auto val : smarteq->m_tpms_index)
      {
      if (val >= 0)
        {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", val);
        writer->printf(" %8s", buf);
        }
      else
        {
        writer->printf(" %8s", "-");
        }
      }
    writer->puts("");
    data_shown = true;
    }

  if (StandardMetrics.ms_v_tpms_pressure->IsDefined())
    {
    metric_unit_t user_pressure = OvmsMetricGetUserUnit(GrpPressure, kPa);
    writer->printf("Pressure...[%s]: ", OvmsMetricUnitLabel(user_pressure));
    for (auto val : StandardMetrics.ms_v_tpms_pressure->AsVector(user_pressure))
      {
      // Validate pressure (typical range 0-500 kPa)
      if (!std::isnan(val) && val >= 0.0f && val <= 500.0f)
        writer->printf(" %8.1f", val);
      else
        writer->printf(" %8s", "-");
      }
    writer->puts(StandardMetrics.ms_v_tpms_pressure->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (StandardMetrics.ms_v_tpms_temp->IsDefined())
    {
    metric_unit_t user_temp = OvmsMetricGetUserUnit(GrpTemp, Celcius);
    writer->printf("Temperature.[%s]: ", OvmsMetricUnitLabel(user_temp));
    for (auto val : StandardMetrics.ms_v_tpms_temp->AsVector(user_temp))
      {
      // Validate temperature (typical range -40 to +150 °C)
      if (!std::isnan(val) && val >= -50.0f && val <= 200.0f)
        writer->printf(" %8.1f", val);
      else
        writer->printf(" %8s", "-");
      }
    writer->puts(StandardMetrics.ms_v_tpms_temp->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (!data_shown)
    writer->puts("Sorry, no data available. Try switching the vehicle on.");

  writer->puts("");
  }
