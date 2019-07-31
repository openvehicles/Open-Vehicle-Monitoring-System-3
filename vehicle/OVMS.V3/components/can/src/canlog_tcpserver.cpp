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
static const char *TAG = "canlog-tcpserver";

#include "can.h"
#include "canformat.h"
#include "canlog_tcpserver.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"

canlog_tcpserver* MyCanLogTcpServer = NULL;

void can_log_tcpserver_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string format(cmd->GetName());
  std::string mode(cmd->GetParent()->GetName());
  canlog_tcpserver* logger = new canlog_tcpserver(argv[0],format,GetFormatModeType(mode));
  logger->Open();

  if (logger->IsOpen())
    {
    if (argc>1)
      { MyCan.AddLogger(logger, argc-1, &argv[1]); }
    else
      { MyCan.AddLogger(logger); }
    writer->printf("CAN logging as TCP server: %s\n", logger->GetInfo().c_str());
    }
  else
    {
    writer->printf("Error: Could not start CAN logging as TCP server: %s\n",logger->GetInfo().c_str());
    delete logger;
    }
  }

class OvmsCanLogTcpServerInit
  {
  public:
    OvmsCanLogTcpServerInit();
    void NetManInit(std::string event, void* data);
    void NetManStop(std::string event, void* data);
  } MyOvmsCanLogTcpServerInit  __attribute__ ((init_priority (4560)));

OvmsCanLogTcpServerInit::OvmsCanLogTcpServerInit()
  {
  ESP_LOGI(TAG, "Initialising CAN logging as TCP server (4560)");

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
        OvmsCommand* start = cmd_can_log_start->RegisterCommand("tcpserver", "CAN logging as TCP server");
        OvmsCommand* discard = start->RegisterCommand("discard","CAN logging as TCP server (discard mode)");
        OvmsCommand* simulate = start->RegisterCommand("simulate","CAN logging as TCP server (simulate mode)");
        OvmsCommand* transmit = start->RegisterCommand("transmit","CAN logging as TCP server (transmit mode)");
        MyCanFormatFactory.RegisterCommandSet(discard, "Start CAN logging as TCP server (discard mode)",
          can_log_tcpserver_start,
          "<port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        MyCanFormatFactory.RegisterCommandSet(simulate, "Start CAN logging as TCP server (simulate mode)",
          can_log_tcpserver_start,
          "<port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        MyCanFormatFactory.RegisterCommandSet(transmit, "Start CAN logging as TCP server (transmit mode)",
          can_log_tcpserver_start,
          "<port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        }
      }
    }

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "network.mgr.init", std::bind(&OvmsCanLogTcpServerInit::NetManInit, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "network.mgr.stop", std::bind(&OvmsCanLogTcpServerInit::NetManStop, this, _1, _2));
  }

void OvmsCanLogTcpServerInit::NetManInit(std::string event, void* data)
  {
  if (MyCanLogTcpServer) MyCanLogTcpServer->Open();
  }

void OvmsCanLogTcpServerInit::NetManStop(std::string event, void* data)
  {
  if (MyCanLogTcpServer) MyCanLogTcpServer->Close();
  }

static void tsMongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  if (MyCanLogTcpServer)
    MyCanLogTcpServer->MongooseHandler(nc, ev, p);
  else if (ev == MG_EV_ACCEPT)
    {
    ESP_LOGI(TAG, "Log service connection rejected (logger not running)");
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    }
  }

void tsPutCallback(uint8_t *buffer, size_t len, void* data)
  {
  struct mg_connection *nc = (struct mg_connection *)data;

  //ESP_LOGD(TAG,"Sent %d bytes of put callback response",len);
  mg_send(nc, buffer, len);
  }

canlog_tcpserver::canlog_tcpserver(std::string path, std::string format, canformat::canformat_serve_mode_t mode)
  : canlog("tcpserver", format, mode)
  {
  MyCanLogTcpServer = this;
  m_path = path;
  if (m_path.find(':') == std::string::npos)
    {
    m_path.append(":3000");
    }
  m_isopen = false;
  m_mgconn = NULL;

  if (m_formatter)
    {
    m_formatter->SetPutCallback(tsPutCallback);
    }
  }

canlog_tcpserver::~canlog_tcpserver()
  {
  Close();
  MyCanLogTcpServer = NULL;
  }

bool canlog_tcpserver::Open()
  {
  if (m_isopen) return true;

  ESP_LOGI(TAG, "Launching TCP server at %s",m_path.c_str());
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  if (mgr != NULL)
    {
    if (MyNetManager.m_network_any)
      {
      m_mgconn = mg_bind(mgr, m_path.c_str(), tsMongooseHandler);
      if (m_mgconn != NULL)
        {
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
      ESP_LOGI(TAG,"Delay TCP server (as network manager not up)");
      return true;
      }
    }
  else
    {
    ESP_LOGE(TAG,"Network manager is not available");
    return false;
    }
  }

void canlog_tcpserver::Close()
  {
  if (m_isopen)
    {
    if (m_smap.size() > 0)
      {
      for (ts_map_t::iterator it=m_smap.begin(); it!=m_smap.end(); ++it)
        {
        it->first->flags |= MG_F_CLOSE_IMMEDIATELY;
        }
      m_smap.clear();
      }
    ESP_LOGI(TAG, "Closed TCP server log: %s", GetStats().c_str());
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    m_isopen = false;
    }
  }

bool canlog_tcpserver::IsOpen()
  {
  return m_isopen;
  }

std::string canlog_tcpserver::GetInfo()
  {
  std::string result = canlog::GetInfo();
  result.append(" Path:");
  result.append(m_path);
  return result;
  }

void canlog_tcpserver::OutputMsg(CAN_log_message_t& msg)
  {
  if (m_formatter == NULL) return;

  std::string result = m_formatter->get(&msg);
  if (result.length()>0)
    {
    OvmsMutexLock lock(&m_mgmutex);
    for (ts_map_t::iterator it=m_smap.begin(); it!=m_smap.end(); ++it)
      {
      if (it->first->send_mbuf.len < 4096)
        {
        // Limit to 4KB queue on output buffer
        mg_send(it->first, (const char*)result.c_str(), result.length());
        }
      else
        {
        m_dropcount++;
        }
      }
    }
  }

void canlog_tcpserver::MongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  char addr[32];

  OvmsMutexLock lock(&m_mgmutex);

  switch (ev)
    {
    case MG_EV_ACCEPT:
      {
      // New network connection has arrived
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP);
      ESP_LOGI(TAG, "Log service connection from %s",addr);
      m_smap[nc] = 1;
      if (m_formatter != NULL)
        {
        std::string result = m_formatter->getheader();
        if (result.length()>0)
          {
          mg_send(nc, (const char*)result.c_str(), result.length());
          }
        }
      break;
      }

    case MG_EV_CLOSE:
      {
      // Network connection has gone
      auto k = m_smap.find(nc);
      if (k != m_smap.end())
        {
        mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP);
        ESP_LOGI(TAG, "Log service disconnection from %s",addr);
        m_smap.erase(k);
        }
      break;
      }

    case MG_EV_RECV:
      {
      // Receive data on the network connection
      size_t used = nc->recv_mbuf.len;
      //ESP_LOGD(TAG,"Received %d bytes of data",used);
      if (m_formatter != NULL)
        {
        used = m_formatter->Serve((uint8_t*)nc->recv_mbuf.buf, used, nc);
        }
      if (used > 0)
        {
        mbuf_remove(&nc->recv_mbuf, used);
        }
      break;
      }

    default:
      break;
    }
  }

#endif // #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
