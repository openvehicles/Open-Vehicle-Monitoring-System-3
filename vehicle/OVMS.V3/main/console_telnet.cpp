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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> // Required for libtelnet.h
#include <string.h>
#include <errno.h>
#include <lwip/def.h>
#include <lwip/sockets.h>
#include "freertos/queue.h"
#include "esp_log.h"
#include "console_telnet.h"

//#define LOGEVENTS
#ifdef LOGEVENTS
static const char *eventToString(telnet_event_type_t type);
#endif

static const char *tag = "telnet";

TelnetServer::TelnetServer()
  {
  xTaskCreatePinnedToCore(TelnetServer::Task, "TelnetServerTask", 4096, (void*)this, 5, &m_taskid, 1);
  }

TelnetServer::~TelnetServer()
  {
  }

void TelnetServer::Task(void*)
  {
  ESP_LOGI(tag, ">> telnet_listenForClients");
  int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  struct sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons(23);

  int rc = bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
  if (rc < 0) {
    ESP_LOGE(tag, "bind socket %d: %d (%s)", serverSocket, errno, strerror(errno));
    vTaskDelete(NULL);
    }

  rc = listen(serverSocket, 5);
  if (rc < 0) {
    ESP_LOGE(tag, "listen: %d (%s)", errno, strerror(errno));
    vTaskDelete(NULL);
    }

  while(1) {
    socklen_t len = sizeof(serverAddr);
    int partnerSocket = accept(serverSocket, (struct sockaddr *)&serverAddr, &len);
    if (partnerSocket < 0) {
      ESP_LOGE(tag, "accept: %d (%s)", errno, strerror(errno));
      break;
      }

    ESP_LOGI(tag, "We have a new client connection!");
    new ConsoleTelnet(partnerSocket);
    }

  vTaskDelete(NULL);
  }

ConsoleTelnet::ConsoleTelnet(int socket)
  {
  m_split_eol = 0;
  m_socket = socket;
  m_queue = xQueueCreate(100, sizeof(Event));
  m_semaphore = xSemaphoreCreateCounting(1, 0);
  MyCommandApp.RegisterCommand("exit", "End telnet session", Exit , "", 0, 0);

  static const telnet_telopt_t options[] =
    {
    { TELNET_TELOPT_ECHO,      TELNET_WILL, TELNET_DONT },
    { TELNET_TELOPT_LINEMODE,  TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_SGA,       TELNET_WILL, TELNET_DO   },
    { TELNET_TELOPT_TTYPE,     TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_COMPRESS2, TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_ZMP,       TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_MSSP,      TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_BINARY,    TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_NAWS,      TELNET_WONT, TELNET_DONT },
    { -1, 0, 0 }
    };
  m_telnet = telnet_init(options, TelnetCallback, 0, (void*)this);
  telnet_negotiate(m_telnet, TELNET_WILL, TELNET_TELOPT_ECHO);

  Initialize("Telnet");

  xTaskCreatePinnedToCore(ConsoleTelnet::TelnetReceiverTask, "TelnetReceiverTask", 4096, (void*)this, 5, &m_receiver_taskid, 1);
  }

ConsoleTelnet::~ConsoleTelnet()
  {
  ESP_LOGI(tag, "Reached ~ConsoleTelnet");
  m_ready = false;
  telnet_t *t = m_telnet;
  m_telnet = NULL;
  telnet_free(t);
  vSemaphoreDelete(m_semaphore);
  vQueueDelete(m_queue);
  ESP_LOGI(tag, "About to delete receiver task");
  vTaskDelete(m_receiver_taskid);
  }

int ConsoleTelnet::puts(const char* s)
  {
  const char nl = '\n';
  if (!m_telnet)
    return -1;
  telnet_send_text(m_telnet, s, strlen(s));
  telnet_send_text(m_telnet, &nl, 1);
  return 0;
  }

int ConsoleTelnet::printf(const char* fmt, ...)
  {
  int ret;
  va_list args;
  if (!m_telnet)
    return 0;
  va_start(args,fmt);
  ret = telnet_vprintf(m_telnet, fmt, args);
  va_end(args);
  return ret;
  }

ssize_t ConsoleTelnet::write(const void *buf, size_t nbyte)
  {
  if (!m_telnet)
    return 0;
  telnet_send_text(m_telnet, (const char*)buf, nbyte);
  return nbyte;
  }

void ConsoleTelnet::finalise()
  {
  }

void ConsoleTelnet::HandleDeviceEvent(void* pEvent)
  {
  Event event = *(Event*)pEvent;
  switch (event.type)
    {
    case INPUT:
      ProcessChars(event.buffer, event.size);
      // Unblock DoTelnet now that we are finished with the buffer
      xSemaphoreGive(m_semaphore);
      break;

    case EXIT:
      ESP_LOGI(tag, "About to delete ConsoleTelnet");
      delete this;
      break;

    default:
      ESP_LOGI(tag, "Unknown event type in ConsoleTelnet");
      break;
    }
  }

void ConsoleTelnet::Exit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  close(((ConsoleTelnet*)writer)->m_socket);
  }

void ConsoleTelnet::TelnetReceiverTask(void *pvParameters)
  {
  ConsoleTelnet* me = (ConsoleTelnet*)pvParameters;
  me->DoTelnet();
  ESP_LOGI(tag, "Leaving TelnetReceiverTask\n");
  while(true);
  }

void ConsoleTelnet::DoTelnet()
  {
  while (true)
    {
    ssize_t len = recv(m_socket, (char *)m_buffer, sizeof(m_buffer), 0);
    if (len == 0)
      break;
    telnet_recv(m_telnet, (char *)m_buffer, len);
    }
  ESP_LOGI(tag, "Telnet partner finished");
  Event event;
  event.type = EXIT;
  BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
  if (ret != pdPASS)
    {
    ESP_LOGI(tag, "Timeout queueing message in ConsoleTelnet::DoTelnet\n");
    }
  ESP_LOGI(tag, "Leaving DoTelnet\n");
  }

void ConsoleTelnet::TelnetCallback(telnet_t *telnet, telnet_event_t *event, void *userData)
  {
  ((ConsoleTelnet*)userData)->TelnetHandler(event);
  }

void ConsoleTelnet::TelnetHandler(telnet_event_t *event)
  {
  int rc;
  #ifdef LOGEVENTS
  ESP_LOGI(tag, "telnet event: %s", eventToString(event->type));
  #endif
  switch (event->type)
    {
    case TELNET_EV_SEND:
      rc = send(m_socket, event->data.buffer, event->data.size, 0);
      if (rc < 0)
        {
        if (errno == EBADF)
          {
          ESP_LOGI(tag, "Telnet connection closed");
          Event event;
          event.type = EXIT;
          BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
          if (ret != pdPASS)
            {
            ESP_LOGI(tag, "Timeout queueing message in ConsoleTelnet::TelnetHandler\n");
            }
          break;
          }
        ESP_LOGE(tag, "send: %d (%s)", errno, strerror(errno));
        }
      break;

    case TELNET_EV_DATA:
      {
      char *buffer = (char *)event->data.buffer;
      size_t size = (size_t)event->data.size;
      size = telnet_translate_eol(m_telnet, buffer, size, &m_split_eol);
      // Translate CR (Enter) from telnet client into \n for microrl
      for (size_t i = 0; i < size; ++i)
        {
        if (buffer[i] == '\r')
          buffer[i] = '\n';
        }
      Event event;
      event.type = INPUT;
      event.buffer = buffer;
      event.size = size;
      BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
      if (ret != pdPASS)
        {
        ESP_LOGI(tag, "Timeout queueing message in ConsoleTelnet::TelnetHandler\n");
        break;
        }
      // Block here until the queued message has been taken;
      xSemaphoreTake(m_semaphore, portMAX_DELAY);
      break;
      }

    case TELNET_EV_DO:
      //ESP_LOGI(tag, "received DO, option=%d", event->neg.telopt);
      break;

    default:
      break;
    }
  }

/**
 * Convert a telnet event type to its string representation.
 */
#ifdef LOGEVENTS
static const char *eventToString(telnet_event_type_t type) {
	switch(type) {
	case TELNET_EV_COMPRESS:
		return "TELNET_EV_COMPRESS";
	case TELNET_EV_DATA:
		return "TELNET_EV_DATA";
	case TELNET_EV_DO:
		return "TELNET_EV_DO";
	case TELNET_EV_DONT:
		return "TELNET_EV_DONT";
	case TELNET_EV_ENVIRON:
		return "TELNET_EV_ENVIRON";
	case TELNET_EV_ERROR:
		return "TELNET_EV_ERROR";
	case TELNET_EV_IAC:
		return "TELNET_EV_IAC";
	case TELNET_EV_MSSP:
		return "TELNET_EV_MSSP";
	case TELNET_EV_SEND:
		return "TELNET_EV_SEND";
	case TELNET_EV_SUBNEGOTIATION:
		return "TELNET_EV_SUBNEGOTIATION";
	case TELNET_EV_TTYPE:
		return "TELNET_EV_TTYPE";
	case TELNET_EV_WARNING:
		return "TELNET_EV_WARNING";
	case TELNET_EV_WILL:
		return "TELNET_EV_WILL";
	case TELNET_EV_WONT:
		return "TELNET_EV_WONT";
	case TELNET_EV_ZMP:
		return "TELNET_EV_ZMP";
	}
	return "Unknown type";
} // eventToString
#endif
