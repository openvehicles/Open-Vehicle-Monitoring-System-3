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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "buffered_shell.h"
#include "log_buffers.h"

#define BUFFER_SIZE 64

BufferedShell::BufferedShell()
  {
  m_print = false;
  m_left = 0;
  m_buffer = NULL;
  m_output = NULL;
  Initialize(m_print);
  }

BufferedShell::BufferedShell(bool print, int verbosity, LogBuffers* output) :
  OvmsShell(verbosity)
  {
  m_print = print;
  m_left = 0;
  m_buffer = NULL;
  if (output)
    m_output = output;
  else
    {
    m_output = new LogBuffers;
    m_output->set(1);
    }
  Initialize(m_print);
  }

BufferedShell::~BufferedShell()
  {
  }

void BufferedShell::Initialize(bool print)
  {
  OvmsShell::Initialize(print);
  ProcessChar('\n');
  }

void BufferedShell::SetBuffer(LogBuffers* output /*=NULL*/)
  {
  if (output)
    {
    if (m_output)
      m_output->release();
    m_output = output;
    }
  else
    {
    if (m_output)
      {
      if (m_output->empty())
        return;
      m_output->release();
      }
    m_output = new LogBuffers;
    m_output->set(1);
    m_left = 0;
    m_buffer = NULL;
    }
  }

int BufferedShell::puts(const char* s)
  {
  write(s, strlen(s));
  write("\n", 1);
  return 0;
  }

int BufferedShell::printf(const char* fmt, ...)
  {
  if (!m_output)
    return 0;
  char *buffer = NULL;
  va_list args;
  va_start(args, fmt);
  int ret = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (ret >= 0)
    {
    write(buffer, ret);
    free(buffer);
    }
  return ret;
  }

ssize_t BufferedShell::write(const void *buf, size_t nbyte)
  {
  if (!m_output)
    return 0;
  size_t done = 0;
  while (nbyte > 0)
    {
    if (m_left == 0)
      {
      m_buffer = (char*)ExternalRamMalloc(BUFFER_SIZE);
      m_left = BUFFER_SIZE - 1;
      m_output->append(m_buffer);
      }
    size_t n = nbyte;
    if (n > m_left)
      n = m_left;
    memcpy(m_buffer + BUFFER_SIZE - 1 - m_left, (char*)buf + done, n);
    done += n;
    m_left -= n;
    *(m_buffer + BUFFER_SIZE - 1 - m_left) = '\0';
    nbyte -= n;
    }
  return done;
  }

void BufferedShell::Log(LogBuffers* message)
  {
  if (!m_output)
    {
    message->release();
    return;
    }
  if (message->last())
    {
    if (m_output->empty())
      {
      m_output->release();
      m_output = message;
      return;
      }
    LogBuffers::iterator before = m_output->begin(), after;
    while (true)
      {
      after = before;
      ++after;
      if (after == m_output->end())
        break;
      before = after;
      }
    m_output->splice_after(before, *message);
    m_left = 0;
    }
  else
    {
    for (LogBuffers::iterator i = message->begin(); i != message->end(); ++i)
      write(*i, strlen(*i));
    }
  message->release();
  return;
  }

// Deliver the buffered output to an OvmsWriter (typically a Console),
// This releases the LogBuffers object so it is freed.
void BufferedShell::Output(OvmsWriter* writer)
  {
  if (m_print)
    printf("\n");
  for (LogBuffers::iterator i = m_output->begin(); i != m_output->end(); ++i)
    writer->write(*i, strlen(*i));
  m_output->release();
  m_output = NULL;
  }

// Concatenate all of the buffered output into the provided string.
// This releases the LogBuffers object so it is freed.
void BufferedShell::Dump(std::string& buf)
  {
  if (!m_output)
    return;
  size_t total = 0;
  for (LogBuffers::iterator i = m_output->begin(); i != m_output->end(); ++i)
    total += strlen(*i);
  if (m_print)
    ++total;
  buf.reserve(total);
  for (LogBuffers::iterator i = m_output->begin(); i != m_output->end(); ++i)
    buf.append(*i);
  if (m_print)
    buf.append("\n");
  m_output->release();
  m_output = NULL;
  }

// Concatenate all of the buffered output into the provided string.
// This releases the LogBuffers object so it is freed.
void BufferedShell::Dump(extram::string& buf)
  {
  if (!m_output)
    return;
  size_t total = 0;
  for (LogBuffers::iterator i = m_output->begin(); i != m_output->end(); ++i)
    total += strlen(*i);
  if (m_print)
    ++total;
  buf.reserve(total);
  for (LogBuffers::iterator i = m_output->begin(); i != m_output->end(); ++i)
    buf.append(*i);
  if (m_print)
    buf.append("\n");
  m_output->release();
  m_output = NULL;
  }

// Concatenate all of the buffered output into one contiguous buffer allocated
// from the heap.  This releases the LogBuffers object so it is freed.
char* BufferedShell::Dump()
  {
  if (!m_output)
    return NULL;
  size_t total = 0;
  for (LogBuffers::iterator i = m_output->begin(); i != m_output->end(); ++i)
    total += strlen(*i);
  if (m_print)
    ++total;
  char* buf = (char*)ExternalRamMalloc(total + 1);
  char* p = buf;
  for (LogBuffers::iterator i = m_output->begin(); i != m_output->end(); ++i)
    p = stpcpy(p, *i);
  if (m_print)
    {
    *p++ = '\n';
    *p = '\0';
    }
  m_output->release();
  m_output = NULL;
  return buf;
  }
