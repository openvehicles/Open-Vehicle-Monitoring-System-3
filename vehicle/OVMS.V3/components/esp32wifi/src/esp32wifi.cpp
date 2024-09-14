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
#include <lwip/inet.h>
#include <lwip/dns.h>
#include "esp32wifi.h"
#include "esp_wifi.h"
#include "ovms.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"
#include "ovms_events.h"
#include "metrics_standard.h"
#if ESP_IDF_VERSION_MAJOR >= 4
#include <esp_wifi_types.h>
#endif
#if ESP_IDF_VERSION_MAJOR >= 5
#include <esp_wifi_ap_get_sta_list.h>
#include <dhcpserver/dhcpserver.h>
#endif

const char* const esp32wifi_mode_names[] = {
  "Modem is off",
  "Client mode",
  "Access-Point mode",
  "Access-Point + Client mode",
  "SCAN mode"
};

static const char* wifi_err_reason_code(wifi_err_reason_t reason)
  {
  const char* code;
  switch (reason)
    {
    case WIFI_REASON_UNSPECIFIED:                 code = "UNSPECIFIED"; break;
    case WIFI_REASON_AUTH_EXPIRE:                 code = "AUTH_EXPIRE"; break;
    case WIFI_REASON_AUTH_LEAVE:                  code = "AUTH_LEAVE"; break;
    case WIFI_REASON_ASSOC_EXPIRE:                code = "ASSOC_EXPIRE"; break;
    case WIFI_REASON_ASSOC_TOOMANY:               code = "ASSOC_TOOMANY"; break;
    case WIFI_REASON_NOT_AUTHED:                  code = "NOT_AUTHED"; break;
    case WIFI_REASON_NOT_ASSOCED:                 code = "NOT_ASSOCED"; break;
    case WIFI_REASON_ASSOC_LEAVE:                 code = "ASSOC_LEAVE"; break;
    case WIFI_REASON_ASSOC_NOT_AUTHED:            code = "ASSOC_NOT_AUTHED"; break;
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:         code = "DISASSOC_PWRCAP_BAD"; break;
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:        code = "DISASSOC_SUPCHAN_BAD"; break;
    case WIFI_REASON_IE_INVALID:                  code = "IE_INVALID"; break;
    case WIFI_REASON_MIC_FAILURE:                 code = "MIC_FAILURE"; break;
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:      code = "4WAY_HANDSHAKE_TIMEOUT"; break;
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:    code = "GROUP_KEY_UPDATE_TIMEOUT"; break;
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:          code = "IE_IN_4WAY_DIFFERS"; break;
    case WIFI_REASON_GROUP_CIPHER_INVALID:        code = "GROUP_CIPHER_INVALID"; break;
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:     code = "PAIRWISE_CIPHER_INVALID"; break;
    case WIFI_REASON_AKMP_INVALID:                code = "AKMP_INVALID"; break;
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:       code = "UNSUPP_RSN_IE_VERSION"; break;
    case WIFI_REASON_INVALID_RSN_IE_CAP:          code = "INVALID_RSN_IE_CAP"; break;
    case WIFI_REASON_802_1X_AUTH_FAILED:          code = "802_1X_AUTH_FAILED"; break;
    case WIFI_REASON_CIPHER_SUITE_REJECTED:       code = "CIPHER_SUITE_REJECTED"; break;
    case WIFI_REASON_BEACON_TIMEOUT:              code = "BEACON_TIMEOUT"; break;
    case WIFI_REASON_NO_AP_FOUND:                 code = "NO_AP_FOUND"; break;
    case WIFI_REASON_AUTH_FAIL:                   code = "AUTH_FAIL"; break;
    case WIFI_REASON_ASSOC_FAIL:                  code = "ASSOC_FAIL"; break;
    case WIFI_REASON_HANDSHAKE_TIMEOUT:           code = "HANDSHAKE_TIMEOUT"; break;
    case WIFI_REASON_CONNECTION_FAIL:             code = "CONNECTION_FAIL"; break;
    default:                                      code = "UNKNOWN"; break;
    }
  return code;
  }


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
    me->StartClientMode("", "");
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

  std::string stassid, stapassword;
  if (argc >= 2 && argv[1][0])
    {
    stassid = argv[1];
    stapassword = MyConfig.GetParamValue("wifi.ssid", argv[1]);
    if (stapassword.empty())
      {
      writer->puts("Error: SSID password must be defined in config wifi.ssid");
      return;
      }
    }

  if (argc >= 3)
    {
    uint8_t bssid[6];
    const char *p = argv[2];
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
    writer->printf("Starting WIFI as Access Point and client to %s (%02x:%02x:%02x:%02x:%02x:%02x)...\n",
      stassid.c_str(),
      bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
    me->StartAccessPointClientMode(argv[0], appassword, stassid, stapassword, bssid);
    }
  else
    {
    writer->puts("Starting WIFI as Access Point and Client...");
    me->StartAccessPointClientMode(argv[0], appassword, stassid, stapassword);
    }
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

  bool json = false;
  if (argc > 0)
    {
    if (strcmp(argv[0], "-j") != 0)
      {
      cmd->PutUsage(writer);
      return;
      }
    json = true;
    }

  me->Scan(writer, json);
  }

void wifi_reconnect(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32wifi *me = MyPeripherals->m_esp32wifi;
  if (me == NULL)
    {
    writer->puts("Error: wifi peripheral could not be found");
    return;
    }

  me->Reconnect(writer);
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

void wifi_ip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
    esp32wifi *me = MyPeripherals->m_esp32wifi;
    const char* ipmode = cmd->GetName();
    if (me == NULL)
      {
      writer->puts("Error: wifi peripheral could not be found");
      return;
      }
    if (strcmp(ipmode, "dhcp")==0)
      {
      writer->puts("Setting dynamic ip details for STA....");
      me->StartDhcpClient();
      return;
      }
    writer->puts("Setting static ip details for STA....");
    if (argc ==  0)
      {
      me->SetSTAWifiIP();
      return;
      }
    else if (argc == 3)
      {
      me->SetSTAWifiIP(argv[0], argv[1], argv[2]);
      return;
      }
    else writer->puts("Error: incorrect number of arguments");
  }

class esp32wifiInit
    {
    public: esp32wifiInit();
  } esp32wifiInit  __attribute__ ((init_priority (8000)));

esp32wifiInit::esp32wifiInit()
  {
  ESP_LOGI(TAG, "Initialising ESP32WIFI (8000)");

  OvmsCommand* cmd_wifi = MyCommandApp.RegisterCommand("wifi","WIFI framework", wifi_status);
  cmd_wifi->RegisterCommand("scan", "Perform a wifi scan", wifi_scan, "[-j]\n-j = output in JSON format", 0, 1);
  cmd_wifi->RegisterCommand("status","Show wifi status",wifi_status);
  cmd_wifi->RegisterCommand("reconnect","Reconnect wifi client",wifi_reconnect);

  OvmsCommand* cmd_mode = cmd_wifi->RegisterCommand("mode","WIFI mode framework");
  cmd_mode->RegisterCommand("client","Connect to a WIFI network as a client",wifi_mode_client,
    "[<ssid>] [<bssid>]\n"
    "Omit <ssid> or pass empty string to activate scanning mode.\n"
    "Set <bssid> to a MAC address to bind to a specific access point.", 0, 2);
  cmd_mode->RegisterCommand("ap","Acts as a WIFI Access Point",wifi_mode_ap, "<ssid>", 1, 1);
  cmd_mode->RegisterCommand("apclient","Acts as a WIFI Access Point and Client",wifi_mode_apclient,
    "<apssid> [<stassid>] [<stabssid>]\n"
    "Omit <stassid> or pass empty string to activate scanning mode.\n"
    "Set <stabssid> to a MAC address to bind to a specific access point.", 1, 3);
  cmd_mode->RegisterCommand("off","Turn off wifi networking",wifi_mode_off);
  OvmsCommand* cmd_mode_ip = cmd_wifi->RegisterCommand("ip","WIFI static/dhcp ip framework");
  cmd_mode_ip->RegisterCommand("static","Set static ip, subnet, gateway",wifi_ip,
    "[<ip> <subnet> <gateway>]\n"
    "Example: 192.168.12.34 255.255.255.0 192.168.12.1\n"
    "The gateway will also be used as the DNS.\n"
    "Omit optional arguments to read the configured setup for the current SSID:\n"
    "Use config set wifi.ssid \"<ssid>.ovms.staticip\" \"<ip>,<subnet>,<gateway>\"\n"
    "to configure persistent static details for a SSID.", 0, 3);
  cmd_mode_ip->RegisterCommand("dhcp","Turn on dhcp client",wifi_ip);
  }

esp32wifi::esp32wifi(const char* name)
  : pcp(name)
  {
  m_mode = ESP32WIFI_MODE_OFF;
  m_sta_ssid = "";
  m_sta_password = "";
  m_sta_bssid_set = false;
  m_ap_ssid = "";
  m_ap_password = "";
  m_previous_reason = 0;
  m_powermode = Off;
  m_poweredup = false;
  m_sta_reconnect = 0;
  m_sta_connected = false;
  m_sta_rssi = -1270;
  m_good_signal = false;
  memset(&m_wifi_ap_cfg,0,sizeof(m_wifi_ap_cfg));
  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  memset(&m_mac_ap,0,sizeof(m_mac_ap));
  memset(&m_mac_sta,0,sizeof(m_mac_sta));
  memset(&m_sta_ap_info,0,sizeof(m_sta_ap_info));
  memset(&m_ip_info_sta,0,sizeof(m_ip_info_sta));
  memset(&m_ip_info_ap,0,sizeof(m_ip_info_ap));
  MyConfig.RegisterParam("wifi.ssid", "WIFI SSID", true, false);
  MyConfig.RegisterParam("wifi.ap", "WIFI Access Point", true, false);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.start",std::bind(&esp32wifi::EventWifiStaState, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.gotip",std::bind(&esp32wifi::EventWifiGotIp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.lostip",std::bind(&esp32wifi::EventWifiLostIp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.connected",std::bind(&esp32wifi::EventWifiStaConnected, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.disconnected",std::bind(&esp32wifi::EventWifiStaDisconnected, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.1",std::bind(&esp32wifi::EventTimer1, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.scan.done",std::bind(&esp32wifi::EventWifiScanDone, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.start",std::bind(&esp32wifi::EventWifiApState, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.stop",std::bind(&esp32wifi::EventWifiApState, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.sta.connected",std::bind(&esp32wifi::EventWifiApUpdate, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.sta.disconnected",std::bind(&esp32wifi::EventWifiApUpdate, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.shuttingdown",std::bind(&esp32wifi::EventSystemShuttingDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.mounted", std::bind(&esp32wifi::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&esp32wifi::ConfigChanged, this, _1, _2));
  ConfigChanged("config.mounted", NULL);
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
      if (apssid == "OVMS" && !MyConfig.IsDefined("password", "module"))
        {
        // factory reset situation:
        ESP_LOGW(TAG, "AutoInit: factory reset detected, starting public AP net 'OVMS' with password 'OVMSinit'");
        appassword = "OVMSinit";
        }
      else
        {
        // fallback to module password:
        ESP_LOGW(TAG, "AutoInit: using module password as AP password");
        appassword = MyConfig.GetParamValue("password", "module");
        }
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
      StartClientMode("", "");
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
      }

    stassid = MyConfig.GetParamValue("auto", "wifi.ssid.client");
    if (!stassid.empty())
      {
      stapassword = MyConfig.GetParamValue("wifi.ssid", stassid);
      if (stapassword.empty())
        {
        ESP_LOGE(TAG, "AutoInit: Wifi client password must be specified for SSID %s", stassid.c_str());
        }
      }

    if (!appassword.empty())
      StartAccessPointClientMode(apssid, appassword, stassid, stapassword);
    else
      StartClientMode(stassid, stapassword);
    }
  }

void esp32wifi::Restart()
  {
  ESP_LOGI(TAG, "Restart");
  if (m_poweredup)
    PowerDown();
  PowerUp();
  AutoInit();
  }

void esp32wifi::SetPowerMode(PowerMode powermode)
  {
  m_powermode = powermode;
  switch (powermode)
    {
    case On:
      if (!m_poweredup) PowerUp();
      break;
    case Sleep:
      if (!m_poweredup) PowerUp();
      esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
      break;
    case DeepSleep:
      if (!m_poweredup) PowerUp();
      break;
    case Off:
      if (m_poweredup) PowerDown();
      break;
    default:
      break;
    };

  m_powermode = powermode;
  }

void esp32wifi::PowerUp()
  {
  ESP_LOGI(TAG, "Powering up WIFI driver");
  if (!m_poweredup)
    {
    OvmsMutexLock exclusive(&m_mutex);
    m_wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&m_wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    AdjustTaskPriority();
    m_poweredup = true;
    }
  }

void esp32wifi::PowerDown()
  {
  StopStation();

  ESP_LOGI(TAG, "Powering down WIFI driver");
  if (m_poweredup)
    {
    OvmsMutexLock exclusive(&m_mutex);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    m_poweredup = false;
    }
  }

void esp32wifi::StartClientMode(std::string ssid, std::string password, uint8_t* bssid)
  {
  m_sta_reconnect = 0;

  // mode reconfiguration?
  if (m_mode == ESP32WIFI_MODE_AP || m_mode == ESP32WIFI_MODE_APCLIENT || m_sta_connected)
    {
    StopStation();
    vTaskDelay(pdMS_TO_TICKS(500)); // wifi task cleanup time
    }

  // set new mode config:
  m_mode = ESP32WIFI_MODE_CLIENT;
  m_sta_ssid = ssid;
  m_sta_password = password;
  if (bssid)
    {
    m_sta_bssid_set = true;
    memcpy(m_sta_bssid, bssid, sizeof(m_sta_bssid));
    }
  else
    {
    m_sta_bssid_set = false;
    memset(m_sta_bssid, 0, sizeof(m_sta_bssid));
    }

  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  // configure wifi mode:
  {
  OvmsMutexLock exclusive(&m_mutex);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  strcpy((char*)m_wifi_sta_cfg.sta.ssid, m_sta_ssid.c_str());
  strcpy((char*)m_wifi_sta_cfg.sta.password, m_sta_password.c_str());
  m_wifi_sta_cfg.sta.bssid_set = m_sta_bssid_set;
  memcpy(&m_wifi_sta_cfg.sta.bssid, m_sta_bssid, 6);
  m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());
  }

  // connection started in EventWifiStaState()
  }

void esp32wifi::StartAccessPointMode(std::string ssid, std::string password)
  {
  m_sta_reconnect = 0;

  // mode reconfiguration?
  if (m_mode == ESP32WIFI_MODE_CLIENT || m_mode == ESP32WIFI_MODE_APCLIENT)
    {
    StopStation();
    vTaskDelay(pdMS_TO_TICKS(500)); // wifi task cleanup time
    }

  // set new mode config:
  m_mode = ESP32WIFI_MODE_AP;
  m_ap_ssid = ssid;
  m_ap_password = password;

  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  // configure wifi mode:
  {
  OvmsMutexLock exclusive(&m_mutex);

  // we need APSTA mode to be able to do scans:
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  SetAPWifiBW();

  memset(&m_wifi_ap_cfg,0,sizeof(m_wifi_ap_cfg));
  m_wifi_ap_cfg.ap.ssid_len = 0;
  m_wifi_ap_cfg.ap.channel = 0;
  m_wifi_ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
  m_wifi_ap_cfg.ap.ssid_hidden = 0;
  m_wifi_ap_cfg.ap.max_connection = 4;
  m_wifi_ap_cfg.ap.beacon_interval = 100;
  strcpy((char*)m_wifi_ap_cfg.ap.ssid, m_ap_ssid.c_str());
  strcpy((char*)m_wifi_ap_cfg.ap.password, m_ap_password.c_str());
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &m_wifi_ap_cfg));

  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));

  ESP_ERROR_CHECK(esp_wifi_start());
  }
  }

void esp32wifi::StartAccessPointClientMode(std::string apssid, std::string appassword, std::string stassid, std::string stapassword, uint8_t* stabssid)
  {
  m_sta_reconnect = 0;

  // mode reconfiguration?
  if (m_mode == ESP32WIFI_MODE_CLIENT || m_mode == ESP32WIFI_MODE_AP || m_sta_connected)
    {
    StopStation();
    vTaskDelay(pdMS_TO_TICKS(500)); // wifi task cleanup time
    }

  // set new mode config:
  m_mode = ESP32WIFI_MODE_APCLIENT;
  m_ap_ssid = apssid;
  m_ap_password = appassword;
  m_sta_ssid = stassid;
  m_sta_password = stapassword;
  if (stabssid)
    {
    m_sta_bssid_set = true;
    memcpy(m_sta_bssid, stabssid, sizeof(m_sta_bssid));
    }
  else
    {
    m_sta_bssid_set = false;
    memset(m_sta_bssid, 0, sizeof(m_sta_bssid));
    }

  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  // configure wifi mode:
  {
  OvmsMutexLock exclusive(&m_mutex);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  SetAPWifiBW();

  memset(&m_wifi_ap_cfg,0,sizeof(m_wifi_ap_cfg));
  m_wifi_ap_cfg.ap.ssid_len = 0;
  m_wifi_ap_cfg.ap.channel = 0;
  m_wifi_ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
  m_wifi_ap_cfg.ap.ssid_hidden = 0;
  m_wifi_ap_cfg.ap.max_connection = 4;
  m_wifi_ap_cfg.ap.beacon_interval=100;
  strcpy((char*)m_wifi_ap_cfg.ap.ssid, m_ap_ssid.c_str());
  strcpy((char*)m_wifi_ap_cfg.ap.password, m_ap_password.c_str());
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &m_wifi_ap_cfg));

  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  strcpy((char*)m_wifi_sta_cfg.sta.ssid, m_sta_ssid.c_str());
  strcpy((char*)m_wifi_sta_cfg.sta.password, m_sta_password.c_str());
  m_wifi_sta_cfg.sta.bssid_set = m_sta_bssid_set;
  memcpy(&m_wifi_sta_cfg.sta.bssid, m_sta_bssid, 6);
  m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());
  }

  // connection started in EventWifiStaState()
  }

void esp32wifi::Reconnect(OvmsWriter* writer)
  {
  if (m_mode != ESP32WIFI_MODE_CLIENT && m_mode != ESP32WIFI_MODE_APCLIENT)
    {
    if (writer)
      writer->puts("ERROR: wifi not in client or apclient mode");
    else
      ESP_LOGE(TAG, "Reconnect: wifi not in client or apclient mode");
    return;
    }

  if (writer)
    writer->puts("Starting Wifi client reconnect.");
  else
    ESP_LOGE(TAG, "Reconnect: starting Wifi client reconnect");
  if (!m_sta_connected)
    {
    m_sta_reconnect = monotonictime + 1;
    }
  else
    {
    esp_err_t res = esp_wifi_disconnect();
    if (res != ESP_OK)
      {
      if (writer)
        writer->printf("ERROR: Wifi disconnect failed, error=0x%x\n", res);
      else
        ESP_LOGE(TAG, "Reconnect: Wifi disconnect failed, error=0x%x\n", res);
      }
    }
  }

void esp32wifi::StopStation()
  {
  OvmsMutexLock exclusive(&m_mutex);

  if (m_mode != ESP32WIFI_MODE_OFF)
    {
    ESP_LOGI(TAG, "Stopping WIFI station");

    MyEvents.SignalEvent("system.wifi.down",NULL);
    if (m_mode == ESP32WIFI_MODE_CLIENT || m_mode == ESP32WIFI_MODE_APCLIENT)
      ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    m_mode = ESP32WIFI_MODE_OFF;
    }

  m_sta_reconnect = 0;
  m_sta_connected = false;

  memset(&m_wifi_ap_cfg,0,sizeof(m_wifi_ap_cfg));
  memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
  memset(&m_mac_ap,0,sizeof(m_mac_ap));
  memset(&m_mac_sta,0,sizeof(m_mac_sta));
  memset(&m_ip_info_sta,0,sizeof(m_ip_info_sta));
  memset(&m_ip_info_ap,0,sizeof(m_ip_info_ap));

  UpdateNetMetrics();
  }

wifi_active_scan_time_t esp32wifi::GetScanTime()
  {
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#scan-configuration
  // On wifi_active_scan_time_t:
  //  min=0, max=0: scan dwells on each channel for 120 ms.
  //  min>0, max=0: scan dwells on each channel for 120 ms.
  //  min=0, max>0: scan dwells on each channel for max ms.
  //  min>0, max>0: the minimum time the scan dwells on each channel is min ms.
  //    If no AP is found during this time frame, the scan switches to the next channel.
  //    Otherwise, the scan dwells on the channel for max ms.

  // Hint: some older Android APs need higher min times, try 200 ms.

  wifi_active_scan_time_t active;
  active.min = MyConfig.GetParamValueInt("network", "wifi.scan.tmin", 120);
  active.max = MyConfig.GetParamValueInt("network", "wifi.scan.tmax", 120);
  if (active.max < active.min)
    active.max = active.min;
  return active;
  }

void esp32wifi::Scan(OvmsWriter* writer, bool json)
  {
  uint16_t apCount = 0;
  esp_err_t res;
  wifi_ap_record_t* list = NULL;
  esp32wifi_mode_t mode = m_mode;

  if (m_mode == ESP32WIFI_MODE_OFF)
    {
    if (m_powermode != On)
      {
      SetPowerMode(On);
      }

    {
    OvmsMutexLock exclusive(&m_mutex);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
    m_wifi_sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    }
    }

  m_mode = ESP32WIFI_MODE_SCAN;
  esp_wifi_scan_stop();
  if (m_sta_reconnect)
    m_sta_reconnect = monotonictime + 10;

  if (!json)
    writer->puts("Scanning for WIFI Access Points...");

  wifi_scan_config_t scanConf;
  memset(&scanConf,0,sizeof(scanConf));
  scanConf.ssid = NULL;
  scanConf.bssid = NULL;
  scanConf.channel = 0;
  scanConf.show_hidden = true;
  scanConf.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  scanConf.scan_time.active = GetScanTime();
  res = esp_wifi_scan_start(&scanConf, true);
  if (res != ESP_OK)
    {
    if (json)
      writer->printf("{\"error\":\"can't start scan, error=0x%x\"}", res);
    else
      writer->printf("ERROR: can't start scan, error=0x%x\n", res);
    }

  if (res == ESP_OK)
    {
    res = esp_wifi_scan_get_ap_num(&apCount);
    if (res != ESP_OK)
      {
      if (json)
        writer->printf("{\"error\":\"can't get AP count, error=0x%x\"}", res);
      else
        writer->printf("ERROR: can't get AP count, error=0x%x\n", res);
      }
    }

  if (res == ESP_OK && apCount > 0)
    {
    list = (wifi_ap_record_t *)InternalRamMalloc(sizeof(wifi_ap_record_t) * apCount);
    if (!list)
      {
      if (json)
        writer->printf("{\"error\":\"can't get AP records, out of memory\"}");
      else
        writer->printf("ERROR: can't get AP records, out of memory\n");
      }
    else
      {
      res = esp_wifi_scan_get_ap_records(&apCount, list);
      if (res != ESP_OK)
        {
        if (json)
          writer->printf("{\"error\":\"can't get AP records, error=0x%x\"}", res);
        else
          writer->printf("ERROR: can't get AP records, error=0x%x\n", res);
        }
      }
    }

  if (res == ESP_OK && list)
    {
    if (json)
      {
      writer->printf("{\"list\":[");
      }
    else
      {
      writer->printf("\n%-32.32s %-17.17s %4s %4s %-22.22s\n","AP SSID","MAC ADDRESS","CHAN","RSSI","AUTHENTICATION");
      writer->printf("%-32.32s %-17.17s %4s %4s %-22.22s\n","================================","=================","====","====","==============");
      }
    for (int k=0; k<apCount; k++)
      {
      std::string authmode;
      switch(list[k].authmode)
        {
        case WIFI_AUTH_OPEN:
          authmode = "OPEN";
          break;
        case WIFI_AUTH_WEP:
          authmode = "WEP";
          break;
        case WIFI_AUTH_WPA_PSK:
          authmode = "WPA_PSK";
          break;
        case WIFI_AUTH_WPA2_PSK:
          authmode = "WPA2_PSK";
          break;
        case WIFI_AUTH_WPA_WPA2_PSK:
          authmode = "WPA_WPA2_PSK";
          break;
        default:
          authmode = "Unknown";
          break;
        }
      if (json)
        {
        std::string ssid = (strlen((char*)list[k].ssid)==0) ? "<HIDDEN>" : json_encode((std::string)(char*)list[k].ssid);
        writer->printf("%s{\"ssid\":\"%s\",\"bssid\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"chan\":%d,\"rssi\":%d,\"auth\":\"%s\"}",
          (k==0) ? "" : ",", ssid.c_str(),
          list[k].bssid[0], list[k].bssid[1], list[k].bssid[2],
          list[k].bssid[3], list[k].bssid[4], list[k].bssid[5],
          list[k].primary, list[k].rssi, authmode.c_str());
        }
      else
        {
        writer->printf("%-32.32s %02x:%02x:%02x:%02x:%02x:%02x %4d %4d %s\n",
          (strlen((char*)list[k].ssid)==0)?"<HIDDEN>":(char*)list[k].ssid,
          list[k].bssid[0], list[k].bssid[1], list[k].bssid[2],
          list[k].bssid[3], list[k].bssid[4], list[k].bssid[5],
          list[k].primary, list[k].rssi, authmode.c_str());
        }
      }
    if (json)
      {
      writer->puts("]}");
      }
    else
      {
      writer->puts("===========================================================================");
      writer->printf("Scan complete: %d access point(s) found\n\n",apCount);
      }
    }
  else if (res == ESP_OK && apCount == 0)
    {
    writer->puts("Scan complete: no access points found\n");
    }

  if (list) free(list);

  // restore mode:
  m_mode = mode;
  if (m_mode == ESP32WIFI_MODE_OFF)
    {
    ESP_ERROR_CHECK(esp_wifi_stop());
    }
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

std::string esp32wifi::GetAPSSID()
  {
  if (m_mode == ESP32WIFI_MODE_AP || m_mode == ESP32WIFI_MODE_APCLIENT)
    return std::string((char*)m_wifi_ap_cfg.ap.ssid);
  else
    return std::string("");
  }

void esp32wifi::UpdateNetMetrics()
  {
  // update/clear access point info:
  if (m_sta_connected && esp_wifi_sta_get_ap_info(&m_sta_ap_info) == ESP_OK)
    {
    int newrssi = m_sta_ap_info.rssi * 10;
    m_sta_rssi = (m_sta_rssi * 3 + newrssi) / 4;
    }
  else
    {
    memset(&m_sta_ap_info, 0, sizeof(m_sta_ap_info));
    m_sta_rssi = -1270;
    }

    float current_dbm = (float)m_sta_rssi / 10;
    StdMetrics.ms_m_net_wifi_network->SetValue(GetSSID());
    StdMetrics.ms_m_net_wifi_sq->SetValue(current_dbm, dbm);
    if (StdMetrics.ms_m_net_type->AsString() == "wifi")
      {
      StdMetrics.ms_m_net_provider->SetValue(GetSSID());
      StdMetrics.ms_m_net_sq->SetValue((int)(m_sta_rssi-5)/10, dbm);
      if (m_good_signal && current_dbm < m_bad_dbm)
        m_good_signal = false;
      if (!m_good_signal && current_dbm > m_good_dbm)
        m_good_signal = true;
      StdMetrics.ms_m_net_good_sq->SetValue(m_good_signal);
      }
  }

void esp32wifi::EventWifiGotIp(std::string event, void* data)
  {
#if ESP_IDF_VERSION_MAJOR >= 4
  ip_event_got_ip_t *info = (ip_event_got_ip_t*)data;
  m_ip_info_sta = info->ip_info;
#else
  system_event_info_t *info = (system_event_info_t*)data;
  m_ip_info_sta = info->got_ip.ip_info;
#endif
  esp_wifi_get_mac(WIFI_IF_STA, m_mac_sta);
  UpdateNetMetrics();
  ESP_LOGI(TAG, "STA got IP with SSID '%s' AP " MACSTR ": MAC: " MACSTR ", IP: " IPSTR ", mask: " IPSTR ", gw: " IPSTR,
    m_wifi_sta_cfg.sta.ssid, MAC2STR(m_sta_ap_info.bssid), MAC2STR(m_mac_sta),
    IP2STR(&m_ip_info_sta.ip), IP2STR(&m_ip_info_sta.netmask), IP2STR(&m_ip_info_sta.gw));
  }

bool esp32wifi::WifiHasIp()
  {
  char numstr[150];
  sprintf(numstr, IPSTR, IP2STR(&m_ip_info_sta.ip));
  return strcmp(numstr, "0.0.0.0") == 1;
  }

void esp32wifi::EventWifiLostIp(std::string event, void* data)
  {
  memset(&m_ip_info_sta,0,sizeof(m_ip_info_sta));
  UpdateNetMetrics();
  ESP_LOGI(TAG, "STA lost IP from SSID '%s' AP " MACSTR,
    m_wifi_sta_cfg.sta.ssid, MAC2STR(m_sta_ap_info.bssid));
  }

void esp32wifi::EventWifiStaConnected(std::string event, void* data)
  {
#if ESP_IDF_VERSION_MAJOR >= 4
  wifi_event_sta_connected_t& conn = *((wifi_event_sta_connected_t*)data);
#else
  system_event_sta_connected_t& conn = ((system_event_info_t*)data)->connected;
#endif

  m_sta_connected = true;
  m_previous_reason = 0;
  UpdateNetMetrics();

  ESP_LOGI(TAG, "STA connected with SSID: %.*s, BSSID: " MACSTR ", Channel: %u, Auth: %s",
    conn.ssid_len, conn.ssid, MAC2STR(conn.bssid), conn.channel,
    conn.authmode == WIFI_AUTH_OPEN ? "None" :
    conn.authmode == WIFI_AUTH_WEP ? "WEP" :
    conn.authmode == WIFI_AUTH_WPA_PSK ? "WPA" :
    conn.authmode == WIFI_AUTH_WPA2_PSK ? "WPA2" :
    conn.authmode == WIFI_AUTH_WPA_WPA2_PSK ? "WPA/WPA2" : "Unknown");
  }

void esp32wifi::EventWifiStaDisconnected(std::string event, void* data)
  {
#if ESP_IDF_VERSION_MAJOR >= 4
  wifi_event_sta_disconnected_t& disconn = *((wifi_event_sta_disconnected_t*)data);
#else
  system_event_sta_disconnected_t& disconn = ((system_event_info_t*)data)->disconnected;
#endif

  if (disconn.reason != m_previous_reason)
    {
    ESP_LOGI(TAG, "STA disconnected with reason %d = %s",
      disconn.reason, wifi_err_reason_code((wifi_err_reason_t)disconn.reason));
    m_previous_reason = disconn.reason;
    }

  m_sta_connected = false;
  memset(&m_ip_info_sta,0,sizeof(m_ip_info_sta));

  UpdateNetMetrics();
  }

void esp32wifi::AdjustTaskPriority()
  {
  // lower wifi task priority from 23 to 22 to prioritize CAN rx:
#ifdef HAVE_TaskGetHandle
  TaskHandle_t wifitask = TaskGetHandle("wifi");
  if (wifitask)
    vTaskPrioritySet(wifitask, 22);
#else
  ESP_LOGW(TAG, "can't adjust wifi task priority (TaskGetHandle not available)");
#endif
  }

void esp32wifi::EventWifiStaState(std::string event, void* data)
  {
  if (event == "system.wifi.sta.start")
    {
    // Set hostname for DHCP request
    std::string vehicleid = MyConfig.GetParamValue("vehicle", "id");
    if (vehicleid.empty()) vehicleid = "ovms";
#if ESP_IDF_VERSION_MAJOR >= 4
    if (ESP_OK != esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), vehicleid.c_str()))
#else
    if (ESP_OK != tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, vehicleid.c_str()))
#endif
      ESP_LOGW(TAG, "failed to set hostname");

    AdjustTaskPriority();

    // connect?
    if ((m_mode == ESP32WIFI_MODE_CLIENT || m_mode == ESP32WIFI_MODE_APCLIENT)
        && !m_sta_connected)
      {
      StartConnect();
      }
    }
  }

void esp32wifi::EventWifiApState(std::string event, void* data)
  {
  if (event == "system.wifi.ap.start")
    {
    // Start
    AdjustTaskPriority();
    esp_wifi_get_mac(WIFI_IF_AP, m_mac_ap);
#if ESP_IDF_VERSION_MAJOR >= 4
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &m_ip_info_ap);
#else
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &m_ip_info_ap);
#endif

    // Disable routing (gateway) and DNS offer on DHCP server for AP:
    // Note: disabling the DNS offer requires esp-idf fix in commit 4195d7c2eec2d93781d5f88fd71f5c7e1ee1ff20
    esp_err_t err;
#if ESP_IDF_VERSION_MAJOR >= 4
    err = esp_netif_dhcps_stop(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"));
#else
    err = tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
#endif
    if (err != ESP_OK)
      {
      ESP_LOGE(TAG, "AP: failed reconfiguring DHCP server; error=%d", err);
      }
    else
      {
      dhcps_offer_t opt_val = 0;
#if ESP_IDF_VERSION_MAJOR >= 4
      err = esp_netif_dhcps_option(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &opt_val, sizeof(opt_val));
#else
      err = tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_DOMAIN_NAME_SERVER, &opt_val, sizeof(opt_val));
#endif
      if (err != ESP_OK)
        ESP_LOGW(TAG, "AP: failed disabling DHCP DNS offer; error=%d", err);
#if ESP_IDF_VERSION_MAJOR >= 4
      err = esp_netif_dhcps_option(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, &opt_val, sizeof(opt_val));
#else
      err = tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_ROUTER_SOLICITATION_ADDRESS, &opt_val, sizeof(opt_val));
#endif
      if (err != ESP_OK)
        ESP_LOGW(TAG, "AP: failed disabling DHCP routing offer; error=%d", err);
#if ESP_IDF_VERSION_MAJOR >= 4
      err = esp_netif_dhcps_start(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"));
#else
      err = tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
#endif
      if (err != ESP_OK)
        ESP_LOGE(TAG, "AP: failed restarting DHCP server; error=%d", err);
      }

    ESP_LOGI(TAG, "AP started with SSID: %s, MAC: " MACSTR ", IP: " IPSTR,
      m_wifi_ap_cfg.ap.ssid, MAC2STR(m_mac_ap), IP2STR(&m_ip_info_ap.ip));
    }
  else
    {
    // Stop
    memset(&m_mac_ap,0,sizeof(m_mac_ap));
    memset(&m_ip_info_ap,0,sizeof(m_ip_info_ap));
    ESP_LOGI(TAG, "AP stopped");
    }
  }

void esp32wifi::EventWifiApUpdate(std::string event, void* data)
  {
  if (event == "system.wifi.ap.sta.connected")
    {
#if ESP_IDF_VERSION_MAJOR >= 4
    wifi_event_ap_staconnected_t& sta_conn = *((wifi_event_ap_staconnected_t*)data);
#else
    system_event_ap_staconnected_t& sta_conn = ((system_event_info_t*)data)->sta_connected;
#endif
    ESP_LOGI(TAG, "AP station connected: id: %d, MAC: " MACSTR,
      sta_conn.aid, MAC2STR(sta_conn.mac));
    }
  else
    {
#if ESP_IDF_VERSION_MAJOR >= 4
    wifi_event_ap_stadisconnected_t& sta_disconn = *((wifi_event_ap_stadisconnected_t*)data);
#else
    system_event_ap_stadisconnected_t& sta_disconn = ((system_event_info_t*)data)->sta_disconnected;
#endif
    ESP_LOGI(TAG, "AP station disconnected: id: %d, MAC: " MACSTR,
      sta_disconn.aid, MAC2STR(sta_disconn.mac));
    }
  }

void esp32wifi::EventTimer1(std::string event, void* data)
  {
  UpdateNetMetrics();

  // reconnect?
  if ((m_mode == ESP32WIFI_MODE_CLIENT || m_mode == ESP32WIFI_MODE_APCLIENT)
      && !m_sta_connected && m_sta_reconnect && monotonictime >= m_sta_reconnect)
    {
    StartConnect();
    }
  }

void esp32wifi::StartConnect()
  {
  // Note: due to a bug (undocumented feature?) in the esp-idf Wifi blob, we do not
  // rely on esp_wifi_connect() picking the right AP, as it will do a round-robin
  // scheme on first call among multiple APs of the same SSID. Instead we always
  // do a scan and connect explicitly to the AP with the strongest signal.

  wifi_scan_config_t scanConf;
  memset(&scanConf,0,sizeof(scanConf));
  scanConf.ssid = NULL;
  scanConf.bssid = NULL;
  scanConf.channel = 0;
  scanConf.show_hidden = true;
  scanConf.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  scanConf.scan_time.active = GetScanTime();
  esp_err_t res = esp_wifi_scan_start(&scanConf, false);
  if (res != ESP_OK)
    ESP_LOGE(TAG, "StartConnect: error 0x%x starting scan", res);
  else
    ESP_LOGV(TAG, "StartConnect: scan started");

  // next regular scan in 10 seconds:
  m_sta_reconnect = monotonictime + 10;
  }

void esp32wifi::EventWifiScanDone(std::string event, void* data)
  {
  uint16_t apCount = 0;
  esp_err_t res;
  wifi_ap_record_t* list = NULL;
  std::string ssid, password;

  if (m_mode == ESP32WIFI_MODE_SCAN) return; // Let scan routine handle it

  res = esp_wifi_scan_get_ap_num(&apCount);
  if (res != ESP_OK)
    {
    ESP_LOGE(TAG, "EventWifiScanDone: can't get AP count, error=0x%x", res);
    return;
    }
  if (apCount == 0)
    {
    ESP_LOGV(TAG, "EventWifiScanDone: no access points found");
    return;
    }

  list = (wifi_ap_record_t *)InternalRamMalloc(sizeof(wifi_ap_record_t) * apCount);
  if (!list)
    {
    ESP_LOGE(TAG, "EventWifiScanDone: can't get AP records: out of internal memory");
    return;
    }
  res = esp_wifi_scan_get_ap_records(&apCount, list);
  if (res != ESP_OK)
    {
    ESP_LOGE(TAG, "EventWifiScanDone: can't get AP records, error=0x%x", res);
    return;
    }

  if (m_mode != ESP32WIFI_MODE_AP && !m_sta_connected)
    {
    int ap_connect = -1;

    // check scan results for usable networks:
    for (int k=0; k<apCount; k++)
      {
      ESP_LOGV(TAG, "ScanDone: #%02d ssid='%s' bssid='" MACSTR "' chan=%d rssi=%d",
        k+1, (const char*)list[k].ssid, MAC2STR(list[k].bssid), list[k].primary, list[k].rssi);
      if (ap_connect >= 0)
        continue;
      if (m_sta_ssid.empty())
        {
        // scanning mode:
        password = MyConfig.GetParamValue("wifi.ssid", (const char*)list[k].ssid);
        if (!password.empty())
          ap_connect = k;
        }
      else
        {
        // fixed mode:
        if ((m_sta_bssid_set && memcmp(m_sta_bssid, list[k].bssid, 6) == 0)
            || (!m_sta_bssid_set && m_sta_ssid == (const char*)list[k].ssid))
          {
          password = m_sta_password;
          ap_connect = k;
          }
        }
      }

    // connect:
    if (ap_connect < 0)
      {
      ESP_LOGV(TAG, "EventWifiScanDone: no known SSID found");
      }
    else
      {
      int k = ap_connect;
      ssid = (const char*)list[k].ssid;
      if (ssid.empty())
        ssid = m_sta_ssid; // assume configured SSID on a hidden entry
      ESP_LOGI(TAG, "ScanDone: connect to ssid='%s' bssid='" MACSTR "' chan=%d rssi=%d",
        ssid.c_str(), MAC2STR(list[k].bssid), list[k].primary, list[k].rssi);
      memset(&m_wifi_sta_cfg,0,sizeof(m_wifi_sta_cfg));
      strcpy((char*)m_wifi_sta_cfg.sta.ssid, ssid.c_str());
      strcpy((char*)m_wifi_sta_cfg.sta.password, password.c_str());
      m_wifi_sta_cfg.sta.bssid_set = true;
      memcpy(m_wifi_sta_cfg.sta.bssid, list[k].bssid, sizeof(m_wifi_sta_cfg.sta.bssid));
      m_wifi_sta_cfg.sta.channel = list[k].primary;
      m_wifi_sta_cfg.sta.scan_method = WIFI_FAST_SCAN;
      m_wifi_sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
      ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &m_wifi_sta_cfg));
      ESP_ERROR_CHECK(esp_wifi_connect());
      std::string ipconfig = MyConfig.GetParamValue("wifi.ssid", ssid + ".ovms.staticip");
      if (!ipconfig.empty())
        {
        SetSTAWifiIP();
        }
      else StartDhcpClient();
      }
    }
  if (list)
    free(list);
  }

void esp32wifi::EventSystemShuttingDown(std::string event, void* data)
  {
  PowerDown();
  }

void esp32wifi::OutputStatus(int verbosity, OvmsWriter* writer)
  {
  writer->printf("Power: %s\nMode: %s\n",
    MyPcpApp.FindPowerModeByType(m_powermode), esp32wifi_mode_names[m_mode]);

  if (m_mode == ESP32WIFI_MODE_CLIENT || m_mode == ESP32WIFI_MODE_APCLIENT)
    {
    writer->printf("\nSTA SSID: %s (%.1f dBm) [%s]\n  MAC: " MACSTR "\n  IP: " IPSTR "/" IPSTR "\n  GW: " IPSTR "\n  AP: " MACSTR "\n",
      m_wifi_sta_cfg.sta.ssid, (float)m_sta_rssi/10, (m_sta_ssid.empty()) ? "scanning" : "fixed", MAC2STR(m_mac_sta),
      IP2STR(&m_ip_info_sta.ip), IP2STR(&m_ip_info_sta.netmask), IP2STR(&m_ip_info_sta.gw),
      MAC2STR(m_sta_ap_info.bssid));
    }

  if (m_mode == ESP32WIFI_MODE_AP || m_mode == ESP32WIFI_MODE_APCLIENT)
    {
    wifi_bandwidth_t bw;
    uint8_t primary;
    wifi_second_chan_t secondary;
    esp_wifi_get_channel(&primary, &secondary);
    esp_wifi_get_bandwidth(WIFI_IF_AP, &bw);
    writer->printf("\nAP SSID: %s (%d MHz, channel %d)\n  MAC: " MACSTR "\n  IP: " IPSTR "\n",
      m_wifi_ap_cfg.ap.ssid, (int)bw*20, primary, MAC2STR(m_mac_ap), IP2STR(&m_ip_info_ap.ip));
    wifi_sta_list_t sta_list;
#if ESP_IDF_VERSION_MAJOR >= 5
    wifi_sta_mac_ip_list_t ip_list;
#elif ESP_IDF_VERSION_MAJOR >= 4
    esp_netif_sta_list_t ip_list;
#else
    tcpip_adapter_sta_list_t ip_list;
#endif
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK)
      {
      writer->printf("  AP Stations: %d\n", sta_list.num);
#if ESP_IDF_VERSION_MAJOR >= 5
      if (esp_wifi_ap_get_sta_list_with_ip(&sta_list, &ip_list) == ESP_OK)
#elif ESP_IDF_VERSION_MAJOR >= 4
      if (esp_netif_get_sta_list(&sta_list, &ip_list) == ESP_OK)
#else
      if (tcpip_adapter_get_sta_list(&sta_list, &ip_list) == ESP_OK)
#endif
        {
        for (int i=0; i<sta_list.num; i++)
          writer->printf("    %d: MAC: " MACSTR ", IP: " IPSTR "\n", i+1, MAC2STR(ip_list.sta[i].mac), IP2STR(&ip_list.sta[i].ip));
        }
      else
        {
        for (int i=0; i<sta_list.num; i++)
          writer->printf("    %d: MAC: " MACSTR "\n", i+1, MAC2STR(sta_list.sta[i].mac));
        }
      }
    else
      {
      writer->printf("  Stations: unknown\n");
      }
    }
  }

void esp32wifi::ConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* param = (OvmsConfigParam*)data;
  if (event == "config.mounted" || !param || param->GetName() == "network")
    {
    // Network config has been changed, apply:
    m_good_dbm = MyConfig.GetParamValueFloat("network", "wifi.sq.good", -87);
    m_bad_dbm = MyConfig.GetParamValueFloat("network", "wifi.sq.bad", -89);
    }
  }

void esp32wifi::StartDhcpClient()
  {
  if (m_mode ==  ESP32WIFI_MODE_CLIENT || m_mode ==  ESP32WIFI_MODE_APCLIENT)
    {
    esp_err_t err;
#if ESP_IDF_VERSION_MAJOR >= 4
    err = esp_netif_dhcpc_start(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
#else
    err = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
    if (err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STARTED)
#endif
      {
      ESP_LOGE(TAG, "TCPIP: failed starting dhcp client; error=%d", err);
      }
    }
  }

void esp32wifi::SetSTAWifiIP(std::string ip, std::string sn, std::string gw)
  {
  if (m_mode ==  ESP32WIFI_MODE_CLIENT || m_mode ==  ESP32WIFI_MODE_APCLIENT)
    { //only set static details if in client mode
      // read static IP config:
    std::string ssid = GetSSID();
    if (ip.empty())
      {
      std::string ipconfig = MyConfig.GetParamValue("wifi.ssid", ssid + ".ovms.staticip");
      if (!ipconfig.empty())
        {// parse static IP config, pattern "<ip>,<sn>,<gw>":
        std::istringstream sbuf(ipconfig);
        std::getline(sbuf, ip, ',');
        std::getline(sbuf, sn, ',');
        std::getline(sbuf, gw, ',');
        ESP_LOGI(TAG,"%s.ovms.staticip param set: %s",ssid.c_str(),ipconfig.c_str());
        }
      }
    if (ip.empty())
      {
      ESP_LOGI(TAG,"%s.ovms.staticip param not set",ssid.c_str());
      return;
      }
    ESP_LOGI(TAG,"STA config ip: %s, sn: %s, gw: %s",ip.c_str(), sn.c_str(), gw.c_str());
    memset(&m_ip_static_sta,0,sizeof(m_ip_static_sta));
    memset(&m_dns_static_sta,0,sizeof(m_dns_static_sta));
    inet_aton(ip.c_str(), &m_ip_static_sta.ip);
    inet_aton(gw.c_str(), &m_ip_static_sta.gw);
    inet_aton(sn.c_str(), &m_ip_static_sta.netmask);
    inet_aton(gw.c_str(), &m_dns_static_sta.ip);
    esp_err_t err;
#if ESP_IDF_VERSION_MAJOR >= 4
    err = esp_netif_dhcpc_stop(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
#else
    err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    if (err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED)
#endif
      {
      ESP_LOGE(TAG, "DHCP: failed stopping DHCP server; error=%d", err);
      return;
      }

#if ESP_IDF_VERSION_MAJOR >= 4
    err = esp_netif_set_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &m_ip_static_sta);
#else
    err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &m_ip_static_sta);
#endif
    if (err != ESP_OK)
      {
      ESP_LOGE(TAG, "TCPIP: failed setting static ip details, restarting dhcp; error=%d", err);
#if ESP_IDF_VERSION_MAJOR >= 4
      esp_netif_dhcpc_start(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
#else
      tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
#endif
      return;
      }

#if ESP_IDF_VERSION_MAJOR >= 4
    err = esp_netif_set_dns_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"),ESP_NETIF_DNS_MAIN, &m_dns_static_sta);
#else
    err = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA,TCPIP_ADAPTER_DNS_MAIN, &m_dns_static_sta);
#endif
    if (err != ESP_OK)
      {
      ESP_LOGE(TAG, "TCPIP: failed setting static dns details; error=%d", err);
      return;
      }
#if ESP_IDF_VERSION_MAJOR >= 4
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"),&m_ip_info_sta);
#else
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA,&m_ip_info_sta);
#endif
    }
  }
  
void esp32wifi::SetAPWifiBW()
  {
  esp_err_t err;
  if (m_mode ==  ESP32WIFI_MODE_AP || m_mode ==  ESP32WIFI_MODE_APCLIENT)
    {
    uint8_t bw = MyConfig.GetParamValueInt("network", "wifi.ap.bw");
    switch (bw)
      {
      case 20:
        err = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
        break;
      case 40:
        err = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40);
        break;
      default:
        err = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
      }
    if (err != ESP_OK)
      {
      ESP_LOGE(TAG, "WIFI: failed changing bandwidth; error=%d", err);
      return;
      }
    }
  }    
