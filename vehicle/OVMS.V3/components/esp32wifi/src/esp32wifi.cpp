/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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
*/

#include "ovms_log.h"
static const char *TAG = "esp32wifi";

#include <string.h>
#include <string>
#include "esp32wifi.h"
#include "esp_wifi.h"
#include "ovms.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"
#include "ovms_events.h"

const char* const esp32wifi_mode_names[] = {
  "Modem is off",
  "Client mode",
  "Scanning-Client mode",
  "Access-Point mode",
  "Access-Point + Client mode",
  "SCAN mode"
};


void wifi_mode_client(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32wifi *me = MyPeripherals->m_esp32wifi;
  if (me == NULL)
    {
    writer->puts("Error: wifi peripheral could not be found");
    return;
    }

  if (argc == 0)
    {
    writer->puts("Starting WIFI as a client for any defined SSID");
    me->StartScanningClientMode();
    return;
    }

  std::string password = MyConfig.GetParamValue("wifi.ssid", argv[0]);
  if (password.empty())
    {
    writer->puts("Error: SSID password must be defined in config wifi.ssid");
    return;
    }

  if (argc == 1)
    {
    writer->printf("Starting WIFI as a client to %s...\n",argv[0]);
    me->StartClientMode(argv[0],password);
    }
  else
    {
    uint8_t bssid[6];
    const char *p = argv[1];
    if (strlen(p)!=17)
      {
      writer->puts("Error: If specified, bssid must be in format aa:aa:aa:aa:aa:aa");
      return;
      }
    for (int k=0;k<6;k++)
      {
      char x[3] = { p[0], p[1], 0 };
      p += 3;
      bssid[k] = (uint8_t)strtol(x,NULL,16);
      }
    writer->printf("Starting WIFI as a client to %s (%02x:%02x:%02x:%02x:%02x:%02x)...\n",
      argv[0],
      bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
    me->StartClientMode(argv[0],password,bssid);
    }
  }

void wifi_mode_ap(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32wifi *me = MyPeripherals->m_esp32wifi;
  if (me == NULL)
    {
    writer->puts("Error: wifi peripheral could not be found");
    return;
    }

  std::string password = MyConfig.GetParamValue("wifi.ap", argv[0]);
  if (password.empty())
    {
    writer->puts("Error: SSID password must be defined in config wifi.ap");
    return;
    }

  if (password.length() < 8)
    {
    writer->puts("Error: SSID password must be at least 8 characters");
    return;
    }

  writer->puts("Starting WIFI as access point...");
  me->StartAccessPointMode(argv[0],password);
  }

void wifi_mode_apclient(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32wifi *me = MyPeripherals->m_esp32wifi;
  if (me == NULL)
    {
    writer->puts("Error: wifi peripheral could not be found");
    return;
    }

  std::string appassword = MyConfig.GetParamValue("wifi.ap", argv[0]);
  if (appassword.empty())
    {
    writer->puts("Error: SSID password must be defined in config wifi.ap");
    return;
    }

  if (appassword.length() < 8)
    {
    writer->puts("Error: SSID password must be at least 8 characters");
    return;
    }

  std::string stapassword = MyConfig.GetParamValue("wifi.ssid", argv[1]);
  if (stapassword.empty())
    {
    writer->puts("Error: SSID password must be defined in config wifi.ssid");
    return;
    }

  writer->puts("Starting WIFI as Access Point and Client...");
  me->StartAccessPointClientMode(argv[0],appassword,argv[1],stapassword);
  }

void wifi_mode_off(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32wifi *me = MyPeripherals->m_esp32wifi;
  if (me == NULL)
    {
    writer->puts("Error: wifi peripheral could not be found");
    return;
    }

  writer->puts("Stopping wifi station...");
  me->StopStation();
  }

void wifi_scan(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32wifi *me = MyPeripherals->m_esp32wifi;
  if (me == NULL)
    {
    writer->puts("Error: wifi peripheral could not be found");
    return;
    }

  if (me->GetMode() == ESP32WIFI_MODE_SCLIENT)
    {
    writer->puts("Error: Can't scan while in scanning client mode");
    return;
    }

  writer->puts("Starting wifi scan...");
  me->Scan();
  }

void wifi_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32wifi *me = MyPeripherals->m_esp32wifi;
  if (me == NULL)
    {
    writer->puts("Error: wifi peripheral could not be found");
    return;
    }

  me->OutputStatus(verbosity, writer);
  }

class esp32wifiInit
    {
    public: esp32wifiInit();
  } esp32wifiInit  __attribute__ ((init_priority (8000)));

esp32wifiInit::esp32wifiInit()
  {
  ESP_LOGI(TAG, "Initialising ESP32WIFI (8000)");

  OvmsCommand* cmd_wifi = MyCommandApp.RegisterCommand("wifi","WIFI framework", wifi_status, "", 0, 1, true);
  cmd_wifi->RegisterCommand("scan","Perform a wifi scan",wifi_scan, "", 0, 0, true);
  cmd_wifi->RegisterCommand("status","Show wifi status",wifi_status, "", 0, 0, true);

  OvmsCommand* cmd_mode = cmd_wifi->RegisterCommand("mode","WIFI mode framework",NULL, "", 0, 0, true);
  cmd_mode->RegisterCommand("client","Connect to a WIFI network as a client",wifi_mode_client, "<ssid> <bssid>", 0, 2, true);
  cmd_mode->RegisterCommand("ap","Acts as a WIFI Access Point",wifi_mode_ap, "<ssid>", 1, 1, true);
  cmd_mode->RegisterCommand("apclient","Acts as a WIFI Access Point and Client",wifi_mode_apclient, "<apssid> <stassid>", 2, 2, true);
  cmd_mode->RegisterCommand("off","Turn off wifi networking",wifi_mode_off, "", 0, 0, true);
  }

esp32wifi::esp32wifi(const char* name)
  : pcp(name)
  {
  m_mode = ESP32WIFI_MODE_OFF;
  m_stareconnect = false;
  memset(&m_wifi_ap_cfg,0,sizeof(m_wifi_ap_cfg));
  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  memset(&m_mac_ap,0,sizeof(m_mac_ap));
  memset(&m_mac_sta,0,sizeof(m_mac_sta));
  memset(&m_ip_info_sta,0,sizeof(m_ip_info_sta));
  memset(&m_ip_info_ap,0,sizeof(m_ip_info_ap));
  MyConfig.RegisterParam("wifi.ssid", "WIFI SSID", true, false);
  MyConfig.RegisterParam("wifi.ap", "WIFI Access Point", true, false);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.gotip",std::bind(&esp32wifi::EventWifiGotIp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.disconnected",std::bind(&esp32wifi::EventWifiStaDisconnected, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.10",std::bind(&esp32wifi::EventTimer10, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.scan.done",std::bind(&esp32wifi::EventWifiScanDone, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.start",std::bind(&esp32wifi::EventWifiApState, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.stop",std::bind(&esp32wifi::EventWifiApState, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.sta.connected",std::bind(&esp32wifi::EventWifiApUpdate, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.sta.disconnected",std::bind(&esp32wifi::EventWifiApUpdate, this, _1, _2));
  }

esp32wifi::~esp32wifi()
  {
  }

void esp32wifi::AutoInit()
  {
  std::string mode = MyConfig.GetParamValue("auto", "wifi.mode", "ap");
  if (mode.empty() || mode == "off")
    return;

  std::string apssid, appassword;
  std::string stassid, stapassword;

  if (mode == "ap")
    {
    apssid = MyConfig.GetParamValue("auto", "wifi.ssid.ap");
    // fallback to initial SSID:
    if (apssid.empty())
      apssid = "OVMS";
    appassword = MyConfig.GetParamValue("wifi.ap", apssid);
    if (appassword.empty())
      {
      // fallback to module password:
      ESP_LOGW(TAG, "AutoInit: using module password as AP password");
      appassword = MyConfig.GetParamValue("password", "module");
      }
    if (appassword.empty())
      ESP_LOGE(TAG, "AutoInit: no AP password set, AP mode inhibited");
    else
      StartAccessPointMode(apssid, appassword);
    }
  else if (mode == "client")
    {
    stassid = MyConfig.GetParamValue("auto", "wifi.ssid.client");
    if (stassid.empty())
      {
      StartScanningClientMode();
      }
    else
      {
      stapassword = MyConfig.GetParamValue("wifi.ssid", stassid);
      if (stapassword.empty())
        ESP_LOGE(TAG, "AutoInit: no password set for SSID %s, mode inhibited", stassid.c_str());
      else
        StartClientMode(stassid, stapassword);
      }
    }
  else if (mode == "apclient")
    {
    apssid = MyConfig.GetParamValue("auto", "wifi.ssid.ap");
    // fallback to initial SSID:
    if (apssid.empty())
      apssid = "OVMS";
    appassword = MyConfig.GetParamValue("wifi.ap", apssid);
    if (appassword.empty())
      {
      // fallback to module password:
      ESP_LOGW(TAG, "AutoInit: using module password as AP password");
      appassword = MyConfig.GetParamValue("password", "module");
      }
    if (appassword.empty())
      {
      ESP_LOGE(TAG, "AutoInit: no AP password set, AP mode inhibited");
      return;
      }

    stassid = MyConfig.GetParamValue("auto", "wifi.ssid.client");
    if (stassid.empty())
      {
      ESP_LOGE(TAG, "AutoInit: Wifi client SSID must be specified in wifi.ssid.client");
      return;
      }

    stapassword = MyConfig.GetParamValue("wifi.ssid", stassid);
    if (stapassword.empty())
      {
      ESP_LOGE(TAG, "AutoInit: WIFI client assword must specified for SSID %s", stassid.c_str());
      return;
      }

    StartAccessPointClientMode(apssid, appassword, stassid, stapassword);
    }
  }

void esp32wifi::SetPowerMode(PowerMode powermode)
  {
  m_powermode = powermode;
  switch (powermode)
    {
    case On:
      break;
    case Sleep:
      esp_wifi_set_ps(WIFI_PS_MODEM);
      break;
    case DeepSleep:
    case Off:
      StopStation();
      break;
    default:
      break;
    };

  m_powermode = powermode;
  }

void esp32wifi::StartClientMode(std::string ssid, std::string password, uint8_t* bssid)
  {
  m_stareconnect = false;

  if ((m_mode == ESP32WIFI_MODE_AP)||(m_mode == ESP32WIFI_MODE_APCLIENT))
    {
    StopStation();
    }
  m_mode = ESP32WIFI_MODE_CLIENT;

  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  m_wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&m_wifi_init_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  strcpy((char*)m_wifi_sta_cfg.sta.ssid, ssid.c_str());
  strcpy((char*)m_wifi_sta_cfg.sta.password, password.c_str());
  m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  if (bssid == NULL)
    {
    m_wifi_sta_cfg.sta.bssid_set = 0;
    }
  else
    {
    m_wifi_sta_cfg.sta.bssid_set = 1;
    memcpy(&m_wifi_sta_cfg.sta.bssid,bssid,6);
    }
  m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
  }

void esp32wifi::StartScanningClientMode()
  {
  m_stareconnect = false;

  if ((m_mode == ESP32WIFI_MODE_AP)||(m_mode == ESP32WIFI_MODE_APCLIENT))
    {
    StopStation();
    }
  m_mode = ESP32WIFI_MODE_SCLIENT;

  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  m_wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&m_wifi_init_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  ESP_ERROR_CHECK(esp_wifi_start());

  // if we are triggered by a startup script, monotonictime will be zero which
  // won't pass the test in EventTimer10()
  m_nextscan = monotonictime+1;
  }

void esp32wifi::StartAccessPointMode(std::string ssid, std::string password)
  {
  m_stareconnect = false;

  if ((m_mode == ESP32WIFI_MODE_CLIENT)||(m_mode == ESP32WIFI_MODE_SCLIENT)||(m_mode == ESP32WIFI_MODE_AP))
    {
    StopStation();
    }
  m_mode = ESP32WIFI_MODE_AP;

  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  m_wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&m_wifi_init_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  memset(&m_wifi_ap_cfg,0,sizeof(m_wifi_ap_cfg));
  m_wifi_ap_cfg.ap.ssid_len = 0;
  m_wifi_ap_cfg.ap.channel = 0;
  m_wifi_ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
  m_wifi_ap_cfg.ap.ssid_hidden = 0;
  m_wifi_ap_cfg.ap.max_connection = 4;
  m_wifi_ap_cfg.ap.beacon_interval=100;
  strcpy((char*)m_wifi_ap_cfg.ap.ssid, ssid.c_str());
  strcpy((char*)m_wifi_ap_cfg.ap.password, password.c_str());
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &m_wifi_ap_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());
  }

void esp32wifi::StartAccessPointClientMode(std::string apssid, std::string appassword, std::string stassid, std::string stapassword)
  {
  m_stareconnect = false;

  if ((m_mode == ESP32WIFI_MODE_CLIENT)||(m_mode == ESP32WIFI_MODE_SCLIENT)||(m_mode == ESP32WIFI_MODE_AP))
    {
    StopStation();
    }
  m_mode = ESP32WIFI_MODE_APCLIENT;

  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  m_wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&m_wifi_init_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

  memset(&m_wifi_ap_cfg,0,sizeof(m_wifi_ap_cfg));
  m_wifi_ap_cfg.ap.ssid_len = 0;
  m_wifi_ap_cfg.ap.channel = 0;
  m_wifi_ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
  m_wifi_ap_cfg.ap.ssid_hidden = 0;
  m_wifi_ap_cfg.ap.max_connection = 4;
  m_wifi_ap_cfg.ap.beacon_interval=100;
  strcpy((char*)m_wifi_ap_cfg.ap.ssid, apssid.c_str());
  strcpy((char*)m_wifi_ap_cfg.ap.password, appassword.c_str());
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &m_wifi_ap_cfg));

  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  strcpy((char*)m_wifi_sta_cfg.sta.ssid, stassid.c_str());
  strcpy((char*)m_wifi_sta_cfg.sta.password, stapassword.c_str());
  m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));

  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
  }

void esp32wifi::StopStation()
  {
  m_stareconnect = false;
  memset(&m_wifi_ap_cfg,0,sizeof(m_wifi_ap_cfg));
  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  memset(&m_mac_ap,0,sizeof(m_mac_ap));
  memset(&m_mac_sta,0,sizeof(m_mac_sta));
  memset(&m_ip_info_sta,0,sizeof(m_ip_info_sta));
  memset(&m_ip_info_ap,0,sizeof(m_ip_info_ap));

  if (m_mode != ESP32WIFI_MODE_OFF)
    {
    MyEvents.SignalEvent("system.wifi.down",NULL);
    if ((m_mode == ESP32WIFI_MODE_CLIENT)||(m_mode == ESP32WIFI_MODE_SCLIENT)||(m_mode == ESP32WIFI_MODE_APCLIENT))
      { ESP_ERROR_CHECK(esp_wifi_disconnect()); }
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    m_mode = ESP32WIFI_MODE_OFF;
    }
  }

void esp32wifi::Scan()
  {
  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  if (m_mode != ESP32WIFI_MODE_OFF)
    {
    StopStation();
    }

  m_wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&m_wifi_init_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());
  m_mode = ESP32WIFI_MODE_SCAN;

  wifi_scan_config_t scanConf;
  memset(&scanConf,0,sizeof(scanConf));
  scanConf.ssid = NULL;
  scanConf.bssid = NULL;
  scanConf.channel = 0;
  scanConf.show_hidden = true;
  scanConf.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, false));
  }

esp32wifi_mode_t esp32wifi::GetMode()
  {
  return m_mode;
  }

std::string esp32wifi::GetSSID()
  {
  if (m_wifi_sta_cfg.sta.ssid[0] != 0)
    return std::string((char*)m_wifi_sta_cfg.sta.ssid);
  else
    return std::string((char*)m_wifi_ap_cfg.ap.ssid);
  }

void esp32wifi::EventWifiGotIp(std::string event, void* data)
  {
  m_stareconnect = false;
  system_event_info_t *info = (system_event_info_t*)data;
  m_ip_info_sta = info->got_ip.ip_info;
  esp_wifi_get_mac(ESP_IF_WIFI_STA, m_mac_sta);
  ESP_LOGI(TAG, "WiFi UP with SSID: %s, MAC: " MACSTR ", IP: " IPSTR ", mask: " IPSTR ", gw: " IPSTR,
    m_wifi_sta_cfg.sta.ssid, MAC2STR(m_mac_sta),
    IP2STR(&m_ip_info_sta.ip), IP2STR(&m_ip_info_sta.netmask), IP2STR(&m_ip_info_sta.gw));
  }

void esp32wifi::EventWifiStaDisconnected(std::string event, void* data)
  {
  if ((m_mode == ESP32WIFI_MODE_CLIENT) ||
      (m_mode == ESP32WIFI_MODE_APCLIENT))
    {
    m_stareconnect = true;
    }
  else if (m_mode == ESP32WIFI_MODE_SCLIENT)
    {
    esp_wifi_disconnect();
    memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
    memset(&m_ip_info_sta,0,sizeof(m_ip_info_sta));
    esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg);
    m_nextscan = monotonictime + 30;
    }
  }

void esp32wifi::EventWifiApState(std::string event, void* data)
  {
  if (event == "system.wifi.ap.start")
    {
    esp_wifi_get_mac(ESP_IF_WIFI_AP, m_mac_ap);
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &m_ip_info_ap);
    ESP_LOGI(TAG, "AP started with SSID: %s, MAC: " MACSTR ", IP: " IPSTR,
      m_wifi_ap_cfg.ap.ssid, MAC2STR(m_mac_ap), IP2STR(&m_ip_info_ap.ip));
    }
  else
    {
    memset(&m_mac_ap,0,sizeof(m_mac_ap));
    memset(&m_ip_info_ap,0,sizeof(m_ip_info_ap));
    ESP_LOGI(TAG, "AP stopped");
    }
  }

void esp32wifi::EventWifiApUpdate(std::string event, void* data)
  {
  system_event_info_t *info = (system_event_info_t*)data;
  if (event == "system.wifi.ap.sta.connected")
    ESP_LOGI(TAG, "AP station connected: id: %d, MAC: " MACSTR,
      info->sta_connected.aid, MAC2STR(info->sta_connected.mac));
  else
    ESP_LOGI(TAG, "AP station disconnected: id: %d, MAC: " MACSTR,
      info->sta_connected.aid, MAC2STR(info->sta_connected.mac));
  }

void esp32wifi::EventTimer10(std::string event, void* data)
  {
  if (((m_mode == ESP32WIFI_MODE_CLIENT)||(m_mode == ESP32WIFI_MODE_APCLIENT))&&(m_stareconnect))
    {
    esp_wifi_connect();
    }

  if ((m_mode == ESP32WIFI_MODE_SCLIENT)&&(monotonictime > m_nextscan)&&(m_nextscan != 0))
    {
    // Start a scan
    m_nextscan = 0;
    wifi_scan_config_t scanConf;
    memset(&scanConf,0,sizeof(scanConf));
    scanConf.ssid = NULL;
    scanConf.bssid = NULL;
    scanConf.channel = 0;
    scanConf.show_hidden = true;
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, false));
    }
  }

void esp32wifi::EventWifiScanDone(std::string event, void* data)
  {
  uint16_t apCount = 0;
  esp_err_t res;
  wifi_ap_record_t* list = NULL;

  res = esp_wifi_scan_get_ap_num(&apCount);
  if (res != ESP_OK)
    {
    ESP_LOGE(TAG, "EventWifiScanDone: can't get AP count, error=0x%x", res);
    return;
    }

  if (apCount > 0)
    {
    list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
    res = esp_wifi_scan_get_ap_records(&apCount, list);
    if (res != ESP_OK)
      {
      ESP_LOGE(TAG, "EventWifiScanDone: can't get AP records, error=0x%x", res);
      return;
      }
    }

  if (m_mode == ESP32WIFI_MODE_SCAN)
    {
    ESP_LOGI(TAG, "SSID scan results...");
    for (int k=0; k<apCount; k++)
      {
      std::string authmode;
      switch(list[k].authmode)
        {
        case WIFI_AUTH_OPEN:
          authmode = "WIFI_AUTH_OPEN";
          break;
        case WIFI_AUTH_WEP:
          authmode = "WIFI_AUTH_WEP";
          break;
        case WIFI_AUTH_WPA_PSK:
          authmode = "WIFI_AUTH_WPA_PSK";
          break;
        case WIFI_AUTH_WPA2_PSK:
          authmode = "WIFI_AUTH_WPA2_PSK";
          break;
        case WIFI_AUTH_WPA_WPA2_PSK:
          authmode = "WIFI_AUTH_WPA_WPA2_PSK";
          break;
        default:
          authmode = "Unknown";
          break;
        }
      ESP_LOGI(TAG, "Found %s %02x:%02x:%02x:%02x:%02x:%02x (CHANNEL %d, RSSI %d, AUTH %s)",
        list[k].ssid,
        list[k].bssid[0], list[k].bssid[1], list[k].bssid[2],
        list[k].bssid[3], list[k].bssid[4], list[k].bssid[5],
        list[k].primary, list[k].rssi, authmode.c_str());
      }
    ESP_LOGI(TAG, "SSID scan completed");
    StopStation();
    }
  else if (m_mode == ESP32WIFI_MODE_SCLIENT)
    {
    for (int k=0; (k<apCount)&&(m_wifi_sta_cfg.sta.ssid[0]==0); k++)
      {
      std::string password = MyConfig.GetParamValue("wifi.ssid", (const char*)list[k].ssid);
      if (!password.empty())
        {
        ESP_LOGI(TAG,"Found SSID %s - trying to connect",(const char*)list[k].ssid);
        memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
        strcpy((char*)m_wifi_sta_cfg.sta.ssid, (const char*)list[k].ssid);
        strcpy((char*)m_wifi_sta_cfg.sta.password, password.c_str());
        m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));
        ESP_ERROR_CHECK(esp_wifi_connect());
        }
      }
    if (m_wifi_sta_cfg.sta.ssid[0]==0)
      {
      // Try another scan a bit later
      m_nextscan = monotonictime + 30;
      }
    }

  if (list)
    free(list);
  }

void esp32wifi::OutputStatus(int verbosity, OvmsWriter* writer)
  {
  writer->printf("WiFi\n  Power: %s\n  Mode: %s\n",
    MyPcpApp.FindPowerModeByType(m_powermode), esp32wifi_mode_names[m_mode]);
  switch (m_mode)
    {
    case ESP32WIFI_MODE_CLIENT:
    case ESP32WIFI_MODE_SCLIENT:
      writer->printf("\n  STA SSID: %s\n    MAC: " MACSTR "\n    IP: " IPSTR "/" IPSTR "\n    GW: " IPSTR "\n",
        m_wifi_sta_cfg.sta.ssid, MAC2STR(m_mac_sta),
        IP2STR(&m_ip_info_sta.ip), IP2STR(&m_ip_info_sta.netmask), IP2STR(&m_ip_info_sta.gw));
      break;
    case ESP32WIFI_MODE_APCLIENT:
      writer->printf("\n  STA SSID: %s\n    MAC: " MACSTR "\n    IP: " IPSTR "/" IPSTR "\n    GW: " IPSTR "\n",
        m_wifi_sta_cfg.sta.ssid, MAC2STR(m_mac_sta),
        IP2STR(&m_ip_info_sta.ip), IP2STR(&m_ip_info_sta.netmask), IP2STR(&m_ip_info_sta.gw));
      // Falling through (no break) to ESP32WIFI_MODE_AP on purpose
    case ESP32WIFI_MODE_AP:
      writer->printf("\n  AP SSID: %s\n    MAC: " MACSTR "\n    IP: " IPSTR "\n",
        m_wifi_ap_cfg.ap.ssid, MAC2STR(m_mac_ap), IP2STR(&m_ip_info_ap.ip));
      wifi_sta_list_t sta_list;
      tcpip_adapter_sta_list_t ip_list;
      if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK)
        {
        writer->printf("    AP Stations: %d\n", sta_list.num);
        if (tcpip_adapter_get_sta_list(&sta_list, &ip_list) == ESP_OK)
          {
          for (int i=0; i<sta_list.num; i++)
            writer->printf("      %d: MAC: " MACSTR ", IP: " IPSTR "\n", i+1, MAC2STR(ip_list.sta[i].mac), IP2STR(&ip_list.sta[i].ip));
          }
        else
          {
          for (int i=0; i<sta_list.num; i++)
            writer->printf("      %d: MAC: " MACSTR "\n", i+1, MAC2STR(sta_list.sta[i].mac));
          }
        }
      else
        {
        writer->printf("    Stations: unknown\n");
        }
      break;
    default:
      break;
    }
  }
