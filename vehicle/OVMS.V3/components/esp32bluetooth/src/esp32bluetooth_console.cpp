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

#include "esp32bluetooth_app_console.h"
#include "esp32bluetooth_console.h"
#include "ovms_log.h"

struct mbuf {
  char *buf;       /* Buffer pointer */
  size_t len;      /* Data length */
};

static const char *tag = "bt-console";
static const char newline = '\n';

OvmsBluetoothConsole::OvmsBluetoothConsole(OvmsBluetoothAppConsole* app)
  {
  m_app = app;
  m_issecure = true;
  m_queue = xQueueCreate(100, sizeof(Event));
  Initialize("Bluetooth");
  ESP_LOGD(tag, "Created OvmsBluetoothConsole");
  }

OvmsBluetoothConsole::~OvmsBluetoothConsole()
  {
  ESP_LOGD(tag, "Deleted OvmsBluetoothConsole");
  // m_queue deleted by OvmsConsole::~OvmsConsole()
  }

void OvmsBluetoothConsole::DataToConsole(uint8_t* data, size_t len)
  {
  OvmsConsole::Event event;
  ESP_LOGV(tag, "DataToConsole %zu bytes: '%.*s'", len, len, (char*)data);
  event.type = OvmsConsole::event_type_t::RECV;
  event.mbuf = (mbuf*) malloc(sizeof(mbuf) + len);
  event.mbuf->buf = (char*)event.mbuf + sizeof(mbuf);
  event.mbuf->len = len;
  bcopy(data, event.mbuf->buf, len);
  BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
  if (ret == pdPASS)
    {
    // Process this input in sequence with any queued logging.
    Poll(0);
    }
  else
    {
    ESP_LOGE(tag, "Timeout queueing message in OvmsBluetoothConsole::DataToConsole\n");
    free(event.mbuf);
    }
  }

void OvmsBluetoothConsole::HandleDeviceEvent(void* pEvent)
  {
  Event event = *(Event*)pEvent;
  switch (event.type)
    {
    case RECV:
      {
      char *buffer = (char *)event.mbuf->buf;
      size_t size = (size_t)event.mbuf->len;
      // Translate CR (Enter) from Bluetooth client into \n for microrl
      for (size_t i = 0; i < size; ++i)
        {
        if (buffer[i] == '\r' && (i == size-1 || buffer[i+1] != '\n'))
          buffer[i] = '\n';
        }
      ESP_LOGV(tag, "Received %zu bytes: '%.*s'", size, size, buffer);
      ProcessChars(buffer, size);
      free(event.mbuf);
      break;
      }

    default:
      ESP_LOGE(tag, "Unknown event type in OvmsBluetoothConsole");
      break;
    }
  }

// This is called to shut down the connection when the "exit" command is input.
void OvmsBluetoothConsole::Exit()
  {
  printf("logout\n");
  }

int OvmsBluetoothConsole::puts(const char* s)
  {
  size_t len = strlen(s);
  ESP_LOGV(tag, "puts %zu bytes: '%.*s'", len, len, s);
  m_app->DataFromConsole(s, len);
  m_app->DataFromConsole(&newline, 1);
  return 0;
  }

int OvmsBluetoothConsole::printf(const char* fmt, ...)
  {
  char *buffer;
  va_list args;
  va_start(args,fmt);
  int ret = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (ret >= 0)
    {
    ESP_LOGV(tag, "printf %d bytes: '%.*s'", ret, ret, buffer);
    m_app->DataFromConsole(buffer, ret);
    free(buffer);
    }
  return ret;
  }

ssize_t OvmsBluetoothConsole::write(const void *buf, size_t nbyte)
  {
  ESP_LOGV(tag, "write %zu bytes: '%.*s'", nbyte, nbyte, (const char*)buf);
  m_app->DataFromConsole((const char*)buf, nbyte);
  return nbyte;
  }
