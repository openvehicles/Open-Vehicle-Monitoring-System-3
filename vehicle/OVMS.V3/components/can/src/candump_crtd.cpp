/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN dump framework
;    Date:          18th January 2018
;
;    (C) 2018       Mark Webb-Johnson
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
//static const char *TAG = "candump-crtd";

#include "candump_crtd.h"

candump_crtd::candump_crtd()
  {
  }

candump_crtd::candump_crtd(struct timeval time, uint8_t bus, CAN_frame_t* frame)
  : candump(time, bus, frame)
  {
  }

candump_crtd::~candump_crtd()
  {
  }

bool candump_crtd::set(std::string msg)
  {
  return false;
  }

std::string candump_crtd::get()
  {
  char buffer[64];

  sprintf(buffer,"%ld.%03ld %dR%s %0*X",
    m_time.tv_sec, m_time.tv_usec/1000,
    m_bus,
    (m_frame.FIR.B.FF == CAN_frame_std) ? "11":"29",
    (m_frame.FIR.B.FF == CAN_frame_std) ? 3 : 8,
    m_frame.MsgID);
  for (int i=0; i<m_frame.FIR.B.DLC; i++)
    sprintf(buffer+strlen(buffer)," %02x", m_frame.data.u8[i]);
  strcat(buffer,"\n");

  return std::string(buffer);
  }

bool candump_crtd::read(FILE* in)
  {
  return false;
  }
