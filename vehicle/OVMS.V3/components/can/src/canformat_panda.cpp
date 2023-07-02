/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN dump framework
;    Date:          18th January 2018
;
;    (C) 2022       Mark Webb-Johnson
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
static const char *TAG = "canformat-panda";

#include <errno.h>
#include "pcp.h"
#include "canlog.h"
#include "canformat_panda.h"
#include "ovms_utils.h"

class OvmsCanFormatPandaInit
  {
  public: OvmsCanFormatPandaInit();
} MyOvmsCanFormatPandaInit  __attribute__ ((init_priority (4505)));

OvmsCanFormatPandaInit::OvmsCanFormatPandaInit()
  {
  ESP_LOGI(TAG, "Registering CAN Format: PANDA (4505)");

  MyCanFormatFactory.RegisterCanFormat<canformat_panda>("panda");
  }

canformat_panda::canformat_panda(const char *type)
  : canformat(type)
  {
  }

canformat_panda::~canformat_panda()
  {
  }

std::string canformat_panda::get(CAN_log_message_t* message)
  {
  struct
    {
    uint32_t w1;
    uint32_t w2;
    uint64_t data;
    } packet;

  switch (message->type)
    {
    case CAN_LogFrame_RX:
    case CAN_LogFrame_TX:
      packet.w1 = (uint32_t)message->frame.MsgID <<21;
      packet.w2 = (message->frame.FIR.B.DLC & 0x0f) | (message->origin->m_busnumber << 4);
      memcpy(&packet.data, message->frame.data.u8, 8);
      return std::string((char*)&packet,sizeof(packet));

    default:
      return std::string("");
    }
  }

std::string canformat_panda::getheader(struct timeval *time)
  {
  return std::string("");
  }

size_t canformat_panda::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, bool* hasmore, canlogconnection* clc)
  {
  return len; // Just ignore incoming data
  }
