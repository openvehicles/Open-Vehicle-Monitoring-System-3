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

#ifndef __OVMS_H__
#define __OVMS_H__

#include <stdint.h>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <sstream>
#include "ovms_malloc.h"

extern uint32_t monotonictime;

#ifdef CONFIG_OVMS_HW_SPIMEM_AGGRESSIVE
#ifndef OVMS_OPERATOR_NEW
#define OVMS_OPERATOR_NEW
inline void* operator new(std::size_t sz)
  {
  return ExternalRamMalloc(sz);
  }
#endif // #ifndef OVMS_OPERATOR_NEW
#endif // #ifdef CONFIG_OVMS_HW_SPIMEM_AGGRESSIVE

class ExternalRamAllocated
  {
  public:
    static void* operator new(std::size_t sz);
    static void* operator new[](std::size_t sz);
  };

// C++11 Allocator:
template <class T>
struct ExtRamAllocator
  {
  typedef T value_type;
  ExtRamAllocator() = default;
  template <class U> constexpr ExtRamAllocator(const ExtRamAllocator<U>&) noexcept {}
  T* allocate(std::size_t n)
    {
    auto p = static_cast<T*>(ExternalRamMalloc(n*sizeof(T)));
    return p;
    }
  void deallocate(T* p, std::size_t) noexcept { std::free(p); }
  };
template <class T, class U>
bool operator==(const ExtRamAllocator<T>&, const ExtRamAllocator<U>&) { return true; }
template <class T, class U>
bool operator!=(const ExtRamAllocator<T>&, const ExtRamAllocator<U>&) { return false; }

namespace extram
  {
  typedef std::basic_string<char, std::char_traits<char>, ExtRamAllocator<char>> string;
  typedef std::basic_ostringstream<char, std::char_traits<char>, ExtRamAllocator<char>> ostringstream;
  }

#endif //#ifndef __OVMS_H__
