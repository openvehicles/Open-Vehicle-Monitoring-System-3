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
#include <esp_task_wdt.h>
#include "ovms_module.h"
#include "ovms_events.h"
#include "ovms_command.h"
#include "ovms_script.h"
#include "ovms_boot.h"
#if ESP_IDF_VERSION_MAJOR >= 4
#include <esp_netif_types.h>
#include <esp_eth_com.h>
#endif
#if ESP_IDF_VERSION_MAJOR >= 5
#include <esp_wifi_types.h>
#endif

#ifdef CONFIG_OVMS_COMP_OTA
#include "ovms_ota.h"
#endif

#if CONFIG_OVMS_HW_EVENT_QUEUE_SIZE < 80
#error "CONFIG ERROR: OVMS_HW_EVENT_QUEUE_SIZE needs to be >= 80"
#endif

OvmsEvents MyEvents __attribute__ ((init_priority (1200)));

typedef void (*event_signal_done_fn)(const char* event, void* data);
static void CheckQueueOverflow(const char* from, char* event);

bool EventMap::GetCompletion(OvmsWriter* writer, const char* token) const
  {
  unsigned int index = 0;
  bool match = false;
  writer->SetCompletion(index, NULL);
  if (token)
    {
    size_t len = strlen(token);
    for (const_iterator it = begin(); it != end(); ++it)
      {
      if (it->first.compare("*") == 0)
        continue;
      if (it->first.compare(0, len, token) == 0)
        {
        writer->SetCompletion(index++, it->first.c_str());
        match = true;
        }
      }
    }
  return match;
  }

void EventStdFree(const char* event, void* data)
  {
  free(data);
  }

void EventLaunchTask(void *pvParameters)
  {
  OvmsEvents* me = (OvmsEvents*)pvParameters;

  me->EventTask();
  }

void event_trace(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (strcmp(cmd->GetName(),"on")==0)
    MyEvents.m_trace = true;
  else
    MyEvents.m_trace = false;

  writer->printf("Event tracing is now %s\n",cmd->GetName());
  }

void event_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Event map has %d listeners, and queue has %d/%d entries\n",
    MyEvents.Map().size(),
    uxQueueMessagesWaiting(MyEvents.m_taskqueue),
    CONFIG_OVMS_HW_EVENT_QUEUE_SIZE);

  EventCallbackEntry* cbe = MyEvents.m_current_callback;
  if (cbe != NULL)
    {
    writer->printf("Currently dispatching:\n");
    writer->printf("  Event: %s\n",MyEvents.m_current_event.c_str());
    writer->printf("  To:    %s\n",cbe->m_caller.c_str());
    writer->printf("  For:   %" PRIu32 " second(s)\n",monotonictime-MyEvents.m_current_started);
    }
  }

void event_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string event;
  for (EventMap::const_iterator itm=MyEvents.Map().begin(); itm != MyEvents.Map().end(); ++itm)
    {
    if (argc > 0 && itm->first.find(argv[0]) == std::string::npos)
      continue;
    event.append(itm->first);
    event.append(":  ");
    EventCallbackList* el = itm->second;
    for (EventCallbackList::iterator itc=el->begin(); itc!=el->end(); )
      {
      EventCallbackEntry* ec = *itc;
      event.append(ec->m_caller);
      if (++itc != el->end())
        event.append(", ");
      }
    event.append("\n");
    }
  writer->printf("%s", event.c_str());
  }

int event_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)
  {
  int argpos = 0;
  for (int i=0; i < argc; i++)
    argpos += (argv[i][0] != '-') ? 1 : 0;
  if (argpos == 1 && MyEvents.Map().GetCompletion(writer, argv[argc-1]))
    return argc;
  return -1;
  }

void event_raise(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string event;
  uint32_t delay_ms = 0;

  for (int i=0; i<argc; i++)
    {
    if (argv[i][0] == '-')
      {
      if (argv[i][1] != 'd')
        {
        cmd->PutUsage(writer);
        return;
        }
      delay_ms = atol(argv[i]+2);
      }
    else
      event = argv[i];
    }

  if (delay_ms)
    {
    writer->printf("Raising event in %" PRIu32 " ms: %s\n", delay_ms, event.c_str());
    MyEvents.SignalEvent(event, NULL, (size_t)0, delay_ms);
    }
  else
    {
    writer->printf("Raising event: %s\n", event.c_str());
    MyEvents.SignalEvent(event, NULL);
    }
  }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

static duk_ret_t DukOvmsRaiseEvent(duk_context *ctx)
  {
  const char *event = duk_to_string(ctx,0);
  uint32_t delay_ms = duk_is_number(ctx,1) ? duk_to_uint32(ctx,1) : 0;

  if (event != NULL)
    {
    MyEvents.SignalEvent(event, NULL, (size_t)0, delay_ms);
    }
  return 0;  /* no return value */
  }

#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

OvmsEvents::OvmsEvents()
  {
  ESP_LOGI(TAG, "Initialising EVENTS (1200)");

  m_current_callback = NULL;

#ifdef CONFIG_OVMS_DEV_DEBUGEVENTS
  m_trace = true;
#else
  m_trace = false;
#endif // #ifdef CONFIG_OVMS_DEV_DEBUGEVENTS

#if ESP_IDF_VERSION_MAJOR >= 4
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_instance_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, ReceiveSystemEvent, (void*)this, &event_handler_instance));
#else
  ESP_ERROR_CHECK(esp_event_loop_init(ReceiveSystemEvent, (void*)this));
#endif

  // Register our commands
  OvmsCommand* cmd_event = MyCommandApp.RegisterCommand("event","EVENT framework", event_status, "", 0, 0, false);
  cmd_event->RegisterCommand("status","Show status of event system",event_status);
  cmd_event->RegisterCommand("list","List registered events",event_list,"[<key>]", 0, 1);
  cmd_event->RegisterCommand("raise","Raise a textual event",event_raise,"[-d<delay_ms>] <event>", 1, 2, true, event_validate);
  OvmsCommand* cmd_eventtrace = cmd_event->RegisterCommand("trace","EVENT trace framework");
  cmd_eventtrace->RegisterCommand("on","Turn event tracing ON",event_trace);
  cmd_eventtrace->RegisterCommand("off","Turn event tracing OFF",event_trace);

  m_taskqueue = xQueueCreate(CONFIG_OVMS_HW_EVENT_QUEUE_SIZE,sizeof(event_queue_t));
  xTaskCreatePinnedToCore(EventLaunchTask, "OVMS Events", 8192, (void*)this, 8, &m_taskid, CORE(1));
  AddTaskToMap(m_taskid);

  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  DuktapeObjectRegistration* dto = new DuktapeObjectRegistration("OvmsEvents");
  dto->RegisterDuktapeFunction(DukOvmsRaiseEvent, 2, "Raise");
  MyDuktape.RegisterDuktapeObject(dto);
  #endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

OvmsEvents::~OvmsEvents()
  {
#if ESP_IDF_VERSION_MAJOR >= 4
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, event_handler_instance));
  ESP_ERROR_CHECK(esp_event_loop_delete_default());
#endif
  }

void OvmsEvents::EventTask()
  {
  event_queue_t msg;

  esp_task_wdt_add(NULL); // WATCHDOG is active for this task
  while(1)
    {
    if (xQueueReceive(m_taskqueue, &msg, pdMS_TO_TICKS(5000)) == pdTRUE)
      {
      esp_task_wdt_reset(); // Reset WATCHDOG timer for this task
      switch(msg.type)
        {
        case EVENT_none:
          break;
        case EVENT_addhandler:
          HandleQueueAddHandler(&msg);
          break;
        case EVENT_removehandlers:
          HandleQueueRemoveHandlers(&msg);
          break;
        case EVENT_signal:
          m_current_event = msg.body.signal.event;
          HandleQueueSignalEvent(&msg);
          esp_task_wdt_reset(); // Reset WATCHDOG timer for this task
          m_current_event.clear();
          break;
        default:
          break;
        }
      }
    else
      {
#ifdef CONFIG_OVMS_COMP_OTA
      // Timeout on xQueueReceive: ignore during OTA flash job:
      OvmsMutexLock m_lock(&MyOTA.m_flashing, 0);
      if (!m_lock.IsLocked())
        continue;
#endif
      // …no OTA flashing in progress => abort:
      ESP_LOGE(TAG, "EventTask: [QueueTimeout] timer service / ticker timer has died => aborting");
      m_current_event = "[QueueTimeout]";
      m_current_started = monotonictime - 5;
      MyCommandApp.CloseLogfile();
      vTaskDelay(pdMS_TO_TICKS(100));
      abort();
      }
    }
  }

void OvmsEvents::HandleQueueSignalEvent(event_queue_t* msg)
  {
  // Log everything but the ticker & clock signals
  if (!startsWith(m_current_event, "ticker.") && !startsWith(m_current_event, "clock."))
    {
    if (m_trace)
      ESP_LOGI(TAG, "Signal(%s)",m_current_event.c_str());
    else
      ESP_LOGD(TAG, "Signal(%s)",m_current_event.c_str());
    }

  // Run callbacks:
    {
    OvmsRecMutexLock lock(&m_map_mutex);

    auto k = m_map.find(m_current_event);
    if (k != m_map.end())
      {
      EventCallbackList* el = k->second;
      if (el)
        {
        for (EventCallbackList::iterator itc=el->begin(); itc!=el->end(); ++itc)
          {
          m_current_started = monotonictime;
          m_current_callback = *itc;
          if (m_current_callback->m_callback)
            {
            m_current_callback->m_callback(m_current_event, msg->body.signal.data);
            }
          m_current_callback = NULL;
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
          m_current_started = monotonictime;
          m_current_callback = *itc;
          if (m_current_callback->m_callback)
            {
            m_current_callback->m_callback(m_current_event, msg->body.signal.data);
            }
          m_current_callback = NULL;
          }
        }
      }
    }

  // Run scripts:
  m_current_started = monotonictime;
  MyScripts.EventScript(m_current_event, msg->body.signal.data);

  FreeQueueSignalEvent(msg);
  }

void OvmsEvents::FreeQueueSignalEvent(event_queue_t* msg)
  {
  if (msg->body.signal.donefn != NULL)
    {
    msg->body.signal.donefn(msg->body.signal.event, msg->body.signal.data);
    }
  free(msg->body.signal.event);
  }


void OvmsEvents::RegisterEvent(std::string caller, std::string event, EventCallback callback)
  {
  // To guarantee a newly registered handler will never be called for the current event
  // if registered from within an event callback, adding the handler is always delegated
  // to the EventTask:

  event_queue_t msg = {};
  msg.type = EVENT_addhandler;
  msg.body.addhandler.event = strdup(event.c_str());
  msg.body.addhandler.handler = new EventCallbackEntry(caller, callback);

  if (xQueueSend(m_taskqueue, &msg, 0) != pdTRUE)
    {
    CheckQueueOverflow("RegisterEvent", msg.body.addhandler.event);
    free(msg.body.addhandler.event);
    delete msg.body.addhandler.handler;
    }
  }

void OvmsEvents::HandleQueueAddHandler(event_queue_t* msg)
  {
  // EventTask command EVENT_addhandler
  std::string event = msg->body.addhandler.event;

  OvmsRecMutexLock lock(&m_map_mutex);

  auto k = m_map.find(event);
  if (k == m_map.end())
    {
    m_map[event] = new EventCallbackList();
    k = m_map.find(event);
    }
  if (k == m_map.end())
    {
    ESP_LOGE(TAG, "Problem registering event %s for caller %s", event.c_str(),
      msg->body.addhandler.handler->m_caller.c_str());
    free(msg->body.addhandler.event);
    delete msg->body.addhandler.handler;
    }
  else
    {
    EventCallbackList *el = k->second;
    el->push_back(msg->body.addhandler.handler);
    free(msg->body.addhandler.event);
    }
  }


void OvmsEvents::DeregisterEvent(std::string caller)
  {
  // We need to invalidate all registered handlers in a first pass, to allow this
  // being called from within a callback (ie within HandleQueueSignalEvent).
  // Actual deletion of the handlers is then delegated to the EventTask.

  // Invalidate callbacks:
    {
    OvmsRecMutexLock lock(&m_map_mutex);
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
          ec->m_callback = nullptr;
          }
        ++itc;
        }
      ++itm;
      }
    }

  // Schedule deletion:
  event_queue_t msg = {};
  msg.type = EVENT_removehandlers;
  msg.body.removehandlers.caller = strdup(caller.c_str());

  if (xQueueSend(m_taskqueue, &msg, 0) != pdTRUE)
    {
    CheckQueueOverflow("DeregisterEvent", msg.body.removehandlers.caller);
    free(msg.body.removehandlers.caller);
    }
  }

void OvmsEvents::HandleQueueRemoveHandlers(event_queue_t* msg)
  {
  // EventTask command EVENT_removehandlers
  std::string caller = msg->body.removehandlers.caller;

  OvmsRecMutexLock lock(&m_map_mutex);

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
  
  free(msg->body.removehandlers.caller);
  }

static void CheckQueueOverflow(const char* from, char* event)
  {
  EventCallbackEntry* cbe = MyEvents.m_current_callback;
  if (cbe != NULL)
    {
    ESP_LOGE(TAG, "%s: queue overflow (running %s->%s for %" PRIu32 " sec), event '%s' dropped",
      from,
      MyEvents.m_current_event.c_str(),
      cbe->m_caller.c_str(),
      monotonictime-MyEvents.m_current_started,
      event);
    }
  else
    {
    ESP_LOGE(TAG, "%s: queue overflow, event '%s' dropped", from, event);
    }
  if (strncmp(event, "ticker.", 7) != 0)
    {
    // We've dropped a potentially important event, system is instable now.
    // As the event queue is full, a normal reboot is no option, so…
#ifdef CONFIG_OVMS_COMP_OTA
    // …wait for a running OTA job to finish…
    OvmsMutexLock m_lock(&MyOTA.m_flashing);
#endif
    // …then abort:
    ESP_LOGE(TAG, "%s: lost important event => aborting", from);
    MyCommandApp.CloseLogfile();
    vTaskDelay(pdMS_TO_TICKS(100));
    abort();
    }
  }

void OvmsEvents::SignalScheduledEvent(TimerHandle_t timer)
  {
  OvmsMutexLock lock(&MyEvents.m_timers_mutex);

  // retrieve timer payload message (event):
  event_queue_t* msg = (event_queue_t*) pvTimerGetTimerID(timer);
  vTimerSetTimerID(timer, NULL);
  if (!msg)
    {
    // This should no longer happen, but keeping the test doesn't hurt.
    // See: https://github.com/espressif/esp-idf/issues/8234
    ESP_LOGW(TAG, "SignalScheduledEvent: duplicate callback invocation detected");
    return;
    }

  // … and pass on to event task:
  if (xQueueSend(MyEvents.m_taskqueue, msg, 0) != pdTRUE)
    {
    CheckQueueOverflow("SignalScheduledEvent", msg->body.signal.event);
    MyEvents.FreeQueueSignalEvent(msg);
    }

  delete msg;
  MyEvents.m_timer_active[timer] = false;
  }

bool OvmsEvents::ScheduleEvent(event_queue_t* msg, uint32_t delay_ms)
  {
  OvmsMutexLock lock(&m_timers_mutex);
  TimerHandle_t timer;
  TimerList::iterator it;
  event_queue_t *msgdup = new event_queue_t(*msg);
  int timerticks = pdMS_TO_TICKS(delay_ms); if (timerticks<1) timerticks=1;

  if (!msgdup)
    {
    ESP_LOGE(TAG, "ScheduleEvent: message duplication failed, event dropped");
    return false;
    }

  // find available timer:
  for (it = m_timers.begin(); it != m_timers.end(); it++)
    {
    timer = *it;
    // Note: xTimerIsTimerActive() must not be used here, it has a
    //  multicore race condition with FreeRTOS V8.2.0 (esp-idf 3.3):
    //  an expired timer is removed from the active list before its
    //  callback is executed, so while the callback is running, the
    //  timer already appears to be free. Workaround is to use our
    //  own timer status map:
    if (!m_timer_active[timer])
      break;
    }
  if (it == m_timers.end())
    {
    // create & register a new timer:
    ESP_LOGI(TAG, "ScheduleEvent: creating new timer");
    timer = xTimerCreate("ScheduleEvent", timerticks, pdFALSE, NULL, SignalScheduledEvent);
    if (!timer)
      {
      ESP_LOGE(TAG, "ScheduleEvent: xTimerCreate failed, event dropped");
      delete msgdup;
      return false;
      }
    m_timers.push_back(timer);
    m_timer_active[timer] = false;
    }

  // set payload, update & start the timer:
  vTimerSetTimerID(timer, msgdup);
  if (xTimerChangePeriod(timer, timerticks, 0) != pdPASS) // also starts the timer
    {
    ESP_LOGE(TAG, "ScheduleEvent: xTimerChangePeriod failed, event dropped");
    vTimerSetTimerID(timer, NULL);
    delete msgdup;
    return false;
    }

  m_timer_active[timer] = true;
  return true;
  }

void OvmsEvents::SignalEvent(std::string event, void* data, event_signal_done_fn callback /*=NULL*/,
                             uint32_t delay_ms /*=0*/)
  {
  event_queue_t msg;
  memset(&msg, 0, sizeof(msg));

  msg.type = EVENT_signal;
  msg.body.signal.event = (char*)ExternalRamMalloc(event.size()+1);
  strcpy(msg.body.signal.event, event.c_str());
  msg.body.signal.data = data;
  msg.body.signal.donefn = callback;

  if (delay_ms == 0)
    {
    if (xQueueSend(m_taskqueue, &msg, 0) != pdTRUE)
      {
      CheckQueueOverflow("SignalEvent", msg.body.signal.event);
      FreeQueueSignalEvent(&msg);
      }
    }
  else
    {
    if (ScheduleEvent(&msg, delay_ms) != true)
      {
      ESP_LOGE(TAG, "SignalEvent: no timer available, event '%s' dropped", msg.body.signal.event);
      FreeQueueSignalEvent(&msg);
      }
    }
  }

void OvmsEvents::SignalEvent(std::string event, void* data, size_t length,
                             uint32_t delay_ms /*=0*/)
  {
  event_queue_t msg;
  memset(&msg, 0, sizeof(msg));

  msg.type = EVENT_signal;
  msg.body.signal.event = (char*)ExternalRamMalloc(event.size()+1);
  strcpy(msg.body.signal.event, event.c_str());
  if (data != NULL)
    {
    msg.body.signal.data = ExternalRamMalloc(length);
    memcpy(msg.body.signal.data, data, length);
    msg.body.signal.donefn = EventStdFree;
    }
  else
    {
    msg.body.signal.data = NULL;
    msg.body.signal.donefn = NULL;
    }

  if (delay_ms == 0)
    {
    if (xQueueSend(m_taskqueue, &msg, 0) != pdTRUE)
      {
      CheckQueueOverflow("SignalEvent", msg.body.signal.event);
      FreeQueueSignalEvent(&msg);
      }
    }
  else
    {
    if (ScheduleEvent(&msg, delay_ms) != true)
      {
      ESP_LOGE(TAG, "SignalEvent: no timer available, event '%s' dropped", msg.body.signal.event);
      FreeQueueSignalEvent(&msg);
      }
    }
  }

#if ESP_IDF_VERSION_MAJOR >= 4
/* Handler for all events */
void OvmsEvents::ReceiveSystemEvent(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
  {
  OvmsEvents* e = (OvmsEvents*)handler_args;
  // e->SignalEvent("system.event",(void*)event_data, sizeof(system_event_t)); // used for legacy mdns approach only ?

  if (base == WIFI_EVENT)
    {
    switch (id)
      {
      case WIFI_EVENT_WIFI_READY:      // ESP32 WiFi ready
        e->SignalEvent("system.wifi.ready", NULL);
        break;
      case WIFI_EVENT_SCAN_DONE:       // ESP32 finish scanning AP
        e->SignalEvent("system.wifi.scan.done", event_data, sizeof(wifi_event_sta_scan_done_t));
        break;
      case WIFI_EVENT_STA_START:       // ESP32 station start
        e->SignalEvent("system.wifi.sta.start", NULL);
        break;
      case WIFI_EVENT_STA_STOP:        // ESP32 station stop
        e->SignalEvent("system.wifi.sta.stop", NULL);
        break;
      case WIFI_EVENT_STA_CONNECTED:   // ESP32 station connected to AP
        e->SignalEvent("system.wifi.sta.connected", event_data, sizeof(wifi_event_sta_connected_t));
        break;
      case WIFI_EVENT_STA_DISCONNECTED:  // ESP32 station disconnected from AP
        e->SignalEvent("system.wifi.sta.disconnected", event_data, sizeof(wifi_event_sta_disconnected_t));
        break;
      case WIFI_EVENT_STA_AUTHMODE_CHANGE:  // the auth mode of AP connected by ESP32 station changed
        e->SignalEvent("system.wifi.sta.authmodechange", event_data, sizeof(wifi_event_sta_authmode_change_t));
        break;
      case WIFI_EVENT_STA_WPS_ER_SUCCESS:  // ESP32 station wps succeeds in enrollee mode
        e->SignalEvent("system.wifi.sta.wpser.success", NULL);
        break;
      case WIFI_EVENT_STA_WPS_ER_FAILED:   // ESP32 station wps fails in enrollee mode
        e->SignalEvent("system.wifi.sta.wpser.failed", event_data, sizeof(wifi_event_sta_wps_fail_reason_t));
        break;
      case WIFI_EVENT_STA_WPS_ER_TIMEOUT:  // ESP32 station wps timeout in enrollee mode
        e->SignalEvent("system.wifi.sta.wpser.timeout", NULL);
        break;
      case WIFI_EVENT_STA_WPS_ER_PIN:      // ESP32 station wps pin code in enrollee mode
        e->SignalEvent("system.wifi.sta.wpser.pin", event_data, sizeof(wifi_event_sta_wps_er_pin_t));
        break;
      case WIFI_EVENT_AP_START:            // ESP32 soft-AP start
        e->SignalEvent("system.wifi.ap.start", NULL);
        break;
      case WIFI_EVENT_AP_STOP:             // ESP32 soft-AP stop
        e->SignalEvent("system.wifi.ap.stop", NULL);
        break;
      case WIFI_EVENT_AP_STACONNECTED:     // a station connected to ESP32 soft-AP
        e->SignalEvent("system.wifi.ap.sta.connected", event_data, sizeof(wifi_event_ap_staconnected_t));
        break;
      case WIFI_EVENT_AP_STADISCONNECTED:  // a station disconnected from ESP32 soft-AP
        e->SignalEvent("system.wifi.ap.sta.disconnected", event_data, sizeof(wifi_event_ap_stadisconnected_t));
        break;
      case WIFI_EVENT_AP_PROBEREQRECVED:   // Receive probe request packet in soft-AP interface
        e->SignalEvent("system.wifi.ap.proberx", event_data, sizeof(wifi_event_ap_probe_req_rx_t));
        break;
      default:
       break;
      }
    }
  else if (base == IP_EVENT)
    {
    switch (id)
      {
      case IP_EVENT_STA_GOT_IP:           // ESP32 station got IP from connected AP
        e->SignalEvent("network.interface.up", NULL);
        e->SignalEvent("system.wifi.sta.gotip", event_data, sizeof(ip_event_got_ip_t));
        break;
      case IP_EVENT_STA_LOST_IP:         // ESP32 station lost IP and the IP is reset to 0
        e->SignalEvent("system.wifi.sta.lostip", NULL);
        break;
      case IP_EVENT_AP_STAIPASSIGNED:    // ESP32 soft-AP assigned an IP to a connected station
        e->SignalEvent("system.wifi.ap.sta.ipassigned", NULL);
        break;
      case IP_EVENT_GOT_IP6:      // ESP32 station or ap interface v6IP addr is preferred
        e->SignalEvent("system.wifi.ap.sta.gotip6", event_data, sizeof(ip_event_got_ip6_t));
        break;
      case IP_EVENT_ETH_GOT_IP:          // ESP32 ethernet got IP from connected AP
        e->SignalEvent("system.eth.gotip",  event_data, sizeof(ip_event_got_ip_t));
        break;
      default:
       break;
      }
    }
  else if (base == ETH_EVENT)
    {
    switch (id)
      {
      case ETHERNET_EVENT_START:           // ESP32 ethernet start
        e->SignalEvent("system.eth.start", NULL);
        break;
      case ETHERNET_EVENT_STOP:            // ESP32 ethernet stop
        e->SignalEvent("system.eth.stop", NULL);
        break;
      case ETHERNET_EVENT_CONNECTED:       // ESP32 ethernet phy link up
        e->SignalEvent("system.eth.connected", NULL);
        break;
      case ETHERNET_EVENT_DISCONNECTED:    // ESP32 ethernet phy link down
        e->SignalEvent("system.eth.disconnected", NULL);
        break;
      default:
       break;
      }
    }
  }

#else

esp_err_t OvmsEvents::ReceiveSystemEvent(void *ctx, system_event_t *event)
  {
  OvmsEvents* e = (OvmsEvents*)ctx;
  e->SignalEvent("system.event",(void*)event,sizeof(system_event_t));
  e->SignalSystemEvent(event);
  return ESP_OK;
  }

void OvmsEvents::SignalSystemEvent(system_event_t *event)
  {
  switch (event->event_id)
    {
    case SYSTEM_EVENT_WIFI_READY:      // ESP32 WiFi ready
      SignalEvent("system.wifi.ready",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_SCAN_DONE:       // ESP32 finish scanning AP
      SignalEvent("system.wifi.scan.done",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_START:       // ESP32 station start
      SignalEvent("system.wifi.sta.start",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_STOP:        // ESP32 station stop
      SignalEvent("system.wifi.sta.stop",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_CONNECTED:   // ESP32 station connected to AP
      SignalEvent("system.wifi.sta.connected",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:  // ESP32 station disconnected from AP
      SignalEvent("system.wifi.sta.disconnected",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:  // the auth mode of AP connected by ESP32 station changed
      SignalEvent("system.wifi.sta.authmodechange",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_GOT_IP:           // ESP32 station got IP from connected AP
      SignalEvent("network.interface.up", NULL);
      SignalEvent("system.wifi.sta.gotip",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_LOST_IP:         // ESP32 station lost IP and the IP is reset to 0
      SignalEvent("system.wifi.sta.lostip",(void*)&event->event_info);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:  // ESP32 station wps succeeds in enrollee mode
      SignalEvent("system.wifi.sta.wpser.success",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:   // ESP32 station wps fails in enrollee mode
      SignalEvent("system.wifi.sta.wpser.failed",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:  // ESP32 station wps timeout in enrollee mode
      SignalEvent("system.wifi.sta.wpser.timeout",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:      // ESP32 station wps pin code in enrollee mode
      SignalEvent("system.wifi.sta.wpser.pin",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_AP_START:            // ESP32 soft-AP start
      SignalEvent("system.wifi.ap.start",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_AP_STOP:             // ESP32 soft-AP stop
      SignalEvent("system.wifi.ap.stop",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:     // a station connected to ESP32 soft-AP
      SignalEvent("system.wifi.ap.sta.connected",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:  // a station disconnected from ESP32 soft-AP
      SignalEvent("system.wifi.ap.sta.disconnected",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:    // ESP32 soft-AP assigned an IP to a connected station
      SignalEvent("system.wifi.ap.sta.ipassigned",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED:   // Receive probe request packet in soft-AP interface
      SignalEvent("system.wifi.ap.proberx",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:      // ESP32 station or ap interface v6IP addr is preferred
      SignalEvent("system.wifi.ap.sta.gotip6",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_ETH_START:           // ESP32 ethernet start
      SignalEvent("system.eth.start",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_ETH_STOP:            // ESP32 ethernet stop
      SignalEvent("system.eth.stop",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:       // ESP32 ethernet phy link up
      SignalEvent("system.eth.connected",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:    // ESP32 ethernet phy link down
      SignalEvent("system.eth.disconnected",(void*)&event->event_info,sizeof(event->event_info));
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:          // ESP32 ethernet got IP from connected AP
      SignalEvent("system.eth.gotip",(void*)&event->event_info,sizeof(event->event_info));
      break;
    default:
     break;
    }
  }

#endif

EventCallbackEntry::EventCallbackEntry(std::string caller, EventCallback callback)
  {
  m_caller = caller;
  m_callback = callback;
  }

EventCallbackEntry::~EventCallbackEntry()
  {
  }
