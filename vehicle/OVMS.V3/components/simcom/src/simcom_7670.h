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

#ifndef __SIMCOM_7670_H__
#define __SIMCOM_7670_H__

#include "ovms_cellular.h"

// time between GPS location requests (seconds)
#define   SIMCOM7670_GNSS_INTERVAL  10

class simcom7670 : public modemdriver
  {
  public:
    simcom7670();
    ~simcom7670();
    void PowerCycle();
    void PowerOff();
    void StartupNMEA();
    void ShutdownNMEA();
    void StatusPoller();
    modem::modem_state1_t State1Ticker1(modem::modem_state1_t state);
    void SendGNSSRequest(std::string event, void* data);
    std::string GetNetTypes();

  private:
    TimerHandle_t m_timer;
    int           m_interval;
  };
#endif //#ifndef __SIMCOM_7670_H__
