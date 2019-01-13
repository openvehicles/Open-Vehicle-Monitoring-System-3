/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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

#ifndef __OVMS_NETMANAGER_H__
#define __OVMS_NETMANAGER_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tcpip_adapter.h"
extern "C"
  {
#include "lwip/netif.h"
  };
#include "ovms_events.h"
#include "ovms_command.h"
#include "string_writer.h"

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
#define MG_LOCALS 1
#include "mongoose.h"

typedef enum
  {
  nmc_none = 0,
  nmc_list,
  nmc_close,
  nmc_cleanup,
  } netman_cmd_t;

typedef struct
  {
  TaskHandle_t caller;
  netman_cmd_t cmd;
  union
    {
    struct
      {
      int verbosity;
      StringWriter* buf;
      int cnt;
      } list;
    struct
      {
      uint32_t id;
      int cnt;
      } close;
    struct
      {
      int cnt;
      } cleanup;
    };
  } netman_job_t;

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

class OvmsNetManager
  {
  public:
    OvmsNetManager();
    ~OvmsNetManager();

  public:
    void WifiUpSTA(std::string event, void* data);
    void WifiDownSTA(std::string event, void* data);
    void WifiUpAP(std::string event, void* data);
    void WifiDownAP(std::string event, void* data);
    void WifiApStaDisconnect(std::string event, void* data);
    void ModemUp(std::string event, void* data);
    void ModemDown(std::string event, void* data);
    void InterfaceUp(std::string event, void* data);
    void ConfigChanged(std::string event, void* data);
    void EventSystemShuttingDown(std::string event, void* data);

  protected:
    void PrioritiseAndIndicate();
    void SetNetType(std::string type);
    void SaveDNSServer(ip_addr_t* dnsstore);
    void SetDNSServer(ip_addr_t* dnsstore);

  public:
    bool m_connected_wifi;
    bool m_connected_modem;
    bool m_connected_any;
    bool m_wifi_ap;
    bool m_network_any;
    ip_addr_t m_dns_wifi[DNS_MAX_SERVERS];
    ip_addr_t m_dns_modem[DNS_MAX_SERVERS];
    struct netif* m_previous_priority;

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  protected:
    void StartMongooseTask();
    void StopMongooseTask();

  protected:
    TaskHandle_t m_mongoose_task;
    struct mg_mgr m_mongoose_mgr;
    bool m_mongoose_running;
    QueueHandle_t m_jobqueue;

  public:
    void MongooseTask();
    TaskHandle_t GetMongooseTaskHandle() { return m_mongoose_task; }
    struct mg_mgr* GetMongooseMgr();
    bool MongooseRunning();
    void ProcessJobs();
    bool ExecuteJob(netman_job_t* job, TickType_t timeout=portMAX_DELAY);
    void ScheduleCleanup();
    int ListConnections(int verbosity, OvmsWriter* writer);
    int CloseConnection(uint32_t id);
    int CleanupConnections();

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  };

extern OvmsNetManager MyNetManager;

#endif //#ifndef __OVMS_NETMANAGER_H__
