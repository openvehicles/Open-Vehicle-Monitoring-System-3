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
//static const char *TAG = "candump";

#include "candump.h"

candump::candump()
  {
  memset(&m_time,0,sizeof(m_time));
  m_bus = 0;
  memset(&m_frame,0,sizeof(m_frame));
  }

candump::candump(struct timeval time, uint8_t bus, CAN_frame_t* frame)
  {
  memcpy(&m_time, &time, sizeof(time));
  m_bus = bus;
  memcpy(&m_frame, frame, sizeof(m_frame));
  }

candump::~candump()
  {
  }

void candump::set(struct timeval time, uint8_t bus, CAN_frame_t* frame)
  {
  memcpy(&m_time, &time, sizeof(time));
  m_bus = bus;
  memcpy(&m_frame, frame, sizeof(m_frame));
  }

bool candump::set(std::string msg)
  {
  return false; // Base implementation can't do this
  }

void candump::clear()
  {
  memset(&m_time,0,sizeof(m_time));
  m_bus = 0;
  memset(&m_frame,0,sizeof(m_frame));
  }

std::string candump::get()
  {
  return std::string("");
  }

bool candump::write(FILE* out)
  {
  std::string result = get();
  return (fwrite(result.c_str(), result.length(), 1, out)>0);
  }

bool candump::read(FILE* in)
  {
  return false;
  }
