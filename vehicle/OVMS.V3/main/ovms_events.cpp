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
static const char *TAG = "events";

#include <string.h>
#include <stdio.h>
#include "esp_event_loop.h"
#include "ovms_events.h"
#include "ovms_command.h"
#include "ovms_script.h"

OvmsEvents MyEvents __attribute__ ((init_priority (1200)));

void event_trace(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (strcmp(cmd->GetName(),"on")==0)
    MyEvents.m_trace = true;
  else
    MyEvents.m_trace = false;

  writer->printf("Event tracing is now %s\n",cmd->GetName());
  }

void event_raise(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string event(argv[0]);

  writer->printf("Raising event: %s\n",argv[0]);
  MyEvents.SignalEvent(event, NULL);
  }

OvmsEvents::OvmsEvents()
  {
  ESP_LOGI(TAG, "Initialising EVENTS (1200)");

#ifdef CONFIG_OVMS_DEV_DEBUGEVENTS
  m_trace = true;
#else
  m_trace = false;
#endif // #ifdef CONFIG_OVMS_DEV_DEBUGEVENTS

  ESP_ERROR_CHECK(esp_event_loop_init(ReceiveSystemEvent, (void*)this));

  // Register our commands
  OvmsCommand* cmd_event = MyCommandApp.RegisterCommand("event","EVENT framework",NULL, "", 1);
  cmd_event->RegisterCommand("raise","Raise a textual event",event_raise,"<event>", 1, 1, true);
  OvmsCommand* cmd_eventtrace = cmd_event->RegisterCommand("trace","EVENT trace framework", NULL, "", 0, 0, false);
  cmd_eventtrace->RegisterCommand("on","Turn event tracing ON",event_trace,"", 0, 0, false);
  cmd_eventtrace->RegisterCommand("off","Turn event tracing OFF",event_trace,"", 0, 0, false);
  }

OvmsEvents::~OvmsEvents()
  {
  }

void OvmsEvents::RegisterEvent(std::string caller, std::string event, EventCallback callback)
  {
  auto k = m_map.find(event);
  if (k == m_map.end())
    {
    m_map[event] = new EventCallbackList();
    k = m_map.find(event);
    }
  if (k == m_map.end())
    {
    ESP_LOGE(TAG, "Problem registering event %s for caller %s",event.c_str(),caller.c_str());
    return;
    }

  EventCallbackList *el = k->second;
  el->push_back(new EventCallbackEntry(caller,callback));
  }

void OvmsEvents::DeregisterEvent(std::string caller)
  {
  EventMap::iterator itm=m_map.begin();
  while (itm!=m_map.end())
    {
    EventCallbackList* el = itm->second;
    EventCallbackList::iterator itc=el->begin();
    while (itc!=el->end())
      {
      EventCallbackEntry* ec = *itc;
      if (ec->m_caller == caller)
        {
        itc = el->erase(itc);
        delete ec;
        }
      else
        {
        ++itc;
        }
      }
    if (el->empty())
      {
      itm = m_map.erase(itm);
      delete el;
      }
    else
      {
      ++itm;
      }
    }
  }

void OvmsEvents::SignalEvent(std::string event, void* data)
  {
  if (m_trace)
    {
    if (event.compare(0,7,"ticker.") != 0)
      {
      // Log everything but the excessively verbose ticker signals
      ESP_LOGI(TAG, "Signal(%s)",event.c_str());
      }
    }

  auto k = m_map.find(event);
  if (k != m_map.end())
    {
    EventCallbackList* el = k->second;
    if (el)
      {
      for (EventCallbackList::iterator itc=el->begin(); itc!=el->end(); ++itc)
        {
        EventCallbackEntry* ec = *itc;
        ec->m_callback(event, data);
        }
      }
    }

  k = m_map.find("*");
  if (k != m_map.end())
    {
    EventCallbackList* el = k->second;
    if (el)
      {
      for (EventCallbackList::iterator itc=el->begin(); itc!=el->end(); ++itc)
        {
        EventCallbackEntry* ec = *itc;
        ec->m_callback(event, data);
        }
      }
    }

  MyScripts.EventScript(event, data);
  }

esp_err_t OvmsEvents::ReceiveSystemEvent(void *ctx, system_event_t *event)
  {
  OvmsEvents* e = (OvmsEvents*)ctx;
  e->SignalSystemEvent(event);
  return ESP_OK;
  }

void OvmsEvents::SignalSystemEvent(system_event_t *event)
  {
  switch (event->event_id)
    {
    case SYSTEM_EVENT_WIFI_READY:      // ESP32 WiFi ready
      SignalEvent("system.wifi.ready",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_SCAN_DONE:       // ESP32 finish scanning AP
      SignalEvent("system.wifi.scan.done",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_START:       // ESP32 station start
      SignalEvent("system.wifi.sta.start",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_STOP:        // ESP32 station stop
      SignalEvent("system.wifi.sta.stop",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_CONNECTED:   // ESP32 station connected to AP
      SignalEvent("system.wifi.sta.connected",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:  // ESP32 station disconnected from AP
      SignalEvent("system.wifi.sta.disconnected",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:  // the auth mode of AP connected by ESP32 station changed
      SignalEvent("system.wifi.sta.authmodechange",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_GOT_IP:           // ESP32 station got IP from connected AP
      SignalEvent("network.interface.up", NULL);
      SignalEvent("system.wifi.sta.gotip",(void*)&event->event_info);
      break;
//    case SYSTEM_EVENT_STA_LOST_IP:         // ESP32 station lost IP and the IP is reset to 0
//      SignalEvent("system.wifi.sta.lostip",(void*)&event->event_info);
//      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:  // ESP32 station wps succeeds in enrollee mode
      SignalEvent("system.wifi.sta.wpser.success",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:   // ESP32 station wps fails in enrollee mode
      SignalEvent("system.wifi.sta.wpser.failed",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:  // ESP32 station wps timeout in enrollee mode
      SignalEvent("system.wifi.sta.wpser.timeout",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:      // ESP32 station wps pin code in enrollee mode
      SignalEvent("system.wifi.sta.wpser.pin",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_AP_START:            // ESP32 soft-AP start
      SignalEvent("system.wifi.ap.start",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_AP_STOP:             // ESP32 soft-AP stop
      SignalEvent("system.wifi.ap.stop",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:     // a station connected to ESP32 soft-AP
      SignalEvent("system.wifi.ap.sta.connected",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:  // a station disconnected from ESP32 soft-AP
      SignalEvent("system.wifi.ap.sta.disconnected",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED:   // Receive probe request packet in soft-AP interface
      SignalEvent("system.wifi.ap.proberx",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:      // ESP32 station or ap interface v6IP addr is preferred
      SignalEvent("system.wifi.ap.sta.gotip6",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_ETH_START:           // ESP32 ethernet start
      SignalEvent("system.eth.start",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_ETH_STOP:            // ESP32 ethernet stop
      SignalEvent("system.eth.stop",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:       // ESP32 ethernet phy link up
      SignalEvent("system.eth.connected",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:    // ESP32 ethernet phy link down
      SignalEvent("system.eth.disconnected",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:          // ESP32 ethernet got IP from connected AP
      SignalEvent("system.eth.gotip",(void*)&event->event_info);
      break;
    default:
     break;
    }
  }

EventCallbackEntry::EventCallbackEntry(std::string caller, EventCallback callback)
  {
  m_caller = caller;
  m_callback = callback;
  }

EventCallbackEntry::~EventCallbackEntry()
  {
  }
