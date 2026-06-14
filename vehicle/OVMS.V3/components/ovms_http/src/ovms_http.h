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

#ifndef __OVMS_HTTP_H__
#define __OVMS_HTTP_H__

#include <string>
#include "ovms_net.h"
#include "ovms_buffer.h"

class OvmsHttpClient : public OvmsNetTcpConnection
  {
  public:
    OvmsHttpClient(long timeout_ms = 10000);
    OvmsHttpClient(std::string url, const char* method = "GET", long timeout_ms = 10000);
    virtual ~OvmsHttpClient();

  public:
    virtual void Disconnect();

  public:
    bool Request(std::string url, const char* method = "GET");
    ssize_t BodyRead(void *buf, size_t nbyte);
    int BodyHasLine();
    std::string BodyReadLine();
    size_t BodySize();
    int ResponseCode();
    std::string GetBodyAsString();
    void Reset();

  protected:
    OvmsBuffer* m_buf;
    size_t m_bodysize;
    int m_responsecode;
    long m_timeout_ms = 10000;
  };

#endif //#ifndef __OVMS_HTTP_H__
