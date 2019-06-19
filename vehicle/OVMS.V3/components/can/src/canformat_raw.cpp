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
  m_bufpos = 0;
  }

canformat_raw::~canformat_raw()
  {
  }

std::string canformat_raw::get(CAN_log_message_t* message)
  {
  return std::string((const char*)message,sizeof(CAN_log_message_t));
  }

std::string canformat_raw::getheader(struct timeval *time)
  {
  return std::string("");
  }

size_t canformat_raw::put(CAN_log_message_t* message, uint8_t *buffer, size_t len)
  {
  size_t need = sizeof(CAN_log_message_t);
  size_t remain = need - m_bufpos;
  size_t consumed = 0;

  if (remain > 0)
    {
    if (len < remain)
      {
      // Just bring in what we can..
      memcpy(m_buf+m_bufpos, buffer, len);
      m_bufpos += len;
      return len;
      }
    else
      {
      memcpy(m_buf+m_bufpos, buffer, remain);
      m_bufpos += remain;
      buffer += remain;
      len -= remain;
      consumed += remain;
      }
    }

  memcpy(message,m_buf,need);
  return consumed;
  }
