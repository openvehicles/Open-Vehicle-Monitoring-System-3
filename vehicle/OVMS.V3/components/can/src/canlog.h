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

#ifndef __CANLOG_H__
#define __CANLOG_H__

#include "freertos/semphr.h"
#include "can.h"
#include "canformat.h"
#include <sdkconfig.h>
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
#include "ovms_netmanager.h"
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
#include "ovms_metrics.h"

/**
 * canlog is the general interface and base implementation for all can loggers.
 *  It provides standard methods to open files and configure message filters
 *  to the CAN framework command "can log".
 *
 * Specific log formats are implemented as sub classes of canlog. Add them
 *  to the type list & method Instantiate(). See canlog_trace & canlog_crtd
 *  for examples & reference.
 *
 * Log messages are sent to a canlog through a queue handled by a separate
 *  task for the logger, so logging doesn't affect CAN framework speed and
 *  a log can be written/streamed to a slow medium.
 *
 * Log entries can be frames, status or info messages (see CAN_LogEntry_t).
 * The timestamp of the original event is preserved.
 *
 * Note: loggers get messages for all interfaces, if a log format does not
 *  allow multiple buses within a file, the logger needs to manage a set
 *  of files or may return false on Open() without a bus filter.
 */

class canlog;
class canlogconnection: public InternalRamAllocated
  {
  public:
    canlogconnection(canlog* logger, std::string format, canformat::canformat_serve_mode_t mode);
    virtual ~canlogconnection();

  public:
    virtual void OutputMsg(CAN_log_message_t& msg, std::string &result);

  public:
    virtual void TransmitCallback(uint8_t *buffer, size_t len);
    virtual void ControlBusConfigure(canbus* bus, CAN_mode_t mode, CAN_speed_t speed);
    virtual void PauseTransmission();
    virtual void ResumeTransmission();
    virtual void ClearFilters();
    virtual void AddFilter(std::string& filter);

  public:
    virtual std::string GetSummary();
    virtual std::string GetStats();

  public:
    canlog*        m_logger;
    canformat*     m_formatter;
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
    mg_connection* m_nc;
#else
    void*          m_nc;
#endif
    std::string    m_peer;
    bool           m_ispaused;
    canfilter*     m_filters;
    uint32_t       m_msgcount;
    uint32_t       m_dropcount;
    uint32_t       m_discardcount;
    uint32_t       m_filtercount;
  };

#define OPERATOR_STARTSWITH 0
#define OPERATOR_ENDSWITH 1
#define OPERATOR_EQUALS 2
#define COUNT_OF_OPERATORS 3
typedef std::array<std::vector<std::string>, COUNT_OF_OPERATORS> conn_filters_arr_t;

class canlog : public InternalRamAllocated
  {
  public:
    canlog(const char* type, std::string format, canformat::canformat_serve_mode_t mode=canformat::Discard);
    virtual ~canlog();

  public:
    static void RxTask(void* context);
    void EventListener(std::string event, void* data);
    void MetricListener(OvmsMetric* metric);

  public:
    const char* GetType();
    const char* GetFormat();
    virtual std::string GetStats();

  public:
    // Methods expected to be implemented by sub-classes
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool IsOpen();
    virtual std::string GetInfo();
    virtual void OutputMsg(CAN_log_message_t& msg);

  public:
    virtual void SetFilter(canfilter* filter);
    virtual void ClearFilter();

  public:
    // Logging API:
    virtual void LogFrame(canbus* bus, CAN_log_type_t type, const CAN_frame_t* p_frame);
    virtual void LogStatus(canbus* bus, CAN_log_type_t type, const CAN_status_t* status);
    virtual void LogInfo(canbus* bus, CAN_log_type_t type, const char* text);

  public:
    const char*         m_type;
    std::string         m_format;
    canformat*          m_formatter;
    canformat::canformat_serve_mode_t m_mode;
    canfilter*          m_filter;

  public:
    #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      typedef std::map<mg_connection*, canlogconnection*> conn_map_t;
    #else
      typedef std::map<void*, canlogconnection*> conn_map_t;
    #endif
    conn_map_t m_connmap;
    OvmsRecMutex m_cmmutex;

  public:
    TaskHandle_t        m_task;
    QueueHandle_t       m_queue;
    bool                m_isopen;
    uint32_t            m_msgcount;
    uint32_t            m_dropcount;
    uint32_t            m_filtercount;

  protected:
    virtual void UpdatedConfig(std::string event, void* data);
    virtual void LoadConfig();

  protected:
    conn_filters_arr_t  m_events_filters;
    size_t              m_events_filters_hash = 0;
    conn_filters_arr_t  m_metrics_filters;
    size_t              m_metrics_filters_hash = 0;
  };

#endif // __CANLOG_H__
