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
static const char *TAG = "canformat-gvret";

#include <errno.h>
#include <endian.h>
#include "pcp.h"
#include "canformat_gvret.h"

////////////////////////////////////////////////////////////////////////
// Initialisation and Registration
////////////////////////////////////////////////////////////////////////

class OvmsCanFormatGVRETInit
  {
  public: OvmsCanFormatGVRETInit();
} MyOvmsCanFormatGVRETInit  __attribute__ ((init_priority (4505)));

OvmsCanFormatGVRETInit::OvmsCanFormatGVRETInit()
  {
  ESP_LOGI(TAG, "Registering CAN Format: GVRET (4505)");

  MyCanFormatFactory.RegisterCanFormat<canformat_gvret_ascii>("gvret-a");
  MyCanFormatFactory.RegisterCanFormat<canformat_gvret_binary>("gvret-b");
  }

////////////////////////////////////////////////////////////////////////
// Base GVRET implementation (utility)
////////////////////////////////////////////////////////////////////////

canformat_gvret::canformat_gvret(const char* type)
  : canformat(type)
  {
  m_bufpos = 0;
  m_discarding = false;
  }

canformat_gvret::~canformat_gvret()
  {
  }

std::string canformat_gvret::get(CAN_log_message_t* message)
  {
  return std::string("");
  }

std::string canformat_gvret::getheader(struct timeval *time)
  {
  return std::string("");
  }

size_t canformat_gvret::put(CAN_log_message_t* message, uint8_t *buffer, size_t len)
  {
  return len;
  }

////////////////////////////////////////////////////////////////////////
// ASCII format GVRET
////////////////////////////////////////////////////////////////////////

canformat_gvret_ascii::canformat_gvret_ascii(const char* type)
  : canformat_gvret(type)
  {
  }

std::string canformat_gvret_ascii::get(CAN_log_message_t* message)
  {
  char buf[CANFORMAT_GVRET_MAXLEN];

  if ((message->type != CAN_LogFrame_RX)&&
      (message->type != CAN_LogFrame_TX))
    {
    return std::string("");
    }

  const char* busnumber;
  if (message->origin != NULL)
    { busnumber = message->origin->GetName()+3; }
  else
    { busnumber = "1"; }

  sprintf(buf,"%u - %x %s %c %d",
    (uint32_t)((message->timestamp.tv_sec * 1000000) + message->timestamp.tv_usec),
    message->frame.MsgID,
    (message->frame.FIR.B.FF == CAN_frame_std) ? "S" : "X",
    busnumber[0]-1,
    message->frame.FIR.B.DLC);
    for (int k=0; k<message->frame.FIR.B.DLC; k++)
      sprintf(buf+strlen(buf)," %02x", message->frame.data.u8[k]);

  strcat(buf,"\n");
  return std::string(buf);
  }

size_t canformat_gvret_ascii::put(CAN_log_message_t* message, uint8_t *buffer, size_t len)
  {
  return len;
  }

////////////////////////////////////////////////////////////////////////
// BINARY format GVRET
////////////////////////////////////////////////////////////////////////

canformat_gvret_binary::canformat_gvret_binary(const char* type)
  : canformat_gvret(type)
  {
  m_expecting = 0;
  }

std::string canformat_gvret_binary::get(CAN_log_message_t* message)
  {
  gvret_binary_frame_t frame;
  memset(&frame,0,sizeof(frame));

  if ((message->type != CAN_LogFrame_RX)&&
      (message->type != CAN_LogFrame_TX))
    {
    return std::string("");
    }

  const char* busnumber;
  if (message->origin != NULL)
    { busnumber = message->origin->GetName()+3; }
  else
    { busnumber = "1"; }


  frame.startbyte = GVRET_START_BYTE;
  frame.command = BUILD_CAN_FRAME;
  frame.microseconds = (uint32_t)((message->timestamp.tv_sec * 1000000) + message->timestamp.tv_usec);
  frame.id = (uint32_t)message->frame.MsgID +
              (message->frame.FIR.B.FF == CAN_frame_std)? 0 : 0x8000;
  frame.lenbus = message->frame.FIR.B.DLC + ((busnumber[0]-1)<<4);
  for (int k=0; k<message->frame.FIR.B.DLC; k++)
    frame.data[k] = message->frame.data.u8[k];
  return std::string((const char*)&frame,12 + message->frame.FIR.B.DLC);
  }

size_t canformat_gvret_binary::put(CAN_log_message_t* message, uint8_t *buffer, size_t len)
  {
  return len;
  }
