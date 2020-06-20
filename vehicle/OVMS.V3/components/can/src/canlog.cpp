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
static const char *TAG = "canlog";

#include "can.h"
#include "canlog.h"
#include <sys/param.h>
#include <ctype.h>
#include <string.h>
#include <string>
#include <sstream>
#include <iomanip>
#include "ovms_utils.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"

////////////////////////////////////////////////////////////////////////
// Command Processing
////////////////////////////////////////////////////////////////////////

void can_log_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyCan.HasLogger())
    {
    writer->puts("Error: No loggers running");
    return;
    }

  if (argc==0)
    {
    // Stop all loggers
    writer->puts("Stopping all loggers");
    MyCan.RemoveLoggers();
    return;
    }

  if (MyCan.RemoveLogger(atoi(argv[0])))
    {
    writer->puts("Stopped logger");
    }
  else
    {
    writer->puts("Error: Cannot find specified logger");
    }
  }

void can_log_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyCan.HasLogger())
    {
    writer->puts("CAN logging inactive");
    return;
    }

  if (argc>0)
    {
    canlog* cl = MyCan.GetLogger(atoi(argv[0]));
    if (cl)
      {
      writer->printf("CAN logging active: %s\n  Statistics: %s\n", cl->GetInfo().c_str(), cl->GetStats().c_str());
      }
    else
      {
      writer->puts("Error: Cannot find specified can logger");
      }
    return;
    }
  else
    {
    // Show the status of all loggers
    OvmsMutexLock lock(&MyCan.m_loggermap_mutex);
    for (can::canlog_map_t::iterator it=MyCan.m_loggermap.begin(); it!=MyCan.m_loggermap.end(); ++it)
      {
      canlog* cl = it->second;
      writer->printf("CAN logger #%d: %s\n  Statistics: %s\n",
        it->first, cl->GetInfo().c_str(), cl->GetStats().c_str());
      }
    }
  }

void can_log_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyCan.HasLogger())
    {
    writer->puts("CAN logging inactive");
    return;
    }
  else
    {
    // Show the list of all loggers
    OvmsMutexLock lock(&MyCan.m_loggermap_mutex);
    for (can::canlog_map_t::iterator it=MyCan.m_loggermap.begin(); it!=MyCan.m_loggermap.end(); ++it)
      {
      canlog* cl = it->second;
      writer->printf("#%d: %s\n", it->first, cl->GetInfo().c_str());
      }
    }
  }

////////////////////////////////////////////////////////////////////////
// CAN Logging System initialisation
////////////////////////////////////////////////////////////////////////

class OvmsCanLogInit
  {
  public: OvmsCanLogInit();
} MyOvmsCanLogInit  __attribute__ ((init_priority (4550)));

OvmsCanLogInit::OvmsCanLogInit()
  {
  ESP_LOGI(TAG, "Initialising CAN logging (4550)");

  OvmsCommand* cmd_can = MyCommandApp.FindCommand("can");
  if (cmd_can == NULL)
    {
    ESP_LOGE(TAG,"Cannot find CAN command - aborting log command registration");
    return;
    }

  OvmsCommand* cmd_canlog = cmd_can->RegisterCommand("log", "CAN logging framework");
  cmd_canlog->RegisterCommand("stop", "Stop logging", can_log_stop,"[<id>]",0,1);
  cmd_canlog->RegisterCommand("status", "Logging status", can_log_status,"[<id>]",0,1);
  cmd_canlog->RegisterCommand("list", "Logging list", can_log_list);
  cmd_canlog->RegisterCommand("start", "CAN logging start framework");
  }

////////////////////////////////////////////////////////////////////////
// CAN Logger class
////////////////////////////////////////////////////////////////////////

canlog::canlog(const char* type, std::string format, canformat::canformat_serve_mode_t mode)
  {
  m_type = type;
  m_format = format;
  m_formatter = MyCanFormatFactory.NewFormat(format.c_str());
  m_formatter->SetServeMode(mode);
  m_filter = NULL;

  m_msgcount = 0;
  m_dropcount = 0;
  m_filtercount = 0;

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(IDTAG, "*", std::bind(&canlog::EventListener, this, _1, _2));

  int queuesize = MyConfig.GetParamValueInt("can", "log.queuesize",100);
  m_queue = xQueueCreate(queuesize, sizeof(CAN_log_message_t));
  xTaskCreatePinnedToCore(RxTask, "OVMS CanLog", 4096, (void*)this, 10, &m_task, CORE(1));
  }

canlog::~canlog()
  {
  MyEvents.DeregisterEvent(IDTAG);

  if (m_task)
    {
    TaskHandle_t t = m_task;
    m_task = NULL;

    vTaskDelete(t);
    }

  if (m_queue)
    {
    QueueHandle_t q = m_queue;
    m_queue = NULL;

    CAN_log_message_t msg;
    while (xQueueReceive(q, &msg, 0) == pdTRUE)
      {
      switch (msg.type)
        {
        case CAN_LogInfo_Comment:
        case CAN_LogInfo_Config:
        case CAN_LogInfo_Event:
          free(msg.text);
          break;
        default:
          break;
        }
      }
    vQueueDelete(q);
    }

  if (m_formatter)
    {
    delete m_formatter;
    m_formatter = NULL;
    }

  if (m_filter)
    {
    delete m_filter;
    m_filter = NULL;
    }
  }

void canlog::RxTask(void *context)
  {
  canlog* me = (canlog*) context;
  CAN_log_message_t msg;
  while (1)
    {
    if (xQueueReceive(me->m_queue, &msg, (portTickType)portMAX_DELAY) == pdTRUE)
      {
      switch (msg.type)
        {
        case CAN_LogInfo_Comment:
        case CAN_LogInfo_Config:
        case CAN_LogInfo_Event:
          me->OutputMsg(msg);
          free(msg.text);
          break;
        default:
          me->OutputMsg(msg);
          break;
        }
      }
    }
  }

void canlog::EventListener(std::string event, void* data)
  {
  if (startsWith(event, "vehicle"))
    LogInfo(NULL, CAN_LogInfo_Event, event.c_str());
  }

const char* canlog::GetType()
  {
  return m_type;
  }

const char* canlog::GetFormat()
  {
  return m_format.c_str();
  }

void canlog::OutputMsg(CAN_log_message_t& msg)
  {
  }

std::string canlog::GetInfo()
  {
  std::ostringstream buf;

  buf << "Type:" << m_type << " Format:" << m_format;
  if (m_formatter)
    {
    buf << "(" << m_formatter->GetServeModeName() << ")";
    }

  if (m_filter)
    {
    buf << " Filter:" << m_filter->Info();
    }
  else
    {
    buf << " Filter:off";
    }

  buf << " Vehicle:" << StdMetrics.ms_v_type->AsString();

  return buf.str();
  }

std::string canlog::GetStats()
  {
  std::ostringstream buf;

  float droprate = (m_msgcount > 0) ? ((float) m_dropcount/m_msgcount*100) : 0;
  uint32_t waiting = uxQueueMessagesWaiting(m_queue);

  buf << "total messages: " << m_msgcount
    << ", dropped: " << m_dropcount
    << ", filtered: " << m_filtercount
    << " = " << std::fixed << std::setprecision(1) << droprate << "%";

  if (waiting > 0)
    buf << ", waiting: " << waiting;

  return buf.str();
  }

void canlog::SetFilter(canfilter* filter)
  {
  if (m_filter)
    {
    delete m_filter;
    }
  m_filter = filter;
  }

void canlog::ClearFilter()
  {
  if (m_filter)
    {
    delete m_filter;
    m_filter = NULL;
    }
  }

void canlog::LogFrame(canbus* bus, CAN_log_type_t type, const CAN_frame_t* frame)
  {
  if (!IsOpen() || !bus || !frame) return;

  if (((m_filter == NULL)||(m_filter->IsFiltered(frame)))&&(m_queue))
    {
    CAN_log_message_t msg;
    msg.type = type;
    gettimeofday(&msg.timestamp,NULL);
    memcpy(&msg.frame,frame,sizeof(CAN_frame_t));
    msg.frame.origin = bus;
    m_msgcount++;
    if (xQueueSend(m_queue, &msg, 0) != pdTRUE) m_dropcount++;
    }
  else
    {
    m_filtercount++;
    }
  }

void canlog::LogStatus(canbus* bus, CAN_log_type_t type, const CAN_status_t* status)
  {
  if (!IsOpen() || !bus) return;

  if (((m_filter == NULL)||(m_filter->IsFiltered(bus)))&&(m_queue))
    {
    CAN_log_message_t msg;
    msg.type = type;
    gettimeofday(&msg.timestamp,NULL);
    msg.origin = bus;
    memcpy(&msg.status,status,sizeof(CAN_status_t));
    m_msgcount++;
    if (xQueueSend(m_queue, &msg, 0) != pdTRUE) m_dropcount++;
    }
  else
    {
    m_filtercount++;
    }
  }

void canlog::LogInfo(canbus* bus, CAN_log_type_t type, const char* text)
  {
  if (!IsOpen() || !text) return;

  if (((m_filter == NULL)||(m_filter->IsFiltered(bus)))&&(m_queue))
    {
    CAN_log_message_t msg;
    msg.type = type;
    gettimeofday(&msg.timestamp,NULL);
    msg.origin = bus;
    msg.text = strdup(text);
    m_msgcount++;
    if (xQueueSend(m_queue, &msg, 0) != pdTRUE) m_dropcount++;
    }
  else
    {
    m_filtercount++;
    }
  }
