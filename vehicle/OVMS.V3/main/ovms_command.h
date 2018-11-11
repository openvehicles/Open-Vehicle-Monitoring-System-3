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
#include <stdarg.h>
#include <string>
#include <map>
#include <set>
#include <limits.h>
#include "ovms.h"
#include "ovms_mutex.h"
#include "task_base.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "microrl_config.h"

#define COMMAND_RESULT_MINIMAL    140
#define COMMAND_RESULT_SMS        160
#define COMMAND_RESULT_NORMAL     1024
#define COMMAND_RESULT_VERBOSE    65535

class OvmsWriter;
class OvmsCommand;
class OvmsCommandMap;
class LogBuffers;
typedef std::map<TaskHandle_t, LogBuffers*> PartialLogs;
typedef bool (*InsertCallback)(OvmsWriter* writer, void* userData, char);

class OvmsWriter
  {
  public:
    OvmsWriter();
    virtual ~OvmsWriter();

  public:
    virtual int puts(const char* s) = 0;
    virtual int printf(const char* fmt, ...) = 0;
    virtual ssize_t write(const void *buf, size_t nbyte) = 0;
    virtual char ** GetCompletion(OvmsCommandMap& children, const char* token) = 0;
    virtual void Log(LogBuffers* message) = 0;
    virtual void Exit();
    virtual bool IsInteractive() { return true; }
    void RegisterInsertCallback(InsertCallback cb, void* ctx);
    void DeregisterInsertCallback(InsertCallback cb);
    virtual void finalise() {}
    virtual void ProcessChar(char c) {}

  public:
    bool IsSecure();
    virtual void SetSecure(bool secure=true);
    bool IsMonitoring() { return m_monitoring; }
    void SetMonitoring(bool mon=true) { m_monitoring = mon; }

  public:
    bool m_issecure;

  protected:
    InsertCallback m_insert;
    void* m_userData;
    bool m_monitoring;
  };

struct CompareCharPtr
  {
  bool operator()(const char* a, const char* b);
  };

class OvmsCommandMap : public std::map<const char*, OvmsCommand*, CompareCharPtr>
  {
  public:
    OvmsCommand* FindUniquePrefix(const char* key);
  };

class OvmsCommand : public ExternalRamAllocated
  {
  public:
    OvmsCommand();
    OvmsCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                const char *usage, int min, int max, bool secure = false);
    virtual ~OvmsCommand();

  public:
    OvmsCommand* RegisterCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                 const char *usage = "", int min = 0, int max = INT_MAX, bool secure = false);
    bool UnregisterCommand(const char* name = NULL);
    const char* GetName();
    const char* GetTitle();
    const char* GetUsage(OvmsWriter* writer);
    char ** Complete(OvmsWriter* writer, int argc, const char * const * argv);
    void Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv);
    OvmsCommand* GetParent();
    OvmsCommand* FindCommand(const char* name);
    bool IsSecure() { return m_secure; }

  private:
    void ExpandUsage(std::string usage, OvmsWriter* writer);

  protected:
    const char* m_name;
    const char* m_title;
    void (*m_execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*);
    const char* m_usage_template;
    std::string m_usage;
    int m_min;
    int m_max;
    bool m_secure;
    OvmsCommandMap m_children;
    OvmsCommand* m_parent;
  };


typedef enum
  {
  OCS_Init,
  OCS_Error,
  OCS_RunOnce,
  OCS_RunLoop,
  OCS_StopRequested,
  } OvmsCommandState_t;

class OvmsCommandTask : public TaskBase
  {
  public:
    OvmsCommandTask(int _verbosity, OvmsWriter* _writer, OvmsCommand* _cmd, int _argc, const char* const* _argv);
    virtual ~OvmsCommandTask();
    virtual OvmsCommandState_t Prepare();
    bool Run();
    static bool Terminator(OvmsWriter* writer, void* userdata, char ch);
    bool IsRunning() { return m_state == OCS_RunLoop; }
    bool IsTerminated() { return m_state == OCS_StopRequested; }

  protected:
    int verbosity;
    OvmsWriter* writer;
    OvmsCommand* cmd;
    int argc;
    char** argv;
    OvmsCommandState_t m_state;
  };

class OvmsCommandApp
  {
  public:
    OvmsCommandApp();
    virtual ~OvmsCommandApp();

  public:
    OvmsCommand* RegisterCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                 const char *usage = "", int min = 0, int max = INT_MAX, bool secure = false);
    bool UnregisterCommand(const char* name);
    OvmsCommand* FindCommand(const char* name);
    void RegisterConsole(OvmsWriter* writer);
    void DeregisterConsole(OvmsWriter* writer);
    int Log(const char* fmt, ...);
    int Log(const char* fmt, va_list args);
    int LogPartial(const char* fmt, ...);
    int HexDump(const char* tag, const char* prefix, const char* data, size_t length, size_t colsize=16);

  public:
    char ** Complete(OvmsWriter* writer, int argc, const char * const * argv);
    void Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv);

  public:
    void ConfigureLogging();
    bool SetLogfile(std::string path);
    void SetLoglevel(std::string tag, std::string level);
    void ExpireLogFiles(int verbosity, OvmsWriter* writer, int keepdays);
    static void ExpireTask(void* data);
    void EventHandler(std::string event, void* data);

  private:
    void CycleLogfile();
    void ReadConfig();

  private:
    int LogBuffer(LogBuffers* lb, const char* fmt, va_list args);
    OvmsMutex m_fsync_mutex;

  private:
    OvmsCommand m_root;
    typedef std::set<OvmsWriter*> ConsoleSet;
    ConsoleSet m_consoles;
    PartialLogs m_partials;
    FILE* m_logfile;
    std::string m_logfile_path;
    size_t m_logfile_size;
    size_t m_logfile_maxsize;

  public:
    TaskHandle_t m_expiretask;
  };

extern OvmsCommandApp MyCommandApp;

#endif //#ifndef __COMMAND_H__
