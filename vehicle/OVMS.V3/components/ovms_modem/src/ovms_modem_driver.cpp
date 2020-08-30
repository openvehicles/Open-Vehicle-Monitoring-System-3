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
static const char *TAG = "modem-auto";

#include <string.h>

#include "ovms_modem.h"
#include "ovms_peripherals.h"
#include "ovms_config.h"

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
  MyModemFactory.RegisterModemDriver<modemdriver>(model,name);
  }

modemdriver::modemdriver()
  {
  m_modem = MyPeripherals->m_modem;
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
    m_modem->SetState1((m_modem->GetState1() != modem::PoweredOff) ? modem::PowerOffOn : modem::PoweringOn);
  else
    m_modem->SetState1(modem::PoweringOff);
  }

void modemdriver::PowerOff()
  {
  PowerSleep(false);
#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
  }

void modemdriver::PowerCycle()
  {
  ESP_LOGI(TAG, "Power Cycle");
  uart_flush(m_modem->m_uartnum); // Flush the ring buffer, to try to address MUX start issues
#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 1); // Modem EN/PWR line high
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
  }

void modemdriver::PowerSleep(bool onoff)
  {
  #ifdef CONFIG_OVMS_COMP_MAX7317
    if (onoff)
      MyPeripherals->m_max7317->Output(MODEM_EGPIO_DTR, 1);
    else
      MyPeripherals->m_max7317->Output(MODEM_EGPIO_DTR, 0);
  #endif // #ifdef CONFIG_OVMS_COMP_MAX7317
  }

int modemdriver::GetMuxChannels()    { return 4; }
int modemdriver::GetMuxChannelCTRL() { return 0; }
int modemdriver::GetMuxChannelNMEA() { return 1; }
int modemdriver::GetMuxChannelDATA() { return 2; }
int modemdriver::GetMuxChannelPOLL() { return 3; }
int modemdriver::GetMuxChannelCMD()  { return 4; }

bool modemdriver::State1Leave(modem::modem_state1_t oldstate)
  {
  return false;
  }

bool modemdriver::State1Enter(modem::modem_state1_t newstate)
  {
  return false;
  }

modem::modem_state1_t modemdriver::State1Activity(modem::modem_state1_t curstate)
  {
  return curstate;
  }

modem::modem_state1_t modemdriver::State1Ticker1(modem::modem_state1_t curstate)
  {
  return curstate;
  }
