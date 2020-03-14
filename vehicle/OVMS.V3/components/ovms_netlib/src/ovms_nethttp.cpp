/*
;    Project:       Open Vehicle Monitor System
;    Date:          9th March 2020
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
static const char *TAG = "ovms-net-http";

#include "ovms.h"
#include "ovms_nethttp.h"
#include "ovms_tls.h"
#include "ovms_utils.h"
#include "ovms_buffer.h"

////////////////////////////////////////////////////////////////////////////////
// OvmsNetHttpAsyncClient
////////////////////////////////////////////////////////////////////////////////

OvmsNetHttpAsyncClient::OvmsNetHttpAsyncClient()
  {
  m_buf = NULL;
  m_httpstate = NetHttpIdle;
  }

OvmsNetHttpAsyncClient::~OvmsNetHttpAsyncClient()
  {
  if (m_buf != NULL)
    {
    delete m_buf;
    m_buf = NULL;
    }
  }

bool OvmsNetHttpAsyncClient::Request(std::string url, const char* method)
  {
  m_url = url;
  m_method = method;

  // First, split URL into server and path components
  if (url.compare(0, 7, "http://", 7) == 0)
    {
    url = url.substr(7);
    m_tls = false;
    }
  else if (url.compare(0, 8, "https://", 8) == 0)
    {
    url = url.substr(8);
    m_tls = true;
    }
  else
    {
    // Just assume http:// was at the start
    m_tls = false;
    }

  size_t delim = url.find('/');
  if (delim==std::string::npos)
    {
    m_dest = url;
    m_path = std::string("");
    }
  else
    {
    m_dest = url.substr(0,delim);
    m_path = url.substr(delim);
    }

  delim = m_dest.find(':');
  if (delim==std::string::npos)
    {
    m_server = m_dest;
    if (m_tls)
      { m_dest.append(":443"); }
    else
      { m_dest.append(":80"); }
    }
  else
    {
    m_server = m_dest.substr(0,delim);
    }

  ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Connect to %s for request %s %s", m_dest.c_str(), method, url.c_str());
  struct mg_connect_opts opts;
  memset(&opts, 0, sizeof(opts));
  if (m_tls)
    {
    opts.ssl_ca_cert = MyOvmsTLS.GetTrustedList();
    opts.ssl_server_name = m_server.c_str();
    }

  m_httpstate = NetHttpConnecting;
  return Connect(m_dest, opts);
  }

int OvmsNetHttpAsyncClient::ResponseCode()
  {
  return m_responsecode;
  }

size_t OvmsNetHttpAsyncClient::BodySize()
  {
  return m_bodysize;
  }

OvmsNetHttpAsyncClient::NetHttpState OvmsNetHttpAsyncClient::GetState()
  {
  return m_httpstate;
  }

OvmsBuffer* OvmsNetHttpAsyncClient::GetBuffer()
  {
  return m_buf;
  }

void OvmsNetHttpAsyncClient::Connected()
  {
  ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Connected");

  std::string req(m_method);
  req.append(" ");
  req.append(m_path);
  req.append(" HTTP/1.0\r\nHost: ");
  req.append(m_server);
  req.append("\r\nUser-Agent: ");
  req.append(get_user_agent());
  req.append("\r\n\r\n");
  SendData((uint8_t*)req.c_str(), req.length());

  m_buf = new OvmsBuffer(4096);
  m_httpstate = NetHttpHeaders;
  }

void OvmsNetHttpAsyncClient::ConnectionFailed()
  {
  ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Connection failed");
  m_httpstate = NetHttpFailed;
  }

void OvmsNetHttpAsyncClient::ConnectionClosed()
  {
  ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Connection closed");
  m_httpstate = NetHttpComplete;
  }

size_t OvmsNetHttpAsyncClient::IncomingData(void *data, size_t length)
  {
  if (m_buf == NULL) return length;

  size_t done = 0;
  while (done < length)
    {
    size_t toread = length;
    if (toread > m_buf->FreeSpace()) toread=m_buf->FreeSpace();
    m_buf->Push((uint8_t*)data,toread);
    done += toread;
    ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Buffered %d/%d (%d remaining, %d used)", done, length, length-done, m_buf->UsedSpace());

    while ((m_httpstate == NetHttpHeaders)&&(m_buf->HasLine() >= 0))
      {
      std::string header = m_buf->ReadLine();
      if (header.length() == 0)
        {
        HeadersAvailable();
        ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Header reception complete");
        m_httpstate = NetHttpBody;
        }
      else
        {
        // Process the header
        ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Headers got %s", header.c_str());
        if (header.compare(0,15,"Content-Length:") == 0)
          {
          m_bodysize = atoi(header.substr(15).c_str());
          ESP_LOGD(TAG, "OvmsNetHttpAsyncClient content-length is %d", m_bodysize);
          }
        if (header.compare(0,5,"HTTP/") == 0)
          {
          size_t space = header.find(' ');
          if (space!=std::string::npos)
            {
            m_responsecode = atoi(header.substr(space+1).c_str());
            ESP_LOGD(TAG, "OvmsNetHttpAsyncClient response-code is %d", m_responsecode);
            }
          }
        }
      }

    if ((m_httpstate == NetHttpHeaders)&&(m_buf->FreeSpace() == 0))
      {
      // We have overflowed the allowable line length
      ESP_LOGE(TAG, "OvmsNetHttpAsyncClient Header line too long");
      delete m_buf;
      m_buf = NULL;
      m_httpstate = NetHttpFailed;
      Disconnect();
      ConnectionFailed();
      return length;
      }

    if ((m_httpstate != NetHttpHeaders) && (m_buf->UsedSpace() > 0))
      {
      // We drained the headers
      BodyAvailable();
      }
    }

  return length;
  }

void OvmsNetHttpAsyncClient::HeadersAvailable()
  {
  ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Headers available");
  }

void OvmsNetHttpAsyncClient::BodyAvailable()
  {
  ESP_LOGD(TAG, "OvmsNetHttpAsyncClient Body data (%d bytes)",m_buf->UsedSpace());
  }
