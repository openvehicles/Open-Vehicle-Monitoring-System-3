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

// CommandCanVector(txid, rxid, hexbytes = {"30010000","30082002"}, reset CAN = true/false, CommandWakeup = true/ CommandWakeup2 = false)
OvmsVehicle::vehicle_command_t  OvmsVehicleSmartEQ::CommandCanVector(uint32_t txid,uint32_t rxid, std::vector<std::string> hexbytes,bool reset,bool wakeup) {
  if(!IsCANwrite())
    {
    ESP_LOGE(TAG, "CommandCanVector failed / no write access");
    return Fail;
    }

  if(m_ddt4all_exec > 1) 
    {
    ESP_LOGE(TAG, "DDT4all command rejected - previous command still processing (%d seconds remaining)",m_ddt4all_exec);
    return Fail;
    }

  // if write access is not enabled, then switch CAN bus to active mode for sending the command
  if (!m_can_active)
    {
    smartCANmode(true);
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
    PollSetState(POLLSTATE_AWAKE);
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

void OvmsVehicleSmartEQ::xsq_canwrite(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
  if(!smarteq->m_ddt4all || !smarteq->IsCANwrite()) {
    ESP_LOGE(TAG, "DDT4all failed / no Canbus write access or DDT4all not enabled");
    writer->puts("DDT4all failed / no Canbus write access or DDT4all not enabled");
    return;
  }

  if(smarteq->m_ddt4all_exec > 0) {
    writer->printf("DDT4all command cooldown active, please wait %d seconds", smarteq->m_ddt4all_exec);
    return;
  }

  if (argc != 1) {
    writer->puts("Error: xsq canwrite requires one argument");
    writer->puts("Usage: xsq canwrite <txid,rxid,hexbytes[,reset,wakeup]>");
    writer->puts("Example: xsq canwrite 0x745,0x765,2E012100,false,true");
    writer->puts("Multiple data: xsq canwrite 0x745,0x765,30082002/30082002,false,true");
    return;
  }
  
  smarteq->CommandCanWrite(argv[0], writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandCanWrite(const std::string& params, OvmsWriter* writer) {
  // Parse comma-separated parameters
  std::vector<std::string> parts;
  std::string current;
  
  // Split by comma and trim whitespace
  for (char c : params) {
    if (c == ',') {
      // Trim whitespace
      size_t start = current.find_first_not_of(" \t");
      size_t end = current.find_last_not_of(" \t");
      if (start != std::string::npos && end != std::string::npos) {
        parts.push_back(current.substr(start, end - start + 1));
      } else if (!current.empty()) {
        parts.push_back(current);
      }
      current.clear();
    } else {
      current += c;
    }
  }
  // Add last part
  if (!current.empty()) {
    size_t start = current.find_first_not_of(" \t");
    size_t end = current.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
      parts.push_back(current.substr(start, end - start + 1));
    } else {
      parts.push_back(current);
    }
  }

  // Validate minimum parameters
  if (parts.size() < 3) {
    writer->puts("Error: Minimum 3 parameters required (txid,rxid,hexbytes)");
    return Fail;
  }

  // Parse txid (hex)
  uint32_t txid = 0;
  if (parts[0].find("0x") == 0 || parts[0].find("0X") == 0) {
    txid = (uint32_t)strtoul(parts[0].c_str(), NULL, 16);
  } else {
    txid = (uint32_t)strtoul(parts[0].c_str(), NULL, 16);
  }

  // Parse rxid (hex)
  uint32_t rxid = 0;
  if (parts[1].find("0x") == 0 || parts[1].find("0X") == 0) {
    rxid = (uint32_t)strtoul(parts[1].c_str(), NULL, 16);
  } else {
    rxid = (uint32_t)strtoul(parts[1].c_str(), NULL, 16);
  }

  // Parse hexbytes (can be multiple /-separated values)
  std::vector<std::string> hexbytes;
  std::string hexbytes_input = parts[2];
  std::string current_hex;
  
  // Split hexbytes by '/' separator
  for (char c : hexbytes_input) {
    if (c == '/') {
      // Trim whitespace
      size_t start = current_hex.find_first_not_of(" \t");
      size_t end = current_hex.find_last_not_of(" \t");
      if (start != std::string::npos && end != std::string::npos) {
        hexbytes.push_back(current_hex.substr(start, end - start + 1));
      } else if (!current_hex.empty()) {
        hexbytes.push_back(current_hex);
      }
      current_hex.clear();
    } else {
      current_hex += c;
    }
  }
  // Add last hexbyte
  if (!current_hex.empty()) {
    size_t start = current_hex.find_first_not_of(" \t");
    size_t end = current_hex.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
      hexbytes.push_back(current_hex.substr(start, end - start + 1));
    } else {
      hexbytes.push_back(current_hex);
    }
  }

  // Parse optional reset parameter (default false)
  bool reset = false;
  if (parts.size() >= 4) {
    std::string reset_str = parts[3];
    // Convert to lowercase for comparison
    for (auto& c : reset_str) c = tolower(c);
    reset = (reset_str == "true" || reset_str == "1" || reset_str == "yes");
  }

  // Parse optional wakeup parameter (default true)
  bool wakeup = true;
  if (parts.size() >= 5) {
    std::string wakeup_str = parts[4];
    // Convert to lowercase for comparison
    for (auto& c : wakeup_str) c = tolower(c);
    wakeup = (wakeup_str == "true" || wakeup_str == "1" || wakeup_str == "yes");
  }

  // Validate txid and rxid
  if (txid == 0 || rxid == 0) {
    writer->puts("Error: Invalid txid or rxid");
    return Fail;
  }

  // Display parsed values
  writer->printf("Sending CAN command:\n");
  writer->printf("  txid:    0x%03X\n", txid);
  writer->printf("  rxid:    0x%03X\n", rxid);
  writer->printf("  data:    ");
  for (size_t i = 0; i < hexbytes.size(); i++) {
    writer->printf("%s", hexbytes[i].c_str());
    if (i < hexbytes.size() - 1) {
      writer->printf(" / ");
    }
  }
  writer->printf("\n");
  writer->printf("  reset:   %s\n", reset ? "true" : "false");
  writer->printf("  wakeup:  %s\n", wakeup ? "true" : "false");

  // Execute command
  OvmsVehicle::vehicle_command_t res = CommandCanVector(txid, rxid, hexbytes, reset, wakeup);

  if (res == Success) {
    writer->puts("Command executed successfully");
  } else {
    writer->puts("Command failed");
  }

  return res;
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
  map_xsq["cfg.preset.ver"] = STR(PRESET_VERSION);

  // modem section - single map operation
  bool need_stream = false;
  {
    auto m = MyConfig.GetParamMap("modem");
    bool changed = false;
    if (m.find("gps.parkpause") == m.end())
      {
      m["gps.parkpause"] = "600";
      m["gps.parkreactivate"] = "50";
      m["gps.parkreactlock"] = "5";
      need_stream = true;
      changed = true;
      }
    if (m.find("gps.parkreactawake") == m.end())
      { m["gps.parkreactawake"] = "yes"; changed = true; }
    if (m.find("net.type") == m.end())
      { m["net.type"] = "4G"; changed = true; }
    if (changed)
      MyConfig.SetParamMap("modem", m);
  }

  // vehicle section - single map operation
  {
    auto m = MyConfig.GetParamMap("vehicle");
    bool changed = false;
    if (m.find("12v.alert") == m.end())
      { m["12v.alert"] = "0.8"; changed = true; }
    if (need_stream)
      { m["stream"] = "10"; changed = true; }
    // TPMS migration: read from already-loaded map_xsq
    if (map_xsq.count("TPMS_FL"))
      {
      static const char* xk[] = {"TPMS_FL","TPMS_FR","TPMS_RL","TPMS_RR"};
      static const char* vk[] = {"tpms.fl","tpms.fr","tpms.rl","tpms.rr"};
      static const char* dd[] = {"0","1","2","3"};
      for (int i = 0; i < 4; i++)
        {
        auto it = map_xsq.find(xk[i]);
        m[vk[i]] = (it != map_xsq.end() && !it->second.empty()) ? it->second : dd[i];
        }
      changed = true;
      }
    if (changed)
      MyConfig.SetParamMap("vehicle", m);
  }

  // network section - single map operation
  {
    auto m = MyConfig.GetParamMap("network");
    bool changed = false;
    if (m.find("wifi.ap2client.enable") == m.end())
      { m["wifi.ap2client.enable"] = "yes"; changed = true; }
    if (m.erase("type") > 0)
      changed = true;
    if (changed)
      MyConfig.SetParamMap("network", m);
  }

  // ota section - migrate old server
  {
    auto m = MyConfig.GetParamMap("ota");
    auto srv = m.find("server");
    auto tag = m.find("tag");
    if (srv != m.end() && srv->second == "https://ovms.dimitrie.eu/firmware/ota" &&
        tag != m.end() && tag->second == "smarteq")
      {
      srv->second = "https://ovms.dexters-web.de/firmware/ota";
      tag->second = "edge";
      MyConfig.SetParamMap("ota", m);
      }
  }

  // Remove deprecated preclimate section
  if (MyConfig.CachedParam("xsq.preclimate"))
    MyConfig.DeregisterParam("xsq.preclimate");

  // Remove deprecated keys from xsq map
  static const char* deprecated_keys[] = {
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
    "adc.samples",
    "obdii.79b.r",
    "obdii.79b.t",
    "obdii.79b.v",
    "obdii.7e4.mod",
    "obdii_743",
    "obdii_745",
    "obdii_79b",
    "obdii_7e4",
    "obdii_7e4_modify",
    "basic_tpms",
    "calc.adcfactor.samples"
  };

  int removed_count = 0;
  for (const char* key : deprecated_keys)
    {
    if (map_xsq.erase(key) > 0)
      removed_count++;
    }

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
  ConfigParamMap map_xsq;
  MyConfig.SetParamMap("xsq", map_xsq);

  // auto section
  auto map_auto = MyConfig.GetParamMap("auto");
  map_auto["init"] = "yes";
  map_auto["ota"] = "no";
  map_auto["modem"] = "yes";
  map_auto["server.v2"] = "yes";
  MyConfig.SetParamMap("auto", map_auto);

  // modem section
  auto map_modem = MyConfig.GetParamMap("modem");
  map_modem["net.type"] = "4G";
  map_modem["gps.parkreactawake"] = "yes";
  map_modem["gps.parkpause"] = "600";
  map_modem["gps.parkreactivate"] = "50";
  map_modem["gps.parkreactlock"] = "5";
  MyConfig.SetParamMap("modem", map_modem);

  // vehicle section
  auto map_vehicle = MyConfig.GetParamMap("vehicle");
  map_vehicle["stream"] = "10";
  map_vehicle["12v.alert"] = "0.8";
  MyConfig.SetParamMap("vehicle", map_vehicle);

  // ota section
  auto map_ota = MyConfig.GetParamMap("ota");
  map_ota["server"] = "https://ovms.dexters-web.de/firmware/ota";
  map_ota["tag"] = "main";
  map_ota["http.mru"] = "https://ovms.dexters-web.de/firmware/ota/v3.3/edge/ovms3.bin";
  MyConfig.SetParamMap("ota", map_ota);

  // network section
  auto map_net = MyConfig.GetParamMap("network");
  map_net["wifi.ap2client.enable"] = "yes";
  map_net.erase("wifi.bad.reconnect");
  map_net.erase("wifi.sq.good");
  map_net.erase("wifi.sq.bad");
  map_net.erase("modem.sq.good");
  map_net.erase("modem.sq.bad");
  MyConfig.SetParamMap("network", map_net);
  
  if (writer) 
    {
    writer->puts("SmartEQ config reset to defaults");
    ESP_LOGI(TAG, "SmartEQ config reset to defaults");
    }
  
  return Success;
}
