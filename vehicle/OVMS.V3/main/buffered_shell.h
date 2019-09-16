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
#ifndef __BUFFERED_SHELL_H__
#define __BUFFERED_SHELL_H__

#include "ovms.h"
#include "ovms_shell.h"

class LogBuffers;
class OvmsCommandMap;

class BufferedShell : public OvmsShell
  {
  public:
    BufferedShell();
    BufferedShell(bool print, int verbosity, LogBuffers* output = NULL);
    ~BufferedShell();

  public:
    void Initialize(bool print);
    void SetBuffer(LogBuffers* output = NULL);
    int puts(const char* s);
    int printf(const char* fmt, ...);
    ssize_t write(const void *buf, size_t nbyte);
    void Log(LogBuffers* message);
    virtual bool IsInteractive() { return false; }
    void Output(OvmsWriter*);
    void Dump(std::string&);
    void Dump(extram::string&);
    char* Dump();

  protected:
    bool m_print;
    size_t m_left;
    char* m_buffer;
    LogBuffers* m_output;
  };

#endif //#ifndef __BUFFERED_SHELL_H__
