/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN dump CRTD format
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

#ifndef __CANFORMAT_CRTD_H__
#define __CANFORMAT_CRTD_H__

#include "canformat.h"

#define CANFORMAT_CRTD_MAXLEN 64

class canformat_crtd : public canformat
  {
  public:
    canformat_crtd(const char* type);
    virtual ~canformat_crtd();

  public:
    virtual std::string get(CAN_log_message_t* message);
    virtual std::string getheader(struct timeval *time);
    virtual size_t put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata=NULL);
  };

#endif // __CANFORMAT_CRTD_H__
