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
static const char *TAG = "SIM7600";

#include <string.h>
#include "simcom_powering.h"
#include "simcom_7600.h"

const char model[] = "SIM7600";
const char name[] = "Experimental support for SIMCOM SIM7600";

class simcom7600Init
  {
  public: simcom7600Init();
} MySimcom7600Init  __attribute__ ((init_priority (4650)));

simcom7600Init::simcom7600Init()
  {
  ESP_LOGI(TAG, "Registering SIM7600 modem driver (4650)");
  MyCellularModemFactory.RegisterCellularModemDriver<simcom7600>(model,name);
  }

simcom7600::simcom7600()
  {
  }

simcom7600::~simcom7600()
  {
  }

std::string simcom7600::GetNetTypes() {
  return "auto 2G 3G 4G";
}

const char* simcom7600::GetModel()
  {
  return model;
  }

const char* simcom7600::GetName()
  {
  return name;
  }

void simcom7600::StartupNMEA()
  {
  // Switch on GPS, subscribe to NMEA sentences…
  //   2 = $..RMC -- UTC time & date
  //  64 = $..GNS -- Position & fix data

  // We need to do this a little differently from the standard, as SIM7600
  // may start GPS on power up, and doesn't like us using CGPS=1,1 when
  // it is already on. So workaround is to first CGPS=0.
  // Also, the SIM7600 NMEA can be controlled via the NMEA MUX channel, so we
  // can keep the CMD MUX channel clear for user commands and don't need to
  // lock the channel.
  if (m_modem->m_mux != NULL)
    {
    m_modem->muxtx(GetMuxChannelNMEA(), "AT+CGPS=0\r\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    // send single commands, as each can fail:
    m_modem->muxtx(GetMuxChannelNMEA(), "AT+CGPSNMEA=258\r\n");
    vTaskDelay(pdMS_TO_TICKS(20));
    m_modem->muxtx(GetMuxChannelNMEA(), "AT+CGPSINFOCFG=5,258\r\n");
    vTaskDelay(pdMS_TO_TICKS(20));
    m_modem->muxtx(GetMuxChannelNMEA(), "AT+CGPS=1,1\r\n");
    }
  else
    { ESP_LOGE(TAG, "Attempt to transmit on non running mux"); }
  }

void simcom7600::ShutdownNMEA()
  {
  // Switch off GPS:
  if (m_modem->m_mux != NULL)
    {
    // send single commands, as each can fail:
    m_modem->muxtx(GetMuxChannelNMEA(), "AT+CGPSNMEA=0\r\n");
    vTaskDelay(pdMS_TO_TICKS(20));
    m_modem->muxtx(GetMuxChannelNMEA(), "AT+CGPSINFOCFG=0\r\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    m_modem->muxtx(GetMuxChannelNMEA(), "AT+CGPS=0\r\n");
    }
  else
    { ESP_LOGE(TAG, "Attempt to transmit on non running mux"); }
  }

  void simcom7600::StatusPoller()
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


void simcom7600::PowerOff()
  {
  ESP_LOGV(TAG, "Power Off");
  modemdriver::PowerSleep(false);
  uart_wait_tx_done(m_modem->m_uartnum, portMAX_DELAY);
  uart_flush(m_modem->m_uartnum); // Flush the ring buffer, to try to address MUX start issues
  SimcomPowerOff(3000);
  }

void simcom7600::PowerCycle()
  {
  ESP_LOGV(TAG, "Power Cycle");
  uart_wait_tx_done(m_modem->m_uartnum, portMAX_DELAY);
  uart_flush(m_modem->m_uartnum);       // Flush the ring buffer, to try to address MUX start issues
  SimcomPowerCycle(-1, 500, 3000);
  }

bool simcom7600::State1Leave(modem::modem_state1_t oldstate)
  {
  return modemdriver::State1Leave(oldstate);
  }

bool simcom7600::State1Enter(modem::modem_state1_t newstate)
  {
  if (newstate == modem::PoweringOff)
    {
    m_modem->ClearNetMetrics();
    m_modem->StopPPP();
    m_modem->StopNMEA();
    MyEvents.SignalEvent("system.modem.stop",NULL);
    PowerOff();
    m_modem->m_state1_timeout_ticks = 20;
    m_modem->m_state1_timeout_goto = modem::CheckPowerOff;
    return true;
    }
  if (newstate == modem::PoweredOn)
    {
    m_modem->ClearNetMetrics();
    MyEvents.SignalEvent("system.modem.poweredon", NULL);
    m_modem->m_state1_timeout_ticks = 40;
    m_modem->m_state1_timeout_goto = modem::PoweringOn;
    return true;
    }
  return modemdriver::State1Enter(newstate);
  }

modem::modem_state1_t simcom7600::State1Activity(modem::modem_state1_t curstate)
  {
  if (curstate == modem::PoweredOn)
    {
    if (m_modem->StandardIncomingHandler(GetMuxChannelCTRL(), &m_modem->m_buffer))
      {
      if (m_modem->m_state1_ticker >= 20) return modem::MuxStart;
      }
    return modem::None;
    }

  return modemdriver::State1Activity(curstate);
  }

modem::modem_state1_t simcom7600::State1Ticker1(modem::modem_state1_t curstate)
  {
  if (curstate == modem::PoweredOn)
    {
    if (m_modem->GetPowerMode() == DeepSleep)
      {
      return modem::NetDeepSleep; // Just hold, without starting the network
      }
    switch (m_modem->m_state1_ticker)
      {
      case 10:
        m_modem->tx("AT+CPIN?;+CREG=1;+CGREG=1;+CEREG=1;+CTZU=1;+CTZR=1;+CLIP=1;+CMGF=1;+CNMI=1,2,0,0,0;+CSDH=1;+CMEE=2;+CSQ;+AUTOCSQ=1,1;E0;S0=0\r\n");
        break;
      case 12:
        m_modem->tx("AT+CGMR;+CICCID\r\n");
        break;
      case 20:
        // start MUX mode, route URCs to MUX channel 3 (POLL)
        // Note: NMEA URCs will now also be sent on channel 3 by the SIMCOM 7600;
        //    without +CATR, NMEA URCs are sent identically on all channels;
        //    there is no option to route these separately to channel 1 (NMEA)
        m_modem->tx("AT+CMUX=0;+CATR=6\r\n");
        break;
      }
    return modem::None;
    }
  
  if (curstate == modem::PoweringOn)
      {
      // min 11s    typical 12s 
      // for uart to start
      if (m_modem->m_state1_ticker > 10) m_modem->tx("AT\r\n");
      }

  return modemdriver::State1Ticker1(curstate);
  }
