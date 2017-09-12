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
static const char *TAG = "ovms-mdns";

#include "ovms_peripherals.h"
#include "ovms_config.h"
#include "ovms_mdns.h"

OvmsMDNS MyMDNS __attribute__ ((init_priority (8100)));

void OvmsMDNS::WifiUp(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Launching MDNS service");
  esp_err_t err;
  if (MyPeripherals->m_esp32wifi->GetMode() == ESP32WIFI_MODE_CLIENT)
    {
    err = mdns_init(TCPIP_ADAPTER_IF_STA, &m_mdns);
    }
  else if (MyPeripherals->m_esp32wifi->GetMode() == ESP32WIFI_MODE_AP)
    {
    err = mdns_init(TCPIP_ADAPTER_IF_AP, &m_mdns);
    }
  else
    {
    return; // wifi is not up in AP or STA mode
    }
  if (err)
    {
    ESP_LOGE(TAG, "MDNS Init failed: %d", err);
    return;
    }

  // Set hostname
  std::string vehicleid = MyConfig.GetParamValue("vehicle", "id");
  if (vehicleid.empty()) vehicleid = "ovms";
  mdns_set_hostname(m_mdns, vehicleid.c_str());

  // Set default instance
  std::string instance("Open Vehicle Monitoring System - ");
  instance.append(vehicleid);
  mdns_set_instance(m_mdns, instance.c_str());

  // Register services
  mdns_service_add(m_mdns, "_http", "_tcp", 80);
  mdns_service_add(m_mdns, "_telnet", "_tcp", 23);
  }

void OvmsMDNS::WifiDown(std::string event, void* data)
  {
  if (m_mdns)
    {
    ESP_LOGI(TAG, "Stopping MDNS service");
    mdns_free(m_mdns);
    m_mdns = NULL;
    }
  }

OvmsMDNS::OvmsMDNS()
  {
  ESP_LOGI(TAG, "Initialising MDNS (8100)");

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.gotip", std::bind(&OvmsMDNS::WifiUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.start", std::bind(&OvmsMDNS::WifiUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.stop", std::bind(&OvmsMDNS::WifiDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.stop", std::bind(&OvmsMDNS::WifiDown, this, _1, _2));
  }

OvmsMDNS::~OvmsMDNS()
  {
  }


