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
static const char *TAG = "canlog-tcpserver";

#include "can.h"
#include "canformat.h"
#include "canlog_tcpserver.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"

void can_log_tcpserver_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string format(cmd->GetName());
  canlog_tcpserver* logger = new canlog_tcpserver(argv[0],format);
  logger->Open();

  if (logger->IsOpen())
    {
    if (argc>1)
      { MyCan.SetLogger(logger, argc-1, &argv[1]); }
    else
      { MyCan.SetLogger(logger); }
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
  public: OvmsCanLogTcpServerInit();
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
        MyCanFormatFactory.RegisterCommandSet(start, "Start CAN logging as TCP server",
          can_log_tcpserver_start,
          "<format> <port> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        }
      }
    }
  }

canlog_tcpserver::canlog_tcpserver(std::string path, std::string format)
  : canlog("tcpserver", format)
  {
  m_path = path;
  }

canlog_tcpserver::~canlog_tcpserver()
  {
  Close();
  }

bool canlog_tcpserver::Open()
  {
  //ESP_LOGI(TAG, "Now logging CAN messages as a tcp server on %s",m_path.c_str());
  //std::string header = m_formatter->getheader();
  //if (header.length()>0)
  //  { ESP_LOGD(TAG,"%s",header.c_str()); }

  ESP_LOGE(TAG, "Not yet implemented");
  return false;
  }

void canlog_tcpserver::Close()
  {
  ESP_LOGI(TAG, "Closed TCP server log: %s", GetStats().c_str());
  }

bool canlog_tcpserver::IsOpen()
  {
  return false;
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
  }
