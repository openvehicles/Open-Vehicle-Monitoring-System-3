/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN logging framework
;    Date:          18th January 2018
;
;    (C) 2018       Michael Balzer
;    (C) 2019       Mark Webb-Johnson
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

#ifndef __CANLOG_MONITOR_H__
#define __CANLOG_MONITOR_H__

#include "canlog.h"

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

class canlog_monitor_conn: public canlogconnection
  {
  public:
    canlog_monitor_conn(canlog* logger, std::string format, canformat::canformat_serve_mode_t mode);
    virtual ~canlog_monitor_conn();

  public:
    virtual void OutputMsg(CAN_log_message_t& msg, std::string &result);
  };

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

class canlog_monitor : public canlog
  {
  public:
    canlog_monitor(std::string format);
    virtual ~canlog_monitor();

  public:
    virtual bool Open();
    virtual void Close();
  };

#endif // __CANLOG_MONITOR_H__
