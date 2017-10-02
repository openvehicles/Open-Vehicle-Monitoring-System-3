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

#ifndef __ESP32WIFI_H__
#define __ESP32WIFI_H__

#include <string>
#include <stdint.h>
#include "pcp.h"
#include "task_base.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "ovms_events.h"

typedef enum {
    ESP32WIFI_MODE_OFF = 0,   // Modem is off
    ESP32WIFI_MODE_CLIENT,    // Client mode
    ESP32WIFI_MODE_AP,        // Access point mode
    ESP32WIFI_MODE_SCAN,      // SCAN mode
    ESP32WIFI_MODE_MAX
} esp32wifi_mode_t;

class esp32wifi : public pcp, public Parent
  {
  public:
    esp32wifi(std::string name);
    ~esp32wifi();

  public:
    void SetPowerMode(PowerMode powermode);

  public:
    void StartClientMode(std::string ssid, std::string password, uint8_t* bssid=NULL);
    void StartAccessPointMode(std::string ssid, std::string password);
    void StopStation();
    void Scan();
    esp32wifi_mode_t GetMode();

  public:
    void EventWifiGotIp(std::string event, void* data);
    void EventWifiScanDone(std::string event, void* data);

  protected:
    esp32wifi_mode_t m_mode;
    tcpip_adapter_ip_info_t m_ip_info;
    wifi_init_config_t m_wifi_init_cfg;
    wifi_config_t m_wifi_apsta_cfg;
  };

#endif //#ifndef __ESP32WIFI_H__
