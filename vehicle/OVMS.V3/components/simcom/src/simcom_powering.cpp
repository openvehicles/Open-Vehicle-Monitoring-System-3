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

#include "simcom_powering.h"

void SimcomPowerOff(int T_off)
  {
  ESP_LOGI("Simcom", "Power Off - %d ms", T_off);
  setPWRLevel(PwrKeyHigh);
  setPWRLevel(PwrKeyLow);
  vTaskDelay(T_off / portTICK_PERIOD_MS);          // Toff > 2500ms
  setPWRLevel(PwrKeyHigh);
  vTaskDelay(5000 / portTICK_PERIOD_MS);          // Wait for UART to be offline (modem sends DETACH message)
  }

void SimcomPowerCycle(int iPwr, int T_on, int T_off)
  {
  T_on = (iPwr >= 0) ? TimePwrOn[iPwr] : T_on;
  ESP_LOGI("Simcom", "Power Cycle - T_off %d ms - T_on %d ms", T_off, T_on);
  setPWRLevel(PwrKeyHigh);              // set POWERKEY of modem to high
  // Power OFf
  setPWRLevel(PwrKeyLow);               // set POWERKEY of modem to low
  vTaskDelay(T_off / portTICK_PERIOD_MS);   // PWR off -> t > 2500ms
  setPWRLevel(PwrKeyHigh);              // set POWERKEY of modem to high
  
  //Wait until powered off (1.9s) and delay to power on again (Tonoff > 2s) 
  vTaskDelay(TPwrOffOn / portTICK_PERIOD_MS); // > 3.9s

  // Power on again
  setPWRLevel(PwrKeyLow);               // set POWERKEY of modem to low
  vTaskDelay(T_on / portTICK_PERIOD_MS);  // PWR on -> t > 180ms  ( > 1000 for 7000)
  setPWRLevel(PwrKeyHigh);              // set POWERKEY of modem to high
  }

