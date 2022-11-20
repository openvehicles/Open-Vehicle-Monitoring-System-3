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

static const char *CAN_PARAM = "can";

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
      writer->printf("#%d: %s %s %s",
        atoi(argv[0]),
        (cl->m_isopen)?"open":"closed",
        cl->GetInfo().c_str(), cl->GetStats().c_str());
      #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      if (cl->m_connmap.size() > 0)
        {
        OvmsRecMutexLock lock(&cl->m_cmmutex);
        for (canlog::conn_map_t::iterator it=cl->m_connmap.begin(); it!=cl->m_connmap.end(); ++it)
          {
          canlogconnection* clc = it->second;
          writer->printf("  %s: %s %s%s%s\n",
            clc->GetSummary().c_str(),
            (clc->m_ispaused)?"paused":"running",
            (clc->m_filters != NULL)?" filter:":"",
            (clc->m_filters != NULL)?clc->m_filters->Info().c_str():"",
            clc->GetStats().c_str());
          }
        }
      #endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
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
    OvmsRecMutexLock lock(&MyCan.m_loggermap_mutex);
    for (can::canlog_map_t::iterator it=MyCan.m_loggermap.begin(); it!=MyCan.m_loggermap.end(); ++it)
      {
      canlog* cl = it->second;
      writer->printf("#%d: %s %s %s\n",
        it->first,
        (cl->m_isopen)?"open":"closed",
        cl->GetInfo().c_str(), cl->GetStats().c_str());
      #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      if (cl->m_connmap.size() > 0)
        {
        OvmsRecMutexLock lock(&cl->m_cmmutex);
        for (canlog::conn_map_t::iterator it=cl->m_connmap.begin(); it!=cl->m_connmap.end(); ++it)
          {
          canlogconnection* clc = it->second;
          writer->printf("  %s: %s %s%s%s\n",
            clc->GetSummary().c_str(),
            (clc->m_ispaused)?"paused":"running",
            (clc->m_filters != NULL)?" filter:":"",
            (clc->m_filters != NULL)?clc->m_filters->Info().c_str():"",
            clc->GetStats().c_str());
          }
        }
      #endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
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
    OvmsRecMutexLock lock(&MyCan.m_loggermap_mutex);
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
// CAN Logger Connection class
////////////////////////////////////////////////////////////////////////


canlogconnection::canlogconnection(canlog* logger, std::string format, canformat::canformat_serve_mode_t mode)
  {
  m_logger = logger;
  m_formatter = MyCanFormatFactory.NewFormat(format.c_str());
  m_formatter->SetServeMode(mode);
  m_nc = NULL;
  m_ispaused = false;
  m_filters = NULL;
  m_msgcount = 0;
  m_dropcount = 0;
  m_discardcount = 0;
  m_filtercount = 0;
  }

canlogconnection::~canlogconnection()
  {
  if (m_filters != NULL)
    {
    delete m_filters;
    m_filters = NULL;
    }
  if (m_formatter != NULL)
    {
    delete m_formatter;
    m_formatter = NULL;
    }
  }

void canlogconnection::OutputMsg(CAN_log_message_t& msg, std::string &result)
  {
  m_msgcount++;

  if ((m_filters != NULL) && (! m_filters->IsFiltered(&msg.frame)))
    {
    m_filtercount++;
    return;
    }

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  // The standard base implemention here is for mongoose network connections
  if (m_nc != NULL)
    {
    if (result.length()>0)
      {
      if (m_nc->send_mbuf.len < 32768)
        {
        mg_send(m_nc, (const char*)result.c_str(), result.length());
        }
      else
        {
        m_dropcount++;
        }
      }
    }
  else
#endif // CONFIG_OVMS_SC_GPL_MONGOOSE
    {
    m_dropcount++;
    }
  }

void canlogconnection::TransmitCallback(uint8_t *buffer, size_t len)
  {
  ESP_LOGD(TAG,"TransmitCallback on %s (%d bytes)",m_peer.c_str(),len);

  m_msgcount++;
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  if ((m_nc != NULL)&&(m_nc->send_mbuf.len < 32768))
    {
    mg_send(m_nc, buffer, len);
    }
  else
#endif // CONFIG_OVMS_SC_GPL_MONGOOSE
    {
    m_dropcount++;
    }
  }

void canlogconnection::ControlBusConfigure(canbus* bus, CAN_mode_t mode, CAN_speed_t speed)
  {
  ESP_LOGI(TAG,"Remote CAN bus configure: %s %s %dKbps",
    bus->GetName(),
    (mode == CAN_MODE_LISTEN)?"listen":"active",
    (int)speed);
  bus->Start(mode, speed);
  }

void canlogconnection::PauseTransmission()
  {
  ESP_LOGI(TAG,"Remote CAN bus pause transmission");
  m_ispaused = true;
  }

void canlogconnection::ResumeTransmission()
  {
  ESP_LOGI(TAG,"Remote CAN bus resume transmission");
  m_ispaused = false;
  }

void canlogconnection::ClearFilters()
  {
  ESP_LOGI(TAG,"Remote CAN bus clear filters");
  if (m_filters)
    {
    delete m_filters;
    m_filters = NULL;
    }
  }

void canlogconnection::AddFilter(std::string& filter)
  {
  ESP_LOGI(TAG,"Remote CAN bus add filter: %s", filter.c_str());
  if (!m_filters) m_filters = new canfilter();
  m_filters->AddFilter(filter.c_str());
  }

std::string canlogconnection::GetSummary()
  {
  return m_peer;
  }

std::string canlogconnection::GetStats()
  {
  std::ostringstream buf;

  float droprate = (m_msgcount > 0) ? ((float) m_dropcount/m_msgcount*100) : 0;

  buf << "Messages:" << m_msgcount
    << " Discarded:" << m_discardcount
    << " Dropped:" << m_dropcount
    << " Filtered:" << m_filtercount
    << " Rate:" << std::fixed << std::setprecision(1) << droprate << "%";

  return buf.str();
  }


////////////////////////////////////////////////////////////////////////
// CAN Logger class
////////////////////////////////////////////////////////////////////////

canlog::canlog(const char* type, std::string format, canformat::canformat_serve_mode_t mode)
  {
  m_type = type;
  m_format = format;
  m_mode = mode;
  m_formatter = MyCanFormatFactory.NewFormat(format.c_str());
  m_formatter->SetServeMode(mode);
  m_filter = NULL;
  m_isopen = false;

  m_msgcount = 0;
  m_dropcount = 0;
  m_filtercount = 0;

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(IDTAG, "*", std::bind(&canlog::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(IDTAG,"config.mounted", std::bind(&canlog::UpdatedConfig, this, _1, _2));
  MyEvents.RegisterEvent(IDTAG,"config.changed", std::bind(&canlog::UpdatedConfig, this, _1, _2));
  MyMetrics.RegisterListener(IDTAG, "*", std::bind(&canlog::MetricListener, this, _1));

  int queuesize = MyConfig.GetParamValueInt(CAN_PARAM, "log.queuesize",100);
  LoadConfig();
  m_queue = xQueueCreate(queuesize, sizeof(CAN_log_message_t));
  xTaskCreatePinnedToCore(RxTask, "OVMS CanLog", 4096, (void*)this, 10, &m_task, CORE(1));
  }

canlog::~canlog()
  {
  MyEvents.DeregisterEvent(IDTAG);
  MyMetrics.DeregisterListener(IDTAG);

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
        case CAN_LogInfo_Metric:
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
        case CAN_LogInfo_Metric:
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

/**
 * Parse a comma-separated list of filters, and assign them to a member of the class.
 * We have 3 kind of comparisons, and an unlimited list of filters.
 *
 * A filter can be:
 * - A "startsWith" comparison - when ending with '*',
 * - An "endsWith" comparison - when starting with '*',
 * - Invalid (and skipped) if empty, or with a '*' in any other position than beginning or end,
 * - A "string equal" comparison for all other cases
 */
static void LoadFilters(conn_filters_arr_t &member, const std::string &value)
  {
  // Empty all previously defined filters, for all operators
  for (int i=0; i<COUNT_OF_OPERATORS; i++)
    {
    member[i].clear();
    }

  if (!value.empty())
    {
    std::stringstream stream (value);
    std::string item;
    unsigned char comparison_operator;

    // Comma-separated list
    while (getline (stream, item, ','))
      {
      trim(item); // Removing leading and trailing spaces

      if (item.empty())
        {
        ESP_LOGW(TAG, "LoadFilters: skipping empty value in the filter list");
        continue;
        }

      // Check if there is a wildcard ('*') in any other place than first or last position
      size_t wildcard_position = item.find('*', 1);
      if ((wildcard_position != std::string::npos) && (wildcard_position != item.size()-1))
        {
        ESP_LOGW(TAG, "LoadFilters: skipping incorrect value (%s) in the filter list (wildcard in wrong position)", item.c_str());
        continue;
        }

      // Depending on the presence and position of the wildcard, push the filter
      // in the proper vector (without the wildcard)
      if (item.front() == '*')
        {
        comparison_operator = OPERATOR_ENDSWITH;
        item.erase(0, 1);
        }
      else if (item.back() == '*')
        {
        comparison_operator = OPERATOR_STARTSWITH;
        item.pop_back();
        }
      else
        {
        comparison_operator = OPERATOR_EQUALS;
        }
      member[comparison_operator].push_back(item);
      }
    }

  // for (int i=0; i<COUNT_OF_OPERATORS; i++)
  //   {
  //   ESP_LOGI(TAG, "LoadFilters: filters for operator %d:", i);
  //   for (std::vector<std::string>::iterator it=member[i].begin(); it!=member[i].end(); ++it)
  //     {
  //     ESP_LOGI(TAG, "LoadFilters: filter value '%s'", it->c_str());
  //     }
  //   if (member[i].begin() == member[i].end())
  //     {
  //     ESP_LOGI(TAG, "LoadFilters: (empty filter list)");
  //     }
  //   }
  }

/**
 * Load, or reload, the configuration of events and metrics filters.
 *
 * The configuration item is a string containing a comma-separated list of filters.
 */
void canlog::LoadConfig()
  {
  std::size_t str_hash;

  std::string list_of_events_filters = MyConfig.GetParamValue(CAN_PARAM, "log.events_filters", "x*,vehicle*");
  str_hash = std::hash<std::string>{}(list_of_events_filters);
  if (str_hash != m_events_filters_hash)
    {
    m_events_filters_hash = str_hash;
    LoadFilters(m_events_filters, list_of_events_filters);
    MyCan.LogInfo(NULL, CAN_LogInfo_Config, ("Events filters: " + list_of_events_filters).c_str());
    }
  std::string list_of_metrics_filters = MyConfig.GetParamValue(CAN_PARAM, "log.metrics_filters");
  str_hash = std::hash<std::string>{}(list_of_metrics_filters);
  if (str_hash != m_metrics_filters_hash)
    {
    m_metrics_filters_hash = str_hash;
    LoadFilters(m_metrics_filters, list_of_metrics_filters);
    MyCan.LogInfo(NULL, CAN_LogInfo_Config, ("Metrics filters: " + list_of_metrics_filters).c_str());
    }
  }

/**
 * Load, or reload, the configuration if a config event occurred.
 */
void canlog::UpdatedConfig(std::string event, void* data)
  {
  if (event == "config.changed")
    {
    // Only reload if our parameter has changed
    OvmsConfigParam*p = (OvmsConfigParam*)data;
    if (p->GetName() != CAN_PARAM)
      {
      return;
      }
    }
  LoadConfig();
  }

/**
 * Check if a value matches in a list of filters.
 *
 * Match can be a:
 * - startsWith match,
 * - endsWith match,
 * - equality match.
 */
static bool CheckFilter(conn_filters_arr_t &member, const std::string &value)
  {
  for (int i=0; i<COUNT_OF_OPERATORS; i++)
    {
    for (std::vector<std::string>::iterator it=member[i].begin(); it!=member[i].end(); ++it)
      {
      if ((i == OPERATOR_STARTSWITH) && (startsWith(value, *it)))
        {
        return true;
        }
      else if ((i == OPERATOR_ENDSWITH) && (endsWith(value, *it)))
        {
        return true;
        }
      else if ((i == OPERATOR_EQUALS) && (value == *it))
        {
        return true;
        }
      }
    }
  return false;
  }

void canlog::EventListener(std::string event, void* data)
  {
  // Log vehicle custom (x…) & framework events:
  if (CheckFilter(m_events_filters, event))
    LogInfo(NULL, CAN_LogInfo_Event, event.c_str());
  }

void canlog::MetricListener(OvmsMetric* metric)
  {
    std::string name = metric->m_name;
  // Log metrics (in JSON for later parsing):
  if (CheckFilter(m_metrics_filters, name))
    {
    std::string metric_text = "{ ";
    metric_text += "\"name\": \"" + json_encode(name) + "\", ";
    metric_text += "\"value\": " + metric->AsJSON() + ", ";
    metric_text += "\"unit\": \"" + json_encode(std::string(OvmsMetricUnitLabel(metric->GetUnits()))) + "\" }";
    LogInfo(NULL, CAN_LogInfo_Metric, metric_text.c_str());
    }
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
  if (m_formatter == NULL)
    {
    m_dropcount++;
    return;
    }

  if (!m_isopen)
    {
    m_dropcount++;
    return;
    }

  std::string result = m_formatter->get(&msg);
  if (result.length()>0)
    {
    OvmsRecMutexLock lock(&m_cmmutex);
    for (conn_map_t::iterator it=m_connmap.begin(); it!=m_connmap.end(); ++it)
      {
      if (it->second->m_ispaused)
        {
        it->second->m_msgcount++;
        it->second->m_discardcount++;
        }
      else
        {
        it->second->OutputMsg(msg, result);
        }
      }
    }
  }

std::string canlog::GetInfo()
  {
  std::ostringstream buf;

  buf << "Type:" << m_type;

  if (m_formatter)
    {
    buf << " Format:" << m_format << "(" << m_formatter->GetServeModeName() << ")";
    }

  if (m_filter)
    {
    buf << " Filter:" << m_filter->Info();
    }

  if (StdMetrics.ms_v_type->IsDefined())
    {
    buf << " Vehicle:" << StdMetrics.ms_v_type->AsString();
    }

  return buf.str();
  }

bool canlog::IsOpen()
  {
  return m_isopen;
  }

std::string canlog::GetStats()
  {
  std::ostringstream buf;

  float droprate = (m_msgcount > 0) ? ((float) m_dropcount/m_msgcount*100) : 0;
  uint32_t waiting = uxQueueMessagesWaiting(m_queue);

  buf << "Messages:" << m_msgcount
    << " Dropped:" << m_dropcount
    << " Filtered:" << m_filtercount
    << " Rate:" << std::fixed << std::setprecision(1) << droprate << "%";

  if (waiting > 0)
    buf << " Queued:" << waiting;

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
    if (xQueueSend(m_queue, &msg, 0) != pdTRUE)
      {
      m_dropcount++;
      free(msg.text);
      }
    }
  else
    {
    m_filtercount++;
    }
  }
