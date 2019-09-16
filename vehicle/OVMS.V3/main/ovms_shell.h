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
#ifndef __OVMS_SHELL_H__
#define __OVMS_SHELL_H__

#include "ovms_command.h"
#include "microrl.h"

#define TOKEN_MAX_LENGTH 32
#define COMPLETION_MAX_TOKENS 20

class OvmsShell : public OvmsWriter
  {
  public:
    OvmsShell(int verbosity = COMMAND_RESULT_MINIMAL);

  public:
    void Initialize(bool print);
    void ProcessChar(char c);
    void ProcessChars(const char* buf, int len);
    void PrintConditional(const char* buf);
    virtual void SetSecure(bool secure=true);
    virtual void SetArgv(const char* const* argv) { m_argv = argv; return; }
    virtual const char* const* GetArgv() { return m_argv; }

  protected:
    virtual void finalise() {}

  protected:
    microrl_t m_rl;
    const char* const* m_argv;

  public:
    int m_verbosity;
  };

#endif //#ifndef __OVMS_SHELL_H__
