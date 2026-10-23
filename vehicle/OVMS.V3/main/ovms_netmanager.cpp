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
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>
#include <lwip/ip.h>
#include "metrics_standard.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_module.h"
#include "ovms_boot.h"

#ifndef CONFIG_OVMS_NETMAN_TASK_PRIORITY
#define CONFIG_OVMS_NETMAN_TASK_PRIORITY 5
#endif

OvmsNetManager MyNetManager __attribute__ ((init_priority (8999)));

void network_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::vector<struct netif> netiflist;
  int netifdefault;

  if (MyNetManager.GetNetifList(netiflist, netifdefault) != ESP_OK)
    {
    writer->puts("ERROR: cannot get netif list!");
    return;
    }

  for (auto ni : netiflist)
    {
    if (ni.name[0]=='l' && ni.name[1]=='o')
      continue;
    writer->printf("Interface#%d: %c%c%d (ifup=%d linkup=%d)\n",
      ni.num,ni.name[0],ni.name[1],ni.num,
      ((ni.flags & NETIF_FLAG_UP) != 0),
      ((ni.flags & NETIF_FLAG_LINK_UP) != 0));
    writer->printf("  IPv4: " IPSTR "/" IPSTR " gateway " IPSTR "\n",
      IP2STR(&ni.ip_addr.u_addr.ip4), IP2STR(&ni.netmask.u_addr.ip4), IP2STR(&ni.gw.u_addr.ip4));
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

  if (netifdefault >= 0)
    {
    struct netif& ni = netiflist[netifdefault];
    writer->printf("\nDefault Interface: %c%c%d (" IPSTR "/" IPSTR " gateway " IPSTR ")\n",
      ni.name[0], ni.name[1], ni.num,
      IP2STR(&ni.ip_addr.u_addr.ip4),
      IP2STR(&ni.netmask.u_addr.ip4),
      IP2STR(&ni.gw.u_addr.ip4));
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

// Resolve an interface name like "st1" or "pp2" to its struct netif*.
// Returns nullptr if not found.
static struct netif* resolve_iface(const char* name)
  {
  for (struct netif* n = netif_list; n != nullptr; n = n->next)
    {
    char nname[8];
    snprintf(nname, sizeof(nname), "%c%c%u", n->name[0], n->name[1], n->num);
    if (strcmp(nname, name) == 0) return n;
    }
  return nullptr;
  }

void network_ping(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* host = nullptr;
  const char* iface_name = nullptr;
  int count = 4;
  int payload_size = 32;
  const int timeout_ms = 2000;
  const int interval_ms = 1000;

  for (int i = 0; i < argc; i++)
    {
    if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) { iface_name = argv[++i]; }
    else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
      {
      count = atoi(argv[++i]);
      if (count < 1) count = 1;
      if (count > 100) count = 100;
      }
    else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
      {
      payload_size = atoi(argv[++i]);
      if (payload_size < 0) payload_size = 0;
      if (payload_size > 1400) payload_size = 1400;
      }
    else if (host == nullptr) { host = argv[i]; }
    }

  if (!host)
    {
    writer->puts("ERROR: missing target host");
    return;
    }

  struct addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_INET;
  struct addrinfo* res = nullptr;
  if (getaddrinfo(host, nullptr, &hint, &res) != 0 || !res)
    {
    writer->printf("ERROR: cannot resolve %s\n", host);
    return;
    }
  struct sockaddr_in target = *(struct sockaddr_in*)res->ai_addr;
  freeaddrinfo(res);
  char target_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &target.sin_addr, target_str, sizeof(target_str));

  struct sockaddr_in src;
  memset(&src, 0, sizeof(src));
  src.sin_family = AF_INET;
  bool bind_src = false;
  if (iface_name)
    {
    struct netif* nif = resolve_iface(iface_name);
    if (!nif)
      {
      writer->printf("ERROR: interface %s not found\n", iface_name);
      return;
      }
    if (ip4_addr_isany_val(*ip_2_ip4(&nif->ip_addr)))
      {
      writer->printf("ERROR: interface %s has no IPv4 address\n", iface_name);
      return;
      }
    src.sin_addr.s_addr = ip_2_ip4(&nif->ip_addr)->addr;
    bind_src = true;
    }

  int s = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
  if (s < 0)
    {
    writer->printf("ERROR: socket() failed: %s\n", strerror(errno));
    return;
    }
  if (bind_src && bind(s, (struct sockaddr*)&src, sizeof(src)) != 0)
    {
    writer->printf("ERROR: bind() failed: %s\n", strerror(errno));
    close(s);
    return;
    }

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  uint16_t ident = (uint16_t)(esp_random() & 0xffff);

  if (bind_src)
    {
    char src_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &src.sin_addr, src_str, sizeof(src_str));
    writer->printf("PING %s (%s) from %s: %d data bytes\n", host, target_str, src_str, payload_size);
    }
  else
    {
    writer->printf("PING %s (%s): %d data bytes\n", host, target_str, payload_size);
    }

  size_t pkt_len = sizeof(struct icmp_echo_hdr) + payload_size;
  uint8_t* pkt = (uint8_t*)malloc(pkt_len);
  if (!pkt)
    {
    writer->puts("ERROR: out of memory");
    close(s);
    return;
    }

  int sent = 0, received = 0;
  uint32_t rtt_min = UINT32_MAX, rtt_max = 0, rtt_sum = 0;

  for (int seq = 0; seq < count; seq++)
    {
    memset(pkt, 0, pkt_len);
    struct icmp_echo_hdr* hdr = (struct icmp_echo_hdr*)pkt;
    ICMPH_TYPE_SET(hdr, ICMP_ECHO);
    ICMPH_CODE_SET(hdr, 0);
    hdr->id = htons(ident);
    hdr->seqno = htons(seq);
    for (int i = 0; i < payload_size; i++)
      pkt[sizeof(struct icmp_echo_hdr) + i] = (uint8_t)(i & 0xff);
    hdr->chksum = 0;
    hdr->chksum = inet_chksum(pkt, pkt_len);

    struct timeval t_send;
    gettimeofday(&t_send, nullptr);

    int n = sendto(s, pkt, pkt_len, 0, (struct sockaddr*)&target, sizeof(target));
    if (n < 0)
      {
      writer->printf("seq=%d sendto failed: %s\n", seq, strerror(errno));
      }
    else
      {
      sent++;
      uint8_t recvbuf[1500];
      bool got_reply = false;
      uint32_t rtt_ms = 0;
      uint8_t recv_ttl = 0;
      while (true)
        {
        struct sockaddr_in from;
        memset(&from, 0, sizeof(from));
        socklen_t flen = sizeof(from);
        int r = recvfrom(s, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&from, &flen);
        if (r <= 0) break;
        if (r < (int)(sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr))) continue;
        struct ip_hdr* iphdr = (struct ip_hdr*)recvbuf;
        size_t iph_len = IPH_HL(iphdr) * 4;
        if (r < (int)(iph_len + sizeof(struct icmp_echo_hdr))) continue;
        struct icmp_echo_hdr* reply = (struct icmp_echo_hdr*)(recvbuf + iph_len);
        if (ICMPH_TYPE(reply) != ICMP_ER) continue;
        if (ntohs(reply->id) != ident) continue;
        if (ntohs(reply->seqno) != seq) continue;
        struct timeval t_recv;
        gettimeofday(&t_recv, nullptr);
        rtt_ms = (uint32_t)((t_recv.tv_sec - t_send.tv_sec) * 1000
                            + (t_recv.tv_usec - t_send.tv_usec) / 1000);
        recv_ttl = IPH_TTL(iphdr);
        got_reply = true;
        break;
        }
      if (got_reply)
        {
        received++;
        if (rtt_ms < rtt_min) rtt_min = rtt_ms;
        if (rtt_ms > rtt_max) rtt_max = rtt_ms;
        rtt_sum += rtt_ms;
        writer->printf("%d bytes from %s: icmp_seq=%d ttl=%u time=%u ms\n",
          payload_size, target_str, seq, (unsigned)recv_ttl, (unsigned)rtt_ms);
        }
      else
        {
        writer->printf("Request timeout for icmp_seq %d\n", seq);
        }
      }

    if (seq < count - 1)
      vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }

  free(pkt);
  close(s);

  writer->printf("\n--- %s ping statistics ---\n", target_str);
  uint32_t loss = (sent > 0) ? (uint32_t)((1.0 - (double)received / sent) * 100) : 0;
  writer->printf("%d packets transmitted, %d received, %u%% packet loss\n",
    sent, received, (unsigned)loss);
  if (received > 0)
    {
    writer->printf("rtt min/avg/max = %u/%u/%u ms\n",
      (unsigned)rtt_min, (unsigned)(rtt_sum / received), (unsigned)rtt_max);
    }
  }

void network_dns(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc < 1)
    {
    writer->puts("ERROR: missing hostname");
    return;
    }
  const char* host = argv[0];

  struct addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  struct addrinfo* res = nullptr;
  int rc = getaddrinfo(host, nullptr, &hint, &res);
  if (rc != 0)
    {
    writer->printf("ERROR: %s: lookup failed (%d)\n", host, rc);
    return;
    }

  writer->printf("DNS lookup: %s\n", host);
  int found = 0;
  for (struct addrinfo* p = res; p != nullptr; p = p->ai_next)
    {
    char addrstr[INET6_ADDRSTRLEN];
    void* addr = nullptr;
    const char* fam = "?";
    if (p->ai_family == AF_INET)
      {
      addr = &((struct sockaddr_in*)p->ai_addr)->sin_addr;
      fam = "A";
      }
    else if (p->ai_family == AF_INET6)
      {
      addr = &((struct sockaddr_in6*)p->ai_addr)->sin6_addr;
      fam = "AAAA";
      }
    else continue;
    inet_ntop(p->ai_family, addr, addrstr, sizeof(addrstr));
    writer->printf("  %s\t%s\n", fam, addrstr);
    found++;
    }
  freeaddrinfo(res);
  if (found == 0)
    writer->puts("(no addresses returned)");
  }

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
  m_cfg_dnslist = "";
  m_cfg_wifi_sq_good = -87;
  m_cfg_wifi_sq_bad = -89;
  m_cfg_wifi_bad_reconnect = false;

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
  m_mongoose_starting = false;
  m_mongoose_stopping = false;
  m_jobqueue = xQueueCreate(CONFIG_OVMS_HW_NETMANAGER_QUEUE_SIZE, sizeof(netman_job_t*));
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

  m_restart_network = false;

  // Register our commands
  OvmsCommand* cmd_network = MyCommandApp.RegisterCommand("network","NETWORK framework",network_status, "", 0, 0, false);
  cmd_network->RegisterCommand("status","Show network status",network_status, "", 0, 0, false);
  cmd_network->RegisterCommand("restart","Restart network",network_restart, "", 0, 0, false);
  cmd_network->RegisterCommand("ping", "Ping (ICMP) a hostname/IP address", network_ping,
    "<host> [-I iface] [-c count] [-s size]\n"
    "  -I iface : source interface name (e.g. st1, pp2)\n"
    "  -c count : number of pings (1-100, default 4)\n"
    "  -s size  : payload bytes (0-1400, default 32)", 1, 7, false);
  cmd_network->RegisterCommand("dns", "DNS lookup of a hostname", network_dns, "<hostname>", 1, 1, false);
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
  if (MongooseRunning() && !IsNetManagerTask())
    {
    // signal task to do the restart:
    ESP_LOGD(TAG, "Requesting network restart");
    m_restart_network = true;
    }
  else
    {
    // perform restart:
    ESP_LOGI(TAG, "Performing network restart");
    m_restart_network = false;
    #ifdef CONFIG_OVMS_COMP_WIFI
      if (MyPeripherals && MyPeripherals->m_esp32wifi)
        MyPeripherals->m_esp32wifi->Restart();
    #endif // #ifdef CONFIG_OVMS_COMP_WIFI

    #ifdef CONFIG_OVMS_COMP_CELLULAR
      if (MyPeripherals && MyPeripherals->m_cellular_modem)
        MyPeripherals->m_cellular_modem->Restart();
    #endif // CONFIG_OVMS_COMP_CELLULAR
    }
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
    if (MyPeripherals && MyPeripherals->m_esp32wifi && m_cfg_wifi_bad_reconnect)
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
  // Mongoose startup requested?
  if (m_mongoose_starting)
    {
    StartMongooseTask();
    }

  // Check for reboot due to continuously lacking network connectivity:
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
    m_cfg_dnslist = MyConfig.GetParamValue("network", "dns");
    m_cfg_reboot_no_connection = MyConfig.GetParamValueBool("network", "reboot.no.ip", false);
    m_cfg_wifi_sq_good = MyConfig.GetParamValueFloat("network", "wifi.sq.good", -87);
    m_cfg_wifi_sq_bad = MyConfig.GetParamValueFloat("network", "wifi.sq.bad",  -89);
    m_cfg_wifi_bad_reconnect = MyConfig.GetParamValueBool("network", "wifi.bad.reconnect");
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
  std::string servers = m_cfg_dnslist;
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

  // Walk through list of network interfaces:
  // (this is safe, as we're in the LwIP context)
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
  int pri, basepri = CONFIG_OVMS_NETMAN_TASK_PRIORITY;

  // Initialise the mongoose manager
  ESP_LOGD(TAG, "MongooseTask starting");
  mg_mgr_init(&m_mongoose_mgr, NULL);

  m_mongoose_starting = false;
  m_mongoose_running = true;
  MyEvents.SignalEvent("network.mgr.init",NULL);

  // Main event loop
  uint32_t busystart = esp_log_timestamp();
  while (!m_mongoose_stopping && !m_restart_network)
    {
    // poll interfaces:
    if (mg_mgr_poll(&m_mongoose_mgr, 100) == 0)
      {
      ESP_LOGD(TAG, "MongooseTask: no interfaces available => exit");
      break;
      }

    // check for netmanager control jobs:
    ProcessJobs();

    // Detect & fix broken mutex priority inheritance:
    // (not solved by esp-idf commit 22d636b7b0d6006e06b5b3cfddfbf6e2cf69b4b8)
    if ((pri = uxTaskPriorityGet(NULL)) != basepri)
      {
      ESP_LOGD(TAG, "MongooseTask: priority changed from %d to %d, reverting", basepri, pri);
      // revert to avoid blocking lower priority tasks:
      vTaskPrioritySet(NULL, basepri);
      }
    
    // avoid busy looping from continuous network activity:
    // mg_mgr_poll() returns immediately if sockets were ready, so force a 10 ms yield per second
    uint32_t now = esp_log_timestamp();
    if (now >= busystart + 1000)
      {
      vTaskDelay(pdMS_TO_TICKS(10));
      busystart = now;
      }
    else
      {
      vTaskDelay(0);
      }
    }

  m_mongoose_running = false;

  // Shutdown cleanly
  ESP_LOGD(TAG, "MongooseTask stopping");
  DiscardJobs();

  // Signal network clients to close & cleanup; to avoid race conditions from concurrent
  // mg_mgr_free() execution, wait for all event listeners to have finished:
    {
    OvmsSemaphore eventdone;
    MyEvents.SignalEvent("network.mgr.stop", NULL, eventdone);
    eventdone.Take();
    }

  // Cleanup mongoose:
  mg_mgr_free(&m_mongoose_mgr);
  memset(&m_mongoose_mgr, 0, sizeof(m_mongoose_mgr)); // clear stale pointers

  if (m_restart_network)
    {
    RestartNetwork();
    }

  uint32_t minstackfree = uxTaskGetStackHighWaterMark(NULL);
  ESP_LOGD(TAG, "MongooseTask done, min stack free=%u", minstackfree);
  m_mongoose_task = NULL;
  vTaskDelete(NULL);
  }

struct mg_mgr* OvmsNetManager::GetMongooseMgr()
  {
  return MongooseRunning() ? &m_mongoose_mgr : NULL;
  }

bool OvmsNetManager::MongooseRunning()
  {
  return m_mongoose_running && !m_mongoose_stopping && !m_restart_network;
  }

void OvmsNetManager::StartMongooseTask()
  {
  if (m_network_any && (!m_mongoose_task || m_mongoose_stopping || m_restart_network))
    {
    // check for previous task still shutting down:
    if ((m_mongoose_stopping || m_restart_network) && m_mongoose_task)
      {
      ESP_LOGD(TAG, "StartMongooseTask: waiting for task shutdown");
      // retry on next ticker.1
      m_mongoose_starting = true;
      return;
      }
    // start new task now:
    m_mongoose_starting = false;
    m_mongoose_stopping = false;
    m_restart_network = false;
    xTaskCreatePinnedToCore(MongooseRawTask, "OVMS NetMan",10*1024, (void*)this,
                            CONFIG_OVMS_NETMAN_TASK_PRIORITY, &m_mongoose_task, CORE(1));
    AddTaskToMap(m_mongoose_task);
    }
  else
    {
    // no network or task already running: cancel start request
    m_mongoose_starting = false;
    }
  }

void OvmsNetManager::StopMongooseTask()
  {
  if (!m_network_any && m_mongoose_running)
    {
    ESP_LOGD(TAG, "StopMongooseTask: requesting task shutdown");
    m_mongoose_starting = false;
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

void OvmsNetManager::DiscardJobs()
  {
  netman_job_t* job;
  while (xQueueReceive(m_jobqueue, &job, 0) == pdTRUE)
    {
    ESP_LOGD(TAG, "MongooseTask: discard cmd %d from %p", job->cmd, job->caller);
    if (job->caller) xTaskNotifyGive(job->caller);
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
  auto mglock = MongooseLock();
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
  auto mglock = MongooseLock();
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
  auto mglock = MongooseLock();

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

  // get snapshot of LwIP network interface list:
  std::vector<struct netif> netiflist;
  int netifdefault;
  if (GetNetifList(netiflist, netifdefault) != ESP_OK)
    {
    ESP_LOGW(TAG, "CleanupConnections: can't get LwIP netif list");
    return 0;
    }

  // walk through Mongoose connections:
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
    struct netif *ni = NULL;
    for (auto candidate : netiflist)
      {
      if (sa.sin.sin_addr.s_addr == ip4_addr_get_u32(netif_ip4_addr(&candidate)))
        {
        ni = &candidate;
        break;
        }
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

/**
 * OvmsNetManager::GetNetifList: get a snapshot (copy) of the LwIP internal network interface list;
 *    this is done via a callback within the LwIP context, to avoid concurrent access
 *    to the LwIP managed netif_list & netif_default.
 * 
 * @param &netiflist      -- ref to vector of netif, will be cleared & filled
 * @param &netifdefault   -- ref to int, will be set to index of default interface in netiflist or -1 if none
 * @result                -- ESP_OK or ESP_FAIL
 */
esp_err_t OvmsNetManager::GetNetifList(std::vector<struct netif>& netiflist, int& netifdefault)
  {
  OvmsSemaphore donesem;

  struct cb_context
    {
    std::vector<struct netif>* netiflist;
    int* netifdefault;
    OvmsSemaphore* donesem;
    } context = { &netiflist, &netifdefault, &donesem };

  auto get_netiflist_cb = [](void* data)
    {
    cb_context* ctx = (cb_context*)data;
    for (struct netif* ni = netif_list; ni; ni = ni->next)
      {
      if (ni == netif_default) *ctx->netifdefault = ctx->netiflist->size();
      ctx->netiflist->push_back(*ni);
      }
    ctx->donesem->Give();
    };

  netiflist.clear();
  netiflist.reserve(4); // lo, st, pp, ap
  netifdefault = -1;

  if (tcpip_callback_with_block(get_netiflist_cb, &context, 1) == ERR_OK)
    {
    donesem.Take();
    return ESP_OK;
    }
  else
    {
    return ESP_FAIL;
    }
  }

bool OvmsNetManager::IsNetManagerTask()
  {
  // Return TRUE if the currently running task is the Net Manager task
  return (m_mongoose_task == xTaskGetCurrentTaskHandle());
  }

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
