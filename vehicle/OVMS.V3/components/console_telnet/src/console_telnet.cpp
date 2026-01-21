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
#undef bind
#include "freertos/queue.h"
#include "ovms_log.h"
#include "ovms_events.h"
#include "console_telnet.h"
#include "ovms_netmanager.h"

//#define LOGEVENTS
#ifdef LOGEVENTS
static const char *eventToString(telnet_event_type_t type);
#endif

static const char *tag = "telnet";
static const char newline = '\n';

//-----------------------------------------------------------------------------
//    Class OvmsTelnet
//-----------------------------------------------------------------------------

OvmsTelnet MyTelnet __attribute__ ((init_priority (8300)));

static void MongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  MyTelnet.EventHandler(nc, ev, p);
  }

void OvmsTelnet::EventHandler(struct mg_connection *nc, int ev, void *p)
  {
  //ESP_LOGI(tag, "Event %d conn %p, data %p", ev, nc, p);
  switch (ev)
    {
    case MG_EV_ACCEPT:
      {
      ConsoleTelnet* child = new ConsoleTelnet(nc);
      nc->user_data = child;
      break;
      }

    case MG_EV_POLL:
      {
      ConsoleTelnet* child = (ConsoleTelnet*)nc->user_data;
      if (child)
        child->Poll(0);
      }
      break;

    case MG_EV_RECV:
      {
      ConsoleTelnet* child = (ConsoleTelnet*)nc->user_data;
      child->Receive();
      child->Poll(0);
      }
      break;

    case MG_EV_CLOSE:
      {
      ConsoleTelnet* child = (ConsoleTelnet*)nc->user_data;
      if (child)
        delete child;
      }
      break;

    default:
      break;
    }
  }

OvmsTelnet::OvmsTelnet()
  {
  ESP_LOGI(tag, "Initialising Telnet (8300)");

  m_running = false;

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(tag,"network.mgr.init", std::bind(&OvmsTelnet::NetManInit, this, _1, _2));
  MyEvents.RegisterEvent(tag,"network.mgr.stop", std::bind(&OvmsTelnet::NetManStop, this, _1, _2));
  }

void OvmsTelnet::NetManInit(std::string event, void* data)
  {
  // Only initialise server for WIFI connections
  // TODO: Disabled as this introduces a network interface ordering issue. It
  //       seems that the correct way to do this is to always start the mongoose
  //       listener, but to filter incoming connections to check that the
  //       destination address is a Wifi interface address.
  // if (!(MyNetManager.m_connected_wifi || MyNetManager.m_wifi_ap))
  //   return;

  m_running = true;

  ESP_LOGI(tag, "Launching Telnet Server");
  auto mglock = console->MongooseLock();
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  mg_connection* nc = mg_bind(mgr, ":23", MongooseHandler);
  if (nc)
    nc->user_data = NULL;
  else
    ESP_LOGE(tag, "Launching Telnet Server failed");
  }

void OvmsTelnet::NetManStop(std::string event, void* data)
  {
  if (m_running)
    {
    ESP_LOGI(tag, "Stopping Telnet Server");
    m_running = false;
    }
  }

//-----------------------------------------------------------------------------
//    Class ConsoleTelnet
//-----------------------------------------------------------------------------

ConsoleTelnet::ConsoleTelnet(struct mg_connection* nc)
  {
  m_connection = nc;
  m_queue = xQueueCreate(100, sizeof(Event));

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
  telnet_negotiate(m_telnet, TELNET_WILL, TELNET_TELOPT_ECHO);
  Initialize("Telnet");
  }

// This destructor is only called by NetManTask deleting the ConsoleTelnet child.

ConsoleTelnet::~ConsoleTelnet()
  {
  telnet_t *telnet = m_telnet;
  m_telnet = NULL;
  telnet_free(telnet);
  // m_queue deleted by OvmsConsole::~OvmsConsole()
  }

void ConsoleTelnet::Receive()
  {
  OvmsConsole::Event event;
  event.type = OvmsConsole::event_type_t::RECV;
  event.mbuf = &m_connection->recv_mbuf;
  BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
  if (ret == pdPASS)
    {
    // Process this input in sequence with any queued logging.
    Poll(0);
    }
  else
    ESP_LOGE(tag, "Timeout queueing message in ConsoleTelnet::Receive\n");
  mbuf_remove(event.mbuf, event.mbuf->len);
  }

void ConsoleTelnet::HandleDeviceEvent(void* pEvent)
  {
  Event event = *(Event*)pEvent;
  switch (event.type)
    {
    case RECV:
      telnet_recv(m_telnet, event.mbuf->buf, event.mbuf->len);
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
  switch (event->type)
    {
    case TELNET_EV_SEND:
      mg_send(m_connection, event->data.buffer, event->data.size);
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
  printf("logout\n");
  m_connection->flags |= MG_F_SEND_AND_CLOSE;
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
  if (!m_telnet || (m_connection->flags & MG_F_SEND_AND_CLOSE))
    return 0;
  telnet_send_text(m_telnet, (const char*)buf, nbyte);
  return nbyte;
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
