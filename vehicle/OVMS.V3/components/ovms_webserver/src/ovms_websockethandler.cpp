/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2018       Michael Balzer
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
static const char *TAG = "websocket";

#include <string.h>
#include <stdio.h>
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "buffered_shell.h"
#include "vehicle.h"


/**
 * WebSocketHandler transmits JSON data in chunks to the WebSocket client
 *  and serializes transmits initiated from all contexts.
 * 
 * On creation it will do a full update of all metrics.
 * Later on it receives TX jobs through the queue.
 * 
 * Job processing & data transmission is protected by the mutex against
 * parallel execution. TX init is done either by the mongoose EventHandler
 * on connect/poll or by the UpdateTicker. The EventHandler triggers immediate
 * successive sends, the UpdateTicker sends collected intermediate updates.
 */

WebSocketHandler::WebSocketHandler(mg_connection* nc, size_t slot, size_t modifier, size_t reader)
  : MgHandler(nc)
{
  ESP_LOGV(TAG, "WebSocketHandler[%p] init: handler=%p modifier=%d", nc, this, modifier);
  
  m_slot = slot;
  m_modifier = modifier;
  m_reader = reader;
  m_jobqueue = xQueueCreate(50, sizeof(WebSocketTxJob));
  m_jobqueue_overflow = 0;
  m_mutex = xSemaphoreCreateMutex();
  m_job.type = WSTX_None;
  m_sent = m_ack = 0;
}

WebSocketHandler::~WebSocketHandler()
{
  while (xQueueReceive(m_jobqueue, &m_job, 0) == pdTRUE)
    ClearTxJob(m_job);
  vQueueDelete(m_jobqueue);
  vSemaphoreDelete(m_mutex);
}


bool WebSocketHandler::Lock(TickType_t xTicksToWait)
{
  if (xSemaphoreGetMutexHolder(m_mutex) == xTaskGetCurrentTaskHandle())
    return true;
  else if (xSemaphoreTake(m_mutex, xTicksToWait) == pdTRUE)
    return true;
  else
    return false;
}

void WebSocketHandler::Unlock()
{
  xSemaphoreGive(m_mutex);
}


void WebSocketHandler::ProcessTxJob()
{
  //ESP_LOGV(TAG, "WebSocketHandler[%p]: ProcessTxJob type=%d, sent=%d ack=%d", m_nc, m_job.type, m_sent, m_ack);
  
  // process job, send next chunk:
  switch (m_job.type)
  {
    case WSTX_Event:
    {
      if (m_sent && m_ack) {
        ESP_LOGV(TAG, "WebSocketHandler[%p]: ProcessTxJob type=%d done", m_nc, m_job.type);
        ClearTxJob(m_job);
      } else {
        std::string msg;
        msg.reserve(128);
        msg = "{\"event\":\"";
        msg += m_job.event;
        msg += "\"}";
        mg_send_websocket_frame(m_nc, WEBSOCKET_OP_TEXT, msg.data(), msg.size());
        m_sent = 1;
      }
      break;
    }
    
    case WSTX_MetricsAll:
    case WSTX_MetricsUpdate:
    {
      // Note: this loops over the metrics by index, keeping the checked count
      //  in m_sent. It will not detect new metrics added between polls if they are
      //  inserted before m_sent, so new metrics may not be sent until first changed.
      //  The Metrics set normally is static, so this should be no problem.
      
      // find start:
      int i;
      OvmsMetric* m;
      for (i=0, m=MyMetrics.m_first; i < m_sent && m != NULL; m=m->m_next, i++);
      
      // build msg:
      std::string msg;
      msg.reserve(XFER_CHUNK_SIZE+128);
      msg = "{\"metrics\":{";
      for (i=0; m && msg.size() < XFER_CHUNK_SIZE; m=m->m_next) {
        if (m->IsModifiedAndClear(m_modifier) || m_job.type == WSTX_MetricsAll) {
          if (i) msg += ',';
          msg += '\"';
          msg += m->m_name;
          msg += "\":";
          msg += m->AsJSON();
          i++;
        }
      }
      
      // send msg:
      if (i) {
        msg += "}}";
        mg_send_websocket_frame(m_nc, WEBSOCKET_OP_TEXT, msg.data(), msg.size());
        m_sent += i;
      }
      
      // done?
      if (!m && m_ack == m_sent) {
        if (m_sent)
          ESP_LOGV(TAG, "WebSocketHandler[%p]: ProcessTxJob type=%d done, sent=%d metrics", m_nc, m_job.type, m_sent);
        ClearTxJob(m_job);
      }
      
      break;
    }
    
    case WSTX_Notify:
    {
      if (m_sent && m_ack == m_job.notification->GetValueSize()+1) {
        // done:
        ESP_LOGV(TAG, "WebSocketHandler[%p]: ProcessTxJob type=%d done, sent %d bytes", m_nc, m_job.type, m_sent);
        ClearTxJob(m_job);
      } else {
        // build frame:
        std::string msg;
        msg.reserve(XFER_CHUNK_SIZE+128);
        int op;
        
        if (m_sent == 0) {
          op = WEBSOCKET_OP_TEXT;
          msg += "{\"notify\":{\"type\":\"";
          msg += m_job.notification->GetType()->m_name;
          msg += "\",\"subtype\":\"";
          msg += mqtt_topic(m_job.notification->GetSubType());
          msg += "\",\"value\":\"";
          m_sent = 1;
        } else {
          op = WEBSOCKET_OP_CONTINUE;
        }
        
        extram::string part = m_job.notification->GetValue().substr(m_sent-1, XFER_CHUNK_SIZE);
        msg += json_encode(part);
        m_sent += part.size();
        
        if (m_sent < m_job.notification->GetValueSize()+1) {
          op |= WEBSOCKET_DONT_FIN;
        } else {
          msg += "\"}}";
        }
        
        // send frame:
        mg_send_websocket_frame(m_nc, op, msg.data(), msg.size());
      }
    }
    
    case WSTX_Config:
    {
      // todo: implement
      ClearTxJob(m_job);
      m_sent = 0;
      break;
    }
    
    default:
      ClearTxJob(m_job);
      m_sent = 0;
      break;
  }
}


void WebSocketTxJob::clear(size_t client)
{
  auto& slot = MyWebServer.m_client_slots[client];
  switch (type) {
    case WSTX_Event:
      if (event)
        free(event);
      break;
    case WSTX_Notify:
      if (notification) {
        OvmsNotifyType* mt = notification->GetType();
        if (mt) mt->MarkRead(slot.reader, notification);
      }
    default:
      break;
  }
  type = WSTX_None;
}

void WebSocketHandler::ClearTxJob(WebSocketTxJob &job)
{
  job.clear(m_slot);
}


bool WebSocketHandler::AddTxJob(WebSocketTxJob job, bool init_tx)
{
  if (xQueueSend(m_jobqueue, &job, 0) != pdTRUE) {
    ++m_jobqueue_overflow;
    if (m_jobqueue_overflow == 1)
      ESP_LOGW(TAG, "WebSocketHandler[%p]: job queue overflow detected", m_nc);
    return false;
  }
  else {
    if (m_jobqueue_overflow) {
      ESP_LOGW(TAG, "WebSocketHandler[%p]: job queue overflow resolved, %d drops", m_nc, m_jobqueue_overflow);
      m_jobqueue_overflow = 0;
    }
    if (init_tx && uxQueueMessagesWaiting(m_jobqueue) == 1)
      RequestPoll();
    return true;
  }
}

bool WebSocketHandler::GetNextTxJob()
{
  if (xQueueReceive(m_jobqueue, &m_job, 0) == pdTRUE) {
    // init new job state:
    m_sent = m_ack = 0;
    return true;
  } else {
    return false;
  }
}


void WebSocketHandler::InitTx()
{
  if (m_job.type != WSTX_None)
    return;
  
  // begin next job if idle:
  while (m_job.type == WSTX_None) {
    if (!GetNextTxJob())
      break;
    ProcessTxJob();
  }
}

void WebSocketHandler::ContinueTx()
{
  m_ack = m_sent;
  
  do {
    // process current job:
    ProcessTxJob();
    // check next if done:
  } while (m_job.type == WSTX_None && GetNextTxJob());
}


int WebSocketHandler::HandleEvent(int ev, void* p)
{
  switch (ev)
  {
    case MG_EV_WEBSOCKET_FRAME:
    {
      // websocket message received
      websocket_message* wm = (websocket_message*) p;
      std::string msg;
      msg.assign((char*) wm->data, wm->size);
      HandleIncomingMsg(msg);
      break;
    }
    
    case MG_EV_POLL:
      // check for new transmission
      //ESP_LOGV(TAG, "WebSocketHandler[%p] EV_POLL qlen=%d jobtype=%d sent=%d ack=%d", m_nc, uxQueueMessagesWaiting(m_jobqueue), m_job.type, m_sent, m_ack);
      InitTx();
      break;
    
    case MG_EV_SEND:
      // last transmission has finished
      //ESP_LOGV(TAG, "WebSocketHandler[%p] EV_SEND qlen=%d jobtype=%d sent=%d ack=%d", m_nc, uxQueueMessagesWaiting(m_jobqueue), m_job.type, m_sent, m_ack);
      ContinueTx();
      break;
    
    default:
      break;
  }
  
  return ev;
}


void WebSocketHandler::HandleIncomingMsg(std::string msg)
{
  ESP_LOGD(TAG, "WebSocketHandler[%p]: received msg '%s'", m_nc, msg.c_str());
  
  std::istringstream input(msg);
  std::string cmd, arg;
  input >> cmd;
  
  if (cmd == "subscribe") {
    while (!input.eof()) {
      input >> arg;
      if (!arg.empty()) Subscribe(arg);
    }
  }
  else if (cmd == "unsubscribe") {
    while (!input.eof()) {
      input >> arg;
      if (!arg.empty()) Unsubscribe(arg);
    }
  }
  else {
    ESP_LOGW(TAG, "WebSocketHandler[%p]: unhandled message: '%s'", m_nc, msg.c_str());
  }
}


/**
 * WebSocketHandler slot registry:
 *  WebSocketSlots keep metrics modifiers once allocated (limited ressource)
 */

WebSocketHandler* OvmsWebServer::CreateWebSocketHandler(mg_connection* nc)
{
  if (xSemaphoreTake(m_client_mutex, portMAX_DELAY) != pdTRUE)
    return NULL;
  
  // find free slot:
  int i;
  for (i=0; i<m_client_slots.size() && m_client_slots[i].handler != NULL; i++);
  
  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  
  if (i == m_client_slots.size()) {
    // create new client slot:
    WebSocketSlot slot;
    slot.handler = NULL;
    slot.modifier = MyMetrics.RegisterModifier();
    slot.reader = MyNotify.RegisterReader("ovmsweb", COMMAND_RESULT_VERBOSE,
                                          std::bind(&OvmsWebServer::IncomingNotification, i, _1, _2), true,
                                          std::bind(&OvmsWebServer::NotificationFilter, i, _1, _2));
    ESP_LOGD(TAG, "new WebSocket slot %d, registered modifier is %d, reader %d", i, slot.modifier, slot.reader);
    m_client_slots.push_back(slot);
  } else {
    // reuse slot:
    MyNotify.RegisterReader(m_client_slots[i].reader, "ovmsweb", COMMAND_RESULT_VERBOSE,
                            std::bind(&OvmsWebServer::IncomingNotification, i, _1, _2), true,
                            std::bind(&OvmsWebServer::NotificationFilter, i, _1, _2));
  }
  
  // create handler:
  WebSocketHandler* handler = new WebSocketHandler(nc, i, m_client_slots[i].modifier, m_client_slots[i].reader);
  m_client_slots[i].handler = handler;
  
  // start ticker:
  m_client_cnt++;
  if (xTimerIsTimerActive(m_update_ticker) == pdFALSE)
    xTimerStart(m_update_ticker, 0);
  
  ESP_LOGD(TAG, "WebSocket[%p] handler %p opened; %d clients active", nc, handler, m_client_cnt);
  MyEvents.SignalEvent("server.web.socket.opened", (void*)m_client_cnt);
  
  xSemaphoreGive(m_client_mutex);
  
  // initial tx:
  handler->AddTxJob({ WSTX_MetricsAll, NULL });
  
  return handler;
}

void OvmsWebServer::DestroyWebSocketHandler(WebSocketHandler* handler)
{
  if (xSemaphoreTake(m_client_mutex, portMAX_DELAY) != pdTRUE)
    return;
  
  // find slot:
  for (int i=0; i<m_client_slots.size(); i++) {
    if (m_client_slots[i].handler == handler) {
      
      // stop ticker:
      m_client_cnt--;
      if (m_client_cnt == 0)
        xTimerStop(m_update_ticker, 0);
      
      // deregister:
      MyNotify.ClearReader(m_client_slots[i].reader);
      
      // destroy handler:
      mg_connection* nc = handler->m_nc;
      m_client_slots[i].handler = NULL;
      delete handler;
      
      ESP_LOGD(TAG, "WebSocket[%p] handler %p closed; %d clients active", nc, handler, m_client_cnt);
      MyEvents.SignalEvent("server.web.socket.closed", (void*)m_client_cnt);
      
      break;
    }
  }
  
  xSemaphoreGive(m_client_mutex);
}


bool OvmsWebServer::AddToBacklog(int client, WebSocketTxJob job)
{
  WebSocketTxTodo todo = { client, job };
  return (xQueueSend(MyWebServer.m_client_backlog, &todo, 0) == pdTRUE);
}


/**
 * EventListener:
 */
void OvmsWebServer::EventListener(std::string event, void* data)
{
  // ticker:
  if (event == "ticker.1") {
    CfgInitTicker();
  }
  
  // forward events to all websocket clients:
  if (xSemaphoreTake(m_client_mutex, 0) != pdTRUE) {
    for (int i=0; i<m_client_cnt; i++) {
      auto& slot = m_client_slots[i];
      if (slot.handler) {
        WebSocketTxJob job = { WSTX_Event, strdup(event.c_str()) };
        if (!AddToBacklog(i, job)) {
          ESP_LOGW(TAG, "EventListener: event '%s' dropped for client %d", event.c_str(), i);
          free(job.event);
        }
      }
    }
    return;
  }
  
  for (auto slot: m_client_slots) {
    if (slot.handler) {
      WebSocketTxJob job = { WSTX_Event, strdup(event.c_str()) };
      if (!slot.handler->AddTxJob(job, false))
        free(job.event);
      // Note: init_tx false to prevent mg_broadcast() deadlock on network events
      //  and keep processing time low
    }
  }
  
  xSemaphoreGive(m_client_mutex);
}


/**
 * UpdateTicker: periodical updates & tx queue checks
 * Note: this is executed in the timer task context. [https://www.freertos.org/RTOS-software-timer.html]
 */
void OvmsWebServer::UpdateTicker(TimerHandle_t timer)
{
  if (xSemaphoreTake(MyWebServer.m_client_mutex, 0) != pdTRUE) {
    ESP_LOGD(TAG, "UpdateTicker: can't lock client list, ticker run skipped");
    return;
  }
  
  // check tx backlog:
  WebSocketTxTodo todo;
  while (xQueuePeek(MyWebServer.m_client_backlog, &todo, 0) == pdTRUE) {
    auto& slot = MyWebServer.m_client_slots[todo.client];
    if (!slot.handler) {
      todo.job.clear(todo.client);
    }
    else if (slot.handler->AddTxJob(todo.job)) {
      xQueueReceive(MyWebServer.m_client_backlog, &todo, 0);
    }
  }
  
  // trigger metrics update:
  for (auto slot: MyWebServer.m_client_slots) {
    if (slot.handler)
      slot.handler->AddTxJob({ WSTX_MetricsUpdate, NULL });
  }
  
  xSemaphoreGive(MyWebServer.m_client_mutex);
}


/**
 * Notifications:
 */

void WebSocketHandler::Subscribe(std::string topic)
{
  for (auto it = m_subscriptions.begin(); it != m_subscriptions.end();) {
    if (mg_mqtt_match_topic_expression(mg_mk_str(topic.c_str()), mg_mk_str((*it).c_str()))) {
      // remove topic covered by new subscription:
      ESP_LOGD(TAG, "WebSocketHandler[%p]: subscription '%s' removed", m_nc, (*it).c_str());
      it = m_subscriptions.erase(it);
    } else if (mg_mqtt_match_topic_expression(mg_mk_str((*it).c_str()), mg_mk_str(topic.c_str()))) {
      // new subscription covered by existing:
      ESP_LOGD(TAG, "WebSocketHandler[%p]: subscription '%s' already covered by '%s'", m_nc, topic.c_str(), (*it).c_str());
      return;
    } else {
      it++;
    }
  }
  m_subscriptions.insert(topic);
  ESP_LOGD(TAG, "WebSocketHandler[%p]: subscription '%s' added", m_nc, topic.c_str());
}

void WebSocketHandler::Unsubscribe(std::string topic)
{
  for (auto it = m_subscriptions.begin(); it != m_subscriptions.end();) {
    if (mg_mqtt_match_topic_expression(mg_mk_str(topic.c_str()), mg_mk_str((*it).c_str()))) {
      ESP_LOGD(TAG, "WebSocketHandler[%p]: subscription '%s' removed", m_nc, (*it).c_str());
      it = m_subscriptions.erase(it);
    } else {
      it++;
    }
  }
}

bool WebSocketHandler::IsSubscribedTo(std::string topic)
{
  for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); it++) {
    if (mg_mqtt_match_topic_expression(mg_mk_str((*it).c_str()), mg_mk_str(topic.c_str()))) {
      return true;
    }
  }
  return false;
}

bool OvmsWebServer::NotificationFilter(int client, OvmsNotifyType* type, const char* subtype)
{
  if (xSemaphoreTake(MyWebServer.m_client_mutex, 0) != pdTRUE) {
    return true; // assume subscription (safe side)
  }

  bool accept = false;
  const auto& slot = MyWebServer.m_client_slots[client];

  if (slot.handler == NULL)
  {
    // client gone:
    accept = false;
  }
  else if (strcmp(type->m_name, "info") == 0 ||
           strcmp(type->m_name, "error") == 0 ||
           strcmp(type->m_name, "alert") == 0)
  {
    // always forward these:
    accept = true;
  }
  else if (strcmp(type->m_name, "data") == 0 ||
           strcmp(type->m_name, "stream") == 0)
  {
    // forward if subscribed:
    std::string topic = std::string("notify/") + type->m_name + "/" + mqtt_topic(subtype);
    accept = slot.handler->IsSubscribedTo(topic);
  }

  xSemaphoreGive(MyWebServer.m_client_mutex);
  return accept;
}

bool OvmsWebServer::IncomingNotification(int client, OvmsNotifyType* type, OvmsNotifyEntry* entry)
{
  WebSocketTxJob job;
  job.type = WSTX_Notify;
  job.notification = entry;
  bool done = false;

  if (xSemaphoreTake(MyWebServer.m_client_mutex, 0) != pdTRUE) {
    if (!MyWebServer.AddToBacklog(client, job)) {
      ESP_LOGW(TAG, "IncomingNotification of type '%s' subtype '%s' dropped for client %d",
               type->m_name, entry->GetSubType(), client);
      done = true;
    }
    return done;
  }

  const auto& slot = MyWebServer.m_client_slots[client];
  if (!slot.handler || !slot.handler->AddTxJob(job, false))
    done = true;

  xSemaphoreGive(MyWebServer.m_client_mutex);
  return done;
}
