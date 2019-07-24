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
static const char *TAG = "canformat-raw";

#include <errno.h>
#include <endian.h>
#include "pcp.h"
#include "canformat_raw.h"

class OvmsCanFormatRawInit
  {
  public: OvmsCanFormatRawInit();
} MyOvmsCanFormatRawInit  __attribute__ ((init_priority (4505)));

OvmsCanFormatRawInit::OvmsCanFormatRawInit()
  {
  ESP_LOGI(TAG, "Registering CAN Format: RAW (4505)");

  MyCanFormatFactory.RegisterCanFormat<canformat_raw>("raw");
  }

canformat_raw::canformat_raw(const char* type)
  : canformat(type)
  {
  }

canformat_raw::~canformat_raw()
  {
  }

std::string canformat_raw::get(CAN_log_message_t* message)
  {
  CAN_log_message_t raw;
  memcpy(&raw,message,sizeof(raw));
  raw.origin = (canbus*)raw.origin->m_busnumber;
  return std::string((const char*)&raw,sizeof(CAN_log_message_t));
  }

std::string canformat_raw::getheader(struct timeval *time)
  {
  return std::string("");
  }

size_t canformat_raw::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata)
  {
  if (m_buf.FreeSpace()==0) SetServeDiscarding(true); // Buffer full, so discard from now on
  if (IsServeDiscarding()) return len;  // Quick return if discarding

  size_t consumed = Stuff(buffer,len);  // Stuff m_buf with as much as possible

  if (m_buf.UsedSpace() < sizeof(CAN_log_message_t)) return consumed; // Insufficient data so far

  m_buf.Pop(sizeof(CAN_log_message_t), (uint8_t*)message);
  message->origin = MyCan.GetBus((int)message->origin);
  return consumed;
  }
