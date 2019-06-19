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

#include "ovms_log.h"
static const char *TAG = "canlog-tcpclient";

#include "can.h"
#include "canformat.h"
#include "canlog_tcpclient.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"

void can_log_tcpclient_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string format(cmd->GetName());
  canlog_tcpclient* logger = new canlog_tcpclient(argv[0],format);
  logger->Open();

  if (logger->IsOpen())
    {
    if (argc>1)
      { MyCan.SetLogger(logger, argc-1, &argv[1]); }
    else
      { MyCan.SetLogger(logger); }
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
  public: OvmsCanLogTcpClientInit();
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
        MyCanFormatFactory.RegisterCommandSet(start, "Start CAN logging as TCP client",
          can_log_tcpclient_start,
          "<format> <host:port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        }
      }
    }
  }

canlog_tcpclient::canlog_tcpclient(std::string path, std::string format)
  : canlog("tcpclient", format)
  {
  m_path = path;
  }

canlog_tcpclient::~canlog_tcpclient()
  {
  Close();
  }

bool canlog_tcpclient::Open()
  {
  //ESP_LOGI(TAG, "Now logging CAN messages as a tcp client to %s",m_path.c_str());
  //std::string header = m_formatter->getheader();
  //if (header.length()>0)
  //  { ESP_LOGD(TAG,"%s",header.c_str()); }

  ESP_LOGE(TAG, "Not yet implemented");
  return false;
  }

void canlog_tcpclient::Close()
  {
  ESP_LOGI(TAG, "Closed TCP client log: %s", GetStats().c_str());
  }

bool canlog_tcpclient::IsOpen()
  {
  return false;
  }

std::string canlog_tcpclient::GetInfo()
  {
  std::string result = canlog::GetInfo();
  result.append(" Path:");
  result.append(m_path);
  return result;
  }

void canlog_tcpclient::OutputMsg(CAN_log_message_t& msg)
  {
  if (m_formatter == NULL) return;
  }
