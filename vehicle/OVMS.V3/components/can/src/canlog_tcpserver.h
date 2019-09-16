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

#ifndef __CANLOG_TCP_SERVER_H__
#define __CANLOG_TCP_SERVER_H__

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

#include "canlog.h"
#include "canlog_tcpserver.h"
#include "ovms_netmanager.h"
#include "ovms_mutex.h"

class canlog_tcpserver : public canlog
  {
  public:
    canlog_tcpserver(std::string path, std::string format, canformat::canformat_serve_mode_t mode);
    virtual ~canlog_tcpserver();

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
    typedef std::map<mg_connection*, uint8_t> ts_map_t;
    OvmsMutex m_mgmutex;
    ts_map_t m_smap;
    bool m_isopen;
    struct mg_connection *m_mgconn;

  public:
    std::string         m_path;
  };

#endif // #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
#endif // __CANLOG_TCP_SERVER_H__
