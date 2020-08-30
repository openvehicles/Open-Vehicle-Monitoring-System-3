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
static const char *TAG = "SIM7000";

#include <string.h>
#include "ovms_peripherals.h"
#include "simcom_7000.h"

const char model[] = "SIM7000";
const char name[] = "Experimental support for SIMCOM SIM7000";

class simcom7000Init
  {
  public: simcom7000Init();
} MySimcom7000Init  __attribute__ ((init_priority (4650)));

simcom7000Init::simcom7000Init()
  {
  ESP_LOGI(TAG, "Registering SIM7000 modem driver (4650)");
  MyModemFactory.RegisterModemDriver<simcom7000>(model,name);
  }

simcom7000::simcom7000()
  {
  }

simcom7000::~simcom7000()
  {
  }

const char* simcom7000::GetModel()
  {
  return model;
  }

const char* simcom7000::GetName()
  {
  return name;
  }

void simcom7000::PowerCycle()
  {
  ESP_LOGI(TAG, "Power Cycle (SIM7000)");
  uart_flush(m_modem->m_uartnum); // Flush the ring buffer, to try to address MUX start issues
#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 1); // Modem EN/PWR line high
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
  }

bool simcom7000::State1Leave(modem::modem_state1_t oldstate)
  {
  return false;
  }

bool simcom7000::State1Enter(modem::modem_state1_t newstate)
  {
  return false;
  }

modem::modem_state1_t simcom7000::State1Activity(modem::modem_state1_t curstate)
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

modem::modem_state1_t simcom7000::State1Ticker1(modem::modem_state1_t curstate)
  {
  if (curstate == modem::PoweredOn)
    {
    if (m_modem->GetPowerMode() == DeepSleep)
      {
      return modem::NetDeepSleep; // Just hold, without starting the network
      }
    switch (m_modem->m_state1_ticker)
      {
      case 8:
        m_modem->tx("ATE0\r\n");
      case 10:
        m_modem->tx("AT+CPIN?;+CREG=1;+CTZU=1;+CTZR=1;+CMGF=1;+CNMI=1,2,0,0,0;+CSDH=1;+CMEE=2;+CSQ;S0=0\r\n");
        break;
      case 12:
        m_modem->tx("AT+CGMR;+ICCID\r\n");
        break;
      case 20:
        m_modem->tx("AT+CMUX=0\r\n");
        break;
      }
    return modem::None;
    }

  return curstate;
  }
