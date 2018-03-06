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

#include "ovms.h"
#include "esp_heap_caps.h"

uint32_t monotonictime = 0;

#ifdef CONFIG_OVMS_HW_SPIMEM_AGGRESSIVE
void* operator new(std::size_t sz)
  {
  return ExternalRamMalloc(sz);
  }
#endif // #ifdef CONFIG_OVMS_HW_SPIMEM_AGGRESSIVE

void* ExternalRamMalloc(std::size_t sz)
  {
  void* ret = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
  if (ret)
    return ret;
  else
    return malloc(sz);
  }

static void* ExternalRamAllocated::operator new(std::size_t sz)
  {
  return ExternalRamMalloc(sz);
  }

static void* ExternalRamAllocated::operator new[](std::size_t sz)
  {
  return ExternalRamMalloc(sz);
  }
