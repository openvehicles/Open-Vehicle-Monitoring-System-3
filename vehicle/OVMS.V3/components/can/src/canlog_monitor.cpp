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
static const char *TAG = "canlog-monitor";

#include "can.h"
#include "canlog_monitor.h"

void can_log_monitor_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string format(cmd->GetName());
  canlog_monitor* logger = new canlog_monitor(format);
  logger->Open();
  MyCan.SetLogger(logger, argc, argv);

  writer->printf("CAN logging to MONITOR active: %s\n", logger->GetInfo().c_str());
  writer->puts("Note: info logging is at debug log level, frame logging is at verbose, and errors as usual");
  }

class OvmsCanLogMonitorInit
  {
  public: OvmsCanLogMonitorInit();
} MyOvmsCanLogMonitorInit  __attribute__ ((init_priority (4560)));

OvmsCanLogMonitorInit::OvmsCanLogMonitorInit()
  {
  ESP_LOGI(TAG, "Initialising CAN logging to MONITOR (4560)");

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
        OvmsCommand* start = cmd_can_log_start->RegisterCommand("monitor", "CAN logging to MONITOR");
        MyCanFormatFactory.RegisterCommandSet(start, "Start CAN logging to MONITOR",
          can_log_monitor_start,
          "<format> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          0, 9);
        }
      }
    }
  }

canlog_monitor::canlog_monitor(std::string format)
  : canlog("monitor", format)
  {
  }

canlog_monitor::~canlog_monitor()
  {
  }

bool canlog_monitor::Open()
  {
  ESP_LOGI(TAG, "Now logging CAN messages to monitor");

  std::string header = m_formatter->getheader();
  if (header.length()>0)
    { ESP_LOGD(TAG,"%s",header.c_str()); }

  return true;
  }

void canlog_monitor::Close()
  {
  ESP_LOGI(TAG, "Closed monitor log: %s", GetStats().c_str());
  }

bool canlog_monitor::IsOpen()
  {
  return true;
  }

void canlog_monitor::OutputMsg(CAN_log_message_t& msg)
  {
  if (m_formatter == NULL) return;

  std::string result = m_formatter->get(&msg);
  if (result.length()>0)
    {
    switch (msg.type)
      {
      case CAN_LogFrame_RX:
      case CAN_LogFrame_TX:
      case CAN_LogFrame_TX_Queue:
      case CAN_LogFrame_TX_Fail:
        ESP_LOGV(TAG,"%s",result.c_str());
        break;
      case CAN_LogStatus_Error:
        ESP_LOGE(TAG,"%s",result.c_str());
        break;
      case CAN_LogStatus_Statistics:
      case CAN_LogInfo_Comment:
      case CAN_LogInfo_Config:
      case CAN_LogInfo_Event:
        ESP_LOGD(TAG,"%s",result.c_str());
        break;
      default:
        break;
      }
    }
  }
