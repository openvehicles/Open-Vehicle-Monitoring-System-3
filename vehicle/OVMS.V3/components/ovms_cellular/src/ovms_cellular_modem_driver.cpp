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
static const char *TAG = "cellular-modem-auto";

#include <string.h>

#include "ovms_cellular.h"
#include "ovms_config.h"
#include "simcom_powering.h"

////////////////////////////////////////////////////////////////////////////////
// The virtual modem driver.
// This base implementation simply provides defaults and enough to identify
// the actual modem model number. Once the model has been identified, the
// driver will be switched to a specific version.

const char model[] = "auto";
const char name[] = "Auto-detect modem (used for identification purposes only)";

class autoInit
  {
  public: autoInit();
} MyAutoInit  __attribute__ ((init_priority (4650)));

autoInit::autoInit()
  {
  ESP_LOGI(TAG, "Registering auto-detect modem driver (4650)");
  MyCellularModemFactory.RegisterCellularModemDriver<modemdriver>(model,name);
  }

modemdriver::modemdriver()
  {
  m_modem = MyPeripherals->m_cellular_modem;
  m_pwridx = 0;
  m_t_pwrcycle = 0;
  m_statuspoller_step = 0;
  }

modemdriver::~modemdriver()
  {
  };

const char* modemdriver::GetModel()
  {
  return model;
  }

const char* modemdriver::GetName()
  {
  return name;
  }

void modemdriver::Restart()
  {
  ESP_LOGI(TAG, "Restart");
  if (MyConfig.GetParamValueBool("auto", "modem", false))
    m_modem->SendSetState1((m_modem->GetState1() != modem::PoweredOff) ? modem::PowerOffOn : modem::PoweringOn);
  else
    m_modem->SendSetState1(modem::PoweringOff);
  }

void modemdriver::PowerOff()
  {
  int T_off = TimePwrOff;
  ESP_LOGI(TAG, "Power Off - timing: %d ms",T_off);
  PowerSleep(false);
  uart_wait_tx_done(m_modem->m_uartnum, portMAX_DELAY);
  uart_flush(m_modem->m_uartnum); // Flush the ring buffer, to try to address MUX start issues

  SimcomPowerOff(T_off);
  }

void modemdriver::PowerCycle()
  {
  // Check time since last Powercycle 
  // State timeout usually occurs after 30s -> Powercycle triggered
  // Frequent PowerCycles -> try different timing
  int dt = (m_t_pwrcycle == 0) ? 0 : (time(NULL) - m_t_pwrcycle);
  if (dt > 10 && dt < 60  ) m_pwridx = (m_pwridx+1) % NPwrTime;  // toggle between different time settings
  m_t_pwrcycle = time(NULL);

  ESP_LOGI(TAG, "Power Cycle");
  uart_wait_tx_done(m_modem->m_uartnum, portMAX_DELAY);
  uart_flush(m_modem->m_uartnum);       // Flush the ring buffer, to try to address MUX start issues

  SimcomPowerCycle(m_pwridx);
  }

void modemdriver::PowerSleep(bool onoff)
  {
    if (onoff)
    {
      setDTRLevel(1);
    }
    else
    {
      setDTRLevel(0);
    }
  }

int modemdriver::GetMuxChannels()    { return 4; }
int modemdriver::GetMuxChannelCTRL() { return 0; }
int modemdriver::GetMuxChannelNMEA() { return 1; }
int modemdriver::GetMuxChannelDATA() { return 2; }
int modemdriver::GetMuxChannelPOLL() { return 3; }
int modemdriver::GetMuxChannelCMD()  { return 4; }

void modemdriver::StartupNMEA()
  {
  // Switch on GPS, subscribe to NMEA sentences…
  //   2 = $..RMC -- UTC time & date
  //  64 = $..GNS -- Position & fix data
  if (m_modem->m_mux != NULL)
    {
    OvmsMutexLock lock(&m_modem->m_cmd_mutex);
    m_modem->muxtx(GetMuxChannelCMD(), "AT+CGPSNMEA=66;+CGPS=1,1\r\n");
    }
  else
    {
    ESP_LOGE(TAG, "Attempt to transmit on non running mux");
    }
  }

void modemdriver::ShutdownNMEA()
  {
  // Switch off GPS:
  if (m_modem->m_mux != NULL)
    {
    OvmsMutexLock lock(&m_modem->m_cmd_mutex);
    // send single commands, as each can fail:
    m_modem->muxtx(GetMuxChannelCMD(), "AT+CGPSNMEA=0\r\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    m_modem->muxtx(GetMuxChannelCMD(), "AT+CGPS=0\r\n");
    }
  else
    {
    ESP_LOGE(TAG, "Attempt to transmit on non running mux");
    }
  }

void modemdriver::StatusPoller()
  {
  if (m_modem->m_mux != NULL)
    { m_modem->muxtx(GetMuxChannelPOLL(), "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n"); }
  }

bool modemdriver::State1Leave(modem::modem_state1_t oldstate)
  {
  m_statuspoller_step = 0;
  return false;
  }

bool modemdriver::State1Enter(modem::modem_state1_t newstate)
  {
  m_statuspoller_step = 0;
  return false;
  }

modem::modem_state1_t modemdriver::State1Activity(modem::modem_state1_t curstate)
  {
  return curstate;
  }

modem::modem_state1_t modemdriver::State1Ticker1(modem::modem_state1_t curstate)
  {
  if (m_statuspoller_step > 0)
    {
    StatusPoller();
    }
  return curstate;
  }

std::string modemdriver::GetNetTypes()
  {
    return "auto";
  }

#define SET_NET_MODE(at) if(m_modem->m_mux != NULL) m_modem->muxtx(GetMuxChannelCMD(), at); else m_modem->tx(at);

bool modemdriver::SetNetworkType(std::string new_net_type)
  {
  if (m_modem != NULL && (new_net_type != net_type || net_type == "undef") )
    {
    std::string net_avail=GetNetTypes();
    net_type = new_net_type;
    ESP_LOGI(TAG, "Set network mode to %s", net_type.c_str());
    if (net_type == "auto" && net_avail.find("auto") < string::npos)
      {
      SET_NET_MODE("AT+CNMP=2\r\n");
      }
    else if (net_type == "2G" && net_avail.find("2G") < string::npos)
      {
      SET_NET_MODE("AT+CNMP=13\r\n");
      }
    else if (net_type == "3G" && net_avail.find("3G") < string::npos)
      {
      SET_NET_MODE("AT+CNMP=14\r\n");
      }
    else if (net_type == "4G" && net_avail.find("4G") < string::npos)
      {
      SET_NET_MODE("AT+CNMP=38\r\n");
      }
    else
      {
      ESP_LOGE(TAG, "Requested network type %s is invalid -> set to automatic mode", net_type.c_str());
      SET_NET_MODE("AT+CNMP=2\r\n");  // no valid mode -> set to AUTO
      net_type = "auto";
      }
    return true;
    }
    return false;
  }

