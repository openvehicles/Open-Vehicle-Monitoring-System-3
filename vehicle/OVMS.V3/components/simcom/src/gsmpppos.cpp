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
static const char *TAG = "gsm-ppp";

#include "gsmpppos.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_events.h"

static u32_t GsmPPPOS_OutputCallback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx)
  {
  GsmPPPOS* me = (GsmPPPOS*)ctx;

  MyCommandApp.HexDump("SIMCOM ppp tx",(const char*)data, len);
  return me->m_mux->tx(me->m_channel, data, len);
  }

static void GsmPPPOS_StatusCallback(ppp_pcb *pcb, int err_code, void *ctx)
  {
  GsmPPPOS* me = (GsmPPPOS*)ctx;
  struct netif *pppif = ppp_netif(pcb);

  switch (err_code)
    {
    case PPPERR_NONE:
      {
      me->m_connected = true;
      ESP_LOGI(TAG, "status_cb: Connected");
#if PPP_IPV4_SUPPORT
      ESP_LOGI(TAG, "   our_ipaddr  = %s", ipaddr_ntoa(&pppif->ip_addr));
      ESP_LOGI(TAG, "   his_ipaddr  = %s", ipaddr_ntoa(&pppif->gw));
      ESP_LOGI(TAG, "   netmask     = %s", ipaddr_ntoa(&pppif->netmask));
#endif /* PPP_IPV4_SUPPORT */
#if PPP_IPV6_SUPPORT
      ESP_LOGI(TAG, "   our6_ipaddr = %s", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
#endif /* PPP_IPV6_SUPPORT */
      MyEvents.SignalEvent("system.modem.gotip",NULL);
      return;
      break;
      }
    case PPPERR_PARAM:
      {
      ESP_LOGE(TAG, "status_cb: Invalid parameter\n");
      break;
      }
    case PPPERR_OPEN:
      {
      ESP_LOGE(TAG, "status_cb: Unable to open PPP session\n");
      break;
      }
    case PPPERR_DEVICE:
      {
      ESP_LOGE(TAG, "status_cb: Invalid I/O device for PPP\n");
      break;
      }
    case PPPERR_ALLOC:
      {
      ESP_LOGE(TAG, "status_cb: Unable to allocate resources\n");
      break;
      }
    case PPPERR_USER:
      {
      ESP_LOGE(TAG, "status_cb: User interrupt\n");
      break;
      }
    case PPPERR_CONNECT:
      {
      ESP_LOGE(TAG, "status_cb: Connection lost\n");
      break;
      }
    case PPPERR_AUTHFAIL:
      {
      ESP_LOGE(TAG, "status_cb: Failed authentication challenge\n");
      break;
      }
    case PPPERR_PROTOCOL:
      {
      ESP_LOGE(TAG, "status_cb: Failed to meet protocol\n");
      break;
      }
    case PPPERR_PEERDEAD:
      {
      ESP_LOGE(TAG, "status_cb: Connection timeout\n");
      break;
      }
    case PPPERR_IDLETIMEOUT:
      {
      ESP_LOGE(TAG, "status_cb: Idle Timeout\n");
      break;
      }
    case PPPERR_CONNECTTIME:
      {
      ESP_LOGE(TAG, "status_cb: Max connect time reached\n");
      break;
      }
    case PPPERR_LOOPBACK:
      {
      ESP_LOGE(TAG, "status_cb: Loopback detected\n");
      break;
      }
    default:
      {
      ESP_LOGE(TAG, "status_cb: Unknown error code %d\n", err_code);
      break;
      }
    }

  me->m_connected = false;
  MyEvents.SignalEvent("system.modem.down",NULL);

  /* ppp_close() was previously called, don't reconnect */
  if (err_code == PPPERR_USER)
    {
    pppapi_free(me->m_ppp);
    me->m_ppp = NULL;
    return;
    }

  /*
   * Try to reconnect in 30 seconds, if you need a modem chatscript you have
   * to do a much better signaling here ;-)
   */
  //ppp_connect(pcb, 30);
  /* OR ppp_listen(pcb); */
  }

GsmPPPOS::GsmPPPOS(GsmMux* mux, int channel)
  {
  m_mux = mux;
  m_channel = channel;
  m_ppp = NULL;
  m_connected = false;
  }

GsmPPPOS::~GsmPPPOS()
  {
  }

void GsmPPPOS::IncomingData(uint8_t *data, size_t len)
  {
  MyCommandApp.HexDump("SIMCOM ppp rx",(const char*)data,len);
  pppos_input_tcpip(m_ppp, (u8_t*)data, (int)len);
  }

void GsmPPPOS::Startup()
  {
  tcpip_adapter_init();
  m_ppp = pppapi_pppos_create(&m_ppp_netif,
            GsmPPPOS_OutputCallback, GsmPPPOS_StatusCallback, this);
  if (m_ppp == NULL)
    {
    ESP_LOGE(TAG, "Error init pppos");
    return;
    }
  pppapi_set_default(m_ppp);
  pppapi_set_auth(m_ppp, PPPAUTHTYPE_PAP,
    MyConfig.GetParamValue("modem", "apn.user").c_str(),
    MyConfig.GetParamValue("modem", "apn.password").c_str());
  pppapi_connect(m_ppp, 0);
  }

void GsmPPPOS::Shutdown()
  {
  if (m_ppp)
    {
    u8_t nocarrier = 0;
    pppapi_close(m_ppp, nocarrier);
    }
  else
    m_connected = false;
  }
