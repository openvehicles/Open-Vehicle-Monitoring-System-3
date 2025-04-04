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
;    (C) 2025       Christian Zeitnitz
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

#ifndef __SIMCOM_POWERING_H__
#define __SIMCOM_POWERING_H__

#include "ovms_peripherals.h"

//
// Timing of PWRKEY pin for Simcom modems
//
// procedure to power off via the PWRKEY pin
//          T_off
// High ---      ------- Wait for uart to be offline
// LOW     ------
//
// procedure to power cycle via the PWRKEY pin
//           T_off  4-5s   T_on
// High ---       --------    -----
// LOW     -------        ----
//
// If initial state (On or Off) is unknown -> timing has to ensure, that the
// modem is in ON state after the power cycle
// - second pulse has to be short enough to avoid another power off
//
// Minimal time (ms) for different Simcom Models
//
// Model  T_on     T_off
// ------+-------+-------
// 5360    180       500
// 7000   1000      1200
// 7600    100      2500
// 7670     50      2500
//
// In case of frequent power cycles, the T_on time is changed (see array below)
//
// Model                  5360/7600/7670  7000
const int TimePwrOn[]  = {     200,       1000 };   // array of T_on (ms) settings
#define NPwrTime    sizeof(TimePwrOn)/sizeof(TimePwrOn[0])

// T_off can be the same for all models
#define TimePwrOff  2500

// time (ms) from PWRKEY Off released to PWRKEY On asserted
#define TPwrOffOn   5000
//
// Powering of Simcom modems via PWRKEY and DTR pin
//

// PWRKEY signal is inverted by NPN transistor
  #define PwrKeyLow  1
  #define PwrKeyHigh 0

#ifdef CONFIG_OVMS_COMP_MAX7317
  #define setPWRLevel(level)  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, level)
#elif defined(MODEM_GPIO_PWR)
  #define setPWRLevel(level)  gpio_set_level((gpio_num_t)MODEM_GPIO_PWR, level)
#else
  #define setPWRLevel(level)  {}
#endif

#ifdef CONFIG_OVMS_COMP_MAX7317
  #define setDTRLevel(level)  MyPeripherals->m_max7317->Output(MODEM_EGPIO_DTR, level)
#elif defined(MODEM_GPIO_DTR)
  #define setDTRLevel(level)  gpio_set_level((gpio_num_t)MODEM_GPIO_DTR, level)
#else
  #define setDTRLevel(level)  {}
#endif

void SimcomPowerOff(int T_off=TimePwrOff);
void SimcomPowerCycle(int iPwr=-1, int T_on=200, int T_off=TimePwrOff);

  #endif // __SIMCOM_POWERING_H__
