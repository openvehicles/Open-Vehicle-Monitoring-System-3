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
//static const char *TAG = "ovms_led";
#include "ovms_led.h"
#include "ovms_peripherals.h"
#include "freertos/timers.h"


void LedHandler( TimerHandle_t xTimer )
  { 
  ovms_led * led = (ovms_led*) pvTimerGetTimerID(xTimer);
  if (led->transitions==0)
    {
    if (led->burst_count == 0)
      {
      // All blinks have finished
      led->SetState(led->default_state);
      xTimerStop(led->m_timer,100);
      } 
    else
      {
      // Start new group of blinks
      if (led->burst_count != -1)
        led->burst_count--;
      led->transitions = led->count * 2 -1;
      led->SetState( (led->state+1)&1 );
      led->blink_state = (led->blink_state+1)&1;
      xTimerChangePeriod(led->m_timer, led->inter_burst_interval / portTICK_PERIOD_MS, 10);
      }
    }
  else
    {
    // New on/off transition of a blink in a burst
    if (led->transitions != -1)
      led->transitions--;
    led->SetState( (led->state+1)&1 );
    led->blink_state = (led->blink_state+1)&1;
    if (led->blink_state)
      xTimerChangePeriod(led->m_timer, led->on_duration / portTICK_PERIOD_MS, 10);
    else
      xTimerChangePeriod(led->m_timer, led->off_duration / portTICK_PERIOD_MS, 10);
    } 
  }

ovms_led::ovms_led( const char * name, int max7317_pin, int defaultState )
  {
  pin = max7317_pin;
  state = 0;
  SetDefaultState(defaultState);
  m_timer = xTimerCreate(name,40 / portTICK_PERIOD_MS,pdFALSE,this,LedHandler);
  }

ovms_led::~ovms_led()
  {
  }

void ovms_led::SetDefaultState( int defaultState )
	{
	default_state = defaultState;
	}

void ovms_led::SetState( int newState )
	{
  if (MyPeripherals) 
    {
 		state = newState;
 		MyPeripherals->m_max7317->Output( pin, state ? 0 : 1);
 		}
  }

void ovms_led::Set( int newState )
  {
  //ESP_LOGD(TAG, "set %s to %d", pcTimerGetTimerName(m_timer), newState);
  if (transitions != 0)
    xTimerStop(m_timer,100);
  SetState(newState);
  }


void ovms_led::Blink( int onDuration, int offDuration, int _count, int burstCount, int interBurstInterval)
	{
  count = _count;
  if ( (count==0) || (burstCount==0) )
    return;
  if (count > 0)
    transitions = (count-1)*2;
  else
    transitions = -1; // blink indefinitely

  on_duration = onDuration;
  if (offDuration != -1)
    off_duration = offDuration;
  else
    off_duration = on_duration;
  burst_count = burstCount;
  if (burst_count != -1)
    burst_count--;
  inter_burst_interval = interBurstInterval;

  // There's a separate blink_state apart from the actual LED state, because if we want to blink an always-on LED, it transitions to off and then back on
  blink_state = 1;
  SetState( (default_state+1)&1 );
  //ESP_LOGD(TAG,"blink transitions %d burst %d state %d",transitions,burst_count,state);
	xTimerChangePeriod(m_timer, on_duration / portTICK_PERIOD_MS, 10);
	}



