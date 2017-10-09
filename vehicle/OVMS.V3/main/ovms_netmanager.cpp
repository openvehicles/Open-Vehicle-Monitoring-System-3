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
static const char *TAG = "netmanager";

#include <string.h>
#include <stdio.h>
#include "ovms_events.h"
#include "metrics_standard.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"

OvmsNetManager MyNetManager __attribute__ ((init_priority (8999)));

OvmsNetManager::OvmsNetManager()
  {
  ESP_LOGI(TAG, "Initialising NETMANAGER (8999)");
  m_connected_wifi = false;
  m_connected_modem = false;
  m_connected_any = false;

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.gotip", std::bind(&OvmsNetManager::WifiUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.stop", std::bind(&OvmsNetManager::WifiDown, this, _1, _2));
  }

OvmsNetManager::~OvmsNetManager()
  {
  }

void OvmsNetManager::WifiUp(std::string event, void* data)
  {
  m_connected_wifi = true;
  m_connected_any = m_connected_wifi || m_connected_modem;
  StandardMetrics.ms_m_net_type->SetValue("wifi");
  StandardMetrics.ms_m_net_provider->SetValue(MyPeripherals->m_esp32wifi->GetSSID());
  MyEvents.SignalEvent("network.up",NULL);
  }

void OvmsNetManager::WifiDown(std::string event, void* data)
  {
  m_connected_wifi = false;
  m_connected_any = m_connected_wifi || m_connected_modem;
  if (!m_connected_any)
    {
    StandardMetrics.ms_m_net_type->SetValue("none");
    StandardMetrics.ms_m_net_provider->SetValue("");
    MyEvents.SignalEvent("network.down",NULL);
    }
  }
