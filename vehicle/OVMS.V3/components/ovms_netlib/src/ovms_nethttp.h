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

#ifndef __OVMS_NETHTTP_H__
#define __OVMS_NETHTTP_H__

#include "ovms_mutex.h"
#include "ovms_netconns.h"
#include "ovms_netmanager.h"
#include "ovms_buffer.h"

class OvmsNetHttpAsyncClient: public OvmsNetTcpClient
  {
  public:
    OvmsNetHttpAsyncClient();
    virtual ~OvmsNetHttpAsyncClient();

  public:
    enum NetHttpState
      {
      NetHttpIdle = 0,
      NetHttpConnecting,
      NetHttpHeaders,
      NetHttpBody,
      NetHttpComplete,
      NetHttpFailed
      };

  public:
    bool Request(std::string url, const char* method = "GET");
    int ResponseCode();
    size_t BodySize();
    OvmsNetHttpAsyncClient::NetHttpState GetState();
    OvmsBuffer* GetBuffer();

  protected:
    virtual void Connected();
    virtual void ConnectionFailed();
    virtual void ConnectionClosed();
    virtual size_t IncomingData(void *data, size_t length);

  public:
    virtual void HeadersAvailable();
    virtual void BodyAvailable();

  protected:
    OvmsBuffer* m_buf;
    std::string m_url;
    bool m_tls;
    std::string m_dest;
    std::string m_server;
    std::string m_path;
    const char* m_method;
    NetHttpState m_httpstate;
    size_t m_bodysize;
    int m_responsecode;
  };

#endif //#ifndef __OVMS_NETCONNS_H__
