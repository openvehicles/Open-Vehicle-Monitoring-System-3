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

// We're using ESP_EARLY_LOG* (direct USB console output) for protocol debug logging.
// To enable protocol debug logging locally, uncomment:
// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "ovms_log.h"
static const char *TAG = "webcommand";

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "buffered_shell.h"
#include "log_buffers.h"
#include "ovms_webserver.h"
#include "ovms_script.h"


struct hcs_writebuf
{
  char* data;
  size_t len;
};


HttpCommandStream::HttpCommandStream(mg_connection* nc, extram::string command,
    bool javascript /*=false*/, int verbosity /*=COMMAND_RESULT_NORMAL*/)
  : OvmsShell(verbosity), MgHandler(nc)
  // Note: due to a gcc bug, the base classes MUST be done in this order,
  //  or compilation will fail with "error: generic thunk code fails for method […] printf".
  // See https://bugzilla.redhat.com/show_bug.cgi?id=1511021
  //  and https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83549
  //  for details.
{
  ESP_EARLY_LOGD(TAG, "HttpCommandStream[%p] init: handler=%p command='%s%s' verbosity=%d", nc, this,
    command.substr(0,200).c_str(), (command.length()>200) ? " [...]" : "", verbosity);
  
  m_command = command;
  m_javascript = javascript;
  m_done = false;
  m_sent = m_ack = 0;
  Initialize(false);
  SetSecure(true); // Note: assuming user is admin
  
  // create write queue & command task:
  m_writequeue = xQueueCreate(30, sizeof(hcs_writebuf));
  char name[configMAX_TASK_NAME_LEN];
  snprintf(name, sizeof(name), "%s", command.c_str());
  xTaskCreatePinnedToCore(CommandTask, name, CONFIG_OVMS_SYS_COMMAND_STACK_SIZE, (void*)this, 4, &m_cmdtask, CORE(1));
}

HttpCommandStream::~HttpCommandStream()
{
  hcs_writebuf wbuf;
  while (xQueueReceive(m_writequeue, &wbuf, 0) == pdTRUE)
    free(wbuf.data);
  vQueueDelete(m_writequeue);
}


void HttpCommandStream::Initialize(bool print)
{
  if (!m_javascript) {
    OvmsShell::Initialize(print);
    ProcessChar('\n');
  }
}


void HttpCommandStream::CommandTask(void* object)
{
  HttpCommandStream* me = (HttpCommandStream*) object;
  
  ESP_LOGI(TAG, "HttpCommandStream[%p]: %d bytes free, executing: %s%s",
    me->m_nc, heap_caps_get_free_size(MALLOC_CAP_8BIT),
    me->m_command.substr(0,200).c_str(), (me->m_command.length()>200) ? " [...]" : "");
  
  // execute command:
  if (me->m_javascript) {
    #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
      MyScripts.DuktapeEvalNoResult(me->m_command.c_str(), me);
    #else
      me->puts("ERROR: Javascript support disabled");
    #endif
  } else {
    me->ProcessChars(me->m_command.data(), me->m_command.size());
    me->ProcessChar('\n');
  }

  me->m_done = true;
  
#if MG_ENABLE_BROADCAST && WEBSRV_USE_MG_BROADCAST
  if (uxQueueMessagesWaiting(me->m_writequeue) > 0) {
    ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] RequestPollLast, qlen=%d done=%d sent=%d ack=%d", me->m_nc, uxQueueMessagesWaiting(me->m_writequeue), me->m_done, me->m_sent, me->m_ack);
    me->RequestPoll();
    ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] RequestPollDone, qlen=%d done=%d sent=%d ack=%d", me->m_nc, uxQueueMessagesWaiting(me->m_writequeue), me->m_done, me->m_sent, me->m_ack);
  }
#endif // MG_ENABLE_BROADCAST && WEBSRV_USE_MG_BROADCAST
  
  while (me->m_nc)
    vTaskDelay(10/portTICK_PERIOD_MS);
  delete me;
  vTaskDelete(NULL);
}


void HttpCommandStream::ProcessQueue()
{
  size_t txlen = 0;
  hcs_writebuf wbuf;
  
  while (txlen < XFER_CHUNK_SIZE && xQueueReceive(m_writequeue, &wbuf, 0) == pdTRUE) {
    if (m_nc) {
      mg_send_http_chunk(m_nc, wbuf.data, wbuf.len);
      txlen += wbuf.len;
    }
    free(wbuf.data);
  }
  
  if (txlen) {
    m_sent += txlen;
    ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] ProcessQueue txlen=%d, qlen=%d done=%d sent=%d ack=%d",
      m_nc, txlen, uxQueueMessagesWaiting(m_writequeue), m_done, m_sent, m_ack);
  }

  if (m_done && m_sent == m_ack) {
    ESP_EARLY_LOGD(TAG, "HttpCommandStream[%p] DONE, %d bytes sent, %d bytes free",
      m_nc, m_sent, heap_caps_get_free_size(MALLOC_CAP_8BIT));
    if (m_nc) {
      m_nc->flags |= MG_F_SEND_AND_CLOSE; // necessary to prevent mg_broadcast lockups
      mg_send_http_chunk(m_nc, "", 0);
      m_nc->user_data = NULL;
      m_nc = NULL;
    }
  }
}


int HttpCommandStream::HandleEvent(int ev, void* p)
{
  switch (ev)
  {
    case MG_EV_POLL:
      // check for new transmission:
      ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] EV_POLL qlen=%d done=%d sent=%d ack=%d",
        m_nc, uxQueueMessagesWaiting(m_writequeue), m_done, m_sent, m_ack);
      if (m_ack == m_sent)
        ProcessQueue();
      break;
    
    case MG_EV_SEND:
      // last transmission has finished:
      ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] EV_SEND qlen=%d done=%d sent=%d ack=%d",
        m_nc, uxQueueMessagesWaiting(m_writequeue), m_done, m_sent, m_ack);
      m_ack = m_sent;
      ProcessQueue();
      break;
    
    case MG_EV_CLOSE:
      ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] EV_CLOSE qlen=%d done=%d sent=%d ack=%d",
        m_nc, uxQueueMessagesWaiting(m_writequeue), m_done, m_sent, m_ack);
      // connection has been closed, possibly externally:
      // we need to let the command task finish normally to prevent problems
      // due to lost/locked ressources, so we just detach:
      m_nc->user_data = NULL;
      m_nc = NULL;
      ProcessQueue();   // empty queue (no tx) to prevent task lockup on write
      ev = 0;           // prevent deletion by main event handler
      break;
    
    default:
      break;
  }
  
  return ev;
}


int HttpCommandStream::puts(const char* s)
{
  if (!m_nc)
    return 0;
  write(s, strlen(s));
  write("\n", 1);
  return 0;
}

int HttpCommandStream::printf(const char* fmt, ...)
{
  if (!m_nc)
    return 0;
  char *buffer = NULL;
  va_list args;
  va_start(args, fmt);
  int ret = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (ret >= 0) {
    write(buffer, ret);
    free(buffer);
  }
  return ret;
}

ssize_t HttpCommandStream::write(const void *buf, size_t nbyte)
{
  if (!m_nc || nbyte == 0)
    return 0;
  
  hcs_writebuf wbuf;
  wbuf.data = (char*) ExternalRamMalloc(nbyte);
  wbuf.len = nbyte;
  memcpy(wbuf.data, buf, nbyte);
  
  if (xQueueSend(m_writequeue, &wbuf, portMAX_DELAY) != pdTRUE) {
    free(wbuf.data);
    return nbyte;
  }
  
#if MG_ENABLE_BROADCAST && WEBSRV_USE_MG_BROADCAST
  if (uxQueueMessagesWaiting(m_writequeue) == 1) {
    ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] RequestPoll, qlen=1 done=%d sent=%d ack=%d", m_nc, m_done, m_sent, m_ack);
    RequestPoll();
    ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] RequestPollDone, qlen=%d done=%d sent=%d ack=%d", m_nc, uxQueueMessagesWaiting(m_writequeue), m_done, m_sent, m_ack);
  }
  else
#endif // MG_ENABLE_BROADCAST && WEBSRV_USE_MG_BROADCAST
    ESP_EARLY_LOGV(TAG, "HttpCommandStream[%p] AddQueue, qlen=%d done=%d sent=%d ack=%d", m_nc, uxQueueMessagesWaiting(m_writequeue), m_done, m_sent, m_ack);
  
  return nbyte;
}

void HttpCommandStream::Log(LogBuffers* message)
{
  // writing could block, logging is done via the websocket stream
  message->release();
}
