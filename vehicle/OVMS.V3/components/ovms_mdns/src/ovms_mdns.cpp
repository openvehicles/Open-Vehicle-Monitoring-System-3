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
static const char *TAG = "ovms-mdns";

#include "ovms_netmanager.h"
#include "ovms_peripherals.h"
#include "ovms_config.h"
#include "ovms_mdns.h"

OvmsMDNS MyMDNS __attribute__ ((init_priority (8100)));

void OvmsMDNS::NetworkUp(std::string event, void* data)
  {
  if (!(MyNetManager.m_wifi_ap || MyNetManager.m_connected_wifi)) return; // Exit if no wifi

  ESP_LOGI(TAG, "Starting MDNS service");
  StartMDNS();
  }

void OvmsMDNS::NetworkInterfaceChange(std::string event, void* data)
  {
  if (m_mdns)
    {
    ESP_LOGI(TAG, "Restarting MDNS service");
    StopMDNS();
    StartMDNS();
    }
  }

void OvmsMDNS::NetworkDown(std::string event, void* data)
  {
  if (m_mdns)
    {
    ESP_LOGI(TAG, "Stopping MDNS service");
    StopMDNS();
    }
  }

void OvmsMDNS::StartMDNS()
  {
  esp_err_t err;
  esp32wifi_mode_t wifimode = MyPeripherals->m_esp32wifi->GetMode();
  switch (wifimode)
    {
    case ESP32WIFI_MODE_CLIENT:
    case ESP32WIFI_MODE_SCLIENT:
    case ESP32WIFI_MODE_AP:
    case ESP32WIFI_MODE_APCLIENT:
      err = mdns_init();
      break;
    default:
      return;
    }

  if (err)
    {
    ESP_LOGE(TAG, "MDNS Init failed: %d", err);
    return;
    }

  m_mdns = true;

  // Set hostname
  std::string vehicleid = MyConfig.GetParamValue("vehicle", "id");
  if (vehicleid.empty()) vehicleid = "ovms";
  mdns_hostname_set(vehicleid.c_str());

  // Set default instance
  std::string instance("Open Vehicle Monitoring System - ");
  instance.append(vehicleid);
  mdns_instance_name_set(instance.c_str());

  // Register services
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  mdns_service_add(NULL, "_telnet", "_tcp", 23, NULL, 0);
  }

void OvmsMDNS::StopMDNS()
  {
  if (m_mdns)
    {
    mdns_free();
    m_mdns = false;
    }
  }

OvmsMDNS::OvmsMDNS()
  {
  ESP_LOGI(TAG, "Initialising MDNS (8100)");

  m_mdns = false;

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"network.mgr.init", std::bind(&OvmsMDNS::NetworkUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.interface.change", std::bind(&OvmsMDNS::NetworkInterfaceChange, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.mgr.stop", std::bind(&OvmsMDNS::NetworkDown, this, _1, _2));
  }

OvmsMDNS::~OvmsMDNS()
  {
  }
