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
static const char *TAG = "net";

#include "ovms_net.h"
#include <string.h>

OvmsNetConnection::OvmsNetConnection()
  {
  m_sock = -1;
  }

OvmsNetConnection::~OvmsNetConnection()
  {
  if (m_sock >= 0)
    {
    close(m_sock);
    m_sock = -1;
    }
  }

bool OvmsNetConnection::IsOpen()
  {
  return (m_sock>=0);
  }

void OvmsNetConnection::Disconnect()
  {
  if (m_sock >= 0)
    {
    close(m_sock);
    m_sock = -1;
    }
  }

int OvmsNetConnection::Socket()
  {
  return m_sock;
  }

ssize_t OvmsNetConnection::Write(const void *buf, size_t nbyte)
  {
  return write(m_sock, buf, nbyte);
  }

size_t OvmsNetConnection::Read(void *buf, size_t nbyte)
  {
  return read(m_sock, buf, nbyte);
  }


OvmsNetTcpConnection::OvmsNetTcpConnection()
  {
  }

OvmsNetTcpConnection::OvmsNetTcpConnection(const char* host, const char* service)
  {
  Connect(host, service);
  }

OvmsNetTcpConnection::~OvmsNetTcpConnection()
  {
  }

bool OvmsNetTcpConnection::Connect(const char* host, const char* service)
  {
  // Disconnect, if necessary
  if (m_sock >= 0)
    {
    close(m_sock);
    m_sock = -1;
    }

  // DNS lookup the host...
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res;
  int err = getaddrinfo(host, service, &hints, &res);
  if ((err != 0) || (res == NULL))
    {
    ESP_LOGW(TAG, "DNS lookup on %s failed err=%d",host,err);
    return false;
    }

  // struct in_addr *addr;
  // addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
  // ESP_LOGI(TAG, "Server %s is at %s",host,inet_ntoa(*addr));

  // Connect to the server...
  m_sock = socket(res->ai_family, res->ai_socktype, 0);
  if (m_sock < 0)
    {
    ESP_LOGW(TAG, "Error: Failed to allocate socket");
    freeaddrinfo(res);
    return false;
    }

  if (connect(m_sock, res->ai_addr, res->ai_addrlen) != 0)
    {
    ESP_LOGW(TAG, "Error: Failed to connect server %s (error: %d)",host,errno);
    close(m_sock);
    m_sock = -1;
    freeaddrinfo(res);
    return false;
    }

  freeaddrinfo(res);
  return true;
  }

