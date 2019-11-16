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

#include "ovms_log.h"
static const char *TAG = "timer";

#include "ovms_timer.h"

OvmsTimer::OvmsTimer(const char* name /*=NULL*/, int maxwait_ms /*=-1*/, bool autoreload /*=false*/)
  {
  m_name = name ? name : "";
  m_maxwait = (maxwait_ms >= 0) ? pdMS_TO_TICKS(maxwait_ms) : portMAX_DELAY;
  m_callback = NULL;
  m_timer = xTimerCreate(m_name, portMAX_DELAY, autoreload, (void*)this, Callback);
  if (!m_timer)
    ESP_LOGE(TAG, "Timer '%s' could not be created", m_name);
  }

OvmsTimer::~OvmsTimer()
  {
  m_callback = NULL;
  if (m_timer)
    {
    TickType_t wait = IsActive() ? portMAX_DELAY : m_maxwait;
    if (xTimerDelete(m_timer, wait) != pdPASS)
      ESP_LOGE(TAG, "Timer '%s' could not be deleted", m_name);
    }
  }

void OvmsTimer::Callback(TimerHandle_t xTimer)
  {
  OvmsTimer* me = (OvmsTimer*) pvTimerGetTimerID(xTimer);
  if (me->m_callback)
    me->m_callback();
  }

bool OvmsTimer::Set(int time_ms, std::function<void()> callback)
  {
  if (!m_timer)
    return false;
  if (xTimerChangePeriod(m_timer, pdMS_TO_TICKS(time_ms), m_maxwait) != pdPASS)
    return false;
  m_callback = callback;
  return true;
  }

bool OvmsTimer::Start(int time_ms, std::function<void()> callback)
  {
  if (!Stop())
    return false;
  if (!Set(time_ms, callback))
    return false;
  return Start();
  }

bool OvmsTimer::IsActive()
  {
  return (m_timer && xTimerIsTimerActive(m_timer));
  }

bool OvmsTimer::Start()
  {
  if (!m_timer)
    return false;
  if (IsActive())
    return Reset();
  return (xTimerStart(m_timer, m_maxwait) == pdPASS);
  }

bool OvmsTimer::Stop()
  {
  if (!m_timer)
    return false;
  if (!IsActive())
    return true;
  return (xTimerStop(m_timer, m_maxwait) == pdPASS);
  }

bool OvmsTimer::Reset()
  {
  if (!m_timer)
    return false;
  if (!IsActive())
    return true;
  return (xTimerReset(m_timer, m_maxwait) == pdPASS);
  }
