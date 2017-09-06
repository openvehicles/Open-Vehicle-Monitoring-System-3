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

#include "esp_log.h"
static const char *TAG = "obd2wifi";

#include <string.h>
#include <string>
#include "esp32wifi.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"
#include "console_telnet.h"

void wifi_mode_client(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32wifi *me = MyPeripherals->m_esp32wifi;
  if (me == NULL)
    {
    writer->puts("Error: wifi peripheral could not be found");
    return;
    }

  if (argc != 1)
    {
    writer->puts("Error: Promiscous client mode not currently supported; please specify SSID");
    return;
    }

  std::string password = MyConfig.GetParamValue("wifi.ssid", argv[0]);
  if (password.empty())
    {
    writer->puts("Error: SSID password must be defined in config wifi.ssid");
    return;
    }

  writer->puts("Starting WIFI as a client...");
  me->StartClientMode(argv[0],password);
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

class esp32wifiInit
    {
    public: esp32wifiInit();
  } esp32wifiInit  __attribute__ ((init_priority (8000)));

esp32wifiInit::esp32wifiInit()
  {
  ESP_LOGI(TAG, "Initialising ESP32WIFI (8000)");

  OvmsCommand* cmd_wifi = MyCommandApp.RegisterCommand("wifi","WIFI framework",NULL, "", 1);
  OvmsCommand* cmd_mode = cmd_wifi->RegisterCommand("mode","WIFI mode framework",NULL, "", 1);
  cmd_mode->RegisterCommand("client","Connect to a WIFI network as a client",wifi_mode_client, "<ssid>", 0, 1);
  cmd_mode->RegisterCommand("off","Turn off wifi networking",wifi_mode_off, "", 0, 0);
  }

esp32wifi::esp32wifi(std::string name)
  : pcp(name)
  {
  m_mode = ESP32WIFI_MODE_OFF;
  MyConfig.RegisterParam("wifi.ssid", "WIFI SSID", true, false);

  tcpip_adapter_init();

  ESP_ERROR_CHECK(esp_event_loop_init(HandleEvent, (void*)this));
  }

esp32wifi::~esp32wifi()
  {
  }

void esp32wifi::SetPowerMode(PowerMode powermode)
  {
  switch (powermode)
    {
    case On:
      esp_wifi_start();
      break;
    case Sleep:
      esp_wifi_set_ps(WIFI_PS_MODEM);
      break;
    case DeepSleep:
    case Off:
      StopStation();
      esp_wifi_stop();
      break;
    default:
      break;
    };

  m_powermode = powermode;
  }

void esp32wifi::StartClientMode(std::string ssid, std::string password)
  {
  m_mode = ESP32WIFI_MODE_CLIENT;

  if (m_powermode != On)
    {
    SetPowerMode(On);
    }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  wifi_config_t sta_config;
  strcpy((char*)sta_config.sta.ssid, ssid.c_str());
  strcpy((char*)sta_config.sta.password, password.c_str());
  sta_config.sta.bssid_set = 0;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
  }

void esp32wifi::StopStation()
  {
  if (m_mode != ESP32WIFI_MODE_OFF)
    {
    DeleteChildren();
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    m_mode = ESP32WIFI_MODE_OFF;
    }
  }

esp_err_t esp32wifi::HandleEvent(void *ctx, system_event_t *event)
  {
  if (event->event_id == SYSTEM_EVENT_STA_GOT_IP)
    {
    esp32wifi* me =(esp32wifi*)ctx;
    me->m_ip_info = event->event_info.got_ip.ip_info;
    me->AddChild(new TelnetServer(me));
    }
  return ESP_OK;
  }
