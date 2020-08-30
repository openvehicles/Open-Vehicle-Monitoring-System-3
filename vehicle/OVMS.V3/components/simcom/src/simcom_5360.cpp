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
static const char *TAG = "SIM5360";

#include <string.h>
#include "simcom_5360.h"

const char model[] = "SIM5360";
const char name[] = "SIMCOM SIM5360 (include US and EU variants)";

class simcom5360Init
  {
  public: simcom5360Init();
} MySimcom5360Init  __attribute__ ((init_priority (4650)));

simcom5360Init::simcom5360Init()
  {
  ESP_LOGI(TAG, "Registering SIM5360 modem driver (4650)");
  MyModemFactory.RegisterModemDriver<simcom5360>(model,name);
  }

simcom5360::simcom5360()
  {
  }

simcom5360::~simcom5360()
  {
  }

const char* simcom5360::GetModel()
  {
  return model;
  }

const char* simcom5360::GetName()
  {
  return name;
  }

bool simcom5360::State1Leave(modem::modem_state1_t oldstate)
  {
  return false;
  }

bool simcom5360::State1Enter(modem::modem_state1_t newstate)
  {
  return false;
  }

modem::modem_state1_t simcom5360::State1Activity(modem::modem_state1_t curstate)
  {
  if (curstate == modem::PoweredOn)
    {
    if (m_modem->StandardIncomingHandler(GetMuxChannelCTRL(), &m_modem->m_buffer))
      {
      if (m_modem->m_state1_ticker >= 20) return modem::MuxStart;
      }
    return modem::None;
    }

  return curstate;
  }

modem::modem_state1_t simcom5360::State1Ticker1(modem::modem_state1_t curstate)
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
        m_modem->tx("AT+CPIN?;+CREG=1;+CTZU=1;+CTZR=1;+CLIP=1;+CMGF=1;+CNMI=1,2,0,0,0;+CSDH=1;+CMEE=2;+CSQ;+AUTOCSQ=1,1;E0;S0=0\r\n");
        break;
      case 12:
        m_modem->tx("AT+CGMR;+ICCID\r\n");
        break;
      case 16:
        m_modem->tx("AT+CMUXSRVPORT=3,1\r\n");
        break;
      case 17:
        m_modem->tx("AT+CMUXSRVPORT=2,1\r\n");
        break;
      case 18:
        m_modem->tx("AT+CMUXSRVPORT=1,1\r\n");
        break;
      case 19:
        m_modem->tx("AT+CMUXSRVPORT=0,5\r\n");
        break;
      case 20:
        m_modem->tx("AT+CMUX=0\r\n");
        break;
      }
    return modem::None;
    }

  return curstate;
  }
