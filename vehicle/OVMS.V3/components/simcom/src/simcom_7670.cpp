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
static const char *TAG = "SIM7670";

#include <string.h>
#include "simcom_7670.h"
#include "ovms_cellular.h"
#include "ovms_config.h"
#include "ovms_notify.h"
#include "ovms_events.h"
#include "simcom_powering.h"

const char model[] = "A7670";
const char name[] = "Experimental support for SIMCOM A7670E";

simcom7670* MySimcom7670 = NULL;

class simcom7670Init
  {
  public: 
    simcom7670Init();
} MySimcom7670Init  __attribute__ ((init_priority (4660)));


simcom7670Init::simcom7670Init()
  {
  ESP_LOGI(TAG, "Registering Simcom A7670E modem driver (4660)");
  MyCellularModemFactory.RegisterCellularModemDriver<simcom7670>(model,name);
  }

simcom7670::simcom7670()
  {
    m_timer = NULL;
    m_interval = SIMCOM7670_GNSS_INTERVAL;
    MySimcom7670 = this;
  }

simcom7670::~simcom7670()
  {
    MySimcom7670 = NULL;
  }

std::string simcom7670::GetNetTypes() {
  return "auto 2G 3G 4G";
}

void simcom7670::StartupNMEA()
  {
    // GPS config for simcom 7670 - will use AT+CGNSSINFO to get location
  if (m_modem->m_mux != NULL)
    {
    std::string atcmd= m_modem->m_gps_hot_start ? "AT+CGNSSPWR=1,1\r\n" : "AT+CGNSSPWR=1\r\n"; 
    m_modem->muxtx(GetMuxChannelCMD(), atcmd.c_str());      //power GPS module on
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    m_modem->muxtx(GetMuxChannelCMD(), "AT+CGNSSMODE=3\r\n");     // select used satellite networks 3: GPS, GLONASS, BEIDOU
    }
  else
    { ESP_LOGE(TAG, "Attempt to transmit on non running mux"); }
  }

void simcom7670::ShutdownNMEA()
  {
  // Switch off GPS:
  if (m_modem->m_mux != NULL)
    {
    std::string atcmd= m_modem->m_gps_hot_start ? "AT+CGNSSPWR=0,1\r\n" : "AT+CGNSSPWR=0\r\n"; 
    m_modem->muxtx(GetMuxChannelCMD(), atcmd.c_str());  // power gps module off
    MyEvents.DeregisterEvent(TAG);
    }
  else
    { ESP_LOGE(TAG, "Attempt to transmit on non running mux"); }
  }

void simcom7670::PowerOff()
  {
  ESP_LOGV(TAG, "Power Off");
  modemdriver::PowerSleep(false);
  uart_wait_tx_done(m_modem->m_uartnum, portMAX_DELAY);
  uart_flush(m_modem->m_uartnum); // Flush the ring buffer, to try to address MUX start issues
  SimcomPowerOff();
  }

void simcom7670::PowerCycle()
  {
  ESP_LOGV(TAG, "Power Cycle");
  uart_wait_tx_done(m_modem->m_uartnum, portMAX_DELAY);
  uart_flush(m_modem->m_uartnum);       // Flush the ring buffer, to try to address MUX start issues
  SimcomPowerCycle(-1, 70);
  }

void simcom7670::StatusPoller()
  {
  if (m_modem->m_mux != NULL)
    {
    // The ESP32 UART queue has a capacity of 128 bytes, NMEA and PPP data may be
    // coming in concurrently, and we cannot use flow control.
    // Reduce the queue stress by distributing the status poll:
    switch (++m_statuspoller_step)
      {
      case 1:
        m_modem->muxtx(GetMuxChannelPOLL(), "AT+CREG?;+CGREG?;+CEREG?;+CCLK?;+CSQ\r\n");
        // → ~ 80 bytes, e.g.
        //  +CREG: 1,5  +CGREG: 1,5  +CEREG: 1,5  +CCLK: "22/09/09,12:54:41+08"  +CSQ: 13,99  
        break;
      case 2:
        m_modem->muxtx(GetMuxChannelPOLL(), "AT+CPSI?\r\n");
        // → ~ 85 bytes, e.g.
        //  +CPSI: LTE,Online,262-02,0xB0F5,13179412,448,EUTRAN-BAND1,100,4,4,-122,-1184,-874,9  
        break;
      case 3:
        m_modem->muxtx(GetMuxChannelPOLL(), "AT+COPS?\r\n");
        // → ~ 35 bytes, e.g.
        //  +COPS: 0,0,"vodafone.de Hologram",7  

        // done, fallthrough:
        FALLTHROUGH;
      default:
        m_statuspoller_step = 0;
        break;
      }
    }
  }

// State1Ticker needed for the Simcom7670 for a successful PPP connection.
// Standard AT command (CGDATA=PPP) in modem State1Ticker fails
modem::modem_state1_t simcom7670::State1Ticker1(modem::modem_state1_t state)
  {
    if (state == modem::NetStart && m_modem->GetPowerMode() != Sleep) 
    {
      if (m_modem->m_state1_ticker == 1)  
        {
        if (!m_modem->m_gps_hot_start)
          {
           m_modem->muxtx(GetMuxChannelCMD(), "AT+CGNSSPWR=?\r\n");     // query, if GPS hot start supported
          }
        m_modem->m_state1_userdata = 1;
        if (m_modem->m_mux != NULL)
          {
          std::string apncmd("AT+CGDCONT=1,\"IP\",\"");
          apncmd.append(MyConfig.GetParamValue("modem", "apn"));
          apncmd.append("\"\r\n");
          m_modem->muxtx(m_modem->m_mux_channel_DATA,apncmd.c_str());
          // CGDATA=PPP does not work -> old school PPP dial up 
          apncmd="ATD*99#\r\n"; 
          ESP_LOGD(TAG,"Netstart %s",apncmd.c_str());
          m_modem->muxtx(m_modem->m_mux_channel_DATA,apncmd.c_str());
          // Dirty hack - increment modem state1_ticker to avoid that a similar action 
          // is performed in the generic modemdriver State1Ticker
          ++m_modem->m_state1_ticker; 
          }
        }
    }
    if (m_modem->m_nmea != NULL && m_modem->GetPowerMode() != Sleep && m_modem->m_state1_ticker%5 == 0)
    {
	    m_modem->muxtx(GetMuxChannelPOLL(), "AT+CGNSSINFO\r\n");
    }
  return state;
  }
