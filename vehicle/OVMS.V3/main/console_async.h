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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>
#include <map>
#include "microrl.h"
#include "command.h"

#ifndef __CONSOLE_ASYNC_H__
#define __CONSOLE_ASYNC_H__

class ConsoleAsync : public OvmsWriter
  {
  public:
    ConsoleAsync();
    virtual ~ConsoleAsync();

    microrl_t* GetRL();

  public:
    int puts(const char* s);
    int printf(const char* fmt, ...);

    ssize_t write(const void *buf, size_t nbyte);
    void finalise();
    char ** GetCompletion(OvmsCommandMap& children, const char* token);

  protected:
    microrl_t m_rl;
    TaskHandle_t m_taskid;
    char *m_completions[COMPLETION_MAX_TOKENS+2];
    char m_space[COMPLETION_MAX_TOKENS+2][TOKEN_MAX_LENGTH];
  };

#endif //#ifndef __CONSOLE_ASYNC_H__
