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
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"


/***************************************************************************************************
 * canlog Factory
 */

static const char* const typelist[] = { "trace", "crtd", NULL };

const char* const* canlog::GetTypeList()
  {
  return typelist;
  }

canlog* canlog::Instantiate(const char* type)
  {
  if (strcasecmp(type, "trace") == 0)
    return new canlog_trace();
  if (strcasecmp(type, "crtd") == 0)
    return new canlog_crtd();

  ESP_LOGE(TAG, "canlog::Instantiate: Unknown type '%s'", type);
  return NULL;
  }


/***************************************************************************************************
 * canlog: Base CAN logger implementation
 */

canlog::canlog(int queuesize /*=30*/)
  {
  m_file = NULL;
  m_path = "";
  m_filtercnt = 0;
  m_queue = xQueueCreate(queuesize, sizeof(CAN_LogMsg_t));
  xTaskCreatePinnedToCore(RxTask, "OVMS CanLog", 4096, (void*)this, 10, &m_task, 1);
  m_msgcount = 0;
  m_dropcount = 0;

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "sd.mounted", std::bind(&canlog::MountListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "sd.unmounting", std::bind(&canlog::MountListener, this, _1, _2));
  }

canlog::~canlog()
  {
  if (m_task)
    vTaskDelete(m_task);

  if (m_queue)
    {
    CAN_LogMsg_t msg;
    while (xQueueReceive(m_queue, &msg, 0) == pdTRUE)
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
    vQueueDelete(m_queue);
    }
  }

void canlog::RxTask(void *context)
  {
  canlog* me = (canlog*) context;
  CAN_LogMsg_t msg;
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

bool canlog::Open(std::string path)
  {
  if (MyConfig.ProtectedPath(path))
    {
    ESP_LOGE(TAG, "canlog[%s].Open: protected path '%s'", GetType(), path.c_str());
    return false;
    }
#ifdef CONFIG_OVMS_COMP_SDCARD
  if (startsWith(path, "/sd") && (!MyPeripherals || !MyPeripherals->m_sdcard || !MyPeripherals->m_sdcard->isavailable()))
    {
    ESP_LOGE(TAG, "canlog[%s].Open: cannot open '%s', SD filesystem not available", GetType(), path.c_str());
    return false;
    }
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

  FILE* file = fopen(path.c_str(), "w");
  if (!file)
    {
    ESP_LOGE(TAG, "canlog[%s].Open: can't write to '%s'", GetType(), path.c_str());
    return false;
    }

  m_path = path;
  m_file = file;
  m_msgcount = 0;
  m_dropcount = 0;

  LogInfo(NULL, CAN_LogInfo_Config, GetInfo().c_str());
  ESP_LOGI(TAG, "canlog[%s].Open: writing to '%s'", GetType(), path.c_str());
  return true;
  }

void canlog::Close()
  {
  if (m_file)
    {
    FILE* file = m_file;
    m_file = NULL;
    fclose(file);
    ESP_LOGI(TAG, "canlog[%s].Close: '%s' closed. Statistics: %s",
      GetType(), m_path.c_str(), GetStats().c_str());
    }
  }

void canlog::MountListener(std::string event, void* data)
  {
  if (event == "sd.unmounting" && startsWith(m_path, "/sd"))
    Close();
  else if (event == "sd.mounted" && startsWith(m_path, "/sd"))
    Open(m_path);
  }

static const char* const CAN_LogEntryTypeName[] = {
  "RX",
  "TX",
  "TX_Queue",
  "TX_Fail",
  "Error",
  "Status",
  "Comment",
  "Info",
  "Event"
};

const char* canlog::GetLogEntryTypeName(CAN_LogEntry_t type)
  {
  return CAN_LogEntryTypeName[type];
  }

std::string canlog::GetInfo()
  {
  std::ostringstream buf;

  buf << "Type:" << GetType();

  if (IsOpen())
    buf << "; Path:'" << GetPath() << "'";

  if (m_filtercnt)
    {
    buf << "; Filter:" << std::hex;
    for (int i=0; i<m_filtercnt; i++)
      {
      buf << ((i==0)?"":",");
      if (m_filter[i].bus)
        buf << m_filter[i].bus << ":";
      buf << m_filter[i].id_from << "-" << m_filter[i].id_to;
      }
    }
  else
    {
    buf << "; Filter:off";
    }

  buf << "; Vehicle:" << StdMetrics.ms_v_type->AsString() << ";";

  return buf.str();
  }

std::string canlog::GetStats()
  {
  std::ostringstream buf;
  float droprate = (m_msgcount > 0) ? ((float) m_dropcount/m_msgcount*100) : 0;
  uint32_t waiting = uxQueueMessagesWaiting(m_queue);
  buf << "total messages: " << m_msgcount
    << ", dropped: " << m_dropcount
    << " = " << std::fixed << std::setprecision(1) << droprate << "%";
  if (waiting > 0)
    buf << ", waiting: " << waiting;
  return buf.str();
  }

void canlog::SetFilter(int filtercnt, canlog_filter_t filter[])
  {
  m_filtercnt = MIN(filtercnt, CANLOG_MAX_FILTERS);
  memcpy(m_filter, filter, m_filtercnt*sizeof(canlog_filter_t));
  LogInfo(NULL, CAN_LogInfo_Config, GetInfo().c_str());
  }

bool canlog::CheckFilter(canbus* bus, CAN_LogEntry_t type, const CAN_frame_t* frame)
  {
  if (!bus || m_filtercnt == 0)
    return true;
  char buskey = bus->GetName()[3];
  for (int i=0; i<m_filtercnt; i++)
    {
    if (m_filter[i].bus && m_filter[i].bus != buskey)
      continue;
    if (!frame)
      return true;
    if (m_filter[i].id_from <= frame->MsgID && frame->MsgID <= m_filter[i].id_to)
      return true;
    }
  return false;
  }

void canlog::LogFrame(canbus* bus, CAN_LogEntry_t type, const CAN_frame_t* frame)
  {
  if (!IsOpen() || !bus || !frame)
    return;
  if (CheckFilter(bus, type, frame))
    {
    CAN_LogMsg_t msg;
    msg.timestamp = esp_log_timestamp();
    msg.bus = bus;
    msg.type = type;
    msg.frame = *frame;
    m_msgcount++;
    if (xQueueSend(m_queue, &msg, 0) != pdTRUE)
      m_dropcount++;
    }
  }

void canlog::LogStatus(canbus* bus, CAN_LogEntry_t type, const CAN_status_t* status)
  {
  if (!IsOpen() || !bus)
    return;
  if (CheckFilter(bus, type))
    {
    CAN_LogMsg_t msg;
    msg.timestamp = esp_log_timestamp();
    msg.bus = bus;
    msg.type = type;
    msg.status = *status;
    m_msgcount++;
    if (xQueueSend(m_queue, &msg, 0) != pdTRUE)
      m_dropcount++;
    }
  }

void canlog::LogInfo(canbus* bus, CAN_LogEntry_t type, const char* text)
  {
  if (!IsOpen() || !text)
    return;
  if (CheckFilter(bus, type))
    {
    CAN_LogMsg_t msg;
    msg.timestamp = esp_log_timestamp();
    msg.bus = bus;
    msg.type = type;
    msg.text = strdup(text);
    m_msgcount++;
    if (xQueueSend(m_queue, &msg, 0) != pdTRUE)
      m_dropcount++;
    }
  }



/***************************************************************************************************
 * canlog_trace: log to syslog
 */

void canlog_trace::OutputMsg(CAN_LogMsg_t& msg)
  {
  switch (msg.type)
    {
    case CAN_LogFrame_RX:
    case CAN_LogFrame_TX:
    case CAN_LogFrame_TX_Queue:
    case CAN_LogFrame_TX_Fail:
      {
      char buffer[100];
      char *hexdump = buffer + snprintf(buffer, sizeof(buffer), "%s %s id %0*x len %d: ",
        GetLogEntryTypeName(msg.type), msg.bus->GetName(),
        (msg.frame.FIR.B.FF == CAN_frame_std) ? 3 : 8, msg.frame.MsgID, msg.frame.FIR.B.DLC);
      FormatHexDump(&hexdump, (const char*)msg.frame.data.u8, msg.frame.FIR.B.DLC, 8);
      esp_log_write(ESP_LOG_VERBOSE, TAG, LOG_FORMAT(V, "%s"), msg.timestamp, TAG, buffer);
      }
      break;

    case CAN_LogStatus_Error:
      esp_log_write(ESP_LOG_ERROR, TAG,
        LOG_FORMAT(E, "%s %s intr=%d rxpkt=%d txpkt=%d errflags=%#x rxerr=%d txerr=%d rxovr=%d txovr=%d txdelay=%d wdgreset=%d"),
        msg.timestamp, TAG,
        GetLogEntryTypeName(msg.type), msg.bus->GetName(),
        msg.status.interrupts,
        msg.status.packets_rx, msg.status.packets_tx,
        msg.status.error_flags, msg.status.errors_rx, msg.status.errors_tx,
        msg.status.rxbuf_overflow, msg.status.txbuf_overflow,
        msg.status.txbuf_delay, msg.status.watchdog_resets);
      break;
    case CAN_LogStatus_Statistics:
      esp_log_write(ESP_LOG_DEBUG, TAG,
        LOG_FORMAT(D, "%s %s intr=%d rxpkt=%d txpkt=%d errflags=%#x rxerr=%d txerr=%d rxovr=%d txovr=%d txdelay=%d wdgreset=%d"),
        msg.timestamp, TAG,
        GetLogEntryTypeName(msg.type), msg.bus->GetName(),
        msg.status.interrupts,
        msg.status.packets_rx, msg.status.packets_tx,
        msg.status.error_flags, msg.status.errors_rx, msg.status.errors_tx,
        msg.status.rxbuf_overflow, msg.status.txbuf_overflow,
        msg.status.txbuf_delay, msg.status.watchdog_resets);
      break;

    case CAN_LogInfo_Comment:
    case CAN_LogInfo_Config:
    case CAN_LogInfo_Event:
      esp_log_write(ESP_LOG_DEBUG, TAG,
        LOG_FORMAT(D, "canlog %s %s %s"),
        msg.timestamp, TAG,
        GetLogEntryTypeName(msg.type), msg.bus ? msg.bus->GetName() : "*", msg.text);
      break;

    default:
      break;
    }
  }


/***************************************************************************************************
 * canlog_crtd: log to file; OVMS CRTD format by Mark Webb-Johnson
 *    see doc/Format-CRTD.md
 */

canlog_crtd::canlog_crtd()
  {
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "*", std::bind(&canlog_crtd::EventListener, this, _1, _2));
  }

canlog_crtd::~canlog_crtd()
  {
  MyEvents.DeregisterEvent(TAG);
  }

void canlog_crtd::EventListener(std::string event, void* data)
  {
  if (startsWith(event, "vehicle"))
    LogInfo(NULL, CAN_LogInfo_Event, event.c_str());
  }

void canlog_crtd::OutputMsg(CAN_LogMsg_t& msg)
  {
  switch (msg.type)
    {
    case CAN_LogFrame_RX:
    case CAN_LogFrame_TX:
      fprintf(m_file, "%d.%03d %s%c%s %0*X",
        msg.timestamp / 1000, msg.timestamp % 1000, msg.bus->GetName()+3,
        (msg.type == CAN_LogFrame_RX) ? 'R' : 'T', (msg.frame.FIR.B.FF == CAN_frame_std) ? "11" : "29",
        (msg.frame.FIR.B.FF == CAN_frame_std) ? 3 : 8, msg.frame.MsgID);
      for (int i=0; i<msg.frame.FIR.B.DLC; i++)
        fprintf(m_file, " %02X", msg.frame.data.u8[i]);
      fputc('\n', m_file);
      break;

    case CAN_LogFrame_TX_Queue:
    case CAN_LogFrame_TX_Fail:
      fprintf(m_file, "%d.%03d %sCEV %s %c%s %0*X",
        msg.timestamp / 1000, msg.timestamp % 1000, msg.bus->GetName()+3,
        GetLogEntryTypeName(msg.type),
        (msg.type == CAN_LogFrame_RX) ? 'R' : 'T', (msg.frame.FIR.B.FF == CAN_frame_std) ? "11" : "29",
        (msg.frame.FIR.B.FF == CAN_frame_std) ? 3 : 8, msg.frame.MsgID);
      for (int i=0; i<msg.frame.FIR.B.DLC; i++)
        fprintf(m_file, " %02X", msg.frame.data.u8[i]);
      fputc('\n', m_file);
      break;

    case CAN_LogStatus_Error:
    case CAN_LogStatus_Statistics:
      fprintf(m_file, "%d.%03d %s%s %s intr=%d rxpkt=%d txpkt=%d errflags=%#x rxerr=%d txerr=%d rxovr=%d txovr=%d txdelay=%d wdgreset=%d\n",
        msg.timestamp / 1000, msg.timestamp % 1000, msg.bus->GetName()+3,
        (msg.type == CAN_LogStatus_Error) ? "CEV" : "CXX",
        GetLogEntryTypeName(msg.type), msg.status.interrupts,
        msg.status.packets_rx, msg.status.packets_tx, msg.status.error_flags,
        msg.status.errors_rx, msg.status.errors_tx, msg.status.rxbuf_overflow, msg.status.txbuf_overflow,
        msg.status.txbuf_delay, msg.status.watchdog_resets);
      break;
      break;

    case CAN_LogInfo_Comment:
    case CAN_LogInfo_Config:
    case CAN_LogInfo_Event:
      fprintf(m_file, "%d.%03d %s%s %s %s\n",
        msg.timestamp / 1000, msg.timestamp % 1000, msg.bus ? msg.bus->GetName()+3 : "",
        (msg.type == CAN_LogInfo_Event) ? "CEV" : "CXX",
        GetLogEntryTypeName(msg.type), msg.text);
      break;

    default:
      break;
    }
  }
