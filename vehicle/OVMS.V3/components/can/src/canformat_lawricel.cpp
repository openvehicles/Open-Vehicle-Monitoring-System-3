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
static const char *TAG = "canformat-lawricel";

#include <errno.h>
#include "pcp.h"
#include "canformat_lawricel.h"

////////////////////////////////////////////////////////////////////////
// Initialisation and Registration
////////////////////////////////////////////////////////////////////////

class OvmsCanFormatLawricelInit
  {
  public: OvmsCanFormatLawricelInit();
} MyOvmsCanFormatLawricelTInit  __attribute__ ((init_priority (4505)));

OvmsCanFormatLawricelInit::OvmsCanFormatLawricelInit()
  {
  ESP_LOGI(TAG, "Registering CAN Format: LAWRICEL (4505)");

  MyCanFormatFactory.RegisterCanFormat<canformat_lawricel>("lawricel");
  }

////////////////////////////////////////////////////////////////////////
// Base GVRET implementation (utility)
////////////////////////////////////////////////////////////////////////

canformat_lawricel::canformat_lawricel(const char* type)
  : canformat(type)
  {
  }

canformat_lawricel::~canformat_lawricel()
  {
  }

std::string canformat_lawricel::get(CAN_log_message_t* message)
  {
  char buf[CANFORMAT_LAWRICEL_MAXLEN];

  if ((message->type != CAN_LogFrame_RX)&&
      (message->type != CAN_LogFrame_TX))
    {
    return std::string("");
    }

  if (message->frame.FIR.B.FF == CAN_frame_std)
    {
    sprintf(buf,"t%03x%01d",message->frame.MsgID, message->frame.FIR.B.DLC);
    }
  else
    {
    sprintf(buf,"T%08x%01d",message->frame.MsgID, message->frame.FIR.B.DLC);
    }

  for (int k=0; k<message->frame.FIR.B.DLC; k++)
    sprintf(buf+strlen(buf),"%02x", message->frame.data.u8[k]);
  sprintf(buf+strlen(buf),"%04lx", message->timestamp.tv_usec/1000);

  strcat(buf,"\n");
  return std::string(buf);
  }

std::string canformat_lawricel::getheader(struct timeval *time)
  {
  return std::string("");
  }

size_t canformat_lawricel::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata)
  {
  if (m_buf.FreeSpace()==0) SetServeDiscarding(true); // Buffer full, so discard from now on
  if (IsServeDiscarding()) return len;  // Quick return if discarding

  size_t consumed = Stuff(buffer,len);  // Stuff m_buf with as much as possible

  if (!m_buf.HasLine())
    {
    return consumed; // No line, so quick exit
    }
  else
    {
    std::string line = m_buf.ReadLine();
    const char *b = line.c_str();
    char hex[9];

    // We look for something like
    // t100401020304000a
    if (*b == 't')
      {
      // Standard frame
      memcpy(hex,b+1,3);
      hex[3] = 0;
      message->type = CAN_LogFrame_RX;
      message->frame.FIR.B.FF = CAN_frame_std;
      message->frame.MsgID = strtol(hex,NULL,16);
      b += 4;
      }
    else if (*b == 'T')
      {
      // Extended frame
      memcpy(hex,b+1,8);
      hex[8] = 0;
      message->type = CAN_LogFrame_RX;
      message->frame.FIR.B.FF = CAN_frame_ext;
      message->frame.MsgID = strtol(hex,NULL,16);
      b += 9;
      }
    else
      {
      // Unknown format - discard
      return consumed; // Discard invalid line
      }

    message->frame.FIR.B.DLC = *b - '0';
    if (message->frame.FIR.B.DLC > 8)
      {
      // Invalid length - discard
      return consumed; // Discard invalid line
      }

    b++;
    for (size_t x=0;x<message->frame.FIR.B.DLC;x++)
      {
      hex[0] = b[0];
      hex[1] = b[2];
      hex[2] = 0;
      b += 2;
      message->frame.data.u8[x] = (uint8_t)strtol(hex,NULL,16);
      }

    gettimeofday(&message->timestamp,NULL);
    message->origin = (canbus*)MyPcpApp.FindDeviceByName("can1");

    return consumed;
    }
  }
