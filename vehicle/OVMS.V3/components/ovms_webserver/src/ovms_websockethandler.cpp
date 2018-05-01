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

WebSocketHandler::WebSocketHandler(mg_connection* nc, size_t modifier)
  : MgHandler(nc)
{
  ESP_LOGV(TAG, "WebSocketHandler[%p] init: nc=%p modifier=%d", this, nc, modifier);
  
  m_modifier = modifier;
  m_jobqueue = xQueueCreate(30, sizeof(WebSocketTxJob));
  m_jobqueue_overflow = 0;
  m_mutex = xSemaphoreCreateMutex();
  m_job.type = WSTX_None;
  m_sent = m_ack = 0;
}

WebSocketHandler::~WebSocketHandler()
{
  while (xQueueReceive(m_jobqueue, &m_job, 0) == pdTRUE)
    FreeTxJob(m_job);
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
  //ESP_LOGV(TAG, "WebSocketHandler[%p]: ProcessTxJob type=%d, sent=%d ack=%d", this, m_job.type, m_sent, m_ack);
  
  // process job, send next chunk:
  switch (m_job.type)
  {
    case WSTX_Event:
    {
      if (m_sent && m_ack) {
        ESP_LOGV(TAG, "WebSocketHandler[%p]: ProcessTxJob type=%d done", this, m_job.type);
        m_job.type = WSTX_None;
      } else {
        std::string msg;
        msg.reserve(128);
        msg = "{\"event\":\"";
        msg += m_job.event;
        msg += "\"}";
        free(m_job.event);
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
          ESP_LOGV(TAG, "WebSocketHandler[%p]: ProcessTxJob type=%d done, sent=%d metrics", this, m_job.type, m_sent);
        m_job.type = WSTX_None;
      }
      
      break;
    }
    
    case WSTX_Config:
    {
      // todo: implement
      m_job.type = WSTX_None;
      m_sent = 0;
      break;
    }
    
    default:
      m_job.type = WSTX_None;
      m_sent = 0;
      break;
  }
}


void WebSocketHandler::FreeTxJob(WebSocketTxJob &job)
{
  switch (job.type) {
    case WSTX_Event:
      if (job.event)
        free(job.event);
      break;
    default:
      break;
  }
}


bool WebSocketHandler::AddTxJob(WebSocketTxJob job, bool init_tx)
{
  if (xQueueSend(m_jobqueue, &job, 0) != pdTRUE) {
    ++m_jobqueue_overflow;
    if (m_jobqueue_overflow == 1)
      ESP_LOGW(TAG, "WebSocketHandler[%p]: job queue overflow detected", this);
    return false;
  }
  else {
    if (m_jobqueue_overflow) {
      ESP_LOGW(TAG, "WebSocketHandler[%p]: job queue overflow resolved, %d drops", this, m_jobqueue_overflow);
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
      ESP_LOGD(TAG, "WebSocketHandler[%p]: received msg '%s'", this, msg.c_str());
      // Note: client commands not yet implemented
      break;
    }
    
    case MG_EV_POLL:
      // check for new transmission
      //ESP_LOGV(TAG, "WebSocketHandler[%p] EV_POLL qlen=%d jobtype=%d sent=%d ack=%d", this, uxQueueMessagesWaiting(m_jobqueue), m_job.type, m_sent, m_ack);
      InitTx();
      break;
    
    case MG_EV_SEND:
      // last transmission has finished
      //ESP_LOGV(TAG, "WebSocketHandler[%p] EV_SEND qlen=%d jobtype=%d sent=%d ack=%d", this, uxQueueMessagesWaiting(m_jobqueue), m_job.type, m_sent, m_ack);
      ContinueTx();
      break;
    
    default:
      break;
  }
  
  return ev;
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
  
  if (i == m_client_slots.size()) {
    // create new client slot:
    WebSocketSlot slot;
    slot.handler = NULL;
    slot.modifier = MyMetrics.RegisterModifier();
    ESP_LOGD(TAG, "new WebSocket slot %d, registered modifier is %d", i, slot.modifier);
    m_client_slots.push_back(slot);
  }
  
  // create handler:
  WebSocketHandler* handler = new WebSocketHandler(nc, m_client_slots[i].modifier);
  m_client_slots[i].handler = handler;
  
  // start ticker:
  m_client_cnt++;
  if (xTimerIsTimerActive(m_update_ticker) == pdFALSE)
    xTimerStart(m_update_ticker, 0);
  
  ESP_LOGD(TAG, "WebSocket connection %p opened; %d clients active", handler, m_client_cnt);
  
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
      
      // destroy handler:
      m_client_slots[i].handler = NULL;
      delete handler;
      
      ESP_LOGD(TAG, "WebSocket connection %p closed; %d clients active", handler, m_client_cnt);
      
      break;
    }
  }
  
  xSemaphoreGive(m_client_mutex);
}


/**
 * EventListener: forward events to all websocket clients
 */
void OvmsWebServer::EventListener(std::string event, void* data)
{
  if (xSemaphoreTake(m_client_mutex, 0) != pdTRUE) {
    ESP_LOGW(TAG, "EventListener: can't lock client list, event update dropped");
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
  
  for (auto slot: MyWebServer.m_client_slots) {
    if (slot.handler)
      slot.handler->AddTxJob({ WSTX_MetricsUpdate, NULL });
  }
  
  xSemaphoreGive(MyWebServer.m_client_mutex);
}
