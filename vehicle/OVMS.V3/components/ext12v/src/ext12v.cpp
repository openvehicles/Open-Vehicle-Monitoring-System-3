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
static const char *TAG = "ext12v";

#include "ext12v.h"
#include "ovms_peripherals.h"
#include "ovms_config.h"

ext12v::ext12v(const char* name)
  : pcp(name)
  {
  }

ext12v::~ext12v()
  {
  }

void ext12v::SetPowerMode(PowerMode powermode)
  {
  m_powermode = powermode;
  switch (powermode)
    {
    case On:
#ifdef CONFIG_OVMS_COMP_MAX7317
      ESP_LOGI(TAG, "Powering on external 12V devices");
      MyPeripherals->m_max7317->Output(MAX7317_SW_CTL, 1);
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
      break;
    case Sleep:
    case DeepSleep:
    case Off:
#ifdef CONFIG_OVMS_COMP_MAX7317
      ESP_LOGI(TAG, "Powering off external 12V devices");
      MyPeripherals->m_max7317->Output(MAX7317_SW_CTL, 0);
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
      break;
    default:
      break;
    }
  }

void ext12v::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "ext12v", false))
    SetPowerMode(On);
  }
