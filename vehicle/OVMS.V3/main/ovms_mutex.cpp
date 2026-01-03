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

#include "ovms_log.h"
static const char *TAG = "mutex";

#include "ovms_mutex.h"

/**
 * Standard Mutex:
 */
OvmsMutex::OvmsMutex()
  {
  m_mutex = xSemaphoreCreateMutex();
  }

OvmsMutex::~OvmsMutex()
  {
  // Note: no concurrency protection here to keep this lightweight.
  //  If you need some, implement on application level.
  TaskHandle_t holder = xSemaphoreGetMutexHolder(m_mutex);
  if (holder == xTaskGetCurrentTaskHandle())
    {
    Unlock();
    }
  else if (holder)
    {
    ESP_LOGE(TAG, "attempt to delete an OvmsMutex still locked by %p => abort", holder);
    abort();
    }
  vSemaphoreDelete(m_mutex);
  }

bool OvmsMutex::Lock(TickType_t timeout)
  {
  return (xSemaphoreTake(m_mutex, timeout) == pdTRUE);
  }

void OvmsMutex::Unlock()
  {
  xSemaphoreGive(m_mutex);
  }

OvmsMutexLock::OvmsMutexLock(OvmsMutex* mutex, TickType_t timeout)
  {
  assert(mutex != NULL);
  m_mutex = mutex;
  m_locked = m_mutex->Lock(timeout);
  }

OvmsMutexLock::OvmsMutexLock(OvmsMutexLock&& src)
  {
  assert(src.m_mutex != NULL);
  m_mutex = src.m_mutex;
  m_locked = src.m_locked;
  src.m_locked = false;
  }

OvmsMutexLock::~OvmsMutexLock()
  {
  Unlock();
  }

bool OvmsMutexLock::Lock(TickType_t timeout)
  {
  if (!m_locked)
    {
    m_locked = m_mutex->Lock(timeout);
    }
  return m_locked;
  }

void OvmsMutexLock::Unlock()
  {
  if (m_locked)
    {
    m_mutex->Unlock();
    m_locked = false;
    }
  }


/**
 * Recursive Mutex:
 */
OvmsRecMutex::OvmsRecMutex()
  {
  m_mutex = xSemaphoreCreateRecursiveMutex();
  m_count = 0;
  }

OvmsRecMutex::~OvmsRecMutex()
  {
  // Note: no concurrency protection here to keep this lightweight.
  //  If you need some, implement on application level.
  TaskHandle_t holder, me = xTaskGetCurrentTaskHandle();
  while ((holder = xSemaphoreGetMutexHolder(m_mutex)) != NULL)
    {
    if (holder == me)
      {
      Unlock();
      }
    else
      {
      ESP_LOGE(TAG, "attempt to delete an OvmsRecMutex still locked by %p => abort", holder);
      abort();
      }
    }
  vSemaphoreDelete(m_mutex);
  }

bool OvmsRecMutex::Lock(TickType_t timeout)
  {
  bool locked = (xSemaphoreTakeRecursive(m_mutex, timeout) == pdTRUE);
  if (locked) m_count++;
  return locked;
  }

void OvmsRecMutex::Unlock()
  {
  if (m_count > 0)
    {
    xSemaphoreGiveRecursive(m_mutex);
    m_count--;
    }
  }

OvmsRecMutexLock::OvmsRecMutexLock(OvmsRecMutex* mutex, TickType_t timeout)
  {
  assert(mutex != NULL);
  m_mutex = mutex;
  m_locked = m_mutex->Lock(timeout);
  }

OvmsRecMutexLock::OvmsRecMutexLock(OvmsRecMutexLock&& src)
  {
  assert(src.m_mutex != NULL);
  m_mutex = src.m_mutex;
  m_locked = src.m_locked;
  src.m_locked = false;
  }

OvmsRecMutexLock::~OvmsRecMutexLock()
  {
  Unlock();
  }

bool OvmsRecMutexLock::Lock(TickType_t timeout)
  {
  if (!m_locked)
    {
    m_locked = m_mutex->Lock(timeout);
    }
  return m_locked;
  }

void OvmsRecMutexLock::Unlock()
  {
  if (m_locked)
    {
    m_mutex->Unlock();
    m_locked = false;
    }
  }

