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
#include "ovms_netmanager.h"
#include "ovms_buffer.h"

class OvmsSyncHttpClient
  {
  public:
    OvmsSyncHttpClient(bool buffer_body = true);
    virtual ~OvmsSyncHttpClient();

  public:
    static void MongooseCallbackEntry(struct mg_connection *nc, int ev, void *ev_data);
    virtual void MongooseCallback(struct mg_connection *nc, int ev, void *ev_data);
    virtual void ConnectionLaunch();
    virtual void ConnectionOk(struct mg_connection *nc);
    virtual void ConnectionFailed(struct mg_connection *nc);
    virtual void ConnectionTimeout(struct mg_connection *nc);
    virtual void ConnectionClosed(struct mg_connection *nc);
    virtual size_t ConnectionData(struct mg_connection *nc, uint8_t* data, size_t len);
    virtual void ConnectionHeaders(struct mg_connection *nc);
    virtual void ConnectionBodyStart(struct mg_connection *nc);
    virtual void ConnectionBodyData(struct mg_connection *nc, uint8_t* data, size_t len);
    virtual void ConnectionBodyFinish(struct mg_connection *nc);

  public:
    bool Request(std::string url, const char* method = "GET");
    std::string GetBodyAsString();
    OvmsBuffer* GetBodyAsBuffer();
    int GetResponseCode();
    std::string GetError();
    bool HasError();
    size_t GetBodySize();
    void Reset();

  protected:
    bool m_buffer_body;
    QueueHandle_t m_waitcompletion;
    struct mg_connection *m_mgconn;
    OvmsBuffer* m_buf;
    std::string m_body;
    std::string m_url;
    std::string m_server;
    std::string m_path;
    bool m_tls;
    const char* m_method;
    bool m_inheaders;
    size_t m_bodysize;
    int m_responsecode;
    std::string m_error;
  };

#endif //#ifndef __OVMS_HTTP_H__
