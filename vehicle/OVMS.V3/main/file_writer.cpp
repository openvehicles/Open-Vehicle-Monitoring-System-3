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
;    (C) 2018       Michael Balzer
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
static const char *TAG = "file-writer";

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "file_writer.h"

FileWriter::FileWriter(std::string path)
  {
  m_path = path;
  }

FileWriter::~FileWriter()
  {
  }

int FileWriter::puts(const char* s)
  {
  FILE* file = OpenAppendFile();
  if (file != NULL)
    {
    fputs(s, file);
    fputc('\n', file);
    CloseAppendFile(file);
    }
  return 0;
  }

int FileWriter::printf(const char* fmt, ...)
  {
  FILE *file = OpenAppendFile();
  char *buffer = NULL;
  va_list args;
  va_start(args, fmt);
  int ret = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (ret >= 0)
    {
    if (file != NULL)
      {
      fwrite(buffer, ret, 1, file);
      CloseAppendFile(file);
      }
    }
  return ret;
  }

ssize_t FileWriter::write(const void *buf, size_t nbyte)
  {
  //append((const char*)buf, nbyte);
  FILE* file = OpenAppendFile();
  if (file != NULL)
    {
    fwrite(buf, nbyte, 1, file);
    CloseAppendFile(file);
    }
  return nbyte;
  }

void FileWriter::NotifyVfsMigration(const std::string& src, const std::string& dst)
  {
  if (m_path == src)
    {
    ESP_LOGI(TAG, "Migrating to write to %s", dst.c_str());
    m_path = dst;
    }
  }

FILE* FileWriter::OpenAppendFile()
  {
  return fopen(m_path.c_str(), "a");
  }

void FileWriter::CloseAppendFile(FILE* file)
  {
  if (file != NULL) fclose(file);
  }
