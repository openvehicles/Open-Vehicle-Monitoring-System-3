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
#include "metrics_standard.h"

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
    // ESP_EARLY_LOGI(TAG, "BodyRead got %d bytes direct (%02x %02x %02x %02x)",n,x[0],x[1],x[2],x[3]);
    return n;
    }
  else
    {
    size_t n = m_buf->Pop(nbyte,(uint8_t*)buf);
    // ESP_EARLY_LOGI(TAG, "Body Read got %d bytes from buf (%d left) (%02x %02x %02x %02x)",n,m_buf->UsedSpace(),x[0],x[1],x[2],x[3]);
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
