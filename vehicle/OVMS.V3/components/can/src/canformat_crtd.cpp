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
static const char *TAG = "canformat-crtd";

#include <errno.h>
#include "pcp.h"
#include "canformat_crtd.h"
#include "ovms_utils.h"

class OvmsCanFormatCRTDInit
  {
  public: OvmsCanFormatCRTDInit();
} MyOvmsCanFormatCRTDInit  __attribute__ ((init_priority (4505)));

OvmsCanFormatCRTDInit::OvmsCanFormatCRTDInit()
  {
  ESP_LOGI(TAG, "Registering CAN Format: CRTD (4505)");

  MyCanFormatFactory.RegisterCanFormat<canformat_crtd>("crtd");
  }

canformat_crtd::canformat_crtd(const char *type)
  : canformat(type)
  {
  }

canformat_crtd::~canformat_crtd()
  {
  }

std::string canformat_crtd::get(CAN_log_message_t* message)
  {
  char buf[CANFORMAT_CRTD_MAXLEN];
  char *p;

  char busnumber;
  if (message->origin != NULL)
    { busnumber = message->origin->m_busnumber + '1'; }
  else
    { busnumber = '1'; }

  switch (message->type)
    {
    case CAN_LogFrame_RX:
    case CAN_LogFrame_TX:
      snprintf(buf,sizeof(buf),"%ld.%06ld %c%c%s %0*X",
        message->timestamp.tv_sec, message->timestamp.tv_usec,
        busnumber,
        (message->type == CAN_LogFrame_RX) ? 'R' : 'T',
        (message->frame.FIR.B.FF == CAN_frame_std) ? "11":"29",
        (message->frame.FIR.B.FF == CAN_frame_std) ? 3 : 8,
        message->frame.MsgID);
      p = buf+strlen(buf);
      for (int k=0; k<message->frame.FIR.B.DLC; k++)
        {
        *p++ = ' ';
        p = HexByte(p,message->frame.data.u8[k]);
        }
      *p = 0;
      break;

    case CAN_LogFrame_TX_Queue:
    case CAN_LogFrame_TX_Fail:
      snprintf(buf,sizeof(buf),"%ld.%06ld %cCER %s %c%s %0*X",
        message->timestamp.tv_sec, message->timestamp.tv_usec,
        busnumber,
        GetCanLogTypeName(message->type),
        (message->type == CAN_LogFrame_RX) ? 'R' : 'T',
        (message->frame.FIR.B.FF == CAN_frame_std) ? "11":"29",
        (message->frame.FIR.B.FF == CAN_frame_std) ? 3 : 8,
        message->frame.MsgID);
        p = buf+strlen(buf);
        for (int k=0; k<message->frame.FIR.B.DLC; k++)
          {
          *p++ = ' ';
          p = HexByte(p,message->frame.data.u8[k]);
          }
        *p = 0;
      break;

    case CAN_LogStatus_Error:
    case CAN_LogStatus_Statistics:
      snprintf(buf,sizeof(buf),"%ld.%06ld %c%s %s intr=%d rxpkt=%d txpkt=%d errflags=%#x rxerr=%d txerr=%d rxovr=%d txovr=%d txdelay=%d wdgreset=%d errreset=%d",
        message->timestamp.tv_sec, message->timestamp.tv_usec,
        busnumber,
        (message->type == CAN_LogStatus_Error) ? "CER" : "CST",
        GetCanLogTypeName(message->type), message->status.interrupts,
        message->status.packets_rx, message->status.packets_tx, message->status.error_flags,
        message->status.errors_rx, message->status.errors_tx, message->status.rxbuf_overflow,
        message->status.txbuf_overflow, message->status.txbuf_delay, message->status.watchdog_resets,
        message->status.error_resets);
      break;
      break;

    case CAN_LogInfo_Comment:
    case CAN_LogInfo_Config:
    case CAN_LogInfo_Event:
      snprintf(buf,sizeof(buf),"%ld.%06ld %c%s %s %s",
        message->timestamp.tv_sec, message->timestamp.tv_usec,
        busnumber,
        (message->type == CAN_LogInfo_Event) ? "CEV" : "CXX",
        GetCanLogTypeName(message->type),
        message->text);
      break;

    default:
      buf[0] = 0;
      break;
    }

  strcat(buf,"\n");
  return std::string(buf);
  }

std::string canformat_crtd::getheader(struct timeval *time)
  {
  char buf[CANFORMAT_CRTD_MAXLEN];
  struct timeval t;

  if (time == NULL)
    {
    gettimeofday(&t,NULL);
    time = &t;
    }

  snprintf(buf,sizeof(buf),"%ld.%06ld CXX OVMS CRTD\n",
    time->tv_sec, time->tv_usec);

  return std::string(buf);
  }

size_t canformat_crtd::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata)
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

    // We look for something like
    // 1524311386.811100 1R11 100 01 02 03
    if (!isdigit(b[0])) return consumed;    // Discard invalid line
    for (;((*b != 0)&&(*b != ' '));b++) {}
    if (*b == 0) return consumed;           // Discard invalid line
    b++;
    char bus = '1';
    if (isdigit(*b))
      {
      bus = *b;
      b++;
      }

    if ((b[0]=='R')&&(b[1]=='1')&&(b[2]=='1'))
      {
      // R11 incoming CAN frame
      message->type = CAN_LogFrame_RX;
      message->frame.FIR.B.FF = CAN_frame_std;
      }
    else if ((b[0]=='R')&&(b[1]=='2')&&(b[2]=='9'))
      {
      // R29 incoming CAN frame
      message->type = CAN_LogFrame_RX;
      message->frame.FIR.B.FF = CAN_frame_ext;
      }
    else if ((b[0]=='T')&&(b[1]=='1')&&(b[2]=='1'))
      {
      // T11 outgoing CAN frame
      message->type = CAN_LogFrame_TX;
      message->frame.FIR.B.FF = CAN_frame_std;
      }
    else if ((b[0]=='T')&&(b[1]=='2')&&(b[2]=='9'))
      {
      // T29 outgoingCAN frame
      message->type = CAN_LogFrame_TX;
      message->frame.FIR.B.FF = CAN_frame_ext;
      }
    else
      return consumed;  // Discard invalid line

    if (b[3] != ' ') return consumed; // Discard invalid line
    b += 4;

    char *p;
    errno = 0;
    message->frame.MsgID = (uint32_t)strtol(b,&p,16);
    if ((message->frame.MsgID == 0)&&(errno != 0)) return consumed; // Discard invalid line
    b = p;
    for (int k=0;k<8;k++)
      {
      if (*b==0) break;
      b++;
      errno = 0;
      long d = strtol(b,&p,16);
      if ((d==0)&&(errno != 0)) break;
      message->frame.data.u8[k] = (uint8_t)d;
      message->frame.FIR.B.DLC++;
      b = p;
      }

    message->origin = MyCan.GetBus(bus - '1');

    return consumed;
    }
  }
