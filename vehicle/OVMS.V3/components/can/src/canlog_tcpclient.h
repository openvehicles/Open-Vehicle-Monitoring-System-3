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

#ifndef __CANLOG_TCP_CLIENT_H__
#define __CANLOG_TCP_CLIENT_H__

#include "canlog.h"
#include "ovms_netmanager.h"
#include "ovms_mutex.h"

class canlog_tcpclient : public canlog
  {
  public:
    canlog_tcpclient(std::string path, std::string format);
    virtual ~canlog_tcpclient();

  public:
    virtual bool Open();
    virtual void Close();
    virtual bool IsOpen();
    virtual std::string GetInfo();

  public:
    virtual void OutputMsg(CAN_log_message_t& msg);

  public:
    void MongooseHandler(struct mg_connection *nc, int ev, void *p);

  public:
    OvmsMutex m_mgmutex;
    struct mg_connection *m_mgconn;
    bool m_isopen;

  public:
    std::string         m_path;
  };

#endif // __CANLOG_TCP_CLIENT_H__
