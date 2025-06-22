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
#if ESP_IDF_VERSION_MAJOR >= 4
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif
#if ESP_IDF_VERSION_MAJOR >= 5
#include "esp_wifi_ap_get_sta_list.h"
#endif
extern "C"
  {
#include "lwip/netif.h"
  };
#include "ovms_events.h"
#include "ovms_command.h"
#include "ovms_metrics.h"
#include "string_writer.h"
#include "ovms_semaphore.h"

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

typedef struct
  {
  OvmsWriter* writer;
  OvmsSemaphore* semaphore;
  } ping_callback_args_t;

class OvmsNetManager
  {
  public:
    OvmsNetManager();
    ~OvmsNetManager();

  public:
#ifdef CONFIG_OVMS_COMP_WIFI
    void WifiStaGotIP(std::string event, void* data);
    void WifiStaLostIP(std::string event, void* data);
    void WifiStaConnected(std::string event, void* data);
    void WifiStaStop(std::string event, void* data);
    void WifiStaGood(std::string event, void* data);
    void WifiStaBad(std::string event, void* data);
    void WifiStaCheckSQ(OvmsMetric* metric);
    void WifiStaSetSQ(bool good);
    void WifiUpAP(std::string event, void* data);
    void WifiDownAP(std::string event, void* data);
    void WifiApStaDisconnect(std::string event, void* data);
#endif // CONFIG_OVMS_COMP_WIFI

    void Ticker1(std::string event, void *data);
    void ModemUp(std::string event, void* data);
    void ModemDown(std::string event, void* data);
    void InterfaceUp(std::string event, void* data);
    void ConfigChanged(std::string event, void* data);
    void EventSystemShuttingDown(std::string event, void* data);
    void RestartNetwork();

  public:
    void DoSafePrioritiseAndIndicate();

  protected:
    void PrioritiseAndIndicate();
#ifdef CONFIG_OVMS_COMP_WIFI
    void WifiConnect();
    void WifiDisconnect();
#endif // CONFIG_OVMS_COMP_WIFI
    void SetNetType(std::string type);
    void SaveDNSServer(ip_addr_t* dnsstore);
    void SetDNSServer(ip_addr_t* dnsstore);

  public:
    int m_not_connected_counter;
    bool m_connected_wifi;
    bool m_connected_modem;
    bool m_connected_any;
    bool m_has_ip;
    bool m_wifi_sta;
    bool m_wifi_good;
    bool m_wifi_ap;
    bool m_network_any;
    ip_addr_t m_dns_wifi[DNS_MAX_SERVERS];
    ip_addr_t m_dns_modem[DNS_MAX_SERVERS];
    ip_addr_t m_previous_dns[DNS_MAX_SERVERS];
    char m_previous_name[2];

  protected:
    bool m_cfg_reboot_no_connection;
    float m_cfg_wifi_sq_good;               // config network wifi.sq.good   [dBm] default -87
    float m_cfg_wifi_sq_bad;                // config network wifi.sq.bad    [dBm] default -89

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  protected:
    void StartMongooseTask();
    void StopMongooseTask();

  protected:
    TaskHandle_t m_mongoose_task;
    struct mg_mgr m_mongoose_mgr;
    bool m_mongoose_running;                // true = Mongoose task is fully operational
    bool m_mongoose_stopping;               // true = Mongoose task shutdown requested
    QueueHandle_t m_jobqueue;
    OvmsSemaphore m_tcpip_callback_done;    // block until tcpip callback done (see PrioritiseAndIndicate)

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
    bool IsNetManagerTask();

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  };

extern OvmsNetManager MyNetManager;

#endif //#ifndef __OVMS_NETMANAGER_H__
