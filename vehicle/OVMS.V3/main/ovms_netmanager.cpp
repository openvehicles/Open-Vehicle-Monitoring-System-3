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

#include "ovms_log.h"
static const char *TAG = "netmanager";

#include <string.h>
#include <stdio.h>
#include <lwip/tcpip.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip/dns.h>
#include <lwip/opt.h>
#include <lwip/sockets.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include "metrics_standard.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_module.h"
#include "ovms_boot.h"
#ifdef CONFIG_OVMS_DEV_NETMANAGER_PING
#include "ping/ping_sock.h"
#endif // CONFIG_OVMS_DEV_NETMANAGER_PING

#ifndef CONFIG_OVMS_NETMAN_TASK_PRIORITY
#define CONFIG_OVMS_NETMAN_TASK_PRIORITY 5
#endif

OvmsNetManager MyNetManager __attribute__ ((init_priority (8999)));

void network_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  struct netif *ni;
  for (ni = netif_list; ni; ni = ni->next)
    {
    if (ni->name[0]=='l' && ni->name[1]=='o')
      continue;
    writer->printf("Interface#%d: %c%c%d (ifup=%d linkup=%d)\n",
      ni->num,ni->name[0],ni->name[1],ni->num,
      ((ni->flags & NETIF_FLAG_UP) != 0),
      ((ni->flags & NETIF_FLAG_LINK_UP) != 0));
    writer->printf("  IPv4: " IPSTR "/" IPSTR " gateway " IPSTR "\n",
      IP2STR(&ni->ip_addr.u_addr.ip4), IP2STR(&ni->netmask.u_addr.ip4), IP2STR(&ni->gw.u_addr.ip4));
    writer->puts("");
    }

  writer->printf("DNS:");
  int dnsservers = 0;
  for (int k=0;k<DNS_MAX_SERVERS;k++)
    {
    const ip_addr_t* srv = dns_getserver(k);
    if (! ip_addr_isany(srv))
      {
      dnsservers++;
      if (srv->type == IPADDR_TYPE_V4)
        writer->printf(" " IPSTR, IP2STR(&srv->u_addr.ip4));
      else if (srv->type == IPADDR_TYPE_V6)
        writer->printf(" " IPSTR, IP2STR(&srv->u_addr.ip6));
      }
    }
  if (dnsservers == 0)
    writer->puts(" None");
  else
    writer->puts("");

  if (netif_default)
    {
    writer->printf("\nDefault Interface: %c%c%d (" IPSTR "/" IPSTR " gateway " IPSTR ")\n",
      netif_default->name[0], netif_default->name[1], netif_default->num,
      IP2STR(&netif_default->ip_addr.u_addr.ip4),
      IP2STR(&netif_default->netmask.u_addr.ip4),
      IP2STR(&netif_default->gw.u_addr.ip4));
    }
  else
    {
    writer->printf("\nDefault Interface: None\n");
    }
  }

void network_restart(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Restarting network...");
  vTaskDelay(pdMS_TO_TICKS(100));
  MyNetManager.RestartNetwork();
  }

#ifdef CONFIG_OVMS_DEV_NETMANAGER_PING

static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
  {
  ping_callback_args_t* ping_callback_args = static_cast<ping_callback_args_t*>(args);
  OvmsWriter* writer = ping_callback_args->writer;
  uint8_t ttl;
  uint16_t seqno;
  uint32_t elapsed_time, recv_len;
  ip_addr_t target_addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
  esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
  writer->printf("%" PRIu32 " bytes from %s: icmp_seq=%" PRIu16 " ttl=%" PRIi8 " time=%" PRIu32 " ms\n",
         recv_len, ipaddr_ntoa((ip_addr_t*)&target_addr), (seqno - 1), ttl, elapsed_time);
  }

static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
  {
  ping_callback_args_t* ping_callback_args = static_cast<ping_callback_args_t*>(args);
  OvmsWriter* writer = ping_callback_args->writer;
  uint16_t seqno;
  ip_addr_t target_addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
  writer->printf("From %s icmp_seq=%" PRIu16 " timeout\n", ipaddr_ntoa((ip_addr_t*)&target_addr), (seqno - 1));
  }

static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
  {
  ping_callback_args_t* ping_callback_args = static_cast<ping_callback_args_t*>(args);
  OvmsWriter* writer = ping_callback_args->writer;
  OvmsSemaphore* pingdone = ping_callback_args->semaphore;
  ip_addr_t target_addr;
  uint32_t transmitted;
  uint32_t received;
  uint32_t total_time_ms;

  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
  esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
  uint32_t loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
  if (IP_IS_V4(&target_addr))
    {
    printf("\n--- %s ping statistics ---\n", inet_ntoa(*ip_2_ip4(&target_addr)));
    }
  else
    {
    printf("\n--- %s ping statistics ---\n", inet6_ntoa(*ip_2_ip6(&target_addr)));
    }
  writer->printf("%" PRIu32 " packets transmitted, %" PRIu32 " received, %" PRIu32 "%% packet loss, time %" PRIu32 "ms\n",
         transmitted, received, loss, total_time_ms);
  if (received > 0)
    {
    writer->printf("round-trip avg = %1.2f ms\n", (total_time_ms * 1.0) / received);
    }

  pingdone->Give();
  // delete the ping sessions, so that we clean up all resources and can create a new ping session
  // we don't have to call delete function in the callback, instead we can call delete function from other tasks
  esp_ping_delete_session(hdl);
  }

void network_ping(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  // parse hostname / IP address
  struct sockaddr_in6 sock_addr6;
  ip_addr_t target_addr;
  memset(&target_addr, 0, sizeof(target_addr));

  if (inet_pton(AF_INET6, argv[0], &sock_addr6.sin6_addr) == 1)
    {
    /* convert ip6 string to ip6 address */
    ipaddr_aton(argv[0], &target_addr);
    }
  else
    {
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    memset(&hint, 0, sizeof(hint));
    /* convert ip4 string or hostname to ip4 or ip6 address */
    if (getaddrinfo(argv[0], NULL, &hint, &res) != 0)
      {
      writer->printf("ERROR: unknown host %s\n", argv[0]);
      return;
      }
    if (res->ai_family == AF_INET)
      {
      struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
      inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
      }
    else
      {
      struct in6_addr addr6 = ((struct sockaddr_in6 *) (res->ai_addr))->sin6_addr;
      inet6_addr_to_ip6addr(ip_2_ip6(&target_addr), &addr6);
      }
    freeaddrinfo(res);
    }

  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();

  ping_config.target_addr = target_addr; // target IP address
  ping_config.count = 4;

  // This semaphore is used to "wait" on the `ping_end` callback
  OvmsSemaphore pingdone;

  // This set of arguments are passed to callbacks.
  ping_callback_args_t args = {
    .writer = writer,
    .semaphore = &pingdone
  };

  /* set callback functions */
  esp_ping_callbacks_t cbs = {
      .cb_args = &args,
      .on_ping_success = test_on_ping_success,
      .on_ping_timeout = test_on_ping_timeout,
      .on_ping_end = test_on_ping_end
  };

  esp_ping_handle_t ping;

  ESP_ERROR_CHECK(esp_ping_new_session(&ping_config, &cbs, &ping));

  writer->printf("PING %s (%s): %" PRIu32 " data bytes\n", argv[0],
      ipaddr_ntoa(&target_addr), ping_config.data_size);

  esp_ping_start(ping);

  // We now wait for completion of the ping session (4 pings) - during this
  // wait the output is suspended and no prompt is printed.
  // The semaphore will be signaled by the `on_ping_end` callback, after which we can
  // return from this function (which will then print the prompt)
  pingdone.Take();

  }

#endif // CONFIG_OVMS_DEV_NETMANAGER_PING

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

void network_connections(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyNetManager.MongooseRunning())
    {
    writer->puts("ERROR: Mongoose task not running");
    return;
    }

  if (xTaskGetCurrentTaskHandle() == MyNetManager.GetMongooseTaskHandle())
    {
    // inline execution in mongoose context:
    if (strcmp(cmd->GetName(), "close") == 0)
      {
      uint32_t id = strtol(argv[0], NULL, 16);
      int cnt = MyNetManager.CloseConnection(id);
      if (cnt == 0)
        writer->printf("ERROR: connection ID %08" PRIx32 " not found\n", id);
      else
        writer->printf("Close signal sent to %d connection(s)\n", cnt);
      }
    else if (strcmp(cmd->GetName(), "cleanup") == 0)
      {
      int cnt = MyNetManager.CleanupConnections();
      if (cnt == 0)
        writer->puts("All connections OK");
      else
        writer->printf("Close signal sent to %d connection(s)\n", cnt);
      }
    else
      {
      MyNetManager.ListConnections(verbosity, writer);
      }
    }
  else
    {
    // remote execution from non-mongoose context:
    static netman_job_t job;
    memset(&job, 0, sizeof(job));
    if (strcmp(cmd->GetName(), "close") == 0)
      {
      job.cmd = nmc_close;
      job.close.id = strtol(argv[0], NULL, 16);
      if (!MyNetManager.ExecuteJob(&job, pdMS_TO_TICKS(5000)))
        writer->puts("ERROR: job failed");
      else if (job.close.cnt == 0)
        writer->printf("ERROR: connection ID %08" PRIx32 " not found\n", job.close.id);
      else
        writer->printf("Close signal sent to %d connection(s)\n", job.close.cnt);
      }
    else if (strcmp(cmd->GetName(), "cleanup") == 0)
      {
      job.cmd = nmc_cleanup;
      if (!MyNetManager.ExecuteJob(&job, pdMS_TO_TICKS(5000)))
        writer->puts("ERROR: job failed");
      else if (job.cleanup.cnt == 0)
        writer->puts("All connections OK");
      else
        writer->printf("Close signal sent to %d connection(s)\n", job.cleanup.cnt);
      }
    else
      {
      // Note: a timeout occurring after the job has begun processing will result in a crash
      //  here due to the StringWriter being deleted. This only applies to the list command.
      //  If this becomes a problem, we need to upgrade the job processing from send/notify
      //  to a full send/receive scheme.
      job.cmd = nmc_list;
      job.list.verbosity = verbosity;
      job.list.buf = new StringWriter();
      if (!MyNetManager.ExecuteJob(&job, pdMS_TO_TICKS(10000)))
        writer->puts("ERROR: job failed");
      else
        writer->write(job.list.buf->data(), job.list.buf->size());
      delete job.list.buf;
      job.list.buf = NULL;
      }
    }
  }

#endif // CONFIG_OVMS_SC_GPL_MONGOOSE

OvmsNetManager::OvmsNetManager()
  {
  ESP_LOGI(TAG, "Initialising NETMANAGER (8999)");
  m_not_connected_counter = 0;
  m_connected_wifi = false;
  m_connected_modem = false;
  m_connected_any = false;
  m_wifi_sta = false;
  m_wifi_good = false;
  m_wifi_ap = false;
  m_network_any = false;
  m_cfg_wifi_sq_good = -87;
  m_cfg_wifi_sq_bad = -89;

  for (int i=0; i<DNS_MAX_SERVERS; i++)
    {
    m_dns_modem[i] = (ip_addr_t)ip_addr_any;
    m_dns_wifi[i] = (ip_addr_t)ip_addr_any;
    m_previous_dns[i] = (ip_addr_t)ip_addr_any;
    }
  m_previous_name[0] = m_previous_name[1] = 0;

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  m_mongoose_task = 0;
  m_mongoose_running = false;
  m_mongoose_stopping = false;
  m_jobqueue = xQueueCreate(CONFIG_OVMS_HW_NETMANAGER_QUEUE_SIZE, sizeof(netman_job_t*));
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

  // Register our commands
  OvmsCommand* cmd_network = MyCommandApp.RegisterCommand("network","NETWORK framework",network_status, "", 0, 0, false);
  cmd_network->RegisterCommand("status","Show network status",network_status, "", 0, 0, false);
  cmd_network->RegisterCommand("restart","Restart network",network_restart, "", 0, 0, false);
#ifdef CONFIG_OVMS_DEV_NETMANAGER_PING
  cmd_network->RegisterCommand("ping", "Ping (ICMP) a hostname/IP address", network_ping, "<host or ip address>", 1, 1, false);
#endif // CONFIG_OVMS_DEV_NETMANAGER_PING
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  cmd_network->RegisterCommand("list", "List network connections", network_connections);
  cmd_network->RegisterCommand("close", "Close network connection(s)", network_connections, "<id>\nUse ID from connection list / 0 to close all", 1, 1);
  cmd_network->RegisterCommand("cleanup", "Close orphaned network connections", network_connections);
#endif // CONFIG_OVMS_SC_GPL_MONGOOSE

  // Register our events
  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;

#ifdef CONFIG_OVMS_COMP_WIFI
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.gotip", std::bind(&OvmsNetManager::WifiStaGotIP, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.lostip", std::bind(&OvmsNetManager::WifiStaLostIP, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.wifi.sta.good", std::bind(&OvmsNetManager::WifiStaGood, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.wifi.sta.bad", std::bind(&OvmsNetManager::WifiStaBad, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.stop", std::bind(&OvmsNetManager::WifiStaStop, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.connected", std::bind(&OvmsNetManager::WifiStaConnected, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.disconnected", std::bind(&OvmsNetManager::WifiStaStop, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.down", std::bind(&OvmsNetManager::WifiStaStop, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.start", std::bind(&OvmsNetManager::WifiUpAP, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.stop", std::bind(&OvmsNetManager::WifiDownAP, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.sta.disconnected", std::bind(&OvmsNetManager::WifiApStaDisconnect, this, _1, _2));
#endif // #ifdef CONFIG_OVMS_COMP_WIFI

  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsNetManager::Ticker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.gotip", std::bind(&OvmsNetManager::ModemUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.stop", std::bind(&OvmsNetManager::ModemDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.down", std::bind(&OvmsNetManager::ModemDown, this, _1, _2));

  MyEvents.RegisterEvent(TAG,"config.mounted", std::bind(&OvmsNetManager::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&OvmsNetManager::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.shuttingdown", std::bind(&OvmsNetManager::EventSystemShuttingDown, this, _1, _2));

  MyConfig.RegisterParam("network", "Network Configuration", true, true);
  // Our instances:
  //   dns                Space-separated list of DNS servers
  //   wifi.sq.good       Threshold for usable wifi signal [dBm], default -87
  //   wifi.sq.bad        Threshold for unusable wifi signal [dBm], default -89

#ifdef CONFIG_OVMS_COMP_WIFI
  MyMetrics.RegisterListener(TAG, MS_N_WIFI_SQ, std::bind(&OvmsNetManager::WifiStaCheckSQ, this, _1));
#endif
  }

OvmsNetManager::~OvmsNetManager()
  {
  }

void OvmsNetManager::RestartNetwork()
  {
#ifdef CONFIG_OVMS_COMP_WIFI
  if (MyPeripherals && MyPeripherals->m_esp32wifi)
    MyPeripherals->m_esp32wifi->Restart();
#endif // #ifdef CONFIG_OVMS_COMP_WIFI

#ifdef CONFIG_OVMS_COMP_CELLULAR
  if (MyPeripherals && MyPeripherals->m_cellular_modem)
    MyPeripherals->m_cellular_modem->Restart();
#endif // CONFIG_OVMS_COMP_CELLULAR
  }

#ifdef CONFIG_OVMS_COMP_WIFI

void OvmsNetManager::WifiConnect()
  {
  if (m_wifi_sta && !m_connected_wifi)
    {
    m_connected_wifi = true;
    PrioritiseAndIndicate();

    MyEvents.SignalEvent("network.wifi.up",NULL);

    if (m_connected_modem)
      {
      ESP_LOGI(TAG, "WIFI client up (with MODEM up): reconfigured for WIFI client priority");
      MyEvents.SignalEvent("network.reconfigured",NULL);
      }
    else
      {
      ESP_LOGI(TAG, "WIFI client up (with MODEM down): starting network with WIFI client");
      MyEvents.SignalEvent("network.up",NULL);
      }

    #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      StartMongooseTask();
    #endif

    MyEvents.SignalEvent("network.interface.change",NULL);
    }
  }

void OvmsNetManager::WifiDisconnect()
  {
  if (m_connected_wifi)
    {
    m_connected_wifi = false;
    PrioritiseAndIndicate();

    MyEvents.SignalEvent("network.wifi.down",NULL);

    if (m_connected_any)
      {
      ESP_LOGI(TAG, "WIFI client down (with MODEM up): reconfigured for MODEM priority");
      MyEvents.SignalEvent("network.reconfigured",NULL);
      #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
        ScheduleCleanup();
      #endif
      }
    else
      {
      ESP_LOGI(TAG, "WIFI client down (with MODEM down): network connectivity has been lost");
      MyEvents.SignalEvent("network.down",NULL);

      #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
        StopMongooseTask();
      #endif
      }

    MyEvents.SignalEvent("network.interface.change",NULL);
    }
  }

void OvmsNetManager::WifiStaGotIP(std::string event, void* data)
  {
  m_wifi_sta = true;
  ESP_LOGI(TAG, "WIFI client got IP");
  SaveDNSServer(m_dns_wifi);

  // Re-prioritise, just in case, as Wifi stack seems to mess with this
  // (in particular if an AP interface is up, and STA goes down, Wifi
  // stack seems to switch default interface to AP)
  PrioritiseAndIndicate();
  #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
    ScheduleCleanup();
  #endif

  WifiStaCheckSQ(StdMetrics.ms_m_net_wifi_sq);
  }

void OvmsNetManager::WifiStaLostIP(std::string event, void* data)
  {
  // Re-prioritise, just in case, as Wifi stack seems to mess with this
  // (in particular if an AP interface is up, and STA goes down, Wifi
  // stack seems to switch default interface to AP)
  PrioritiseAndIndicate();
  #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
    ScheduleCleanup();
  #endif
  }

void OvmsNetManager::WifiStaConnected(std::string event, void* data)
  {
  // Re-prioritise, just in case, as Wifi stack seems to mess with this
  // (in particular if an AP interface is up, and STA goes down, Wifi
  // stack seems to switch default interface to AP)
  PrioritiseAndIndicate();
  #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
    ScheduleCleanup();
  #endif
  }

void OvmsNetManager::WifiStaStop(std::string event, void* data)
  {
  WifiStaSetSQ(false);
  if (m_wifi_sta)
    {
    m_wifi_sta = false;
    ESP_LOGI(TAG, "WIFI client stop");
    if (m_connected_wifi)
      {
      WifiDisconnect();
      return;
      }
    }

  // Re-prioritise, just in case, as Wifi stack seems to mess with this
  // (in particular if an AP interface is up, and STA goes down, Wifi
  // stack seems to switch default interface to AP)
  PrioritiseAndIndicate();
  #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
    ScheduleCleanup();
  #endif
  }

void OvmsNetManager::WifiStaGood(std::string event, void* data)
  {
  if (m_wifi_sta && !m_connected_wifi)
    {
    ESP_LOGI(TAG, "WIFI client has good signal quality (%.1f dBm); connect",
             StdMetrics.ms_m_net_wifi_sq->AsFloat());
    WifiConnect();
    }
  }

void OvmsNetManager::WifiStaBad(std::string event, void* data)
  {
  if (m_wifi_sta && m_connected_wifi)
    {
    ESP_LOGI(TAG, "WIFI client has bad signal quality (%.1f dBm); disconnect",
             StdMetrics.ms_m_net_wifi_sq->AsFloat());
    // Stop using the Wifi network:
    WifiDisconnect();
    // If enabled, start reconnecting immediately (for fast transition to next best
    //  AP instead of trying to reassociate to current AP):
    if (MyPeripherals && MyPeripherals->m_esp32wifi &&
        MyConfig.GetParamValueBool("network", "wifi.bad.reconnect") == true)
      {
      MyPeripherals->m_esp32wifi->Reconnect(NULL);
      }
    }
  }

void OvmsNetManager::WifiStaCheckSQ(OvmsMetric* metric)
  {
  if (m_wifi_sta)
    {
    float sq = StdMetrics.ms_m_net_wifi_sq->AsFloat();
    if ((!metric || !m_wifi_good) && sq >= m_cfg_wifi_sq_good)
      {
      m_wifi_good = true;
      MyEvents.SignalEvent("network.wifi.sta.good", NULL);
      }
    else if ((!metric || m_wifi_good) && sq <= m_cfg_wifi_sq_bad)
      {
      m_wifi_good = false;
      MyEvents.SignalEvent("network.wifi.sta.bad", NULL);
      }
    }
  }

void OvmsNetManager::WifiStaSetSQ(bool good)
  {
  if (m_wifi_sta && m_wifi_good != good)
    {
    m_wifi_good = good;
    MyEvents.SignalEvent(good ? "network.wifi.sta.good" : "network.wifi.sta.bad", NULL);
    }
  }

void OvmsNetManager::WifiUpAP(std::string event, void* data)
  {
  m_wifi_ap = true;
  PrioritiseAndIndicate();

  ESP_LOGI(TAG, "WIFI access point is up");

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  StartMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

  MyEvents.SignalEvent("network.interface.change",NULL);
  }

void OvmsNetManager::WifiDownAP(std::string event, void* data)
  {
  m_wifi_ap = false;
  PrioritiseAndIndicate();

  ESP_LOGI(TAG, "WIFI access point is down");

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  StopMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

  MyEvents.SignalEvent("network.interface.change",NULL);
  }

void OvmsNetManager::WifiApStaDisconnect(std::string event, void* data)
  {
  ESP_LOGI(TAG, "WIFI access point station disconnected");
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  ScheduleCleanup();
#endif
  }

#endif // #ifdef CONFIG_OVMS_COMP_WIFI


void OvmsNetManager::Ticker1(std::string event, void *data)
  {
  if (m_cfg_reboot_no_connection)
    {
    if (m_connected_any && !m_has_ip && StdMetrics.ms_m_net_good_sq->AsBool())
      {
      ESP_LOGI(TAG, "Connection available with good signal, but no IP address; rebooting in %i seconds", 300-m_not_connected_counter);
      m_not_connected_counter++;
      if (m_not_connected_counter > 300)
        {
        MyBoot.Restart();
        }
      }
    else
      {
      m_not_connected_counter = 0;
      }
    }
  }

void OvmsNetManager::ModemUp(std::string event, void* data)
  {
  m_connected_modem = true;
  SaveDNSServer(m_dns_modem);
  PrioritiseAndIndicate();

  MyEvents.SignalEvent("network.modem.up",NULL);

  if (!m_connected_wifi)
    {
    ESP_LOGI(TAG, "MODEM up (with WIFI client down): starting network with MODEM");
    MyEvents.SignalEvent("network.up",NULL);
    }
  else
    {
    ESP_LOGI(TAG, "MODEM up (with WIFI client up): staying with WIFI client priority");
    }

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  StartMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

  MyEvents.SignalEvent("network.interface.change",NULL);
  }

void OvmsNetManager::ModemDown(std::string event, void* data)
  {
  if (m_connected_modem)
    {
    m_connected_modem = false;
    PrioritiseAndIndicate();

    MyEvents.SignalEvent("network.modem.down",NULL);

    if (m_connected_any)
      {
      ESP_LOGI(TAG, "MODEM down (with WIFI client up): staying with WIFI client priority");
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      ScheduleCleanup();
#endif
      }
    else
      {
      ESP_LOGI(TAG, "MODEM down (with WIFI client down): network connectivity has been lost");
      MyEvents.SignalEvent("network.down",NULL);

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      StopMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      }

    MyEvents.SignalEvent("network.interface.change",NULL);
    }
  else
    {
    // Re-prioritise, just in case, as Wifi stack seems to mess with this
    // (in particular if an AP interface is up, and STA goes down, Wifi
    // stack seems to switch default interface to AP)
    PrioritiseAndIndicate();
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
    ScheduleCleanup();
#endif
    }
  }

void OvmsNetManager::ConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* param = (OvmsConfigParam*)data;
  if (!param || param->GetName() == "network")
    {
    // Network config has been changed, apply:
    m_cfg_reboot_no_connection = MyConfig.GetParamValueBool("network", "reboot.no.ip", false);
    m_cfg_wifi_sq_good = MyConfig.GetParamValueFloat("network", "wifi.sq.good", -87);
    m_cfg_wifi_sq_bad = MyConfig.GetParamValueFloat("network", "wifi.sq.bad",  -89);
    if (m_cfg_wifi_sq_good < m_cfg_wifi_sq_bad)
      {
      float x = m_cfg_wifi_sq_good;
      m_cfg_wifi_sq_good = m_cfg_wifi_sq_bad;
      m_cfg_wifi_sq_bad = x;
      }
    #ifdef CONFIG_OVMS_COMP_WIFI
      WifiStaCheckSQ(NULL);
    #endif
    if (param && m_network_any)
      PrioritiseAndIndicate();
    }
  }

void OvmsNetManager::EventSystemShuttingDown(std::string event, void* data)
  {
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  StopMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  }

void OvmsNetManager::SaveDNSServer(ip_addr_t* dnsstore)
  {
  for (int i=0; i<DNS_MAX_SERVERS; i++)
    {
    dnsstore[i] = *dns_getserver(i);
    ESP_LOGD(TAG, "Saved DNS#%d %s", i, inet_ntoa(dnsstore[i]));
    }
  }

void OvmsNetManager::SetDNSServer(ip_addr_t* dnsstore)
  {
  // Read DNS configuration:
  std::string servers = MyConfig.GetParamValue("network", "dns");
  if (!servers.empty())
    {
    int spos = 0;
    size_t sep = 0;
    while ((spos<DNS_MAX_SERVERS)&&(sep != string::npos))
      {
      // Set the specified DNS servers
      size_t next = servers.find(' ',sep);
      std::string cserver;
      ip_addr_t serverip;
      if (next == std::string::npos)
        {
        // The last one
        cserver = std::string(servers,sep,string::npos);
        sep = string::npos;
        }
      else
        {
        // One of many
        cserver = std::string(servers,sep,next-sep);
        sep = next+1;
        }
      ip4_addr_set_u32(ip_2_ip4(&serverip), ipaddr_addr(cserver.c_str()));
      serverip.type = IPADDR_TYPE_V4;
      if (!ip_addr_cmp(&serverip, &m_previous_dns[spos]))
        {
        m_previous_dns[spos] = serverip;
        ESP_LOGI(TAG, "Set DNS#%d %s",spos,cserver.c_str());
        }
      dns_setserver(spos++, &serverip);
      }
    for (;spos<DNS_MAX_SERVERS;spos++)
      {
      dns_setserver(spos, IP_ADDR_ANY);
      }
    }
  else if (dnsstore)
    {
    // Set stored DNS servers:
    for (int i=0; i<DNS_MAX_SERVERS; i++)
      {
      if (ip_addr_cmp(&dnsstore[i], &m_previous_dns[i]))
        {
        m_previous_dns[i] = dnsstore[i];
        ESP_LOGI(TAG, "Set DNS#%d %s", i, inet_ntoa(dnsstore[i]));
        }
      dns_setserver(i, &dnsstore[i]);
      }
    }
  }

void OvmsNetManager::SetNetType(std::string type)
  {
  if (type == "wifi")
    {
    StdMetrics.ms_m_net_type->SetValue(type);
#ifdef CONFIG_OVMS_COMP_WIFI
    if (MyPeripherals && MyPeripherals->m_esp32wifi)
      MyPeripherals->m_esp32wifi->UpdateNetMetrics();
#endif // CONFIG_OVMS_COMP_WIFI
    }
  else if (type == "modem")
    {
    StdMetrics.ms_m_net_type->SetValue(type);
#ifdef CONFIG_OVMS_COMP_CELLULAR
    if (MyPeripherals && MyPeripherals->m_cellular_modem)
      MyPeripherals->m_cellular_modem->UpdateNetMetrics();
#endif // CONFIG_OVMS_COMP_CELLULAR
    }
  else
    {
      StdMetrics.ms_m_net_type->SetValue("none");
      StdMetrics.ms_m_net_provider->SetValue("");
      StdMetrics.ms_m_net_sq->SetValue(0, dbm);
    }
  }

void SafePrioritiseAndIndicate(void* ctx)
  {
  MyNetManager.DoSafePrioritiseAndIndicate();
  }

void OvmsNetManager::PrioritiseAndIndicate()
  {
  if (tcpip_callback_with_block(SafePrioritiseAndIndicate, NULL, 1) == ERR_OK)
    m_tcpip_callback_done.Take();
  }

void OvmsNetManager::DoSafePrioritiseAndIndicate()
  {
  const char *search = NULL;
  ip_addr_t* dns = NULL;

  // A convenient place to keep track of connectivity in general
  m_connected_any = m_connected_wifi || m_connected_modem;
  m_network_any = m_connected_wifi || m_connected_modem || m_wifi_ap;
  StdMetrics.ms_m_net_connected->SetValue(m_connected_any);


  // Priority order...
  if (m_connected_wifi)
    {
    // Wifi is up
    SetNetType("wifi");
    search = "st";
    dns = m_dns_wifi;
    m_has_ip = MyPeripherals->m_esp32wifi->WifiHasIp();
    }
  else if (m_connected_modem)
    {
    // Modem is up
    SetNetType("modem");
    search = "pp";
    dns = m_dns_modem;
    m_has_ip = MyPeripherals->m_cellular_modem->ModemIsNetMode() &&
              MyPeripherals->m_cellular_modem->m_mux->IsMuxUp() &&
              MyPeripherals->m_cellular_modem->m_ppp->m_connected;
    }
  StdMetrics.ms_m_net_ip->SetValue(m_has_ip);

  if (search == NULL)
    {
    SetNetType("none");
    m_tcpip_callback_done.Give();
    return;
    }

  for (struct netif *pri = netif_list; pri != NULL; pri=pri->next)
    {
    if ((pri->name[0]==search[0])&&
        (pri->name[1]==search[1]))
      {
      if (search[0] != m_previous_name[0] || search[1] != m_previous_name[1])
        {
        ESP_LOGI(TAG, "Interface priority is %c%c%d (" IPSTR "/" IPSTR " gateway " IPSTR ")",
          pri->name[0], pri->name[1], pri->num,
          IP2STR(&pri->ip_addr.u_addr.ip4), IP2STR(&pri->netmask.u_addr.ip4), IP2STR(&pri->gw.u_addr.ip4));
        m_previous_name[0] = search[0];
        m_previous_name[1] = search[1];
        for (int i=0; i<DNS_MAX_SERVERS; i++)
          m_previous_dns[i] = (ip_addr_t)ip_addr_any;
        }
      netif_set_default(pri);
      SetDNSServer(dns);
      m_tcpip_callback_done.Give();
      return;
      }
    }
  ESP_LOGE(TAG, "Inconsistent state: no interface of type '%s' found", search);
  m_tcpip_callback_done.Give();
  }

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

static void MongooseRawTask(void *pvParameters)
  {
  OvmsNetManager* me = (OvmsNetManager*)pvParameters;
  me->MongooseTask();
  }

void OvmsNetManager::MongooseTask()
  {
  int pri, lastpri = CONFIG_OVMS_NETMAN_TASK_PRIORITY;

  // Initialise the mongoose manager
  ESP_LOGD(TAG, "MongooseTask starting");
  mg_mgr_init(&m_mongoose_mgr, NULL);
  MyEvents.SignalEvent("network.mgr.init",NULL);

  m_mongoose_running = true;

  // Main event loop
  while (!m_mongoose_stopping)
    {
    // poll interfaces:
    if (mg_mgr_poll(&m_mongoose_mgr, 250) == 0)
      {
      ESP_LOGD(TAG, "MongooseTask: no interfaces available => exit");
      break;
      }

    // check for netmanager control jobs:
    ProcessJobs();

    // Detect broken mutex priority inheritance:
    // (may be removed if solved by esp-idf commit 22d636b7b0d6006e06b5b3cfddfbf6e2cf69b4b8)
    if ((pri = uxTaskPriorityGet(NULL)) != lastpri)
      {
      ESP_LOGD(TAG, "MongooseTask: priority changed from %d to %d", lastpri, pri);
      lastpri = pri;
      }
    }

  m_mongoose_running = false;

  // Shutdown cleanly
  ESP_LOGD(TAG, "MongooseTask stopping");
  MyEvents.SignalEvent("network.mgr.stop",NULL);
  mg_mgr_free(&m_mongoose_mgr);
  m_mongoose_task = NULL;
  vTaskDelete(NULL);
  }

struct mg_mgr* OvmsNetManager::GetMongooseMgr()
  {
  return &m_mongoose_mgr;
  }

bool OvmsNetManager::MongooseRunning()
  {
  return m_mongoose_running && !m_mongoose_stopping;
  }

void OvmsNetManager::StartMongooseTask()
  {
  if (m_network_any && (!m_mongoose_task || m_mongoose_stopping))
    {
    // wait for previous task to finish shutting down:
    int wait_time = 0;
    while (m_mongoose_stopping && m_mongoose_task)
      {
      if (wait_time % 1000 == 0)
        ESP_LOGD(TAG, "StartMongooseTask: waiting for task shutdown");
      vTaskDelay(pdMS_TO_TICKS(50));
      wait_time += 50;
      if (wait_time >= 5000)
        {
        // we're stuck, no other way to solve than rebooting:
        ESP_LOGE(TAG, "StartMongooseTask: timeout on task shutdown -- reboot");
        MyBoot.Restart();
        return;
        }
      }
    // start new task:
    m_mongoose_stopping = false;
    xTaskCreatePinnedToCore(MongooseRawTask, "OVMS NetMan",10*1024, (void*)this,
                            CONFIG_OVMS_NETMAN_TASK_PRIORITY, &m_mongoose_task, CORE(1));
    AddTaskToMap(m_mongoose_task);
    }
  }

void OvmsNetManager::StopMongooseTask()
  {
  if (!m_network_any && m_mongoose_running)
    {
    ESP_LOGD(TAG, "StopMongooseTask: requesting task shutdown");
    m_mongoose_stopping = true;
    }
  }

void OvmsNetManager::ProcessJobs()
  {
  netman_job_t* job;
  while (xQueueReceive(m_jobqueue, &job, 0) == pdTRUE)
    {
    ESP_LOGD(TAG, "MongooseTask: got cmd %d from %p", job->cmd, job->caller);
    switch (job->cmd)
      {
      case nmc_none:
        break;
      case nmc_list:
        job->list.cnt = ListConnections(job->list.verbosity, job->list.buf);
        break;
      case nmc_close:
        job->close.cnt = CloseConnection(job->close.id);
        break;
      case nmc_cleanup:
        job->cleanup.cnt = CleanupConnections();
        break;
      default:
        ESP_LOGW(TAG, "MongooseTask: got unknown cmd %d from %p", job->cmd, job->caller);
      }
    if (job->caller)
      xTaskNotifyGive(job->caller);
    ESP_LOGD(TAG, "MongooseTask: done cmd %d from %p", job->cmd, job->caller);
    }
  }

bool OvmsNetManager::ExecuteJob(netman_job_t* job, TickType_t timeout /*=portMAX_DELAY*/)
  {
  if (!MongooseRunning())
    return false;
  if (timeout)
    {
    job->caller = xTaskGetCurrentTaskHandle();
    xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, 0); // clear state
    }
  else
    job->caller = 0;
  ESP_LOGD(TAG, "send cmd %d from %p", job->cmd, job->caller);
  if (xQueueSend(m_jobqueue, &job, timeout) != pdTRUE)
    {
    ESP_LOGW(TAG, "ExecuteJob: cmd %d: queue overflow", job->cmd);
    return false;
    }
  if (timeout && ulTaskNotifyTake(pdTRUE, timeout) == 0)
    {
    // try to prevent delayed processing (cannot stop if already started):
    netman_cmd_t cmd = job->cmd;
    job->cmd = nmc_none;
    job->caller = 0;
    ESP_LOGW(TAG, "ExecuteJob: cmd %d: timeout", cmd);
    return false;
    }
  ESP_LOGD(TAG, "done cmd %d from %p", job->cmd, job->caller);
  return true;
  }

void OvmsNetManager::ScheduleCleanup()
  {
  static netman_job_t job;
  memset(&job, 0, sizeof(job));
  job.cmd = nmc_cleanup;
  ExecuteJob(&job, 0);
  }

int OvmsNetManager::ListConnections(int verbosity, OvmsWriter* writer)
  {
  if (!MongooseRunning())
    return 0;
  mg_connection *c;
  int cnt = 0;
  char local[48], remote[48];
  writer->printf("ID        Flags     Handler   Local                  Remote\n");
  for (c = mg_next(&m_mongoose_mgr, NULL); c; c = mg_next(&m_mongoose_mgr, c))
    {
    if (c->flags & MG_F_LISTENING)
      continue;
    mg_conn_addr_to_str(c, local, sizeof(local), MG_SOCK_STRINGIFY_IP|MG_SOCK_STRINGIFY_PORT);
    mg_conn_addr_to_str(c, remote, sizeof(remote), MG_SOCK_STRINGIFY_IP|MG_SOCK_STRINGIFY_PORT|MG_SOCK_STRINGIFY_REMOTE);
    writer->printf("%08" PRIx32 "  %08" PRIx32 "  %08" PRIx32 "  %-21s  %s\n", (uint32_t)c, (uint32_t)c->flags, (uint32_t)c->user_data, local, remote);
    cnt++;
    }
  return cnt;
  }

int OvmsNetManager::CloseConnection(uint32_t id)
  {
  if (!MongooseRunning())
    return 0;
  mg_connection *c;
  int cnt = 0;
  for (c = mg_next(&m_mongoose_mgr, NULL); c; c = mg_next(&m_mongoose_mgr, c))
    {
    if (c->flags & MG_F_LISTENING)
      continue;
    if (id == 0 || c == (mg_connection*)id)
      {
      c->flags |= MG_F_CLOSE_IMMEDIATELY;
      cnt++;
      }
    }
  return cnt;
  }

int OvmsNetManager::CleanupConnections()
  {
  if (!MongooseRunning())
    return 0;

  struct netif *ni;
  mg_connection *c;
#if ESP_IDF_VERSION_MAJOR >= 5
  wifi_sta_mac_ip_list_t ap_ip_list;
#elif ESP_IDF_VERSION_MAJOR >= 4
  esp_netif_sta_list_t ap_ip_list;
#else
  tcpip_adapter_sta_list_t ap_ip_list;
#endif
  union socket_address sa;
  socklen_t slen = sizeof(sa);
  int cnt = 0;

  // get IP addresses of stations connected to our wifi AP:
  ap_ip_list.num = 0;
#ifdef CONFIG_OVMS_COMP_WIFI
  if (m_wifi_ap)
    {
    wifi_sta_list_t sta_list;
    if (esp_wifi_ap_get_sta_list(&sta_list) != ESP_OK)
      ESP_LOGW(TAG, "CleanupConnections: can't get AP station list");

#if ESP_IDF_VERSION_MAJOR >= 5
    else if (esp_wifi_ap_get_sta_list_with_ip(&sta_list, &ap_ip_list) != ESP_OK)
#elif ESP_IDF_VERSION_MAJOR >= 4
    else if (esp_netif_get_sta_list(&sta_list, &ap_ip_list) != ESP_OK)
#else
    else if (tcpip_adapter_get_sta_list(&sta_list, &ap_ip_list) != ESP_OK)
#endif
      ESP_LOGW(TAG, "CleanupConnections: can't get AP station IP list");
    }
#endif // #ifdef CONFIG_OVMS_COMP_WIFI

  for (c = mg_next(&m_mongoose_mgr, NULL); c; c = mg_next(&m_mongoose_mgr, c))
    {
    if (c->flags & MG_F_LISTENING)
      continue;

    // get local address:
    memset(&sa, 0, sizeof(sa));
    if (getsockname(c->sock, &sa.sa, &slen) != 0)
      {
      ESP_LOGW(TAG, "CleanupConnections: conn %08" PRIx32 ": getsockname failed", (uint32_t)c);
      continue;
      }

    if (sa.sa.sa_family != AF_INET || sa.sin.sin_addr.s_addr == IPADDR_ANY || sa.sin.sin_addr.s_addr == IPADDR_NONE)
      continue;

    // find interface:
    for (ni = netif_list; ni; ni = ni->next)
      {
      if (sa.sin.sin_addr.s_addr == ip4_addr_get_u32(netif_ip4_addr(ni)))
        break;
      }

    if (ni)
      ESP_LOGD(TAG, "CleanupConnections: conn %08" PRIx32 " -> iface %c%c%d", (uint32_t)c, ni->name[0], ni->name[1], ni->num);
    else
      ESP_LOGD(TAG, "CleanupConnections: conn %08" PRIx32 " -> no iface", (uint32_t)c);

    if (!ni || !(ni->flags & NETIF_FLAG_UP) || !(ni->flags & NETIF_FLAG_LINK_UP))
      {
      ESP_LOGI(TAG, "CleanupConnections: closing conn %08" PRIx32 ": interface/link down", (uint32_t)c);
      c->flags |= MG_F_CLOSE_IMMEDIATELY;
      cnt++;
      continue;
      }

    // check remote address on wifi AP:
    if (ni->name[0] == 'a' && ni->name[1] == 'p')
      {
      memset(&sa, 0, sizeof(sa));
      if (getpeername(c->sock, &sa.sa, &slen) != 0)
        {
        ESP_LOGW(TAG, "CleanupConnections: conn %08" PRIx32 ": getpeername failed", (uint32_t)c);
        continue;
        }
      int i;
      for (i = 0; i < ap_ip_list.num; i++)
        {
        if (sa.sin.sin_addr.s_addr == ap_ip_list.sta[i].ip.addr)
          break;
        }

      if (i < ap_ip_list.num)
        {
        ESP_LOGD(TAG, "CleanupConnections: conn %08" PRIx32 " -> AP IP " IPSTR, (uint32_t)c, IP2STR(&ap_ip_list.sta[i].ip));
        }
      else
        {
        ESP_LOGI(TAG, "CleanupConnections: closing conn %08" PRIx32 ": AP peer disconnected", (uint32_t)c);
        c->flags |= MG_F_CLOSE_IMMEDIATELY;
        cnt++;
        }
      } // AP remotes
    } // connection loop
  return cnt;
  }

bool OvmsNetManager::IsNetManagerTask()
  {
  // Return TRUE if the currently running task is the Net Manager task
  extern void *pxCurrentTCB[portNUM_PROCESSORS];

  return (m_mongoose_task == pxCurrentTCB[xPortGetCoreID()]);
  }

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
