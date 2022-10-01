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
static const char *TAG = "canformat-cs11";

#include <errno.h>
#include "pcp.h"
#include "canlog.h"
#include "canformat_canswitch.h"
#include "ovms_utils.h"

class OvmsCanFormatCanSwitchInit
  {
  public: OvmsCanFormatCanSwitchInit();
} OvmsCanFormatCanSwitchInit  __attribute__ ((init_priority (4505)));

OvmsCanFormatCanSwitchInit::OvmsCanFormatCanSwitchInit()
  {
  ESP_LOGI(TAG, "Registering CAN Format: CS11 (4505)");

  MyCanFormatFactory.RegisterCanFormat<canformat_cs11>("cs11");
  }

canformat_cs11::canformat_cs11(const char *type)
  : canformat(type)
  {
  m_syncing = false;
  }

canformat_cs11::~canformat_cs11()
  {
  }

std::string canformat_cs11::get(CAN_log_message_t* message)
  {
  char buf[CANFORMAT_CS11_MAXLEN];

  char busnumber;
  if (message->origin != NULL)
    { busnumber = message->origin->m_busnumber + '1'; }
  else
    { busnumber = '1'; }

  switch (message->type)
    {
    case CAN_LogFrame_RX:
      if (message->frame.FIR.B.FF == CAN_frame_std)
        {
        buf[0] = message->frame.FIR.B.DLC + 3;
        buf[1] = busnumber - '1';
        buf[2] = message->frame.MsgID & 0xff;
        buf[3] = (message->frame.MsgID >> 8) & 0xff;
        memcpy(buf+4,message->frame.data.u8,message->frame.FIR.B.DLC);
        return std::string(buf,message->frame.FIR.B.DLC+4);
        }
      break;
    default:
      break;
    }

  return std::string("");
  }

std::string canformat_cs11::getheader(struct timeval *time)
  {
  return std::string("");
  }

size_t canformat_cs11::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, bool* hasmore, canlogconnection* clc)
  {
  char buf[CANFORMAT_CS11_MAXLEN];

  if (m_buf.FreeSpace()==0) SetServeDiscarding(true); // Buffer full, so discard from now on
  if (IsServeDiscarding()) return len;  // Quick return if discarding

  size_t consumed = Stuff(buffer,len);  // Stuff m_buf with as much as possible
  if (m_buf.UsedSpace() >= 1)
    {
    uint8_t len = m_buf.Peek();
    if (len==0)
      {
      *hasmore = true;  // Call us again to see if we have more frames to process
      m_syncing = true;
      m_buf.Pop(1,(uint8_t*)buf);
      return consumed;  // CMD_SYNC
      }
    else if (len==255)
      {
      if (m_buf.UsedSpace() >= 3)
        {
        *hasmore = true;  // Call us again to see if we have more frames to process
        m_buf.Pop(3,(uint8_t*)buf);
        canbus* bus = MyCan.GetBus(buf[1]);
        if (bus != NULL)
          {
          CAN_speed_t speed;
          switch(buf[2])
            {
            case 0:
              speed = CAN_SPEED_125KBPS;
              break;
            case 1:
              speed = CAN_SPEED_250KBPS;
              break;
            case 2:
              speed = CAN_SPEED_500KBPS;
              break;
            case 3:
              speed = CAN_SPEED_1000KBPS;
              break;
            case 4:
              speed = CAN_SPEED_83KBPS;
              break;
            default:
              return consumed;
            }
          bus->Start(CAN_MODE_ACTIVE, speed);
          }
        return consumed;  // CMD_SYNC
        }
      }
    else if (m_buf.UsedSpace() >= (len+1))
      {
      // We have enough data for one packet
      *hasmore = true;  // Call us again to see if we have more frames to process
      m_buf.Pop(len+1,(uint8_t*)buf);
      message->type = CAN_LogFrame_TX;
      message->frame.FIR.B.FF = CAN_frame_std;
      message->origin = MyCan.GetBus(buf[1]);
      message->frame.MsgID = ((uint16_t)buf[3]<<8) + buf[2];
      message->frame.FIR.B.DLC = len-3;
      memcpy(message->frame.data.u8,buf+4,message->frame.FIR.B.DLC);

      if (m_syncing)
        {
        uint8_t ack[2];
        ack[0] = 1;
        ack[1] = buf[1];
        if (clc) clc->TransmitCallback(ack,2);
        }

      return consumed;
      }
    }
  return consumed;
  }
