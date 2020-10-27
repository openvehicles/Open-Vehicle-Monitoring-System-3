/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris Staite
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

#ifndef __RE_TOOLS_TESTER_PRESENT_H__
#define __RE_TOOLS_TESTER_PRESENT_H__

#include "can.h"

class OvmsReToolsTesterPresent
{
  public:
    OvmsReToolsTesterPresent(canbus* bus, uint16_t ecu, int interval);

    canbus* Bus() const { return m_bus; }
    uint16_t Ecu() const { return m_id; }
    int Interval() const { return m_interval; }

    void Ticker1();

  private:
    void SendTesterPresent();

    /// The CAN bus that is being used
    canbus* m_bus;
    /// The ID of the ECU to scan
    uint16_t m_id;
    /// The current ticker value
    uint32_t m_ticker;
    /// the interval at which to send the signal
    int m_interval;
};

#endif  // __RE_TOOLS_TESTER_PRESENT_H__
