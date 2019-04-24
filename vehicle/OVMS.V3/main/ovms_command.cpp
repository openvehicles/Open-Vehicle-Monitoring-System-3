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

#include "ovms_log.h"
static const char *TAG = "command";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <functional>
#include <esp_log.h>
#include <sys/types.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_peripherals.h"
#include "ovms_utils.h"
#include "ovms_script.h"
#include "buffered_shell.h"
#include "log_buffers.h"

OvmsCommandApp MyCommandApp __attribute__ ((init_priority (1000)));

bool CompareCharPtr::operator()(const char* a, const char* b)
  {
  return strcmp(a, b) < 0;
  }

OvmsWriter::OvmsWriter()
  {
  std::string p = MyConfig.GetParamValue("password","module");
  if (p.empty())
    m_issecure = true;
  else
    m_issecure = false;
  m_insert = NULL;
  m_userData = NULL;
  m_monitoring = false;
  }

OvmsWriter::~OvmsWriter()
  {
  }

void OvmsWriter::Exit()
  {
  puts("This console cannot exit.");
  }

void OvmsWriter::RegisterInsertCallback(InsertCallback cb, void* ctx)
  {
  m_insert = cb;
  m_userData = ctx;
  }

void OvmsWriter::DeregisterInsertCallback(InsertCallback cb)
  {
  if (m_insert == cb)
    {
    m_insert = NULL;
    m_userData = NULL;
    finalise();
    ProcessChar('\n');
    }
  }

bool OvmsWriter::IsSecure()
  {
  return m_issecure;
  }

void OvmsWriter::SetSecure(bool secure)
  {
  m_issecure = secure;
  }

const std::string NameStringMap::m_null = std::string();

const std::string& NameStringMap::FindUniquePrefix(const char* token) const
  {
  size_t len = strlen(token);
  const std::string* found = &m_null;
  for (const_iterator it = begin(); it != end(); ++it)
    {
    if (it->first.compare(0, len, token) == 0)
      {
      if (len == it->first.length())
	return it->second;
      if (found == &m_null)
	return m_null;
      else
	found = &it->second;
      }
    }
  return *found;
  }

bool NameStringMap::GetCompletion(OvmsWriter* writer, const char* token) const
  {
  unsigned int index = 0;
  bool match = false;
  writer->SetCompletion(index, NULL);
  if (token)
    {
    size_t len = strlen(token);
    for (const_iterator it = begin(); it != end(); ++it)
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

int NameStringMap::Validate(OvmsWriter* writer, int argc, const char* token, bool complete) const
  {
  if (complete)
    {
    if (!GetCompletion(writer, token))
      return -1;
    }
  else
    {
    if (FindUniquePrefix(token).empty())
      {
      writer->printf("Error: %s is not defined\n", token);
      return -1;
      }
    }
  return argc;
  }

OvmsCommand* OvmsCommandMap::FindUniquePrefix(const char* key)
  {
  int len = strlen(key);
  OvmsCommand* found = NULL;
  for (iterator it = begin(); it != end(); ++it)
    {
    if (strncmp(it->first, key, len) == 0)
      {
      if (len == strlen(it->first))
        {
        return it->second;
        }
      if (found)
        {
        return NULL;
        }
      else
        {
        found = it->second;
        }
      }
    }
  return found;
  }

OvmsCommand* OvmsCommandMap::FindCommand(const char* key)
  {
  iterator it = find(key);
  if (it == end())
    return NULL;
  else
    return it->second;
  }

char ** OvmsCommandMap::GetCompletion(OvmsWriter* writer, const char* token)
  {
  unsigned int index = 0;
  char** tokens = writer->SetCompletion(index, NULL);
  if (token)
    {
    for (iterator it = begin(); it != end(); ++it)
      {
      if (it->second->IsSecure() && !writer->IsSecure())
        continue;
      if (strncmp(it->first, token, strlen(token)) == 0)
        writer->SetCompletion(index++, it->first);
      }
    }
  return tokens;
  }

OvmsCommand::OvmsCommand()
  {
  m_parent = NULL;
  }

OvmsCommand::OvmsCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                         const char *usage, int min, int max, bool secure,
                         int (*validate)(OvmsWriter*, OvmsCommand*, int, const char* const*, bool))
  {
  m_name = name;
  m_title = title;
  m_execute = execute;
  m_usage_template= usage;
  m_min = min;
  m_max = max;
  m_parent = NULL;
  m_secure = secure;
  m_validate = validate;
  }

OvmsCommand::~OvmsCommand()
  {
  for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
    OvmsCommand* cmd = it->second;
    delete cmd;
    }
  m_children.clear();
  }

const char* OvmsCommand::GetName()
  {
  return m_name;
  }

const char* OvmsCommand::GetTitle()
  {
  return m_title;
  }

// Dynamic generation of "Usage:" messages.  Syntax of the usage template string:
// - Prefix is "Usage: " followed by names from ancestors (if any) and name of self
// - "$C" expands to children as child1|child2|child3
// - "[$C]" expands to optional children as [child1|child2|child3]
// - $G$ expands to the usage of the first child (typically used after $C)
// - $Gfoo$ expands to the usage of the child named "foo"
// - $L lists a full usage line for each of the children
// - Parameters after command and subcommand tokens may be explicit like " <metric>"
// - Empty usage template "" defaults to "$C" for non-terminal OvmsCommand
void OvmsCommand::PutUsage(OvmsWriter* writer)
  {
  const char* usage = !m_usage_template ? "" : (!*m_usage_template && !m_execute) ? "$C" : m_usage_template;
  std::string result ="Usage: ";
  size_t pos = result.size();
  for (OvmsCommand* parent = m_parent; parent && parent->m_parent; parent = parent->m_parent)
    {
    if (parent->m_validate)
      {
      size_t len = strlen(parent->m_usage_template);
      char* dollar = index(parent->m_usage_template, '$');
      if (dollar)
        {
        len = dollar - parent->m_usage_template;
        if (len > 0 && *(dollar-1) == '[')
          --len;
        }
      else
        result.insert(pos, " ");
      result.insert(pos, parent->m_usage_template, len);
      }
    result.insert(pos, " ");
    result.insert(pos, parent->m_name);
    }
  result += m_name;
  result += " ";
  ExpandUsage(usage, writer, result);
  writer->puts(result.c_str());
  }

void OvmsCommand::ExpandUsage(const char* templ, OvmsWriter* writer, std::string& result)
  {
  std::string usage = templ;
  size_t pos;
  if ((pos = usage.find("$L")) != std::string::npos)
    {
    result += usage.substr(0, pos);
    pos += 2;
    size_t z = result.size();
    bool found = false;
    for (OvmsCommandMap::iterator it = m_children.begin(); it != m_children.end(); ++it)
      {
      OvmsCommand* child = it->second;
      if (!child->m_secure || writer->m_issecure)
        {
        if (found)
          {
          result += "\n";
          result += result.substr(0, z);
          }
        result += it->first;
        result += " ";
        found = true;
        child->ExpandUsage(child->m_usage_template, writer, result);
        }
      }
    if (result.size() == z)
      {
      result = "All subcommands require 'enable' mode";
      pos = usage.size();       // Don't append any more
      }
    result += usage.substr(pos);
    return;
    }

  if ((pos = usage.find("$C")) != std::string::npos)
    {
    result += usage.substr(0, pos);
    pos += 2;
    size_t z = result.size();
    bool found = false;
    for (OvmsCommandMap::iterator it = m_children.begin(); it != m_children.end(); ++it)
      {
      if (!it->second->m_secure || writer->m_issecure)
        {
        if (found)
          result += "|";
        result += it->first;
        found = true;
        }
      }
    if (result.size() == z)
      {
      result = "All subcommands require 'enable' mode";
      pos = usage.size();       // Don't append any more
      }
    }
  else pos = 0;
  size_t pos2;
  if ((pos2 = usage.find("$G", pos)) != std::string::npos)
    {
    result += usage.substr(pos, pos2-pos);
    pos2 += 2;
    size_t pos3;
    OvmsCommandMap::iterator it = m_children.end();
    if ((pos3 = usage.find('$', pos2)) != std::string::npos)
      {
      if (pos3 == pos2)
        it = m_children.begin();
      else
        it = m_children.find(usage.substr(pos2, pos3-pos2).c_str());
      pos = pos3 + 1;
      if (it != m_children.end() && (!it->second->m_secure || writer->m_issecure))
        {
        OvmsCommand* child = it->second;
        child->ExpandUsage(child->m_usage_template, writer, result);
        }
      }
    else
      pos = pos2;
    if (it == m_children.end())
      result += "ERROR IN USAGE TEMPLATE";
    }
  result += usage.substr(pos);
  }

OvmsCommand* OvmsCommand::RegisterCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                          const char *usage, int min, int max, bool secure,
                                          int (*validate)(OvmsWriter*, OvmsCommand*, int, const char* const*, bool))
  {
  // Protect against duplicate registrations
  OvmsCommand* cmd = FindCommand(name);
  if (cmd == NULL)
    {
    cmd = new OvmsCommand(name, title, execute, usage, min, max, secure, validate);
    m_children[name] = cmd;
    cmd->m_parent = this;
    }
  return cmd;
  }

bool OvmsCommand::UnregisterCommand(const char* name)
  {
  if (name == NULL)
    {
    // Unregister this command
    return m_parent->UnregisterCommand(m_name);
    }

  // Unregister the specified child command
  auto pos = m_children.find(name);
  if (pos == m_children.end())
    {
    return false;
    }
  else
    {
    OvmsCommand* cmd = pos->second;
    m_children.erase(pos);
    delete cmd;
    return true;
    }
  }

char ** OvmsCommand::Complete(OvmsWriter* writer, int argc, const char * const * argv)
  {
  if (m_validate)
    {
    int used = -1;
    if (argc > 0)
      used = m_validate(writer, this, argc > m_max ? m_max : argc, argv, true);
    if (used < 0 || used == argc)
      return writer->GetCompletions();
    argc -= used;
    argv += used;
    }
  if (argc <= 1)
    {
    return m_children.GetCompletion(writer, argc > 0 ? argv[0] : "");
    }
  OvmsCommand* cmd = m_children.FindUniquePrefix(argv[0]);
  if (!cmd)
    {
    return writer->SetCompletion(0, NULL);
    }
  return cmd->Complete(writer, argc-1, ++argv);
  }

void OvmsCommand::Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv)
  {
//  if (argc>0)
//    {
//    printf("Execute(%s/%d) verbosity=%d argc=%d first=%s\n",m_title, m_children.size(), verbosity,argc,argv[0]);
//    }
//  else
//    {
//    printf("Execute(%s/%d) verbosity=%d (no args)\n",m_title, m_children.size(), verbosity);
//    }

  if (m_execute && (m_children.empty() || argc == 0))
    {
    //puts("Executing directly...");
    if (argc < m_min || argc > m_max || (argc > 0 && strcmp(argv[argc-1],"?")==0))
      {
      PutUsage(writer);
      return;
      }
    if ((!m_secure)||(m_secure && writer->m_issecure))
      m_execute(verbosity,writer,this,argc,argv);
    else
      writer->puts("Error: Secure command requires 'enable' mode");
    return;
    }
  else
    {
    if (m_validate && argc >= m_min)
      {
      int used = m_validate(writer, this, argc, argv, false);
      if (used < 0)
        {
        if (argc > 0 && strcmp(argv[argc-1],"?") != 0)
          writer->puts("Unrecognised command");
        if (m_usage_template && *m_usage_template)
          PutUsage(writer);
        return;
        }
      argc -= used;
      argv += used;
      }
    //puts("Looking for a matching command");
    if (argc <= 0)
      {
      if (m_execute)
        {
        if ((!m_secure)||(m_secure && writer->m_issecure))
          m_execute(verbosity,writer,this,argc,argv);
        else
          writer->puts("Error: Secure command requires 'enable' mode");
        return;
        }
      writer->puts("Subcommand required");
      PutUsage(writer);
      return;
      }
    if (strcmp(argv[0],"?")==0)
      {
      if (m_usage_template && *m_usage_template && m_execute)
        PutUsage(writer);
      // Show available commands
      int avail = 0;
      for (OvmsCommandMap::iterator it=m_children.begin(); it!=m_children.end(); ++it)
        {
        if (it->second->IsSecure() && !writer->m_issecure)
          continue;
        const char* k = it->first;
        const char* v = it->second->GetTitle();
        writer->printf("%-20.20s %s\n",k,v);
        ++avail;
        }
      if (!avail)
        writer->printf("All subcommands require 'enable' mode\n");
      return;
      }
    OvmsCommand* cmd = m_children.FindUniquePrefix(argv[0]);
    if (!cmd)
      {
      writer->puts("Unrecognised command");
      if (m_usage_template && *m_usage_template)
        PutUsage(writer);
      return;
      }
    cmd->Execute(verbosity,writer,argc-1,++argv);
    }
  }

OvmsCommand* OvmsCommand::GetParent()
  {
  return m_parent;
  }

OvmsCommand* OvmsCommand::FindCommand(const char* name)
  {
  return m_children.FindCommand(name);
  }

void help(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Enter a single \"?\" to get the root command list.");
  writer->puts("Commands can have multiple levels of subcommands.");
  writer->puts("Use \"command [...] ?\" to get the list of subcommands and parameters.");
  writer->puts("Commands can be abbreviated, push <TAB> for auto completion at any level");
  writer->puts("including at the start of a subcommand to get a list of subcommands.");
  writer->puts("Use \"enable\" to enter secure (admin) mode.");
  }

void Exit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->Exit();
  }

void log_level(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* tag = "*";
  if (argc > 0)
    tag = argv[0];
  const char* title = cmd->GetTitle();
  esp_log_level_t level_num = (esp_log_level_t)(*(title+strlen(title)-2) - '0');
  esp_log_level_set(tag, level_num);
  writer->printf("Logging level for %s set to %s\n",tag,cmd->GetName());
  }

void log_file(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 0)
    {
    MyCommandApp.SetLogfile("");
    writer->puts("Closed log file");
    return;
    }

  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }

  if (!MyCommandApp.SetLogfile(argv[0]))
    {
    writer->puts("Error: VFS file cannot be opened for append");
    return;
    }
  writer->printf("Logging to file: %s\n", argv[0]);
  }

void log_expire(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyCommandApp.m_expiretask)
    {
    writer->puts("Abort: expire task is currently running");
    return;
    }
  int keepdays;
  if (argc == 0)
    keepdays = MyConfig.GetParamValueInt("log", "file.keepdays", 30);
  else
    keepdays = atoi(argv[0]);
  MyCommandApp.ExpireLogFiles(verbosity, writer, keepdays);
  }

static OvmsCommand* monitor;
static OvmsCommand* monitor_yes;

void log_monitor(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool state;
  if (cmd == monitor)
    state = !writer->IsMonitoring();
  else
    state = (cmd == monitor_yes);
  writer->printf("Monitoring log messages %s\n", state ? "enabled" : "disabled");
  writer->SetMonitoring(state);
  }

typedef struct
  {
  std::string password;
  int tries;
  } PasswordContext;

bool enableInsert(OvmsWriter* writer, void* v, char ch)
  {
  PasswordContext* pc = (PasswordContext*)v;
  if (ch == '\n')
    {
    std::string p = MyConfig.GetParamValue("password","module");
    if (p.compare(pc->password) == 0)
      {
      writer->SetSecure(true);
      writer->printf("\nSecure mode");
      delete pc;
      return false;
      }
    if (++pc->tries == 3)
      {
      writer->printf("\nError: %d incorrect password attempts", pc->tries);
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      delete pc;
      return false;
      }
    writer->printf("\nSorry, try again.\nPassword:");
    pc->password.erase();
    return true;
    }
  if (ch == 'C'-0100)
    {
    delete pc;
    return false;
    }
  if (ch != '\r')
    pc->password.append(1, ch);
  return true;
  }

void enable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string p = MyConfig.GetParamValue("password","module");
  if ((p.empty())||((argc==1)&&(p.compare(argv[0])==0)))
    {
    writer->SetSecure(true);
    writer->puts("Secure mode");
    }
  else if (argc == 1)
    {
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    writer->puts("Error: Invalid password");
    }
  else
    {
    PasswordContext* pc = new PasswordContext;
    pc->tries = 0;
    writer->printf("Password:");
    writer->RegisterInsertCallback(enableInsert, pc);
    }
  }

void disable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->SetSecure(false);
  }

OvmsCommandApp::OvmsCommandApp()
  {
  ESP_LOGI(TAG, "Initialising COMMAND (1000)");

  m_logfile = NULL;
  m_logfile_path = "";
  m_logfile_size = 0;
  m_logfile_maxsize = 0;
  m_expiretask = 0;

  m_root.RegisterCommand("help", "Ask for help", help, "", 0, 0, false);
  m_root.RegisterCommand("exit", "End console session", Exit , "", 0, 0, false);
  OvmsCommand* cmd_log = MyCommandApp.RegisterCommand("log","LOG framework");
  cmd_log->RegisterCommand("file", "Start logging to specified file", log_file , "<vfspath>", 0, 1);
  cmd_log->RegisterCommand("close", "Stop logging to file", log_file);
  cmd_log->RegisterCommand("expire", "Expire old log files", log_expire, "[<keepdays>]", 0, 1);
  OvmsCommand* level_cmd = cmd_log->RegisterCommand("level", "Set logging level", NULL, "$C [<tag>]", 0, 0, false);
  level_cmd->RegisterCommand("verbose", "Log at the VERBOSE level (5)", log_level , "[<tag>]", 0, 1);
  level_cmd->RegisterCommand("debug", "Log at the DEBUG level (4)", log_level , "[<tag>]", 0, 1);
  level_cmd->RegisterCommand("info", "Log at the INFO level (3)", log_level , "[<tag>]", 0, 1);
  level_cmd->RegisterCommand("warn", "Log at the WARN level (2)", log_level , "[<tag>]", 0, 1, false);
  level_cmd->RegisterCommand("error", "Log at the ERROR level (1)", log_level , "[<tag>]", 0, 1, false);
  level_cmd->RegisterCommand("none", "No logging (0)", log_level , "[<tag>]", 0, 1, false);
  monitor = cmd_log->RegisterCommand("monitor", "Monitor log on this console", log_monitor , "[$C]");
  monitor_yes = monitor->RegisterCommand("yes", "Monitor log", log_monitor);
  monitor->RegisterCommand("no", "Don't monitor log", log_monitor);
  m_root.RegisterCommand("enable","Enter secure mode (enable access to all commands)", enable, "[<password>]", 0, 1, false);
  m_root.RegisterCommand("disable","Leave secure mode (disable access to most commands)", disable);
  }

OvmsCommandApp::~OvmsCommandApp()
  {
  }

void OvmsCommandApp::ConfigureLogging()
  {
  MyConfig.RegisterParam("log", "Logging configuration", true, true);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&OvmsCommandApp::EventHandler, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "sd.mounted", std::bind(&OvmsCommandApp::EventHandler, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "sd.unmounting", std::bind(&OvmsCommandApp::EventHandler, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "ticker.3600", std::bind(&OvmsCommandApp::EventHandler, this, _1, _2));

  ReadConfig();
  }

OvmsCommand* OvmsCommandApp::RegisterCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                             const char *usage, int min, int max, bool secure)
  {
  return m_root.RegisterCommand(name, title, execute, usage, min, max, secure);
  }

bool OvmsCommandApp::UnregisterCommand(const char* name)
  {
  // Unregister the specified child command
  return m_root.UnregisterCommand(name);
  }

OvmsCommand* OvmsCommandApp::FindCommand(const char* name)
  {
  return m_root.FindCommand(name);
  }

void OvmsCommandApp::RegisterConsole(OvmsWriter* writer)
  {
  m_consoles.insert(writer);
  }

void OvmsCommandApp::DeregisterConsole(OvmsWriter* writer)
  {
  m_consoles.erase(writer);
  }

int OvmsCommandApp::Log(const char* fmt, ...)
  {
  va_list args;
  va_start(args, fmt);
  size_t ret = Log(fmt, args);
  va_end(args);
  return ret;
  }

int OvmsCommandApp::Log(const char* fmt, va_list args)
  {
  LogBuffers* lb;
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  PartialLogs::iterator it = m_partials.find(task);
  if (it == m_partials.end())
    lb = new LogBuffers();
  else
    {
    lb = it->second;
    m_partials.erase(task);
    }
  int ret = LogBuffer(lb, fmt, args);
  lb->set(m_consoles.size());
  for (ConsoleSet::iterator it = m_consoles.begin(); it != m_consoles.end(); ++it)
    {
    (*it)->Log(lb);
    }
  return ret;
  }

int OvmsCommandApp::LogPartial(const char* fmt, ...)
  {
  LogBuffers* lb;
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  PartialLogs::iterator it = m_partials.find(task);
  if (it == m_partials.end())
    {
    lb = new LogBuffers();
    m_partials[task] = lb;
    }
  else
    {
    lb = it->second;
    }
  va_list args;
  va_start(args, fmt);
  int ret = LogBuffer(lb, fmt, args);
  va_end(args);
  return ret;
  }

int OvmsCommandApp::LogBuffer(LogBuffers* lb, const char* fmt, va_list args)
  {
  char *buffer;
  int ret = vasprintf(&buffer, fmt, args);
  if (ret < 0) return ret;

  // Replace CR/LF except last by "|", but don't leave '|' at the end.
  // An ESC sequence to change color may be appended after the log text.
  char* s;
  for (s=buffer; *s; s++)
    {
    if (*s=='\r' || *s=='\n')
      {
      char *t = s;
      if (*(s+1) == '\033')
        ++s;
      else if (*(s+1) != '\0')
        {
        *s = '|';
        continue;
        }
      while (t > buffer && *(t-1) == '|')
        --t;
      while ((*t++ = *s++)) ;
      break;
      }
    }

  if (m_logfile)
    {
    // Log to the log file as well...
    // We need to protect this with a mutex lock, as fsync appears to NOT be thread safe
    // https://github.com/espressif/esp-idf/issues/1837
    OvmsMutexLock fsynclock(&m_fsync_mutex);
    m_logfile_size += fwrite(buffer,1,strlen(buffer),m_logfile);
    if (m_logfile_maxsize && m_logfile_size > (m_logfile_maxsize*1024))
      {
      CycleLogfile();
      }
    else
      {
      fflush(m_logfile);
      fsync(fileno(m_logfile));
      }
    }
  lb->append(buffer);
  return ret;
  }

int OvmsCommandApp::HexDump(const char* tag, const char* prefix, const char* data, size_t length, size_t colsize /*=16*/)
  {
  char* buffer = NULL;
  int rlength = (int)length;

  while (rlength>0)
    {
    rlength = FormatHexDump(&buffer, data, rlength, colsize);
    data += colsize;
    ESP_LOGV(tag, "%s: %s", prefix, buffer);
    }

  if (buffer)
    free(buffer);
  return length;
  }

char ** OvmsCommandApp::Complete(OvmsWriter* writer, int argc, const char * const * argv)
  {
  return m_root.Complete(writer, argc, argv);
  }

void OvmsCommandApp::Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv)
  {
  if (argc == 0)
    {
    writer->puts("Error: Empty command unrecognised");
    }
  else
    {
    writer->SetArgv(argv);
    m_root.Execute(verbosity, writer, argc, argv);
    }
  }

bool OvmsCommandApp::SetLogfile(std::string path)
  {
  // close old file:
  if (m_logfile)
    {
    fclose(m_logfile);
    m_logfile = NULL;
    ESP_LOGI(TAG, "SetLogfile: file logging stopped");
    }
  // open new file:
  if (!path.empty())
    {
    if (MyConfig.ProtectedPath(path))
      {
      ESP_LOGE(TAG, "SetLogfile: '%s' is a protected path", path.c_str());
      return false;
      }
#ifdef CONFIG_OVMS_COMP_SDCARD
    if (startsWith(path, "/sd") && (!MyPeripherals || !MyPeripherals->m_sdcard || !MyPeripherals->m_sdcard->isavailable()))
      {
      ESP_LOGW(TAG, "SetLogfile: cannot open '%s', will retry on SD mount", path.c_str());
      return false;
      }
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
      m_logfile_size = st.st_size;
    else
      m_logfile_size = 0;
    FILE* file = fopen(path.c_str(), "a+");
    if (file == NULL)
      {
      ESP_LOGE(TAG, "SetLogfile: cannot open '%s'", path.c_str());
      return false;
      }
    else
      {
      ESP_LOGI(TAG, "SetLogfile: now logging to file '%s'", path.c_str());
      }
    m_logfile = file;
    }
  return true;
  }

void OvmsCommandApp::SetLoglevel(std::string tag, std::string level)
  {
  int level_num;
  if (level == "verbose")
    level_num = 5;
  else if (level == "debug")
    level_num = 4;
  else if (level == "info")
    level_num = 3;
  else if (level == "warn")
    level_num = 2;
  else if (level == "error")
    level_num = 1;
  else if (level == "none")
    level_num = 0;
  else
    level_num = CONFIG_LOG_DEFAULT_LEVEL;

  if (tag.empty())
    esp_log_level_set("*", (esp_log_level_t)level_num);
  else
    esp_log_level_set(tag.c_str(), (esp_log_level_t)level_num);
  }

void OvmsCommandApp::CycleLogfile()
  {
  if (!m_logfile)
    return;
  fclose(m_logfile);
  m_logfile = NULL;
  char ts[20];
  time_t tm = time(NULL);
  strftime(ts, sizeof(ts), ".%Y%m%d-%H%M%S", localtime(&tm));
  std::string archpath = m_logfile_path;
  archpath.append(ts);
  if (rename(m_logfile_path.c_str(), archpath.c_str()) == 0)
    ESP_LOGI(TAG, "CycleLogfile: log file '%s' archived as '%s'", m_logfile_path.c_str(), archpath.c_str());
  else
    ESP_LOGE(TAG, "CycleLogfile: rename log file '%s' to '%s' failed", m_logfile_path.c_str(), archpath.c_str());
  SetLogfile(m_logfile_path);
  }

void OvmsCommandApp::ExpireLogFiles(int verbosity, OvmsWriter* writer, int keepdays)
  {
  if (keepdays <= 0)
    {
    if (writer)
      writer->printf("Abort: expire disabled (keepdays=%d)\n", keepdays);
    else
      ESP_LOGD(TAG, "ExpireLogFiles: disabled (keepdays=%d)", keepdays);
    return;
    }

  // get archive directory:
  std::string archdir = m_logfile_path;
  std::string::size_type p = archdir.find_last_of('/');
  if (p == std::string::npos)
    {
    if (writer)
      writer->puts("Error: log path not set");
    else
      ESP_LOGE(TAG, "ExpireLogFiles: log path not set");
    return;
    }
  archdir.resize(p);
  DIR *dir = opendir(archdir.c_str());
  if (!dir)
    {
    if (writer)
      writer->printf("Error: cannot open log directory '%s'\n", archdir.c_str());
    else
      ESP_LOGE(TAG, "ExpireLogFiles: cannot open log directory '%s'", archdir.c_str());
    return;
    }
  else if (writer && verbosity >= COMMAND_RESULT_NORMAL)
    {
    writer->printf("Scanning directory '%s'...\n", archdir.c_str());
    }

  time_t tm = time(NULL) - keepdays * 86400;
  struct dirent *dp;
  char path[PATH_MAX];
  struct stat st;
  int delcnt = 0;

  while ((dp = readdir(dir)) != NULL)
    {
    snprintf(path, sizeof(path), "%s/%s", archdir.c_str(), dp->d_name);
    if (strncmp(path, m_logfile_path.c_str(), m_logfile_path.size()) != 0)
      continue;
    if (stat(path, &st))
      {
      if (writer)
        writer->printf("Error: cannot stat '%s'\n", path);
      else
        ESP_LOGE(TAG, "ExpireLogFiles: cannot stat '%s'", path);
      continue;
      }
    if (st.st_mtime < tm)
      {
      if (writer && verbosity >= COMMAND_RESULT_NORMAL)
        writer->printf("Deleting '%s'\n", path);
      else
        ESP_LOGD(TAG, "ExpireLogFiles: deleting '%s'", path);
      if (unlink(path))
        {
        if (writer)
          writer->printf("Error: cannot delete '%s'\n", path);
        else
          ESP_LOGE(TAG, "ExpireLogFiles: cannot delete '%s'", path);
        }
      else
        delcnt++;
      }
    }

  closedir(dir);

  if (writer)
    writer->printf("Done, %d file(s) deleted.\n", delcnt);
  else
    ESP_LOGI(TAG, "ExpireLogFiles: %d file(s) deleted", delcnt);
  }

void OvmsCommandApp::ExpireTask(void* data)
  {
  int keepdays = MyConfig.GetParamValueInt("log", "file.keepdays", 30);
  MyCommandApp.ExpireLogFiles(0, NULL, keepdays);
  MyCommandApp.m_expiretask = 0;
  vTaskDelete(NULL);
  }

void OvmsCommandApp::EventHandler(std::string event, void* data)
  {
  if (event == "config.changed")
    {
    OvmsConfigParam* param = (OvmsConfigParam*) data;
    if (param && param->GetName() == "log")
      ReadConfig();
    }
  else if (event == "sd.mounted")
    {
    if (startsWith(m_logfile_path, "/sd"))
      SetLogfile(m_logfile_path);
    }
  else if (event == "sd.unmounting")
    {
    if (startsWith(m_logfile_path, "/sd"))
      SetLogfile("");
    }
  else if (event == "ticker.3600")
    {
    int keepdays = MyConfig.GetParamValueInt("log", "file.keepdays", 30);
    time_t utm = time(NULL);
    struct tm* ltm = localtime(&utm);
    if (keepdays && ltm->tm_hour == 0 && !m_expiretask)
      xTaskCreatePinnedToCore(ExpireTask, "OVMS ExpireLogs", 4096, NULL, 0, &m_expiretask, 1);
    }
  }

void OvmsCommandApp::ReadConfig()
  {
  OvmsConfigParam* param = MyConfig.CachedParam("log");

  // configure log levels:
  std::string level = MyConfig.GetParamValue("log", "level");
  if (!level.empty())
    SetLoglevel("*", level);
  for (auto const& kv : param->m_map)
    {
    if (startsWith(kv.first, "level.") && !kv.second.empty())
      SetLoglevel(kv.first.substr(6), kv.second);
    }

  // configure log file:
  m_logfile_maxsize = MyConfig.GetParamValueInt("log", "file.maxsize", 1024);
  if (MyConfig.GetParamValueBool("log", "file.enable", false) == false)
    m_logfile_path = "";
  else
    m_logfile_path = MyConfig.GetParamValue("log", "file.path");
  if (!m_logfile_path.empty())
    SetLogfile(m_logfile_path);
  }


OvmsCommandTask::OvmsCommandTask(int _verbosity, OvmsWriter* _writer, OvmsCommand* _cmd, int _argc, const char* const* _argv)
  : TaskBase(_cmd->GetName(), CONFIG_OVMS_SYS_COMMAND_STACK_SIZE)
  {
  m_state = OCS_Init;

  // clone command arguments:
  verbosity = _verbosity;
  writer = _writer;
  cmd = _cmd;
  argc = _argc;
  if (argc == 0)
    argv = NULL;
  else
    {
    argv = (char**) ExternalRamMalloc(argc * sizeof(char*));
    for (int i=0; i < argc; i++)
      argv[i] = strdup(_argv[i]);
    }
  }

OvmsCommandState_t OvmsCommandTask::Prepare()
  {
  return (writer->IsInteractive()) ? OCS_RunLoop : OCS_RunOnce;
  }

bool OvmsCommandTask::Run()
  {
  m_state = Prepare();
  switch (m_state)
    {
    case OCS_RunLoop:
      // start task:
      writer->RegisterInsertCallback(Terminator, (void*) this);
      if (!Instantiate())
        {
        delete this;
        return false;
        }
      return true;
      break;

    case OCS_RunOnce:
      Service();
      Cleanup();
      delete this;
      return true;
      break;

    default:
      // preparation failed:
      delete this;
      return false;
      break;
    }
  }

OvmsCommandTask::~OvmsCommandTask()
  {
  if (m_state == OCS_StopRequested)
    writer->puts("^C");
  writer->DeregisterInsertCallback(Terminator);
  if (argv)
    {
    for (int i=0; i < argc; i++)
      free(argv[i]);
    free(argv);
    }
  }

bool OvmsCommandTask::Terminator(OvmsWriter* writer, void* userdata, char ch)
  {
  if (ch == 3) // Ctrl-C
    ((OvmsCommandTask*) userdata)->m_state = OCS_StopRequested;
  return true;
  }
