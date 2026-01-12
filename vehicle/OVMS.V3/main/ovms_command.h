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
#include <list>
#include <functional>
#include <limits.h>
#include "ovms.h"
#include "ovms_utils.h"
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
typedef bool (*InsertCallback)(OvmsWriter* writer, void* userData, char);

class OvmsWriter
  {
  public:
    OvmsWriter();
    virtual ~OvmsWriter();

  public:
    virtual int puts(const char* s) { return 0; }
    virtual int printf(const char* fmt, ...) __attribute__ ((format (printf, 2, 3))) { return 0; }
    virtual ssize_t write(const void *buf, size_t nbyte) { return 0; }
    virtual char** SetCompletion(int index, const char* token, bool isfinal = true) { return NULL; }
    virtual char** GetCompletions(int &common_len, bool &finished ) { return NULL; }
    virtual void SetArgv(const char* const* argv) { return; }
    virtual const char* const* GetArgv() { return NULL; }
    virtual void Log(LogBuffers* message) {};
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

template <typename T>
class NameMap : public std::map<std::string, T>
  {
  public:
    const T* FindUniquePrefix(const char* token) const
      {
      size_t len = strlen(token);
      const T* found = NULL;
      for (typename NameMap<T>::const_iterator it = NameMap<T>::begin(); it != NameMap<T>::end(); ++it)
        {
        if (it->first.compare(0, len, token) == 0)
          {
          if (len == it->first.length())
            return &it->second;
          if (found)
            return NULL;
          else
            found = &it->second;
          }
        }
      return found;
      }

    bool GetCompletion(OvmsWriter* writer, const char* token) const
      {
      unsigned int index = 0;
      bool match = false;
      writer->SetCompletion(index, NULL);
      if (token)
        {
        size_t len = strlen(token);
        for (typename NameMap<T>::const_iterator it = NameMap<T>::begin(); it != NameMap<T>::end(); ++it)
          {
          if (it->first.compare(0, len, token) == 0)
            {
            writer->SetCompletion(index++, it->first.c_str());
            match = true;
            }
          }
        }
      return match;
      }

    int Validate(OvmsWriter* writer, int argc, const char* token, bool complete) const
      {
      if (complete)
        {
        if (!GetCompletion(writer, token))
          return -1;
        }
      else
        {
        if (FindUniquePrefix(token) == NULL)
          {
          if (strcmp(token, "?") != 0)
            writer->printf("Error: %s is not defined\n", token);
          return -1;
          }
        }
      return argc;
      }
  };

template <typename T>
class CNameMap : public std::map<const char*, T, CmpStrOp>
  {
  public:
    const T* FindUniquePrefix(const char* token) const
      {
      size_t len = strlen(token);
      const T* found = NULL;
      for (typename CNameMap<T>::const_iterator it = CNameMap<T>::begin(); it != CNameMap<T>::end(); ++it)
        {
        if (strncmp(it->first, token, len) == 0)
          {
          if (len == strlen(it->first))
            return &it->second;
          if (found)
            return NULL;
          else
            found = &it->second;
          }
        }
      return found;
      }

    bool GetCompletion(OvmsWriter* writer, const char* token) const
      {
      unsigned int index = 0;
      bool match = false;
      writer->SetCompletion(index, NULL);
      if (token)
        {
        size_t len = strlen(token);
        for (typename CNameMap<T>::const_iterator it = CNameMap<T>::begin(); it != CNameMap<T>::end(); ++it)
          {
          if (strncmp(it->first, token, len) == 0)
            {
            writer->SetCompletion(index++, it->first);
            match = true;
            }
          }
        }
      return match;
      }

    int Validate(OvmsWriter* writer, int argc, const char* token, bool complete) const
      {
      if (complete)
        {
        if (!GetCompletion(writer, token))
          return -1;
        }
      else
        {
        if (FindUniquePrefix(token) == NULL)
          {
          if (strcmp(token, "?") != 0)
            writer->printf("Error: %s is not defined\n", token);
          return -1;
          }
        }
      return argc;
      }
  };

struct CompareCharPtr
  {
  bool operator()(const char* a, const char* b) const;
  bool operator()(const std::string& a, const char* b) const;
  bool operator()(const std::string& a, const std::string& b) const;
  };

class OvmsCommandMap : public std::map<std::string, OvmsCommand*, CompareCharPtr>
  {
  public:
    OvmsCommand* FindUniquePrefix(const char* key);
    OvmsCommand* FindCommand(const char* key);
    char** GetCompletion(OvmsWriter* writer, const char* token);
  };
enum class OvmsCommandType : short {
  System,
  SystemAllowUserCmd,
  SystemAllowUsrDir,
  SystemUsrDir,
  User
};

typedef std::function<void(int, OvmsWriter*, OvmsCommand*, int, const char* const*)> OvmsCommandExecuteCallback_t;
typedef std::function<int(OvmsWriter*, OvmsCommand*, int, const char* const*, bool)> OvmsCommandValidateCallback_t;

// Compiler utilities: pick the matching method overload in case of ambiguity:
#define PickOvmsCommandExecuteCallback(method) static_cast<void(*)(int, OvmsWriter*, OvmsCommand*, int, const char* const*)>(method)
#define PickOvmsCommandValidateCallback(method) static_cast<int(*)(OvmsWriter*, OvmsCommand*, int, const char* const*, bool)>(method)

class OvmsCommand : public ExternalRamAllocated
  {
  public:
    OvmsCommand(OvmsCommandType type = OvmsCommandType::System);
    OvmsCommand(const char* name, const char* title,
                OvmsCommandExecuteCallback_t execute,
                const char *usage, int min, int max, bool secure,
                OvmsCommandValidateCallback_t validate,
                OvmsCommandType type);
    virtual ~OvmsCommand();

  public:
    OvmsCommand* RegisterCommand(const char* name, const char* title,
                                 OvmsCommandExecuteCallback_t execute = NULL,
                                 const char *usage = "", int min = 0, int max = 0, bool secure = true,
                                 OvmsCommandValidateCallback_t validate = NULL,
                                 OvmsCommandType type = OvmsCommandType::System);
    bool UnregisterCommand(const char* name = NULL);
    const char* GetName();
    const char* GetTitle();
    const char* GetUsage(OvmsWriter* writer);

    std::string GetFullName();
    OvmsCommandType GetType()
    { return m_type; }

    char ** Complete(OvmsWriter* writer, int argc, const char * const * argv, int &common_len, bool &finished);
    void Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv);
    OvmsCommand* GetParent();
    OvmsCommand* FindCommand(const char* name);
    bool IsSecure() { return m_secure; }
    void Display(OvmsWriter* writer, int level);
    void PutUsage(OvmsWriter* writer);

    void UpdateCommand(const char *title,
         const char *usage = "", int min = 0, int max = 0, bool secure = true);

  private:
    void ExpandUsage(const char* templ, OvmsWriter* writer, std::string& result);
  protected:
    std::string m_name;
    std::string m_title;
    OvmsCommandExecuteCallback_t m_execute;
    OvmsCommandValidateCallback_t m_validate;
    std::string m_usage_template;
    int m_min;
    int m_max;
    bool m_secure;
    OvmsCommandMap m_children;
    OvmsCommand* m_parent;
    OvmsCommandType m_type;
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

class OvmsCommandApp : public OvmsWriter
  {
  public:
    OvmsCommandApp();
    virtual ~OvmsCommandApp();

  public:
    OvmsCommand* RegisterCommand(const char* name, const char* title,
                                 OvmsCommandExecuteCallback_t execute = NULL,
                                 const char *usage = "", int min = 0, int max = 0, bool secure = true,
                                 OvmsCommandValidateCallback_t validate = NULL,
                                 OvmsCommandType type = OvmsCommandType::System);
    bool UnregisterCommand(const char* name);
    OvmsCommand* FindCommand(const char* name);
    OvmsCommand* FindCommandFullName(const char* name, bool allow_create_user= false);
    OvmsCommand* CheckCreateUsr(const char* name, OvmsCommand *command);
    void RegisterConsole(OvmsWriter* writer);
    void DeregisterConsole(OvmsWriter* writer);
    int Log(const char* fmt, ...) __attribute__ ((format (printf, 2, 3)));
    int Log(const char* fmt, va_list args) __attribute__ ((format (printf, 2, 0)));
    int HexDump(const char* tag, const char* prefix, const char* data, size_t length, size_t colsize=16);
    void Display(OvmsWriter* writer);

  public:
    char ** Complete(OvmsWriter* writer, int argc, const char * const * argv, int &common_len, bool &finished);
    void Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv);

  public:
    void ConfigureLogging();
    void LogTask();
    bool StartLogTask(FILE* file);
    bool StopLogTask();
    bool OpenLogfile();
    bool CloseLogfile();
    bool SetLogfile(std::string path);
    std::string GetLogfile() { return m_logfile_path; }
    void SetLoglevel(std::string tag, std::string level);
    void ExpireLogFiles(int verbosity, OvmsWriter* writer, int keepdays);
    void ShowLogStatus(int verbosity, OvmsWriter* writer);
    static void ExpireTask(void* data);
    void EventHandler(std::string event, void* data);

  private:
    bool CycleLogfile();
    void ReadConfig();

    OvmsCommand* CheckCreateUsr(OvmsCommand *, bool allow_create_user);
  private:
    int LogBuffer(LogBuffers* lb, const char* fmt, va_list args) __attribute__ ((format (printf, 3, 0)));

  public:
    void Log(LogBuffers* message);

  private:
    OvmsCommand m_root;

    typedef std::set<OvmsWriter*> ConsoleSet;
    ConsoleSet m_consoles;                          // set of registered consoles (= log receivers)
    OvmsMutex m_consoles_mutex;                     // m_consoles access synchronization

    FILE* m_logfile;
    std::string m_logfile_path;
    size_t m_logfile_size;
    size_t m_logfile_maxsize;
    int m_logfile_syncperiod;
    TaskHandle_t m_logtask;
    OvmsMutex m_logtask_mutex;
    QueueHandle_t m_logtask_queue;
    uint32_t m_logtask_dropcnt;
    uint32_t m_logfile_cyclecnt;
    uint32_t m_logtask_linecnt;
    uint32_t m_logtask_fsynctime;
    time_t m_logtask_laststamp;
    struct timeval m_logtask_basetime;

  public:
    TaskHandle_t m_expiretask;
  };

// Utility class to help with completion where there are '-' options
class option_completer_t
  {
  private:
    OvmsWriter *m_writer;
    bool m_complete;
    bool m_valid;
    int m_complete_idx;
    bool m_found_param;
    int m_final_posn;
    int m_argc;
    const char * const* m_argv;

  public:
    option_completer_t(OvmsWriter *writer, bool complete, int argc, const char * const* argv)
    : m_writer(writer),
      m_complete(complete),
      m_valid(false),
      m_complete_idx(0),
      m_found_param(false),
      m_argc(argc),
      m_argv(argv)
      {
      }
    /** Check if the param is there / needs completion.
     */
    bool check_param(char param, bool has_more = false);

    // Return the (0 based) current completion parameter # (without '-' options).
    int param_index();

    bool do_return()
      {
      if (m_complete_idx > 0)
        return true;
      return m_complete && (m_valid || m_found_param);
      }
    int return_val()
      {
      return m_valid ? m_argc : -1;
      }
  };

extern OvmsCommandApp MyCommandApp;

#endif //#ifndef __COMMAND_H__
