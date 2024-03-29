/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN logging framework
;    Date:          18th January 2018
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

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

#include "ovms_log.h"
static const char *TAG = "canlog-udpserver";

#include "can.h"
#include "canformat.h"
#include "canlog_udpserver.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_peripherals.h"

#define UDP_TIMEOUT 30

canlog_udpserver* MyCanLogUdpServer = NULL;

udpcanlogconnection::udpcanlogconnection(canlog* logger, std::string format, canformat::canformat_serve_mode_t mode)
  : canlogconnection(logger, format, mode)
  {
  m_timeout = monotonictime + UDP_TIMEOUT;
  }

udpcanlogconnection::~udpcanlogconnection()
  {
  }

void udpcanlogconnection::OutputMsg(CAN_log_message_t& msg, std::string &result)
  {
  m_msgcount++;

  if ((m_filters != NULL) && (! m_filters->IsFiltered(&msg.frame)))
    {
    m_filtercount++;
    return;
    }

  if (result.length()>0)
    {
    sendto(m_sock, (const char*)result.c_str(), result.length(), 0, &m_sa, sizeof(m_sa));
    }
  }

void udpcanlogconnection::Tickle()
  {
  m_timeout = monotonictime + UDP_TIMEOUT;
  }

void can_log_udpserver_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string format(cmd->GetName());
  std::string mode(cmd->GetParent()->GetName());
  canlog_udpserver* logger = new canlog_udpserver(argv[0],format,GetFormatModeType(mode));

  if (logger->Open())
    {
    if (argc>1)
      { MyCan.AddLogger(logger, argc-1, &argv[1]); }
    else
      { MyCan.AddLogger(logger); }
    writer->printf("CAN logging as UDP server: %s\n", logger->GetInfo().c_str());
    }
  else
    {
    writer->printf("Error: Could not start CAN logging as UDP server: %s\n",logger->GetInfo().c_str());
    delete logger;
    }
  }

class OvmsCanLogUdpServerInit
  {
  public:
    OvmsCanLogUdpServerInit();
    void NetManInit(std::string event, void* data);
    void NetManStop(std::string event, void* data);
  } MyOvmsCanLogUdpServerInit  __attribute__ ((init_priority (4560)));

OvmsCanLogUdpServerInit::OvmsCanLogUdpServerInit()
  {
  ESP_LOGI(TAG, "Initialising CAN logging as UDP server (4560)");

  OvmsCommand* cmd_can = MyCommandApp.FindCommand("can");
  if (cmd_can)
    {
    OvmsCommand* cmd_can_log = cmd_can->FindCommand("log");
    if (cmd_can_log)
      {
      OvmsCommand* cmd_can_log_start = cmd_can_log->FindCommand("start");
      if (cmd_can_log_start)
        {
        // We have a place to put our command tree..
        OvmsCommand* start = cmd_can_log_start->RegisterCommand("udpserver", "CAN logging as UDP server");
        OvmsCommand* discard = start->RegisterCommand("discard","CAN logging as UDP server (discard mode)");
        OvmsCommand* simulate = start->RegisterCommand("simulate","CAN logging as UDP server (simulate mode)");
        OvmsCommand* transmit = start->RegisterCommand("transmit","CAN logging as UDP server (transmit mode)");
        MyCanFormatFactory.RegisterCommandSet(discard, "Start CAN logging as UDP server (discard mode)",
          can_log_udpserver_start,
          "<port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        MyCanFormatFactory.RegisterCommandSet(simulate, "Start CAN logging as UDP server (simulate mode)",
          can_log_udpserver_start,
          "<port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        MyCanFormatFactory.RegisterCommandSet(transmit, "Start CAN logging as UDP server (transmit mode)",
          can_log_udpserver_start,
          "<port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        }
      }
    }

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "network.mgr.init", std::bind(&OvmsCanLogUdpServerInit::NetManInit, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "network.mgr.stop", std::bind(&OvmsCanLogUdpServerInit::NetManStop, this, _1, _2));
  }

void OvmsCanLogUdpServerInit::NetManInit(std::string event, void* data)
  {
  if (MyCanLogUdpServer) MyCanLogUdpServer->Open();
  }

void OvmsCanLogUdpServerInit::NetManStop(std::string event, void* data)
  {
  if (MyCanLogUdpServer) MyCanLogUdpServer->Close();
  }

static void tsMongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  if (MyCanLogUdpServer)
    MyCanLogUdpServer->MongooseHandler(nc, ev, p);
  else if (ev == MG_EV_ACCEPT)
    {
    ESP_LOGI(TAG, "Log service connection rejected (logger not running)");
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    }
  }

canlog_udpserver::canlog_udpserver(std::string path, std::string format, canformat::canformat_serve_mode_t mode)
  : canlog("udpserver", format, mode)
  {
  MyCanLogUdpServer = this;
  m_path = path;
  if (m_path.find(':') == std::string::npos)
    {
    m_path.insert(0, "udp://");
    }
  m_isopen = false;
  m_mgconn = NULL;

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.10", std::bind(&canlog_udpserver::Ticker, this, _1, _2));
  }

canlog_udpserver::~canlog_udpserver()
  {
  Close();
  MyCanLogUdpServer = NULL;
  MyEvents.DeregisterEvent(TAG);
  }

bool canlog_udpserver::Open()
  {
  if (m_isopen) return true;

  ESP_LOGI(TAG, "Launching UDP server at %s",m_path.c_str());
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  if (mgr != NULL)
    {
    if (MyNetManager.m_network_any)
      {
      m_mgconn = mg_bind(mgr, m_path.c_str(), tsMongooseHandler);
      if (m_mgconn != NULL)
        {
        ESP_LOGI(TAG,"Listening with nc %p",m_mgconn);
        m_isopen = true;
        return true;
        }
      else
        {
        ESP_LOGE(TAG,"Could not listen on %s",m_path.c_str());
        return false;
        }
      }
    else
      {
      ESP_LOGI(TAG,"Delay UDP server (as network manager not up)");
      return true;
      }
    }
  else
    {
    ESP_LOGE(TAG,"Network manager is not available");
    return false;
    }
  }

void canlog_udpserver::Close()
  {
  if (m_isopen)
    {
    if (m_connmap.size() > 0)
      {
      OvmsRecMutexLock lock(&m_cmmutex);
      for (conn_map_t::iterator it=m_connmap.begin(); it!=m_connmap.end(); ++it)
        {
        it->first->flags |= MG_F_CLOSE_IMMEDIATELY;
        delete it->second;
        }
      m_connmap.clear();
      }
    ESP_LOGI(TAG, "Closed UDP server log: %s", GetStats().c_str());
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    m_isopen = false;
    }
  }

std::string canlog_udpserver::GetInfo()
  {
  std::string result = canlog::GetInfo();
  result.append(" Path:");
  result.append(m_path);
  return result;
  }

void canlog_udpserver::Ticker(std::string event, void* data)
  {
  OvmsRecMutexLock lock(&m_cmmutex);

  for (conn_map_t::iterator it=m_connmap.begin(); it!=m_connmap.end(); ++it)
    {
    udpcanlogconnection* clc = (udpcanlogconnection*)it->second;
    if (clc->m_timeout < monotonictime)
      {
      // This client has timed out
      ESP_LOGD(TAG,"Timed out connection from %s",clc->m_peer.c_str());
      m_connmap.erase(it);
      delete clc;
      }
    }
  }

void canlog_udpserver::MongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  char addr[32];

  switch (ev)
    {
    case MG_EV_RECV:
      {
      OvmsRecMutexLock lock(&m_cmmutex);
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP|MG_SOCK_STRINGIFY_PORT);

      size_t used = nc->recv_mbuf.len;

      // Try to find an existing matching connection
      udpcanlogconnection* clc = NULL;
      for (conn_map_t::iterator it=m_connmap.begin(); it!=m_connmap.end(); ++it)
        {
        clc = (udpcanlogconnection*)it->second;
        if (memcmp(&clc->m_sa, &nc->sa, sizeof(&nc->sa.sin)) == 0)
          {
          // We have found an existing one, so just handle and tickle it
          ESP_LOGD(TAG,"Tickle connection from %s",addr);
          used = clc->m_formatter->Serve((uint8_t*)nc->recv_mbuf.buf, used, clc);
          if (used>0) mbuf_remove(&nc->recv_mbuf, used);
          clc->Tickle();
          return;
          }
        }

      // No matching connection, so make a new one
      ESP_LOGD(TAG,"New connection from %s",addr);
      clc = new udpcanlogconnection(this, m_format, m_mode);
      clc->m_nc = m_mgconn;
      clc->m_sock = m_mgconn->sock;
      memcpy(&clc->m_sa,&nc->sa.sin,sizeof(nc->sa.sin));
      clc->m_peer = std::string(addr);
      m_connmap[&clc->m_fakenc] = clc;
      std::string result = clc->m_formatter->getheader();
      if (result.length()>0)
        {
        mg_send(nc, (const char*)result.c_str(), result.length());
        }
      used = clc->m_formatter->Serve((uint8_t*)nc->recv_mbuf.buf, used, clc);
      if (used > 0) mbuf_remove(&nc->recv_mbuf, used);

      break;
      }
    default:
      break;
    }
  }

#endif // #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
