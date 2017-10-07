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

#include <string.h>
#include "ovms_shell.h"


void Print(microrl_t* rl, const char * str)
  {
  ((OvmsWriter*)rl->userdata)->write(str,strlen(str));
  }

void NoPrint(microrl_t* rl, const char * str)
  {
  }

int Execute (microrl_t* rl, int argc, const char * const * argv )
  {
  MyCommandApp.Execute(COMMAND_RESULT_VERBOSE, (OvmsWriter*)rl->userdata, argc, argv);
  return 0;
  }

void OvmsShell::Initialize(bool print)
  {
  microrl_init (&m_rl, print ? Print : NoPrint);
  m_rl.userdata = (void*)this;
  microrl_set_execute_callback(&m_rl, Execute);
  }

void OvmsShell::ProcessChar(const char c)
  {
  microrl_insert_char(&m_rl, c);
  }

void OvmsShell::ProcessChars(const char* buf, int len)
  {
  for (int i = 0; i < len; ++i)
    {
    microrl_insert_char(&m_rl, *buf++);
    }
  }
