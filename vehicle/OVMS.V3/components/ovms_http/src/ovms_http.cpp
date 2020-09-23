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

#include "ovms_log.h"
static const char *TAG = "http";

#include "ovms_http.h"
#include "ovms_config.h"
#include "ovms_tls.h"

////////////////////////////////////////////////////////////////////////////////
// OvmsSyncHttpClient
//
// A synchronous HTTP client, built on top of mongoose networking

OvmsSyncHttpClient::OvmsSyncHttpClient(bool buffer_body)
  {
  m_buffer_body = buffer_body;
  m_waitcompletion = NULL;
  m_mgconn = NULL;
  m_buf = NULL;
  m_inheaders = false;
  m_bodysize = 0;
  m_responsecode = 0;
  }

OvmsSyncHttpClient::~OvmsSyncHttpClient()
  {
  if (m_buf)
    {
    delete m_buf;
    m_buf = NULL;
    }
  }

void OvmsSyncHttpClient::MongooseCallbackEntry(struct mg_connection *nc, int ev, void *ev_data)
  {
  OvmsSyncHttpClient* me = (OvmsSyncHttpClient*)nc->user_data;
  if (me) me->MongooseCallback(nc, ev, ev_data);
  }

void OvmsSyncHttpClient::MongooseCallback(struct mg_connection *nc, int ev, void *ev_data)
  {
  if (nc != m_mgconn) return; // ignore events of previous connections

  switch (ev)
    {
    case MG_EV_CONNECT:
      {
      int err = *(int*) ev_data;
      const char* errdesc = strerror(err);
      if (err)
        {
        ESP_LOGE(TAG, "OvmsSyncHttpClient: MG_EV_CONNECT err=%d/%s", err, errdesc);
        #if MG_ENABLE_SSL
        if (err == MG_SSL_ERROR)
          m_error = "SSL error";
        else
        #endif
          m_error = (errdesc && *errdesc) ? errdesc : "unknown";
        ConnectionFailed(nc);
        nc->flags |= MG_F_CLOSE_IMMEDIATELY;
        }
      else
        ConnectionOk(nc);
      }
      break;

    case MG_EV_TIMER:
      {
      ESP_LOGE(TAG, "OvmsSyncHttpClient: MG_EV_TIMER");
      m_error = "timeout";
      ConnectionTimeout(nc);
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      }
      break;

    case MG_EV_CLOSE:
      {
      if (m_responsecode == 0 && m_error.empty())
        {
        ESP_LOGE(TAG, "OvmsSyncHttpClient: MG_EV_CLOSE: abort");
        m_error = "abort";
        }
      else
        {
        ESP_LOGD(TAG, "OvmsSyncHttpClient: MG_EV_CLOSE status=%d", m_responsecode);
        }
      // Mongoose part done:
      ConnectionClosed(nc);
      nc->user_data = NULL;
      m_mgconn = NULL;
      }
      break;

    case MG_EV_RECV:
      mbuf_remove(&nc->recv_mbuf, ConnectionData(nc,(uint8_t*)nc->recv_mbuf.buf, nc->recv_mbuf.len));
      break;

    default:
      break;
    }
  }

void OvmsSyncHttpClient::ConnectionLaunch()
  {
  m_bodysize = 0;
  m_responsecode = 0;

  // Split URL into server and path components
  if (m_url.compare(0, 8, "https://", 8) == 0)
    {
    m_url = m_url.substr(8);
    m_tls = true;
    }
  else if (m_url.compare(0, 7, "http://", 7) == 0)
    {
    m_url = m_url.substr(7);
    m_tls = false;
    }
  else
    {
    m_tls = false;
    }

  size_t delim = m_url.find('/');
  if (delim==std::string::npos)
    {
    m_server = m_url;
    m_path = std::string("");
    }
  else
    {
    m_server = m_url.substr(0,delim);
    m_path = m_url.substr(delim);
    }

  std::string address = m_server;
  delim = m_server.find(':');
  if (delim==std::string::npos)
    {
    address.append((m_tls)?":443":":80");
    }
  else
    {
    m_server = m_server.substr(0,delim);
    }

  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  struct mg_connect_opts opts;
  const char* err;
  memset(&opts, 0, sizeof(opts));
  opts.error_string = &err;
  opts.user_data = this;
  if (m_tls)
    {
    opts.ssl_ca_cert = MyOvmsTLS.GetTrustedList();
    opts.ssl_server_name = m_server.c_str();
    }

  ESP_LOGD(TAG,"OvmsSyncHttpClient: Connect to %s",address.c_str());
  m_waitcompletion = xSemaphoreCreateBinary();
  if ((m_mgconn = mg_connect_opt(mgr, address.c_str(), MongooseCallbackEntry, opts)) == NULL)
    {
    ESP_LOGE(TAG, "mg_connect(%s) failed: %s", address.c_str(), err);
    m_error = std::string(err);
    vSemaphoreDelete(m_waitcompletion);
    m_waitcompletion = NULL;
    return;
    }

  if (m_buf) delete m_buf;
  m_buf = new OvmsBuffer(4096);
  m_body.clear();
  m_inheaders = true;

  ESP_LOGD(TAG, "OvmsSyncHttpClient: waiting for completion");
  xSemaphoreTake(m_waitcompletion, portMAX_DELAY);
  vSemaphoreDelete(m_waitcompletion);
  m_waitcompletion = NULL;
  }

void OvmsSyncHttpClient::ConnectionOk(struct mg_connection *nc)
  {
  ESP_LOGD(TAG,"OvmsSyncHttpClient: ConnectionOk()");

  std::string req(m_method);
  req.append(" ");
  req.append(m_path);
  req.append(" HTTP/1.0\r\nHost: ");
  req.append(m_server);
  req.append("\r\nUser-Agent: ");
  req.append(get_user_agent());
  req.append("\r\n\r\n");
  mg_send(nc, req.c_str(), req.length());
  }

void OvmsSyncHttpClient::ConnectionFailed(struct mg_connection *nc)
  {
  ESP_LOGD(TAG,"OvmsSyncHttpClient: ConnectionFailed()");

  if (m_waitcompletion != NULL)
    {
    xSemaphoreGive(m_waitcompletion);
    }
  }

void OvmsSyncHttpClient::ConnectionTimeout(struct mg_connection *nc)
  {
  ESP_LOGD(TAG,"OvmsSyncHttpClient: ConnectionTimeout()");

  if (m_waitcompletion != NULL)
    {
    xSemaphoreGive(m_waitcompletion);
    }
  }

void OvmsSyncHttpClient::ConnectionClosed(struct mg_connection *nc)
  {
  ESP_LOGD(TAG,"OvmsSyncHttpClient: ConnectionClosed()");

  if (m_inheaders)
    {
    if (m_error.empty())
      m_error = std::string("Connection closed while processing headers");
    }
  else
    {
    if ((m_body.length() != m_bodysize)&&(m_error.empty()))
      {
      ESP_LOGE(TAG,"OvmsSyncHttpClient: expected %d, actual %d", m_bodysize, m_body.length());
      m_error = std::string("Premature connection close");
      }
    }

  if (m_waitcompletion != NULL)
    {
    xSemaphoreGive(m_waitcompletion);
    }
  }

size_t OvmsSyncHttpClient::ConnectionData(struct mg_connection *nc, uint8_t* data, size_t len)
  {
  size_t consumed = 0;

  if (m_inheaders)
    {
    // Add it to the buffer
    size_t avail = m_buf->FreeSpace();
    if (avail < len)
      {
      m_buf->Push(data,avail);
      data += avail;
      consumed += avail;
      len -= avail;
      }
    else
      {
      m_buf->Push(data,len);
      data += len;
      consumed += len;
      len = 0;
      }

    ConnectionHeaders();
    }

  if (! m_inheaders)
    {
    size_t bu = m_buf->UsedSpace();
    if (!m_buffer_body)
      {
      // Don't buffer the body...
      if (bu > 0)
        {
        uint8_t* buf = new uint8_t[bu];
        m_buf->Pop(bu,buf);
        ConnectionBodyData(buf,bu);
        delete [] buf;
        }
      if (len > 0)
        {
        //ESP_LOGD(TAG, "OvmsSyncHttpClient:: body: append %d bytes",len);
        ConnectionBodyData(data,len);
        consumed += len;
        }
      }
    else
      {
      // Buffer the body...
      if (bu > 0)
        {
        // Empty any remaining buffer left over from header processing
        //ESP_LOGD(TAG, "OvmsSyncHttpClient:: body: buffer is %d bytes",bu);
        uint8_t* buf = new uint8_t[bu];
        m_buf->Pop(bu,buf);
        m_body.append((const char*)buf,bu);
        delete [] buf;
        }
      if (len > 0)
        {
        //ESP_LOGD(TAG, "OvmsSyncHttpClient:: body: append %d bytes",len);
        m_body.append((const char*)data,len);
        consumed += len;
        }
      }
    }

  return consumed;
  }

void OvmsSyncHttpClient::ConnectionHeaders()
  {
  int k = m_buf->HasLine();
  while ((m_inheaders)&&(k >= 0))
    {
    std::string header = m_buf->ReadLine();
    ESP_LOGD(TAG, "OvmsSyncHttpClient:: header %s",header.c_str());
    if (k == 0)
      {
      m_inheaders = false;
      return;
      }
    else
      {
      if (header.compare(0,15,"Content-Length:") == 0)
        {
        m_bodysize = atoi(header.substr(15).c_str());
        m_body.reserve(m_bodysize+1);
        }
      else if (header.compare(0,5,"HTTP/") == 0)
        {
        size_t space = header.find(' ');
        if (space!=std::string::npos)
          {
          m_responsecode = atoi(header.substr(space+1).c_str());
          }
        }
      k = m_buf->HasLine();
      }
    }
  }

void OvmsSyncHttpClient::ConnectionBodyData(uint8_t* data, size_t len)
  {
  }

bool OvmsSyncHttpClient::Request(std::string url, const char* method)
  {
  m_url = url;
  m_method = method;

  ConnectionLaunch();

  return (m_error.empty());
  }

std::string OvmsSyncHttpClient::GetBodyAsString()
  {
  return m_body;
  }

OvmsBuffer* OvmsSyncHttpClient::GetBodyAsBuffer()
  {
  if (m_buf) delete m_buf;

  m_buf = new OvmsBuffer(m_body.length());
  m_buf->Push((uint8_t*)m_body.c_str(),m_body.length());
  m_body.clear();

  return m_buf;
  }

int OvmsSyncHttpClient::GetResponseCode()
  {
  return m_responsecode;
  }

std::string OvmsSyncHttpClient::GetError()
  {
  return m_error;
  }

void OvmsSyncHttpClient::Reset()
  {
  if (m_buf)
    {
    delete m_buf;
    m_buf = NULL;
    }
  m_inheaders = false;
  m_bodysize = 0;
  m_responsecode = 0;
  }


////////////////////////////////////////////////////////////////////////////////
// OvmsHttpClient
//
// Obsolete

OvmsHttpClient::OvmsHttpClient()
  {
  m_buf = NULL;
  m_bodysize = 0;
  m_responsecode = 0;
  }

OvmsHttpClient::OvmsHttpClient(std::string url, const char* method)
  {
  m_buf = NULL;
  Request(url, method);
  }

OvmsHttpClient::~OvmsHttpClient()
  {
  if (m_buf)
    {
    delete m_buf;
    m_buf = NULL;
    }
  }

bool OvmsHttpClient::Request(std::string url, const char* method)
  {
  m_bodysize = 0;
  m_responsecode = 0;

  // First, split URL into server and path components
  if (url.compare(0, 7, "http://", 7) == 0)
    {
    url = url.substr(7);
    }
  std::string server;
  std::string path;
  std::string service;
  size_t delim = url.find('/');
  if (delim==std::string::npos)
    {
    server = url;
    path = std::string("");
    }
  else
    {
    server = url.substr(0,delim);
    path = url.substr(delim);
    }
  delim = server.find(':');
  if (delim==std::string::npos)
    {
    service = std::string("80");
    }
  else
    {
    service = server.substr(delim+1);
    server = server.substr(0,delim);
    }

  Connect(server.c_str(), service.c_str());
  if (!IsOpen())
    {
    return false;
    }

  // Now, we need to send the HTTP request...
  // ESP_LOGI(TAG, "Server is %s, path is %s",server.c_str(),path.c_str());
  std::string req(method);
  req.append(" ");
  req.append(path);
  req.append(" HTTP/1.0\r\nHost: ");
  req.append(server);
  req.append("\r\nUser-Agent: ");
  req.append(get_user_agent());
  req.append("\r\n\r\n");
  if (Write(req.c_str(), req.length()) < 0)
    {
    ESP_LOGE(TAG, "Unable to write to server connection");
    Disconnect();
    return false;
    }

  m_buf = new OvmsBuffer(1024);
  bool inheaders = true;
  // ESP_LOGI(TAG,"Reading headers...");
  while((inheaders)&&(m_buf->PollSocket(m_sock, 10000) >= 0))
    {
    // ESP_LOGI(TAG, "Now buffered %d bytes",m_buf->UsedSpace());
    int k = m_buf->HasLine();
    while ((inheaders)&&(k >= 0))
      {
      if (k == 0)
        {
        inheaders = 0;
        }
      else
        {
        // ESP_LOGI(TAG, "Got response %s",m_buf->ReadLine().c_str());
        std::string header = m_buf->ReadLine();
        if (header.compare(0,15,"Content-Length:") == 0)
          {
          m_bodysize = atoi(header.substr(15).c_str());
          }
        if (header.compare(0,5,"HTTP/") == 0)
          {
          size_t space = header.find(' ');
          if (space!=std::string::npos)
            {
            m_responsecode = atoi(header.substr(space+1).c_str());
            }
          }
        k = m_buf->HasLine();
        }
      }
    }
  if (inheaders)
    {
    ESP_LOGE(TAG, "Error: Premature end of server response");
    Disconnect();
    return false;
    }

  // Process the separator line
  // m_buf->Diagnostics();
  m_buf->ReadLine(); // Discard the empty header/body line

  return true;
  }

void OvmsHttpClient::Disconnect()
  {
  if (m_buf != NULL)
    {
    delete m_buf;
    m_buf = NULL;
    }
  OvmsNetTcpConnection::Disconnect();
  }

size_t OvmsHttpClient::BodyRead(void *buf, size_t nbyte)
  {
  // char *x = (char*)buf;
  if ((m_buf == NULL)||(m_buf->UsedSpace() == 0))
    {
    size_t n = Read(buf,nbyte);
    // ESP_LOGI(TAG, "BodyRead got %d bytes direct (%02x %02x %02x %02x)",n,x[0],x[1],x[2],x[3]);
    return n;
    }
  else
    {
    size_t n = m_buf->Pop(nbyte,(uint8_t*)buf);
    // ESP_LOGI(TAG, "Body Read got %d bytes from buf (%d left) (%02x %02x %02x %02x)",n,m_buf->UsedSpace(),x[0],x[1],x[2],x[3]);
    if (m_buf->UsedSpace() == 0)
      {
      delete m_buf;
      m_buf = NULL;
      }
    return n;
    }
  }

int OvmsHttpClient::BodyHasLine()
  {
  if (m_buf == NULL)
    return -1;

  if (m_buf->HasLine()<0)
    {
    m_buf->PollSocket(m_sock, 10000);
    // m_buf->Diagnostics();
    }
  return m_buf->HasLine();
  }

std::string OvmsHttpClient::BodyReadLine()
  {
  if (m_buf == NULL)
    return std::string("");

  if (m_buf->HasLine()<0)
    {
    m_buf->PollSocket(m_sock, 10000);
    // m_buf->Diagnostics();
    }

  return m_buf->ReadLine();
  }

size_t OvmsHttpClient::BodySize()
  {
  return m_bodysize;
  }

int OvmsHttpClient::ResponseCode()
  {
  return m_responsecode;
  }
