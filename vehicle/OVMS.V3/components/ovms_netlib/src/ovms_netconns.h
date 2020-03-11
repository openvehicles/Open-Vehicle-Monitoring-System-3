/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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

#ifndef __OVMS_NETCONNS_H__
#define __OVMS_NETCONNS_H__

#include "ovms_mutex.h"
#include "ovms_netmanager.h"

class OvmsMongooseWrapper
  {
  public:
    OvmsMongooseWrapper();
    virtual ~OvmsMongooseWrapper();

  public:
    virtual void Mongoose(struct mg_connection *nc, int ev, void *ev_data);

  protected:
    OvmsMutex m_mgconn_mutex;
  };

class OvmsNetTcpClient: public OvmsMongooseWrapper
  {
  public:
    OvmsNetTcpClient();
    virtual ~OvmsNetTcpClient();

  public:
    virtual void Mongoose(struct mg_connection *nc, int ev, void *ev_data);

  public:
    bool Connect(std::string dest, struct mg_connect_opts opts);
    void Disconnect();
    size_t SendData(uint8_t *data, size_t length);
    bool IsConnected();

  public:
    virtual void Connected();
    virtual void ConnectionFailed();
    virtual void ConnectionClosed();
    virtual size_t IncomingData(void *data, size_t length);

  public:
    enum NetState
      {
      NetConnIdle = 0,
      NetConnConnecting,
      NetConnConnected,
      NetConnDisconnected,
      NetConnFailed
      };

  protected:
    std::string m_dest;
    struct mg_connection *m_mgconn;
    NetState m_netstate;
  };

#endif //#ifndef __OVMS_NETCONNS_H__
