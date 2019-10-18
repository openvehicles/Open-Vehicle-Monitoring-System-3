/*
;    Project:       Open Vehicle Monitor System
;    Date:          27th March 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019       Marko Juhanne
;
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

#ifndef __OVMS_LED_H__
#define __OVMS_LED_H__

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

class ovms_led
  {

  public:
    ovms_led(const char * name, int pin, int defaultState=0 );
    ~ovms_led();

    void Set( int state );
    void SetDefaultState( int defaultState );
    void Blink( int onDuration, int offDuration=-1, int count=1, int burstCount=1, int interBurstInterval=1000 );

    void SetState( int state );
    int pin;
  	int transitions;
    int blink_state;
  	int state;
  	int default_state;
    int count;
    int burst_count;
    int inter_burst_interval;
    int on_duration, off_duration;
    TimerHandle_t m_timer;
  };

#endif //#ifndef __OVMS_LED_H__

