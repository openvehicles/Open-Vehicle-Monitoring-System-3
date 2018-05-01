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
;    (C) 2018       Michael Balzer
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

#include "ovms_semaphore.h"

OvmsSemaphore::OvmsSemaphore(int maxcount /*=1*/, int initcount /*=0*/)
  {
  if (maxcount == 1)
    {
    m_semaphore = xSemaphoreCreateBinary();
    if (initcount == 1)
      Give();
    }
  else
    {
    m_semaphore = xSemaphoreCreateCounting(maxcount, initcount);
    }
  }

OvmsSemaphore::~OvmsSemaphore()
  {
  vSemaphoreDelete(m_semaphore);
  }

bool OvmsSemaphore::Take(TickType_t timeout)
  {
  return (xSemaphoreTake(m_semaphore, timeout) == pdTRUE);
  }

void OvmsSemaphore::Give()
  {
  xSemaphoreGive(m_semaphore);
  }

bool OvmsSemaphore::IsAvail()
  {
  return (uxSemaphoreGetCount(m_semaphore) > 0);
  }

int OvmsSemaphore::GetCount()
  {
  return (int) uxSemaphoreGetCount(m_semaphore);
  }
