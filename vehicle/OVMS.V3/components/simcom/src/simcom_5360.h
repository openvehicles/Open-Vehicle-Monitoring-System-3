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

#ifndef __SIMCOM_5360_H__
#define __SIMCOM_5360_H__

#include "ovms_modem.h"

class simcom5360 : public modemdriver
  {
  public:
    simcom5360();
    ~simcom5360();

  public:
    const char* GetModel();
    const char* GetName();

    int GetMuxChannels()    { return 4; }
    int GetMuxChannelCTRL() { return 0; }
    int GetMuxChannelNMEA() { return 1; }
    int GetMuxChannelDATA() { return 2; }
    int GetMuxChannelPOLL() { return 3; }
    int GetMuxChannelCMD()  { return 4; }

    bool State1Leave(modem::modem_state1_t oldstate);
    bool State1Enter(modem::modem_state1_t newstate);
    modem::modem_state1_t State1Activity(modem::modem_state1_t curstate);
    modem::modem_state1_t State1Ticker1(modem::modem_state1_t curstate);
  };

#endif //#ifndef __SIMCOM_5360_H__
