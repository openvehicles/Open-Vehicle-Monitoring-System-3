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
#include "esp_err.h"
#include "esp_wifi.h"
#include "ovms.h"
#include "ovms_events.h"
#include "ovms_mutex.h"

typedef enum {
    ESP32WIFI_MODE_OFF = 0,   // Modem is off
    ESP32WIFI_MODE_CLIENT,    // Client mode
    ESP32WIFI_MODE_SCLIENT,   // Scanning-Client mode
    ESP32WIFI_MODE_AP,        // Access point mode
    ESP32WIFI_MODE_APCLIENT,  // Access point + Client mode
    ESP32WIFI_MODE_SCAN,      // SCAN mode
    ESP32WIFI_MODE_MAX
} esp32wifi_mode_t;

class esp32wifi : public pcp, public InternalRamAllocated
  {
  public:
    esp32wifi(const char* name);
    ~esp32wifi();

  public:
    void AutoInit();
    void SetPowerMode(PowerMode powermode);
    void PowerUp();
    void PowerDown();
    void InitCSI();
    static void CSIRxCallback(void* ctx, wifi_csi_info_t* data);

  public:
    void StartClientMode(std::string ssid, std::string password, uint8_t* bssid=NULL);
    void StartScanningClientMode();
    void StartAccessPointMode(std::string ssid, std::string password);
    void StartAccessPointClientMode(std::string apssid, std::string appassword, std::string stassid, std::string stapassword);
    void StopStation();
    void Scan(OvmsWriter* writer);
    esp32wifi_mode_t GetMode();
    std::string GetSSID();
    std::string GetAPSSID();
    void UpdateNetMetrics();
    void AdjustTaskPriority();

  public:
    void EventWifiStaState(std::string event, void* data);
    void EventWifiGotIp(std::string event, void* data);
    void EventWifiStaConnected(std::string event, void* data);
    void EventWifiStaDisconnected(std::string event, void* data);
    void EventWifiApState(std::string event, void* data);
    void EventWifiApUpdate(std::string event, void* data);
    void EventTimer1(std::string event, void* data);
    void EventTimer10(std::string event, void* data);
    void EventWifiScanDone(std::string event, void* data);
    void EventSystemShuttingDown(std::string event, void* data);
    void OutputStatus(int verbosity, OvmsWriter* writer);

  protected:
    bool m_poweredup;
    OvmsMutex m_mutex;
    esp32wifi_mode_t m_mode;
    uint8_t m_mac_sta[6];
    uint8_t m_mac_ap[6];
    tcpip_adapter_ip_info_t m_ip_info_sta;
    tcpip_adapter_ip_info_t m_ip_info_ap;
    wifi_init_config_t m_wifi_init_cfg;
    wifi_config_t m_wifi_ap_cfg;
    wifi_config_t m_wifi_sta_cfg;
    bool m_stareconnect;
    uint32_t m_nextscan;
    int m_sta_rssi;                               // smoothed RSSI [dBm/10]
  };

#endif //#ifndef __ESP32WIFI_H__
