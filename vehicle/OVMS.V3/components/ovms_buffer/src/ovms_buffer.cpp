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
static const char *TAG = "buffer";

#include "ovms_buffer.h"
#include "ovms_command.h"
#include <string.h>		// Needed for memset by LWIP's sys/socket.h
#include <sys/socket.h>

OvmsBuffer::OvmsBuffer(size_t size, void* userdata)
  {
  m_buffer = new uint8_t[size];
  m_head = 0;
  m_tail = 0;
  m_size = size;
  m_used = 0;
  m_userdata = userdata;
  }

OvmsBuffer::~OvmsBuffer()
  {
  delete [] m_buffer;
  }

size_t OvmsBuffer::Size()
  {
  return m_size;
  }

size_t OvmsBuffer::FreeSpace()
  {
  return m_size - m_used;
  }

size_t OvmsBuffer::UsedSpace()
  {
  return m_used;
  }

void OvmsBuffer::EmptyAll()
  {
  m_head = 0;
  m_tail = 0;
  m_used = 0;
  }

bool OvmsBuffer::Push(uint8_t byte)
  {
  if (m_used == m_size) return false;

  m_used++;
  m_buffer[m_head++] = byte;
  if (m_head >= m_size) m_head=0;

  return true;
  }

bool OvmsBuffer::Push(uint8_t *byte, size_t count)
  {
  if ((m_size-m_used)<count) return false;

  m_used += count;
  for (size_t k=0;k<count;k++)
    {
    // ESP_LOGI(TAG, "Push() %02x at %d",byte[k],m_head);
    m_buffer[m_head++] = byte[k];
    if (m_head >= m_size) m_head=0;
    }

  return true;
  }

uint8_t OvmsBuffer::Pop()
  {
  if (m_used==0) return 0;

  m_used--;
  uint8_t result = m_buffer[m_tail++];
  if (m_tail >= m_size) m_tail=0;

  return result;
  }

size_t OvmsBuffer::Pop(size_t count, uint8_t *dest)
  {
  size_t done = 0;

  while ((m_used>0)&&(done < count))
    {
    m_used--;
    dest[done++] = m_buffer[m_tail++];
    if (m_tail >= m_size) m_tail=0;
    }

  return done;
  }

uint8_t OvmsBuffer::Peek()
  {
  if (m_used==0) return 0;

  return m_buffer[m_tail];
  }

size_t OvmsBuffer::Peek(size_t count, uint8_t *dest)
  {
  size_t done = 0;

  size_t tail = m_tail;
  while ((done<m_used)&&(done<count))
    {
    dest[done++] = m_buffer[tail++];
    if (tail >= m_size) tail=0;
    }

  return done;
  }

void OvmsBuffer::Diagnostics()
  {
  size_t hl = HasLine();
  ESP_LOGI(TAG, "OvmsBuffer has %d/%d bytes (head %d, tail %d), hasline %d",
    m_used,m_size,m_head,m_tail,hl);
  }

int OvmsBuffer::HasLine()
  {
  size_t tail = m_tail;

  if (m_used==0) return -1;

  // ESP_LOGI(TAG, "HasLine() with tail at %d (%d used)",m_tail,m_used);
  for (size_t done=0;done<m_used;done++)
    {
    // ESP_LOGI(TAG, "HasLine() at tail %d is %02x (%d done)",tail,m_buffer[tail],done);
    if ((m_buffer[tail]=='\r')||(m_buffer[tail]=='\n'))
      {
      // ESP_LOGI(TAG, "HasLine() found a CR/LF at tail %d (done=%d)",tail,done);
      return done;
      }
    tail++;
    if (tail >= m_size) tail=0;
    }

  // ESP_LOGI(TAG, "HasLine() didnt find a CR/LF");
  return -1;
  }

std::string OvmsBuffer::ReadLine()
  {
  int hl = HasLine();
  if (hl<0) return std::string("");

  uint8_t result[hl+1];
  Pop(hl, result);

  if (Peek() == '\r') Pop();
  if (Peek() == '\n') Pop();

  return std::string((char*)result,hl);
  }

int OvmsBuffer::PollSocket(int sock, long timeoutms)
  {
  fd_set fds;

  if (sock < 0) return -1;

  // ESP_LOGI(TAG, "Polling Socket %d", sock);
  FD_ZERO(&fds);
  FD_SET(sock,&fds);
  // MyCommandApp.HexDump(TAG, "FD SET", (const char*)&fds,sizeof(fds));

  struct timeval timeout;
  timeout.tv_sec = timeoutms/1000;
  timeout.tv_usec = (timeoutms%1000)*1000;
  int result = select((int) sock + 1, &fds, 0, 0, &timeout);
  // ESP_LOGI(TAG, "Polling Socket select result %d",result);
  if (result <= 0) return -1;

  // We have some data ready to read
  size_t avail = FreeSpace();
  if (avail==0) return 0;
  // ESP_LOGI(TAG,"Polling Socket allocating %d bytes",avail);
  uint8_t *buf = new uint8_t[avail];
  size_t n = read(sock, buf, avail);
  // ESP_LOGI(TAG,"Polling Socket read %d bytes",n);
  // MyCommandApp.HexDump(TAG, "PollSocket", (const char*)buf, n);
  if (n == 0)
    {
    n = -1;
    }
  else if (n > 0)
    {
    Push(buf,n);
    }
  delete [] buf;
  return n;
  }

