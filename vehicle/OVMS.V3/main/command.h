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

#include <string>
#include <map>

#ifndef __COMMAND_H__
#define __COMMAND_H__

#define COMMAND_RESULT_MINIMAL    140
#define COMMAND_RESULT_NORMAL     1024
#define COMMAND_RESULT_VERBOSE    65535

class OvmsWriter
  {
  public:
    OvmsWriter();
    ~OvmsWriter();

  public:
    virtual int puts(const char* s);
    virtual int printf(const char* fmt, ...);
    virtual ssize_t write(const void *buf, size_t nbyte);
    virtual void finalise();
  };

class OvmsCommand
  {
  public:
    OvmsCommand();
    OvmsCommand(std::string title, void (*execute)(int, OvmsWriter*, int, const char* const*));
    virtual ~OvmsCommand();

  public:
    OvmsCommand* RegisterCommand(std::string name, std::string title, void (*execute)(int, OvmsWriter*, int, const char* const*));
    std::string GetTitle();
    void Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv);

  protected:
    std::string m_title;
    void (*m_execute)(int, OvmsWriter*, int, const char* const*);
    std::map<std::string, OvmsCommand*> m_children;
  };

class OvmsCommandApp
  {
  public:
    OvmsCommandApp();
    virtual ~OvmsCommandApp();

  public:
    OvmsCommand* RegisterCommand(std::string name, std::string title, void (*execute)(int, OvmsWriter*, int, const char* const*));

  public:
    void Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv);

  private:
    OvmsCommand m_root;
  };

extern OvmsCommandApp MyCommandApp;

#endif //#ifndef __COMMAND_H__
