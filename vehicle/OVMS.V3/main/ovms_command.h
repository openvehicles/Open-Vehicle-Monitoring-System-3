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
#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <unistd.h>
#include <string>
#include <map>
#include <set>
#include <limits.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define COMMAND_RESULT_MINIMAL    140
#define COMMAND_RESULT_NORMAL     1024
#define COMMAND_RESULT_VERBOSE    65535

class OvmsCommand;
class OvmsCommandMap;
class LogBuffers;
typedef std::map<TaskHandle_t, LogBuffers*> PartialLogs;

class OvmsWriter
  {
  public:
    OvmsWriter();
    virtual ~OvmsWriter();

  public:
    virtual int puts(const char* s) = 0;
    virtual int printf(const char* fmt, ...) = 0;
    virtual ssize_t write(const void *buf, size_t nbyte) = 0;
    virtual void finalise() = 0;
    virtual char ** GetCompletion(OvmsCommandMap& children, const char* token) = 0;
    virtual void Log(char* message) = 0;
    virtual void Log(LogBuffers* message) = 0;
    static void Exit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    virtual void DoExit();
  };

class OvmsCommandMap : public std::map<std::string, OvmsCommand*>
  {
  public:
    OvmsCommand* FindUniquePrefix(const std::string& key);
  };

class OvmsCommand
  {
  public:
    OvmsCommand();
    OvmsCommand(std::string name, std::string title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                const char *usage, int min, int max);
    virtual ~OvmsCommand();

  public:
    OvmsCommand* RegisterCommand(std::string name, std::string title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                 const char *usage = "", int min = 0, int max = INT_MAX);
    std::string GetName();
    std::string GetTitle();
    std::string GetUsage();
    char ** Complete(OvmsWriter* writer, int argc, const char * const * argv);
    void Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv);
    OvmsCommand* GetParent();

  private:
      size_t ExpandUsage(std::string usage, std::string& result);

  protected:
    std::string m_name;
    std::string m_title;
    void (*m_execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*);
    std::string m_usage;
    int m_min;
    int m_max;
    OvmsCommandMap m_children;
    OvmsCommand* m_parent;
  };

class OvmsCommandApp
  {
  public:
    OvmsCommandApp();
    virtual ~OvmsCommandApp();

  public:
    OvmsCommand* RegisterCommand(std::string name, std::string title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                 const char *usage = "", int min = 0, int max = INT_MAX);
    void RegisterConsole(OvmsWriter* writer);
    void DeregisterConsole(OvmsWriter* writer);
    int Log(const char* fmt, ...);
    int LogPartial(const char* fmt, ...);
    int HexDump(const char* prefix, const char* data, size_t length);

  public:
    char ** Complete(OvmsWriter* writer, int argc, const char * const * argv);
    void Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv);

  private:
    OvmsCommand m_root;
    typedef std::set<OvmsWriter*> ConsoleSet;
    ConsoleSet m_consoles;
    PartialLogs m_partials;
  };

extern OvmsCommandApp MyCommandApp;

#endif //#ifndef __COMMAND_H__
