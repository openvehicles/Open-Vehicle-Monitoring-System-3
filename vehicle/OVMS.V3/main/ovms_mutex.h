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

#ifndef __OVMS_MUTEX_H__
#define __OVMS_MUTEX_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <freertos/semphr.h>

/**
 * Standard Mutex:
 */
class OvmsMutex
  {
  public:
    OvmsMutex();
    ~OvmsMutex();

  public:
    bool Lock(TickType_t timeout = portMAX_DELAY);
    void Unlock();

  protected:
    QueueHandle_t m_mutex;
  };

class OvmsMutexLock
  {
  public:
    OvmsMutexLock(OvmsMutex* mutex, TickType_t timeout = portMAX_DELAY);
    OvmsMutexLock(const OvmsMutexLock& src) = delete; // copying not allowed
    OvmsMutexLock& operator=(const OvmsMutexLock& src) = delete;
    OvmsMutexLock(OvmsMutexLock&& src);
    ~OvmsMutexLock();

  public:
    bool Lock(TickType_t timeout = portMAX_DELAY);
    void Unlock();

  public:
    bool IsLocked() const { return m_locked; }
    operator bool() const { return m_locked; }

  protected:
    OvmsMutex* m_mutex;
    bool m_locked;
  };

/**
 * Recursive Mutex:
 */
class OvmsRecMutex
  {
  public:
    OvmsRecMutex();
    ~OvmsRecMutex();

  public:
    bool Lock(TickType_t timeout = portMAX_DELAY);
    void Unlock();

  public:
    unsigned int GetCount() { return m_count; }

  protected:
    QueueHandle_t m_mutex;
    unsigned m_count;       // can be removed when FreeRTOS exposes uxRecursiveCallCount
  };

class OvmsRecMutexLock
  {
  public:
    OvmsRecMutexLock(OvmsRecMutex* mutex, TickType_t timeout = portMAX_DELAY);
    OvmsRecMutexLock(const OvmsRecMutexLock& src) = delete; // copying not allowed
    OvmsRecMutexLock& operator=(const OvmsRecMutexLock& src) = delete;
    OvmsRecMutexLock(OvmsRecMutexLock&& src);
    ~OvmsRecMutexLock();

  public:
    bool Lock(TickType_t timeout = portMAX_DELAY);
    void Unlock();

  public:
    bool IsLocked() const { return m_locked; }
    operator bool() const { return m_locked; }
    unsigned int GetCount() { return m_mutex->GetCount(); }

  protected:
    OvmsRecMutex* m_mutex;
    bool m_locked;
  };

#endif //#ifndef __OVMS_MUTEX_H__
