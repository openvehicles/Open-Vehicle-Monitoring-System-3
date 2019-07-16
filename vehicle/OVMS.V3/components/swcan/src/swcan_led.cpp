/*
;    Project:       Open Vehicle Monitor System
;    Date:          27th March 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019       Marko Juhanne
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
//static const char *TAG = "swcan_led";
#include "swcan_led.h"
#include "ovms_peripherals.h"
#include "freertos/timers.h"


void LedHandler( TimerHandle_t xTimer )
  { 
  swcan_led * led = (swcan_led*) pvTimerGetTimerID(xTimer);
  if (led->blinks > 0) 
    {
      led->blinks--;
      led->SetState( (led->state+1)&1 );

      xTimerReset(led->m_timer,10);
    } 
  else
  	led->SetState( led->default_state );
  }


swcan_led::swcan_led( const char * name, int max7317_pin, int defaultState )
  {
  pin = max7317_pin;
  state = 0;
  SetDefaultState(defaultState);
  m_timer = xTimerCreate(name,40 / portTICK_PERIOD_MS,pdFALSE,this,LedHandler);
  }

swcan_led::~swcan_led()
  {
  }

void swcan_led::SetDefaultState( int defaultState )
	{
	default_state = defaultState;
	}

void swcan_led::SetState( int newState )
	{
    if (MyPeripherals) 
 		{
    	//ESP_LOGE(TAG, "set %s to %d", pcTimerGetTimerName(m_timer), newState);
   		state = newState;
   		MyPeripherals->m_max7317->Output( pin, state ? 0 : 1);
   		}
   	}


void swcan_led::Blink( int duration, int count )
	{
	if (count > 0)
		{
		blinks = count * 2 -1;
		SetState( (state+1)&1 );
		xTimerChangePeriod(m_timer, duration / portTICK_PERIOD_MS, 10);
		}
	}
