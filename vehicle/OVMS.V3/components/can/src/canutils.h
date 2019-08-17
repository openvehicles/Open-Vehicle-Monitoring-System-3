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


// Helper macros for filling out the CAN frame 

#define FRAME_FILL_0(frame) ;

#define FRAME_FILL_1(frame,d1)  \
         frame.data.u8[0] = d1; 

#define FRAME_FILL_2(frame,d1,d2) \
         FRAME_FILL_1(frame,d1)   \
         frame.data.u8[1] = d2;   

#define FRAME_FILL_3(frame,d1,d2,d3)  \
         FRAME_FILL_2(frame,d1,d2)    \
         frame.data.u8[2] = d3;       

#define FRAME_FILL_4(frame,d1,d2,d3,d4) \
         FRAME_FILL_3(frame,d1,d2,d3)   \
         frame.data.u8[3] = d4;

#define FRAME_FILL_5(frame,d1,d2,d3,d4,d5) \
         FRAME_FILL_4(frame,d1,d2,d3,d4)   \
         frame.data.u8[4] = d5;

#define FRAME_FILL_6(frame,d1,d2,d3,d4,d5,d6) \
         FRAME_FILL_5(frame,d1,d2,d3,d4,d5)   \
         frame.data.u8[5] = d6;

#define FRAME_FILL_7(frame,d1,d2,d3,d4,d5,d6,d7) \
         FRAME_FILL_6(frame,d1,d2,d3,d4,d5,d6)   \
         frame.data.u8[6] = d7;

#define FRAME_FILL_8(frame,d1,d2,d3,d4,d5,d6,d7,d8) \
         FRAME_FILL_7(frame,d1,d2,d3,d4,d5,d6,d7)   \
         frame.data.u8[7] = d8;

#define GET_9TH_ARG(x, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, ...) arg9

// This is a neat preprocessor trick to select the right FRAME_FILL_x macro depending on parameter count.
#define FRAME_FILL_MACRO_CHOOSER(...) \
    GET_9TH_ARG(,##__VA_ARGS__,  FRAME_FILL_8, FRAME_FILL_7, FRAME_FILL_6, FRAME_FILL_5, \
                FRAME_FILL_4, FRAME_FILL_3, FRAME_FILL_2, FRAME_FILL_1, FRAME_FILL_0)

/* 
Usage:
  ext = 1 (extended frame), 0 (standard frame)
  bus = bus the CAN Message is sent
  frame = reference to the user space CAN frame structure
  id = CAN header ID
 Optional bytes:
  d1, ... ,d8 (payload)
*/
#define FRAME_FILL(ext,bus,frame,id,len, ...) \
  memset(&frame,0,sizeof(frame)); \
  frame.origin = bus; \
  frame.MsgID = id;   \
  frame.FIR.B.FF = (ext ? CAN_frame_ext : CAN_frame_std);     \
  frame.FIR.B.DLC = len; \
  FRAME_FILL_MACRO_CHOOSER(__VA_ARGS__)(frame ,##__VA_ARGS__)

// Convenience macros for initializing and sending standard and exteneded frame CAN messages
#define SEND_STD_FRAME(bus,frame,id,len, ...) \
  { \
  FRAME_FILL(0, bus,frame,id,len, ##__VA_ARGS__) \
  bus->Write(&frame); \
  }

#define SEND_EXT_FRAME(bus,frame,id,len, ...) \
  { \
  FRAME_FILL(1, bus,frame,id,len, ##__VA_ARGS__)  \
  bus->Write(&frame); \
  }

// Helper functions for getting 1-4 byte chunks from CAN message payload
#define GET8(frame, pos) (frame->data.u8[pos])
#define GET16(frame, pos) ((frame->data.u8[pos] << 8) | frame->data.u8[pos+1])
#define GET24(frame, pos) ((frame->data.u8[pos] << 16) | (frame->data.u8[pos+1] << 8) | frame->data.u8[pos+2])
#define GET32(frame, pos) ((frame->data.u8[pos] << 24) | (frame->data.u8[pos+1] << 16) | (frame->data.u8[pos+2] << 8) | frame->data.u8[pos+3])

#endif //#ifndef __CAN_UTILS_H__
