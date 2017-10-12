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
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
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
static const char newline = '\n';

//-----------------------------------------------------------------------------
//    Class TelnetServer
//-----------------------------------------------------------------------------

TelnetServer::TelnetServer(Parent* parent)
  : TaskBase(parent)
  {
  m_socket = -1;
  CreateTaskPinned(1, "TelnetServerTask");
  }

TelnetServer::~TelnetServer()
  {
  Cleanup();
  }

void TelnetServer::Service()
  {
  m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_socket < 0) {
    ESP_LOGE(tag, "socket: %d (%s)", errno, strerror(errno));
    return;
    }

  struct sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons(23);

  int rc = bind(m_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
  if (rc < 0) {
    ESP_LOGE(tag, "bind socket %d: %d (%s)", m_socket, errno, strerror(errno));
    return;
    }

  rc = listen(m_socket, 5);
  if (rc < 0) {
    ESP_LOGE(tag, "listen: %d (%s)", errno, strerror(errno));
    return;
    }

  fd_set readfds, writefds, errorfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);
  FD_SET(m_socket, &readfds);
  while (true) {
    int rc = select(m_socket+1, &readfds, &writefds, &errorfds, (struct timeval *)NULL);
    if (rc < 0)
      {
      ESP_LOGE(tag, "select: %d (%s)", errno, strerror(errno));
      break;
      }
    if (m_socket < 0)   // Was server socket closed (by wifi shutting down)?
      break;

    socklen_t len = sizeof(serverAddr);
    int partnerSocket = accept(m_socket, (struct sockaddr *)&serverAddr, &len);
    if (partnerSocket < 0)
      {
      if (errno != ECONNABORTED)
        ESP_LOGE(tag, "accept: %d (%s)", errno, strerror(errno));
      break;
      }

    AddChild(new ConsoleTelnet(this, partnerSocket));
    }
  }

void TelnetServer::Cleanup()
  {
  if (m_socket >= 0)
    {
    int socket = m_socket;
    m_socket = -1;
    if (closesocket(socket) < 0)
      {
      ESP_LOGE(tag, "closesocket %d: %d (%s)", socket, errno, strerror(errno));
      }
    }
  }

//-----------------------------------------------------------------------------
//    Class TelnetReceiver
//-----------------------------------------------------------------------------

// The TelnetReceiverTask is required so that we can use a blocking recv() to
// take input from the telnet connection.  It must queue Events to the
// ConsoleTelnetTask, rather than taking direct actions itself, so that
// libtelnet code is not executed simultaneously by both tasks.

TelnetReceiver::TelnetReceiver(ConsoleTelnet* parent, int socket, char* buffer,
                               QueueHandle_t queue, SemaphoreHandle_t sem)
  : TaskBase(parent)
  {
  m_socket = socket;
  m_buffer = buffer;
  m_queue = queue;
  m_semaphore = sem;
  CreateTaskPinned(1, "TelnetReceiverTask");
  }

TelnetReceiver::~TelnetReceiver()
  {
  }

void TelnetReceiver::Service()
  {
  while (true)
    {
    OvmsConsole::Event event;
    event.type = OvmsConsole::event_type_t::RECV;
    event.size = recv(m_socket, m_buffer, BUFFER_SIZE, 0);
    BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
    if (ret == pdPASS)
      {
      // Block here until the queued message has been taken.
      xSemaphoreTake(m_semaphore, portMAX_DELAY);
      }
    else
      ESP_LOGE(tag, "Timeout queueing message in TelnetReceiver::Service\n");
    // Stop if recv() got nothing (because the connection was closed).
    if (event.size == 0)
      break;
    }
  }

//-----------------------------------------------------------------------------
//    Class ConsoleTelnet
//-----------------------------------------------------------------------------

ConsoleTelnet::ConsoleTelnet(TelnetServer* server, int socket)
  : OvmsConsole(server)
  {
  m_socket = socket;
  m_queue = xQueueCreate(100, sizeof(Event));
  m_semaphore = xSemaphoreCreateCounting(1, 0);

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
  m_telnet = telnet_init(options, TelnetCallback, TELNET_FLAG_NVT_EOL, (void*)this);
  AddChild(new TelnetReceiver(this, socket, m_buffer, m_queue, m_semaphore));
  telnet_negotiate(m_telnet, TELNET_WILL, TELNET_TELOPT_ECHO);

  Initialize("Telnet");
  }

// This destructor may be called by ConsoleTelnetTask to delete itself or by
// TelnetServerTask if the server gets shut down, but not by TelnetReceiverTask.

ConsoleTelnet::~ConsoleTelnet()
  {
  m_ready = false;
  DeleteChildren();
  int socket = m_socket;
  m_socket = -1;
  if (socket >= 0 && closesocket(socket) < 0)
    ESP_LOGE(tag, "closesocket %d: %d (%s)", socket, errno, strerror(errno));
  telnet_t *telnet = m_telnet;
  m_telnet = NULL;
  telnet_free(telnet);
  vSemaphoreDelete(m_semaphore);
  vQueueDelete(m_queue);
  }

int ConsoleTelnet::puts(const char* s)
  {
  if (!m_telnet)
    return -1;
  telnet_send_text(m_telnet, s, strlen(s));
  telnet_send_text(m_telnet, &newline, 1);
  return 0;
  }

int ConsoleTelnet::printf(const char* fmt, ...)
  {
  if (!m_telnet)
    return 0;
  va_list args;
  va_start(args,fmt);
  int ret = telnet_vprintf(m_telnet, fmt, args);
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

//-----------------------------------------------------------------------------
//    ConsoleTelnetTask
//-----------------------------------------------------------------------------

// The following code is executed by the ConsoleTelnetTask created by the
// constructor calling OvmsConsole::Initialize().

void ConsoleTelnet::HandleDeviceEvent(void* pEvent)
  {
  Event event = *(Event*)pEvent;
  switch (event.type)
    {
    case RECV:
      if (event.size > 0)
        {
        telnet_recv(m_telnet, m_buffer, event.size);
        // Unblock TelnetReceiverTask now that we are finished with
        // the buffer.
        xSemaphoreGive(m_semaphore);
        }
      else
        {
        // Unblock TelnetReceiverTask to let it exit.
        xSemaphoreGive(m_semaphore);
        DeleteFromParent();
        // Execution does not return here because this task kills itself.
        }
      break;

    default:
      ESP_LOGE(tag, "Unknown event type in ConsoleTelnet");
      break;
    }
  }

void ConsoleTelnet::TelnetCallback(telnet_t *telnet, telnet_event_t *event, void *userData)
  {
  ((ConsoleTelnet*)userData)->TelnetHandler(event);
  }

void ConsoleTelnet::TelnetHandler(telnet_event_t *event)
  {
  int rc;
  switch (event->type)
    {
    case TELNET_EV_SEND:
      rc = send(m_socket, event->data.buffer, event->data.size, 0);
      if (rc < 0)
        DeleteFromParent();
      break;

    case TELNET_EV_DATA:
      {
      char *buffer = (char *)event->data.buffer;
      size_t size = (size_t)event->data.size;
      // Translate CR (Enter) from telnet client into \n for microrl
      for (size_t i = 0; i < size; ++i)
        {
        if (buffer[i] == '\r')
          buffer[i] = '\n';
        }
      ProcessChars(buffer, size);
      break;
      }

    default:
#ifdef LOGEVENTS
      ESP_LOGI(tag, "telnet event: %s", eventToString(event->type));
#endif
      break;
    }
  }

// This is called to shut down the Telnet connection when the "exit" command is input.
void ConsoleTelnet::Exit()
  {
  DeleteFromParent();
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
