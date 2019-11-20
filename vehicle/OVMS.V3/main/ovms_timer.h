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
;    (C) 2019       Michael Balzer
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

#ifndef __OVMS_TIMER_H__
#define __OVMS_TIMER_H__

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <functional>

class OvmsTimer
  {
  public:
    OvmsTimer(const char* name=NULL, int maxwait_ms=-1, bool autoreload=false);
    ~OvmsTimer();

  protected:
    static void Callback(TimerHandle_t xTimer);

  public:
    bool Set(int time_ms, std::function<void()> callback);
    bool Start(int time_ms, std::function<void()> callback);
    bool Start();
    bool Stop();
    bool IsActive();
    bool Reset();

  protected:
    const char* m_name;
    TickType_t m_maxwait;
    TimerHandle_t m_timer;
    std::function<void()> m_callback;
  };

class OvmsTimeout : public OvmsTimer
  {
  public:
    OvmsTimeout(const char* name=NULL, int maxwait_ms=-1)
      : OvmsTimer(name, maxwait_ms, false) {}
  };

class OvmsInterval : public OvmsTimer
  {
  public:
    OvmsInterval(const char* name=NULL, int maxwait_ms=-1)
      : OvmsTimer(name, maxwait_ms, true) {}
  };

#endif //__OVMS_TIMER_H__
