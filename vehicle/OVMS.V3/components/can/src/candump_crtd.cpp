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

candump_crtd::~candump_crtd()
  {
  }

const char* candump_crtd::formatname()
  {
  return "crtd";
  }

std::string candump_crtd::get(struct timeval *time, CAN_frame_t *frame)
  {
  char buffer[64];
  char busnumber[2]; busnumber[0]=0; busnumber[1]=0;

  if (frame->origin)
    {
    const char* name = frame->origin->GetName();
    if (*name != 0)
      {
      while (name[1] != 0) name++;
      busnumber[0] = *name;
      }
    }

  sprintf(buffer,"%ld.%06ld %sR%s %0*X",
    time->tv_sec, time->tv_usec,
    busnumber,
    (frame->FIR.B.FF == CAN_frame_std) ? "11":"29",
    (frame->FIR.B.FF == CAN_frame_std) ? 3 : 8,
    frame->MsgID);

  for (int k=0; k<frame->FIR.B.DLC; k++)
    sprintf(buffer+strlen(buffer)," %02x", frame->data.u8[k]);

  strcat(buffer,"\n");

  return std::string(buffer);
  }
