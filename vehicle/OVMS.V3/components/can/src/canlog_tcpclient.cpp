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
static const char *TAG = "canlog-tcpclient";

#include "can.h"
#include "canformat.h"
#include "canlog_tcpclient.h"
#include "ovms_config.h"

canlog_tcpclient* MyCanLogTcpClient = NULL;

void can_log_tcpclient_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string format(cmd->GetName());
  std::string mode(cmd->GetParent()->GetName());
  canlog_tcpclient* logger = new canlog_tcpclient(argv[0],format,GetFormatModeType(mode));
  logger->Open();

  if (logger->IsOpen())
    {
    if (argc>1)
      { MyCan.AddLogger(logger, argc-1, &argv[1]); }
    else
      { MyCan.AddLogger(logger); }
    writer->printf("CAN logging as TCP client: %s\n", logger->GetInfo().c_str());
    }
  else
    {
    writer->printf("Error: Could not start CAN logging as TCP client: %s\n",logger->GetInfo().c_str());
    delete logger;
    }
  }

class OvmsCanLogTcpClientInit
  {
  public:
    OvmsCanLogTcpClientInit();
    void NetManInit(std::string event, void* data);
    void NetManStop(std::string event, void* data);
  } MyOvmsCanLogTcpClientInit  __attribute__ ((init_priority (4560)));

OvmsCanLogTcpClientInit::OvmsCanLogTcpClientInit()
  {
  ESP_LOGI(TAG, "Initialising CAN logging as TCP client (4560)");

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
        OvmsCommand* start = cmd_can_log_start->RegisterCommand("tcpclient", "CAN logging as TCP client");
        OvmsCommand* discard = start->RegisterCommand("discard", "CAN logging as TCP client (discard mode)");
        OvmsCommand* simulate = start->RegisterCommand("simulate", "CAN logging as TCP client (simulate mode)");
        OvmsCommand* transmit = start->RegisterCommand("transmit", "CAN logging as TCP client (transmit mode)");
        MyCanFormatFactory.RegisterCommandSet(discard, "Start CAN logging as TCP client (discard mode)",
          can_log_tcpclient_start,
          "<host:port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        MyCanFormatFactory.RegisterCommandSet(simulate, "Start CAN logging as TCP client (simulate mode)",
          can_log_tcpclient_start,
          "<host:port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        MyCanFormatFactory.RegisterCommandSet(transmit, "Start CAN logging as TCP client (transmit mode)",
          can_log_tcpclient_start,
          "<host:port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        }
      }
    }
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "network.mgr.init", std::bind(&OvmsCanLogTcpClientInit::NetManInit, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "network.mgr.stop", std::bind(&OvmsCanLogTcpClientInit::NetManStop, this, _1, _2));
  }

void OvmsCanLogTcpClientInit::NetManInit(std::string event, void* data)
  {
  if (MyCanLogTcpClient) MyCanLogTcpClient->Open();
  }

void OvmsCanLogTcpClientInit::NetManStop(std::string event, void* data)
  {
  if (MyCanLogTcpClient) MyCanLogTcpClient->Close();
  }

static void tcMongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  if (MyCanLogTcpClient)
    MyCanLogTcpClient->MongooseHandler(nc, ev, p);
  else if (ev == MG_EV_ACCEPT)
    {
    ESP_LOGI(TAG, "Log service connection rejected (logger not running)");
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    }
  }

canlog_tcpclient::canlog_tcpclient(std::string path, std::string format, canformat::canformat_serve_mode_t mode)
  : canlog("tcpclient", format, mode)
  {
  MyCanLogTcpClient = this;
  m_isopen = false;
  m_path = path;
  }

canlog_tcpclient::~canlog_tcpclient()
  {
  Close();
  MyCanLogTcpClient = NULL;
  }

bool canlog_tcpclient::Open()
  {
  if (m_isopen) return true;

  auto mglock = MongooseLock();
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  if (mgr != NULL)
    {
    if (MyNetManager.m_network_any)
      {
      ESP_LOGI(TAG, "Launching TCP client to %s",m_path.c_str());
      struct mg_connect_opts opts;
      const char* err;
      memset(&opts, 0, sizeof(opts));
      opts.error_string = &err;
      if (mg_connect_opt(mgr, m_path.c_str(), tcMongooseHandler, opts) != NULL)
        {
        // Wait 10s max for connection establishment...
        m_connecting.Take(pdMS_TO_TICKS(10*1000));
        return m_isopen;
        }
      else
        {
        ESP_LOGE(TAG, "Could not connect to %s", m_path.c_str());
        return false;
        }
      }
    else
      {
      ESP_LOGI(TAG, "Delay TCP client (as network manager not up)");
      return true;
      }
    }
  else
    {
    ESP_LOGE(TAG, "Network manager is not available");
    return false;
    }
  }

void canlog_tcpclient::Close()
  {
  if (m_isopen)
    {
    ESP_LOGI(TAG, "Closed TCP client log: %s", GetStats().c_str());
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
    m_isopen = false;
    }
  }

std::string canlog_tcpclient::GetInfo()
  {
  std::string result = canlog::GetInfo();
  result.append(" Path:");
  result.append(m_path);
  return result;
  }

void canlog_tcpclient::MongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  switch (ev)
    {
    case MG_EV_CONNECT:
      {
      int *success = (int*)p;
      ESP_LOGV(TAG, "MongooseHandler(MG_EV_CONNECT=%d)",*success);
      if (*success == 0)
        { // Connection successful
        OvmsRecMutexLock lock(&m_cmmutex);
        ESP_LOGI(TAG, "Connection successful to %s", m_path.c_str());
        canlogconnection* clc = new canlogconnection(this, m_format, m_mode);
        clc->m_nc = nc;
        clc->m_peer = m_path;
        m_connmap[nc] = clc;
        m_isopen = true;
        std::string result = clc->m_formatter->getheader();
        if (result.length()>0)
          {
          mg_send(nc, (const char*)result.c_str(), result.length());
          }
        }
      else
        { // Connection failed
        ESP_LOGE(TAG, "Connection failed to %s", m_path.c_str());
        m_isopen = false;
        }
      m_connecting.Give();
      }
      break;
    case MG_EV_CLOSE:
      ESP_LOGV(TAG, "MongooseHandler(MG_EV_CLOSE)");
      if (m_isopen)
        {
        OvmsRecMutexLock lock(&m_cmmutex);
        ESP_LOGE(TAG, "Disconnected from %s", m_path.c_str());
        m_isopen = false;
        auto k = m_connmap.find(nc);
        if (k != m_connmap.end())
          {
          delete k->second;
          m_connmap.erase(k);
          }
        }
      break;
    case MG_EV_RECV:
      {
      ESP_LOGV(TAG, "MongooseHandler(MG_EV_RECV)");
      size_t used = nc->recv_mbuf.len;
      if (m_formatter != NULL)
        {
        OvmsRecMutexLock lock(&m_cmmutex);
        canlogconnection* clc = NULL;
        auto k = m_connmap.find(nc);
        if (k != m_connmap.end()) clc = k->second;
        used = m_formatter->Serve((uint8_t*)nc->recv_mbuf.buf, used, clc);
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
