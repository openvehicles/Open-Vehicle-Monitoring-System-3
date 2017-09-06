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

#include <string.h>
#include "esp32wifi.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "ovms_config.h"
#include "console_telnet.h"

// YOUR network SSID
#define SSID "yourssid"

// YOUR network password
#define PASSWORD "yourpassword"

esp32wifi::esp32wifi(std::string name)
  : pcp(name)
  {
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
      //esp_wifi_start();
      InitStation();
      break;
    case Sleep:
      //esp_wifi_set_ps(WIFI_PS_MODEM);
      break;
    case DeepSleep:
    case Off:
      //esp_wifi_stop();
      StopStation();
      break;
    default:
      break;
    };
  }

void esp32wifi::InitStation()
  {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  wifi_config_t sta_config;
  strcpy((char*)sta_config.sta.ssid, SSID);
  strcpy((char*)sta_config.sta.password, PASSWORD);
  sta_config.sta.bssid_set = 0;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
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

void esp32wifi::StopStation()
  {
  DeleteChildren();
  ESP_ERROR_CHECK(esp_wifi_disconnect());
  ESP_ERROR_CHECK(esp_wifi_stop());
  ESP_ERROR_CHECK(esp_wifi_deinit());
  }
