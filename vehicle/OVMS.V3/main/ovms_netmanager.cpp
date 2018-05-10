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
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip/dns.h>
#include "metrics_standard.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_module.h"

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
    ip_addr_t srv = dns_getserver(k);
    if (! ip_addr_isany(&srv))
      {
      dnsservers++;
      if (srv.type == IPADDR_TYPE_V4)
        writer->printf(" " IPSTR, IP2STR(&srv.u_addr.ip4));
      else if (srv.type == IPADDR_TYPE_V6)
        writer->printf(" " IPSTR, IP2STR(&srv.u_addr.ip6));
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

OvmsNetManager::OvmsNetManager()
  {
  ESP_LOGI(TAG, "Initialising NETMANAGER (8999)");
  m_connected_wifi = false;
  m_connected_modem = false;
  m_connected_any = false;
  m_wifi_ap = false;
  m_network_any = false;

  for (int i=0; i<DNS_MAX_SERVERS; i++)
    {
    m_dns_modem[i] = (ip_addr_t)ip_addr_any;
    m_dns_wifi[i] = (ip_addr_t)ip_addr_any;
    }

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  m_mongoose_task = 0;
  m_mongoose_running = false;
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

  // Register our commands
  OvmsCommand* cmd_network = MyCommandApp.RegisterCommand("network","NETWORK framework",network_status, "", 0, 1);
  cmd_network->RegisterCommand("status","Show network status",network_status, "", 0, 0);

  // Register our events
  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.gotip", std::bind(&OvmsNetManager::WifiUpSTA, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.start", std::bind(&OvmsNetManager::WifiUpAP, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.stop", std::bind(&OvmsNetManager::WifiDownSTA, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.ap.stop", std::bind(&OvmsNetManager::WifiDownAP, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.sta.disconnected", std::bind(&OvmsNetManager::WifiDownSTA, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.wifi.down", std::bind(&OvmsNetManager::WifiDownSTA, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.gotip", std::bind(&OvmsNetManager::ModemUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.stop", std::bind(&OvmsNetManager::ModemDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.down", std::bind(&OvmsNetManager::ModemDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&OvmsNetManager::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.shuttingdown", std::bind(&OvmsNetManager::EventSystemShuttingDown, this, _1, _2));

  MyConfig.RegisterParam("network", "Network Configuration", true, true);
  // Our instances:
  //   'dns': Space-separated list of DNS servers
  }

OvmsNetManager::~OvmsNetManager()
  {
  }

void OvmsNetManager::WifiUpSTA(std::string event, void* data)
  {
  m_connected_wifi = true;
  SaveDNSServer(m_dns_wifi);
  PrioritiseAndIndicate();

  StandardMetrics.ms_m_net_type->SetValue("wifi");
#ifdef CONFIG_OVMS_COMP_WIFI
  StandardMetrics.ms_m_net_provider->SetValue(MyPeripherals->m_esp32wifi->GetSSID());
#endif // #ifdef CONFIG_OVMS_COMP_WIFI
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
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

  MyEvents.SignalEvent("network.interface.change",NULL);
  }

void OvmsNetManager::WifiDownSTA(std::string event, void* data)
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
      }
    else
      {
      ESP_LOGI(TAG, "WIFI client down (with MODEM down): network connectivity has been lost");
      StandardMetrics.ms_m_net_type->SetValue("none");
      StandardMetrics.ms_m_net_provider->SetValue("");
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

void OvmsNetManager::ModemUp(std::string event, void* data)
  {
  m_connected_modem = true;
  SaveDNSServer(m_dns_modem);
  PrioritiseAndIndicate();

  StandardMetrics.ms_m_net_type->SetValue("modem");
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
      }
    else
      {
      ESP_LOGI(TAG, "MODEM down (with WIFI client down): network connectivity has been lost");
      StandardMetrics.ms_m_net_type->SetValue("none");
      StandardMetrics.ms_m_net_provider->SetValue("");
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
    }
  }

void OvmsNetManager::ConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* param = (OvmsConfigParam*)data;
  if (param && param->GetName() == "network")
    {
    // Network config has been changed, apply:
    if (m_network_any)
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
    dnsstore[i] = dns_getserver(i);
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
      ESP_LOGI(TAG, "Set DNS#%d %s",spos,cserver.c_str());
      ip4_addr_set_u32(ip_2_ip4(&serverip), ipaddr_addr(cserver.c_str()));
      serverip.type = IPADDR_TYPE_V4;
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
      ESP_LOGI(TAG, "Set DNS#%d %s", i, inet_ntoa(dnsstore[i]));
      dns_setserver(i, &dnsstore[i]);
      }
    }
  }

void OvmsNetManager::PrioritiseAndIndicate()
  {
  const char *search = NULL;
  ip_addr_t* dns = NULL;

  // A convenient place to keep track of connectivity in general
  m_connected_any = m_connected_wifi || m_connected_modem;
  m_network_any = m_connected_wifi || m_connected_modem || m_wifi_ap;

  // Priority order...
  if (m_connected_wifi)
    {
    // Wifi is up
    search = "st";
    dns = m_dns_wifi;
    }
  else if (m_connected_modem)
    {
    // Modem is up
    search = "pp";
    dns = m_dns_modem;
    }

  if (search == NULL) return;
  for (struct netif *pri = netif_list; pri != NULL; pri=pri->next)
    {
    if ((pri->name[0]==search[0])&&
        (pri->name[1]==search[1]))
      {
      ESP_LOGI(TAG, "Interface priority is %c%c%d (" IPSTR "/" IPSTR " gateway " IPSTR ")",
        pri->name[0], pri->name[1], pri->num,
        IP2STR(&pri->ip_addr.u_addr.ip4), IP2STR(&pri->netmask.u_addr.ip4), IP2STR(&pri->gw.u_addr.ip4));
      netif_set_default(pri);
      SetDNSServer(dns);
      return;
      }
    }
  }

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

static void MongooseRawTask(void *pvParameters)
  {
  OvmsNetManager* me = (OvmsNetManager*)pvParameters;
  me->MongooseTask();
  }

void OvmsNetManager::MongooseTask()
  {
  // Initialise the mongoose manager
  ESP_LOGD(TAG, "MongooseTask starting");
  mg_mgr_init(&m_mongoose_mgr, NULL);
  MyEvents.SignalEvent("network.mgr.init",NULL);

  // Main event loop
  while (m_mongoose_running)
    {
    mg_mgr_poll(&m_mongoose_mgr, 250);
    }

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
  return m_mongoose_running;
  }

void OvmsNetManager::StartMongooseTask()
  {
  if (!m_mongoose_running)
    {
    if (m_network_any)
      {
      // wait for previous task to finish shutting down:
      while (m_mongoose_task)
        vTaskDelay(pdMS_TO_TICKS(50));
      // start new task:
      m_mongoose_running = true;
      xTaskCreatePinnedToCore(MongooseRawTask, "OVMS NetMan", 7*1024, (void*)this, 5, &m_mongoose_task, 1);
      AddTaskToMap(m_mongoose_task);
      }
    }
  }

void OvmsNetManager::StopMongooseTask()
  {
  if (!m_network_any)
    m_mongoose_running = false;
  }

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
