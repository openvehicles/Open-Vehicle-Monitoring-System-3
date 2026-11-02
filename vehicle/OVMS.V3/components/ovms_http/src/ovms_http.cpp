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
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

OvmsHttpClient::OvmsHttpClient(long timeout_ms /*= 10000*/)
  {
  m_buf = NULL;
  m_bodysize = 0;
  m_responsecode = 0;
  m_chunked = false;
  m_has_content_length = false;
  m_timeout_ms = timeout_ms;
  }

OvmsHttpClient::OvmsHttpClient(std::string url, const char* method, long timeout_ms /*= 10000*/, size_t range_start /*= 0*/)
  {
  m_buf = NULL;
  m_bodysize = 0;
  m_responsecode = 0;
  m_chunked = false;
  m_has_content_length = false;
  m_timeout_ms = timeout_ms;
  Request(url, method, range_start);
  }

OvmsHttpClient::~OvmsHttpClient()
  {
  if (m_buf)
    {
    delete m_buf;
    m_buf = NULL;
    }
  }

bool OvmsHttpClient::Request(std::string url, const char* method, size_t range_start /*= 0*/)
  {
  if (m_buf != NULL || IsOpen())
    {
    Disconnect();
    }
  m_bodysize = 0;
  m_responsecode = 0;
  m_chunked = false;
  m_has_content_length = false;

  // First, split URL into server and path components
  if (url.compare(0, 7, "http://", 7) == 0)
    {
    url = url.substr(7);
    }
  else if (url.compare(0, 8, "https://", 8) == 0)
      {
      url = url.substr(8);
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
  if (range_start > 0)
    {
    char range_hdr[48];
    snprintf(range_hdr, sizeof(range_hdr), "\r\nRange: bytes=%u-", (unsigned)range_start);
    req.append(range_hdr);
    }
  req.append("\r\n\r\n");
  if (Write(req.c_str(), req.length()) < 0)
    {
    ESP_LOGE(TAG, "Unable to write to server connection");
    Disconnect();
    return false;
    }

  m_buf = new OvmsBuffer(4096);
  assert(m_buf);
  bool inheaders = true;
  // ESP_LOGI(TAG,"Reading headers...");
  while((inheaders)&&(m_buf->PollSocket(m_sock, m_timeout_ms) > 0))
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
        if (strncasecmp(header.c_str(), "Content-Length:", 15) == 0)
          {
          m_bodysize = atoi(header.substr(15).c_str());
          m_has_content_length = true;
          }
        else if (strncasecmp(header.c_str(), "Transfer-Encoding:", 18) == 0)
          {
          std::string encoding = header.substr(18);
          for (std::string::iterator it = encoding.begin(); it != encoding.end(); ++it)
            *it = tolower((unsigned char)*it);
          m_chunked = encoding.find("chunked") != std::string::npos;
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

ssize_t OvmsHttpClient::BodyRead(void *buf, size_t nbyte)
  {
  if (m_buf == NULL)
    {
    m_buf = new OvmsBuffer(4096);
    assert(m_buf);
    }

  if (m_buf->UsedSpace() == 0)
    {
    // fill buffer using select() (non-blocking):
    ssize_t rlen = m_buf->PollSocket(m_sock, m_timeout_ms);
    if (rlen <= 0) return rlen; // EOF or error
    }

  // read from buffer:
  size_t n = m_buf->Pop(nbyte, (uint8_t*) buf);
  return (ssize_t) n;
  }

int OvmsHttpClient::BodyHasLine()
  {
  if (m_buf == NULL)
    return -1;

  if (m_buf->HasLine()<0)
    {
    m_buf->PollSocket(m_sock, m_timeout_ms);
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
    m_buf->PollSocket(m_sock, m_timeout_ms);
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

static bool http_read_line(OvmsHttpClient& http, std::string& line)
  {
  line.clear();
  char ch;
  while (line.size() < 1024)
    {
    ssize_t n = http.BodyRead(&ch, 1);
    if (n != 1)
      return false;
    if (ch == '\n')
      {
      if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
      return true;
      }
    line += ch;
    }
  return false;
  }

std::string OvmsHttpClient::GetBodyAsString()
  {
  std::string body;

  if (!IsOpen())
    {
    ESP_LOGE(TAG, "http request failed");
    return body;
    }

  if (m_chunked)
    {
    char rbuf[512];
    std::string line;
    bool failed = false;
    bool done = false;

    while (!done && !failed)
      {
      if (!http_read_line(*this, line))
        {
        failed = true;
        break;
        }

      if (line.empty())
        {
        failed = true;
        break;
        }

      size_t extension = line.find(';');
      if (extension != std::string::npos)
        line.erase(extension);

      if (line.empty())
        {
        failed = true;
        break;
        }
      for (std::string::const_iterator it = line.begin(); it != line.end(); ++it)
        {
        if (!isxdigit((unsigned char)*it))
          {
          failed = true;
          break;
          }
        }
      if (failed)
        break;

      errno = 0;
      char* end = NULL;
      unsigned long chunk_size = strtoul(line.c_str(), &end, 16);
      if (errno == ERANGE || end == line.c_str() || *end != '\0')
        {
        failed = true;
        break;
        }

      if (chunk_size == 0)
        {
        while (true)
          {
          if (!http_read_line(*this, line))
            {
            failed = true;
            break;
            }
          if (line.empty())
            {
            done = true;
            break;
            }
          }
        break;
        }

      size_t remaining = chunk_size;
      while (remaining > 0)
        {
        size_t request_size = (remaining < sizeof(rbuf)) ? remaining : sizeof(rbuf);
        ssize_t n = BodyRead(rbuf, request_size);
        if (n <= 0)
          {
          failed = true;
          break;
          }
        body.append(rbuf, (size_t)n);
        remaining -= n;
        }
      if (failed)
        break;

      char delimiter[2];
      size_t received = 0;
      while (received < sizeof(delimiter))
        {
        ssize_t n = BodyRead(delimiter + received, sizeof(delimiter) - received);
        if (n <= 0)
          {
          failed = true;
          break;
          }
        received += n;
        }
      if (failed || delimiter[0] != '\r' || delimiter[1] != '\n')
        {
        failed = true;
        break;
        }
      }

    if (failed || !done)
      {
      ESP_LOGE(TAG, "Invalid or incomplete chunked HTTP response body");
      body.clear();
      }
    else
      {
      m_bodysize = body.size();
      }
    Disconnect();
    return body;
    }

  size_t expected = BodySize();
  body.reserve(expected);

  uint8_t rbuf[512];
  size_t filesize = 0;
  ssize_t k;
  while ((k = BodyRead(rbuf,512)) > 0)
    {
    filesize += k;
    body.append((const char*)rbuf,k);
    }
  Disconnect();

  if (m_has_content_length && filesize != expected)
    {
    ESP_LOGE(TAG, "Download file size (%d) does not match expected (%d)", filesize, expected);
    body.clear();
    }
  else if (!m_has_content_length)
    {
    m_bodysize = filesize;
    }

  return body;
  }

void OvmsHttpClient::Reset()
  {
  if (m_buf)
    {
    delete m_buf;
    m_buf = NULL;
    }
  m_bodysize = 0;
  m_responsecode = 0;
  m_chunked = false;
  m_has_content_length = false;
  }
