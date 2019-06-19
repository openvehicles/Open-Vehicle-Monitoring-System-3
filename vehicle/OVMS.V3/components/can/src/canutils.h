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
;
; Portions of this are based on the work of Thomas Barth, and licensed
; under MIT license.
; Copyright (c) 2017, Thomas Barth, barth-dev.de
; https://github.com/ThomasBarth/ESP32-CAN-Driver
*/

#ifndef __CAN_UTILS_H__
#define __CAN_UTILS_H__

#include "can.h"

/**
 * canbitset<StoreType>: CAN data packed bit extraction utility
 *  Usage examples:
 *    canbitset<uint64_t> canbits(p_frame->data.u8, 8) -- load whole CAN frame
 *    canbitset<uint32_t> canbits(data+1, 3) -- load 3 bytes beginning at second
 *    val = canbits.get(12,23) -- extract bits 12-23 of bytes loaded
 *  Note: no sanity checks.
 *  Hint: for best performance on ESP32, avoid StoreType uint64_t
 */
template <typename StoreType>
class canbitset
  {
  private:
    StoreType m_store;

  public:
    canbitset(uint8_t* src, int len)
      {
      load(src, len);
      }

    // load message bytes:
    void load(uint8_t* src, int len)
      {
      m_store = 0;
      for (int i=0; i<len; i++)
        {
        m_store <<= 8;
        m_store |= src[i];
        }
      m_store <<= ((sizeof(StoreType)-len) << 3);
      }

    // extract message part:
    //  - start,end: bit positions 0…63, 0=MSB of first msg byte
    StoreType get(int start, int end)
      {
      return (StoreType) ((m_store << start) >> (start + (sizeof(StoreType)<<3) - end - 1));
      }
  };

#endif //#ifndef __CAN_UTILS_H__
