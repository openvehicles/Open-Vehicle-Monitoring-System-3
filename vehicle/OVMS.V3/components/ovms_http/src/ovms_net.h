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

#ifndef __OVMS_NET_H__
#define __OVMS_NET_H__

#include <stdint.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

class OvmsNetConnection
  {
  public:
    OvmsNetConnection();
    virtual ~OvmsNetConnection();

  public:
    virtual bool IsOpen();
    virtual void Disconnect();
    virtual int Socket();

  public:
    virtual ssize_t Write(const void *buf, size_t nbyte);
    virtual size_t Read(void *buf, size_t nbyte);

  protected:
    int m_sock;
  };

class OvmsNetTcpConnection : public OvmsNetConnection
  {
  public:
    OvmsNetTcpConnection();
    OvmsNetTcpConnection(const char* host, const char* service);
    virtual ~OvmsNetTcpConnection();

  public:
    virtual bool Connect(const char* host, const char* service);
  };

#endif //#ifndef __OVMS_NET_H__
