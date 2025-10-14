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
  if(!m_enable_write) {
    ESP_LOGE(TAG, "CommandClimateControl failed / no write access");
    return Fail;
  }
  if (StdMetrics.ms_v_env_hvac->AsBool()) {
    MyNotify.NotifyString("info", "hvac.enabled", "Climate already on");
    ESP_LOGI(TAG, "CommandClimateControl already on");
    return Success;
  }
  ESP_LOGI(TAG, "CommandClimateControl %s", enable ? "ON" : "OFF");

  OvmsVehicle::vehicle_command_t res;

  if (enable) {
    uint8_t data[4] = {0x40, 0x01, 0x00, 0x00};
    canbus *obd;
    obd = m_can1;

    res = CommandWakeup();
    if (res == Success) {
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      for (int i = 0; i < 10; i++) {
        obd->WriteStandard(0x634, 4, data);
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
      m_climate_start = true;      
      res = Success;
    } else {
      res = Fail;
    }
  } else {
    res = NotImplemented;
  }

  // fallback to default implementation?
  if (res == NotImplemented) {
    res = OvmsVehicle::CommandClimateControl(enable);
  }
  return res;
}

OvmsVehicle::vehicle_command_t  OvmsVehicleSmartEQ::CommandCan(uint32_t txid,uint32_t rxid,bool reset,bool wakeup) {
  if(!m_enable_write) {
    ESP_LOGE(TAG, "CommandCan failed / no write access");
    return Fail;
  }

  if(m_ddt4all_exec > 1) {
    ESP_LOGE(TAG, "DDT4all command rejected - previous command still processing (%d seconds remaining)",
             m_ddt4all_exec);
    return Fail;
  }

  m_ddt4all_exec = 45; // 45 seconds delay for next DDT4ALL command execution

  ESP_LOGI(TAG, "CommandCan");

  std::string request;
  std::string response;
  std::string reqstr = m_hl_canbyte;
  if (reqstr.size() % 2 != 0) 
    {
    ESP_LOGE(TAG,"Invalid hex length");
    return Fail;
    }

  if (wakeup)
    CommandWakeup();
  else
    CommandWakeup2();

  vTaskDelay(2000 / portTICK_PERIOD_MS);
  uint8_t protocol = ISOTP_STD;
  int timeout_ms = 500;

  request = hexdecode("10C0");
  PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  request = hexdecode(reqstr);
  PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);

  if (reset) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    request = hexdecode("1103");  // key on/off
    PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);
  }
  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandHomelink(int button, int durationms) {
  // This is needed to enable climate control via Homelink for the iOS app
  ESP_LOGI(TAG, "CommandHomelink button=%d durationms=%d", button, durationms);
  OvmsVehicle::vehicle_command_t res = NotImplemented;

  if (StdMetrics.ms_v_bat_soc->AsInt() < 31) {
    ESP_LOGI(TAG, "Battery SOC is too low for climate control");
    return Fail;
  }

  switch (button)
  {
    case 0:
    {
      res = CommandClimateControl(true);
      break;
    }
    case 1:
    {
      m_climate_ticker = 2;
      res = CommandClimateControl(true);
      break;
    }
    case 2:
    {      
      m_climate_ticker = 3;
      res = CommandClimateControl(true);
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

  OvmsVehicle::vehicle_command_t res;

  ESP_LOGI(TAG, "Send Wakeup Command");
  res = Fail;
  if(!mt_bus_awake->AsBool()) 
    {
    uint8_t data[4] = {0x40, 0x00, 0x00, 0x00};
    canbus *obd;
    obd = m_can1;

    for (int i = 0; i < 20; i++) 
      {
      obd->WriteStandard(0x634, 4, data);
      vTaskDelay(200 / portTICK_PERIOD_MS);
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
  OvmsVehicle::vehicle_command_t res;

  ESP_LOGI(TAG, "Send Wakeup Command 2");
  res = Fail;

  if(!mt_bus_awake->AsBool()) 
    {
    ESP_LOGI(TAG, "Send Wakeup CommandWakeup2");
    uint8_t data[8] = {0xc3, 0x1b, 0x73, 0x57, 0x14, 0x70, 0x96, 0x85};
    canbus *obd;
    obd = m_can1;
    for (int i = 0; i < 10; i++) 
      {
      obd->WriteStandard(0x350, 8, data);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      }
    mt_bus_awake->SetValue(true);
    ESP_LOGI(TAG, "Vehicle is awake");
    if(mt_bus_awake->AsBool()) res = Success;
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
  CommandWakeup();
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  
  uint32_t txid = 0x745, rxid = 0x765;
  uint8_t protocol = ISOTP_STD;
  int timeout_ms = 200;
  
  std::string request;
  std::string response;
  std::string reqstr = MyConfig.GetParamValue("xsq", "lock.byte", "30010000");
  if (reqstr.size() % 2 != 0) 
    {
    ESP_LOGE(TAG,"Invalid hex length");
    return Fail;
    }
  
  request = hexdecode("10C0");
  int err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);
  
  request = hexdecode(reqstr);
  err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);

  if(m_indicator) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    std::string indstr = MyConfig.GetParamValue("xsq", "indicator", "30082002");
    request = hexdecode(indstr); // indicator light
    err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);
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

  StdMetrics.ms_v_env_locked->SetValue(true);
  return Success;
}

// unlock: can can1 tx st 745 04 30 01 00 01
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandUnlock(const char* pin) {

  if(!m_enable_write) {
    ESP_LOGE(TAG, "CommandUnlock failed / no write access");
    return Fail;
  }
  ESP_LOGI(TAG, "CommandUnlock");
  CommandWakeup();
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  
  uint32_t txid = 0x745, rxid = 0x765;
  uint8_t protocol = ISOTP_STD;
  int timeout_ms = 200;
  std::string request;
  std::string response;
  std::string reqstr = MyConfig.GetParamValue("xsq", "unlock.byte", "30010001");
  if (reqstr.size() % 2 != 0) 
    {
    ESP_LOGE(TAG,"Invalid hex length");
    return Fail;
    }
  
  request = hexdecode("10C0");
  int err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);
  
  request = hexdecode(reqstr);
  err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);

  if(m_indicator) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    std::string indstr = MyConfig.GetParamValue("xsq", "indicator", "30082002");
    request = hexdecode(indstr); // indicator light
    err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);
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

  StdMetrics.ms_v_env_locked->SetValue(false);
  return Success;
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
  if (mt_evc_hv_energy->IsDefined())
    {
    const std::string& hv_energy = mt_evc_hv_energy->AsUnitString("-", ToUser, 3);
    writer->printf("usable Energy: %s\n", hv_energy.c_str());
    }
  
  if (m_extendedStats)
  { 
    if (mt_use_at_reset->IsDefined())
      {
      const std::string& use_at_reset = mt_use_at_reset->AsUnitString("-", ToUser, 1);
      writer->printf("RESET kWh/100km: %s\n", use_at_reset.c_str());
      }
    
    if (mt_obd_trip_km->IsDefined())
      {
      const std::string& obd_trip_km = mt_obd_trip_km->AsUnitString("-", ToUser, 1);
      writer->printf("RESET Trip km: %s\n", obd_trip_km.c_str());
      }
    if (mt_obd_trip_time->IsDefined())
      {
      const std::string& obd_trip_time = mt_obd_trip_time->AsUnitString("-", ToUser, 1);
      writer->printf("RESET Trip time: %s\n", obd_trip_time.c_str());
      }
    if (mt_use_at_start->IsDefined())
      {
      const std::string& use_at_reset = mt_use_at_start->AsUnitString("-", ToUser, 1);
      writer->printf("START kWh/100km: %s\n", use_at_reset.c_str());
      }
    if (mt_obd_start_trip_km->IsDefined())
      {
      const std::string& obd_trip_km = mt_obd_start_trip_km->AsUnitString("-", ToUser, 1);
      writer->printf("START Trip km: %s\n", obd_trip_km.c_str());
      }
    if (mt_obd_start_trip_time->IsDefined())
      {
      const std::string& obd_trip_time = mt_obd_start_trip_time->AsUnitString("-", ToUser, 1);
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
    writer->printf("  distance: %s\n", (char*) mt_obd_start_trip_km->AsUnitString("-", rangeUnit, 1).c_str());
    writer->printf("  time: %s\n", (char*) mt_obd_start_trip_time->AsUnitString("-", Native, 1).c_str());
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
    writer->printf("  distance: %s\n", (char*) mt_obd_trip_km->AsUnitString("-", rangeUnit, 1).c_str());
    writer->printf("  time: %s\n", (char*) mt_obd_trip_time->AsUnitString("-", Native, 1).c_str());
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
    writer->printf("  days: %d\n", mt_obd_mt_day_usual->AsInt());
    writer->printf("  level: %s\n", mt_obd_mt_level->AsString().c_str());
    writer->printf("  km: %d\n", mt_obd_mt_km_usual->AsInt());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyMaintenance() {
  StringWriter buf(200);
  CommandMaintenance(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.maintenance",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_climate(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
    smarteq->CommandSetClimate(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandSetClimate(int verbosity, OvmsWriter* writer) {
    int _ds = mt_climate_ds->AsInt();
    int _de = mt_climate_de->AsInt() == 0 ? 6 : mt_climate_de->AsInt() - 1;
    int _min = mt_climate_1to3->AsInt();
    const char* _days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* _time[3] = {"5 min", "10 min", "15 min"};

    writer->puts("Climate values:");
    writer->printf("  on: %s\n", mt_climate_on->AsBool() ? "yes" : "no");
    writer->printf("  time: %s:%s h\n", mt_climate_h->AsString().c_str(),mt_climate_m->AsString().c_str());
    writer->printf("  weekly: %s\n", mt_climate_weekly->AsBool() ? "yes" : "no");
    writer->printf("  start Day: %s\n", _days[_ds]);
    writer->printf("  end Day: %s\n", _days[_de]);
    writer->printf("  runtime: %s\n", _time[_min]);
    return Success;
}
void OvmsVehicleSmartEQ::NotifyClimateTimer() {
  StringWriter buf(200);
  CommandSetClimate(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.climate.timer",buf.c_str());
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandClimate(int verbosity, OvmsWriter* writer) {
  writer->puts("Climate on:");
  writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("  CAC: %s\n", (char*) StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
  writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
  return Success;
}
void OvmsVehicleSmartEQ::NotifyClimate() {
  StringWriter buf(200);
  CommandClimate(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.climate",buf.c_str());
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
  m_12v_charge_state = false;
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
  for (int i = 0; i < 4; i++) {
    m_tpms_pressure[i] = dummy_pressure; // kPa
    setTPMSValue(i, m_tpms_index[i]);
  }
  writer->printf("set TPMS dummy pressure: %.2f", dummy_pressure);
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
    if (val < 11.0 || val > 15.0) 
      {
      writer->puts("Error: voltage out of plausible range (11.0–15.0)");
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
    OvmsVehicle::vehicle_command_t res;

    res = smarteq->CommandWakeup2();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if (res != Success) 
      {
      writer->puts("Error: vehicle not awake");
      return;
      } 
        
    float can12V = smarteq->mt_evc_LV_DCDC_act_req->AsBool(false) 
                     ? smarteq->mt_evc_LV_DCDC_volt->AsFloat(0.0f) + 0.25f   // DCDC voltage + offset
                     : smarteq->mt_evc_LV_USM_volt->AsFloat(0.0f);           // USM voltage
    if (can12V < 11.0f || can12V > 15.0f) 
      {
      writer->puts("Error: CAN 12V voltage out of plausible range (11.0–15.0)");
      return;
      }
    smarteq->ReCalcADCfactor(can12V, writer);
    writer->printf("Recalculating ADC factor using %.2fV CAN voltage\n", can12V);
    return;
   }
  #else
    writer->puts("ADC component not enabled");
    return;
  #endif
}
