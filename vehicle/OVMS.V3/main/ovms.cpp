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
#include <sdkconfig.h>

uint32_t monotonictime = 0;

#ifdef CONFIG_OVMS_HW_SPIMEM_AGGRESSIVE
using namespace std;
void * operator new(size_t size)
  {
  return ExternalRamMalloc(size);
  }

void * operator new[](size_t size)
  {
  return ExternalRamMalloc(size);
  }
#endif // #ifdef CONFIG_OVMS_HW_SPIMEM_AGGRESSIVE


static void* ExternalRamAllocated::operator new(std::size_t sz)
  {
  return ExternalRamMalloc(sz);
  }

static void* ExternalRamAllocated::operator new[](std::size_t sz)
  {
  return ExternalRamMalloc(sz);
  }

char* ExternalRamAllocated::strdup(const char* src)
  {
  if (!src)
    return NULL;
  size_t size = strlen(src) + 1;
  char* dupe = (char*)ExternalRamMalloc(size);
  if (dupe)
    memcpy(dupe, src, size);
  return dupe;
  }

int ExternalRamAllocated::asprintf(char** strp, const char* fmt, ...)
  {
  int size = 0;
  va_list args;
  va_start(args, fmt);
  size = vasprintf(strp, fmt, args);
  va_end(args);
  return size;
  }

int ExternalRamAllocated::vasprintf(char** strp, const char* fmt, va_list ap)
  {
  int size = 0;
  char *p = NULL;

  // determine required size:
  va_list apsz;
  va_copy(apsz, ap);
  size = vsnprintf(NULL, 0, fmt, apsz);
  va_end(apsz);

  if (size < 0)
    return -1;

  size++; // for '\0'
  p = (char*)ExternalRamMalloc(size);
  if (p == NULL)
    return -1;

  size = vsnprintf(p, size, fmt, ap);
  if (size < 0)
    {
    free(p);
    return -1;
    }

  *strp = p;
  return size;
  }


static void* InternalRamAllocated::operator new(std::size_t sz)
  {
  return InternalRamMalloc(sz);
  }

static void* InternalRamAllocated::operator new[](std::size_t sz)
  {
  return InternalRamMalloc(sz);
  }

char* InternalRamAllocated::strdup(const char* src)
  {
  if (!src)
    return NULL;
  size_t size = strlen(src) + 1;
  char* dupe = (char*)InternalRamMalloc(size);
  if (dupe)
    memcpy(dupe, src, size);
  return dupe;
  }

int InternalRamAllocated::asprintf(char** strp, const char* fmt, ...)
  {
  int size = 0;
  va_list args;
  va_start(args, fmt);
  size = vasprintf(strp, fmt, args);
  va_end(args);
  return size;
  }

int InternalRamAllocated::vasprintf(char** strp, const char* fmt, va_list ap)
  {
  int size = 0;
  char *p = NULL;

  // determine required size:
  va_list apsz;
  va_copy(apsz, ap);
  size = vsnprintf(NULL, 0, fmt, apsz);
  va_end(apsz);

  if (size < 0)
    return -1;

  size++; // for '\0'
  p = (char*)InternalRamMalloc(size);
  if (p == NULL)
    return -1;

  size = vsnprintf(p, size, fmt, ap);
  if (size < 0)
    {
    free(p);
    return -1;
    }

  *strp = p;
  return size;
  }
