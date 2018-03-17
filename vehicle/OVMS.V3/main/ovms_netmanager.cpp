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

OvmsNetManager MyNetManager __attribute__ ((init_priority (8999)));

void network_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  struct netif *ni = netif_list;
  while (ni)
    {
    writer->printf("Interface#%d: %c%c%d (ifup=%d linkup=%d)\n",
      ni->num,ni->name[0],ni->name[1],ni->num,
      ((ni->flags & NETIF_FLAG_UP) != 0),
      ((ni->flags & NETIF_FLAG_LINK_UP) != 0));
    writer->printf("  IPv4: " IPSTR "/" IPSTR " gateway " IPSTR "\n",
      IP2STR(&ni->ip_addr.u_addr.ip4), IP2STR(&ni->netmask.u_addr.ip4), IP2STR(&ni->gw.u_addr.ip4));
    ni = ni->next;
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
  MyEvents.RegisterEvent(TAG,"network.interface.up", std::bind(&OvmsNetManager::InterfaceUp, this, _1, _2));

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
  m_connected_any = m_connected_wifi || m_connected_modem;
  SetInterfacePriority();
  StandardMetrics.ms_m_net_type->SetValue("wifi");
#ifdef CONFIG_OVMS_COMP_WIFI
  StandardMetrics.ms_m_net_provider->SetValue(MyPeripherals->m_esp32wifi->GetSSID());
#endif // #ifdef CONFIG_OVMS_COMP_WIFI
  MyEvents.SignalEvent("network.wifi.up",NULL);
  MyEvents.SignalEvent("network.up",NULL);
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  StartMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  }

void OvmsNetManager::WifiDownSTA(std::string event, void* data)
  {
  if (m_connected_wifi)
    {
    m_connected_wifi = false;
    m_connected_any = m_connected_wifi || m_connected_modem;
    SetInterfacePriority();
    MyEvents.SignalEvent("network.wifi.down",NULL);
    if (m_connected_any)
      MyEvents.SignalEvent("network.reconfigured",NULL);
    else
      {
      StandardMetrics.ms_m_net_type->SetValue("none");
      StandardMetrics.ms_m_net_provider->SetValue("");
      MyEvents.SignalEvent("network.down",NULL);
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      StopMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      }
    }
  }

void OvmsNetManager::WifiUpAP(std::string event, void* data)
  {
  m_wifi_ap = true;
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  StartMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  }

void OvmsNetManager::WifiDownAP(std::string event, void* data)
  {
  m_wifi_ap = false;
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  StopMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  }

void OvmsNetManager::ModemUp(std::string event, void* data)
  {
  m_connected_modem = true;
  m_connected_any = m_connected_wifi || m_connected_modem;
  SetInterfacePriority();
  StandardMetrics.ms_m_net_type->SetValue("modem");
  MyEvents.SignalEvent("network.modem.up",NULL);
  MyEvents.SignalEvent("network.up",NULL);
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  StartMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  }

void OvmsNetManager::ModemDown(std::string event, void* data)
  {
  if (m_connected_modem)
    {
    m_connected_modem = false;
    m_connected_any = m_connected_wifi || m_connected_modem;
    SetInterfacePriority();
    MyEvents.SignalEvent("network.modem.down",NULL);
    if (m_connected_any)
      MyEvents.SignalEvent("network.reconfigured",NULL);
    else
      {
      StandardMetrics.ms_m_net_type->SetValue("none");
      StandardMetrics.ms_m_net_provider->SetValue("");
      MyEvents.SignalEvent("network.down",NULL);
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      StopMongooseTask();
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
      }
    }
  }

void OvmsNetManager::InterfaceUp(std::string event, void* data)
  {
  // A network interface has come up. We need to set DNS, if necessary
  std::string servers = MyConfig.GetParamValue("network", "dns");
  if (servers.empty()) return;

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

void OvmsNetManager::SetInterfacePriority()
  {
  const char *search = NULL;

  // Priority order...
  if (m_connected_wifi)
    {
    // Wifi is up
    search = "st";
    }
  else if (m_connected_modem)
    {
    // Modem is up
    search = "pp";
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
  mg_mgr_init(&m_mongoose_mgr, NULL);
  MyEvents.SignalEvent("network.mgr.init",NULL);

  // Main event loop
  while (m_mongoose_running)
    {
    mg_mgr_poll(&m_mongoose_mgr, 250);
    }

  // Shutdown cleanly
  MyEvents.SignalEvent("network.mgr.stop",NULL);
  mg_mgr_free(&m_mongoose_mgr);
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
    m_mongoose_running = true;
    xTaskCreatePinnedToCore(MongooseRawTask, "NetManTask", 7*1024, (void*)this, 5, &m_mongoose_task, 1);
    }
  }

void OvmsNetManager::StopMongooseTask()
  {
  m_mongoose_running = false;
  }

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
