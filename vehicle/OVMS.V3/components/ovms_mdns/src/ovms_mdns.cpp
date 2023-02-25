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

#include <sdkconfig.h>
#include "ovms_netmanager.h"
#include "ovms_peripherals.h"
#include "ovms_config.h"
#include "ovms_mdns.h"

OvmsMDNS MyMDNS __attribute__ ((init_priority (8100)));

#if ESP_IDF_VERSION_MAJOR < 4
void OvmsMDNS::SystemEvent(std::string event, void* data)
  {
  system_event_t *ev = (system_event_t *)data;
  if (m_mdns) mdns_handle_system_event(NULL, ev);
  }
#endif

void OvmsMDNS::SystemStart(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Starting MDNS");
  StartMDNS();
  }

void OvmsMDNS::EventSystemShuttingDown(std::string event, void* data)
  {
  StopMDNS();
  }

void OvmsMDNS::StartMDNS()
  {
  if (m_mdns) return; // Quick exit if already started

  esp_err_t err = mdns_init();
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
  std::string instance("OVMS - ");
  instance.append(vehicleid);
  mdns_instance_name_set(instance.c_str());

  // Register services
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
#ifdef CONFIG_OVMS_COMP_SSH
  mdns_service_add(NULL, "_ssh", "_tcp", 22, NULL, 0);
#endif // #ifdef CONFIG_OVMS_COMP_SSH
#ifdef CONFIG_OVMS_COMP_TELNET
  mdns_service_add(NULL, "_telnet", "_tcp", 23, NULL, 0);
#endif // #ifdef CONFIG_OVMS_COMP_TELNET
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
#if ESP_IDF_VERSION_MAJOR < 4
  MyEvents.RegisterEvent(TAG,"system.event", std::bind(&OvmsMDNS::SystemEvent, this, _1, _2));
#endif
  MyEvents.RegisterEvent(TAG,"system.start", std::bind(&OvmsMDNS::SystemStart, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.shuttingdown",std::bind(&OvmsMDNS::EventSystemShuttingDown, this, _1, _2));
  }

OvmsMDNS::~OvmsMDNS()
  {
  StopMDNS();
  }
