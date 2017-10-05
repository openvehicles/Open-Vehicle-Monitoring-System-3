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

#include "esp_log.h"
static const char *TAG = "command";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "ovms_command.h"
#include "log_buffers.h"

OvmsCommandApp MyCommandApp __attribute__ ((init_priority (1000)));

OvmsWriter::OvmsWriter()
  {
  }

OvmsWriter::~OvmsWriter()
  {
  }

void OvmsWriter::Exit()
  {
  puts("This console cannot exit.");
  }

OvmsCommand* OvmsCommandMap::FindUniquePrefix(const std::string& key)
  {
  OvmsCommand* found = NULL;
  for (iterator it = begin(); it != end(); ++it)
    {
    if (it->first.compare(0, key.size(), key) == 0)
      {
      if (key.size() == it->first.size())
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

OvmsCommand::OvmsCommand()
  {
  m_parent = NULL;
  }

OvmsCommand::OvmsCommand(std::string name, std::string title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                         const char *usage, int min, int max)
  {
  m_name = name;
  m_title = title;
  m_execute = execute;
  m_usage = usage;
  m_min = min;
  m_max = max;
  m_parent = NULL;
  }

OvmsCommand::~OvmsCommand()
  {
  }

std::string OvmsCommand::GetName()
  {
  return m_name;
  }

std::string OvmsCommand::GetTitle()
  {
  return m_title;
  }

// Dynamic generation of "Usage:" messages.  Syntax of the usage template string:
// - Prefix is "Usage: " followed by names from ancestors (if any) and name of self
// - "<$C>" expands to children as <child1|child2|child3>
// - "[$C]" expands to optional children as [child1|child2|child3]
// - $G$ expands to the usage of the first child (typically used after $C)
// - $Gfoo$ expands to the usage of the child named "foo"
// - Parameters after command and subcommand tokens may be explicit like " <metric>"
// - Empty usage template "" defaults to "<$C>" for non-terminal OvmsCommand
std::string OvmsCommand::GetUsage()
  {
  std::string usage = m_usage.empty() && !m_execute ? "<$C>" : m_usage;
  std::string result("Usage: ");
  size_t pos = result.size();
  for (OvmsCommand* parent = m_parent; parent && parent->m_parent; parent = parent->m_parent)
    {
    result.insert(pos, " ");
    result.insert(pos, parent->m_name);
    }
  result += m_name + " ";
  pos = ExpandUsage(usage, result);
  result += usage.substr(pos);
  return result;
  }

size_t OvmsCommand::ExpandUsage(std::string usage, std::string& result)
  {
  size_t pos;
  if ((pos = usage.find_first_of("$C")) != std::string::npos)
    {
    result += usage.substr(0, pos);
    pos += 2;
    for (OvmsCommandMap::iterator it = m_children.begin(); ; )
      {
      result += it->first;
      if (++it == m_children.end())
        break;
      result += "|";
      }
    }
  else pos = 0;
  size_t pos2;
  if ((pos2 = usage.find_first_of("$G", pos)) != std::string::npos)
    {
    result += usage.substr(pos, pos2-pos);
    pos2 += 2;
    size_t pos3;
    OvmsCommandMap::iterator it = m_children.end();
    if ((pos3 = usage.find_first_of("$", pos2)) != std::string::npos)
      {
      if (pos3 == pos2)
        it = m_children.begin();
      else
        it = m_children.find(usage.substr(pos2, pos3-pos2));
      pos = pos3 + 1;
      if (it != m_children.end())
        {
        OvmsCommand* child = it->second;
        pos3 = child->ExpandUsage(child->m_usage, result);
        result += child->m_usage.substr(pos3);
        }
      }
    else
      pos = pos2;
    if (it == m_children.end())
      result += "ERROR IN USAGE TEMPLATE";
    }
  return pos;
  }

OvmsCommand* OvmsCommand::RegisterCommand(std::string name, std::string title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                          const char *usage, int min, int max)
  {
  OvmsCommand* cmd = new OvmsCommand(name, title, execute, usage, min, max);
  m_children[name] = cmd;
  cmd->m_parent = this;
  //printf("Registered '%s' under '%s'\n",name.c_str(),m_title.c_str());
  return cmd;
  }

char ** OvmsCommand::Complete(OvmsWriter* writer, int argc, const char * const * argv)
  {
  if (argc <= 1)
    {
    return writer->GetCompletion(m_children, argc > 0 ? argv[0] : "");
    }
  OvmsCommand* cmd = m_children.FindUniquePrefix(argv[0]);
  if (!cmd)
    {
    return writer->GetCompletion(m_children, NULL);
    }
  return cmd->Complete(writer, argc-1, ++argv);
  }

void OvmsCommand::Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv)
  {
//  if (argc>0)
//    {
//    printf("Execute(%s/%d) verbosity=%d argc=%d first=%s\n",m_title.c_str(), m_children.size(), verbosity,argc,argv[0]);
//    }
//  else
//    {
//    printf("Execute(%s/%d) verbosity=%d (no args)\n",m_title.c_str(), m_children.size(), verbosity);
//    }

  if (m_execute)
    {
    //puts("Executing directly...");
    if (argc < m_min || argc > m_max || (argc > 0 && strcmp(argv[argc-1],"?")==0))
      {
      writer->puts(GetUsage().c_str());
      return;
      }
    m_execute(verbosity,writer,this,argc,argv);
    return;
    }
  else
    {
    //puts("Looking for a matching command");
    if (argc <= 0)
      {
      writer->puts("Subcommand required");
      writer->puts(GetUsage().c_str());
      return;
      }
    if (strcmp(argv[0],"?")==0)
      {
      // Show available commands
      for (OvmsCommandMap::iterator it=m_children.begin(); it!=m_children.end(); ++it)
        {
        const char* k = it->first.c_str();
        const std::string v = it->second->GetTitle();
        writer->printf("%-20.20s %s\n",k,v.c_str());
        }
      return;
      }
    OvmsCommand* cmd = m_children.FindUniquePrefix(argv[0]);
    if (!cmd)
      {
      writer->puts("Unrecognised command");
      if (!m_usage.empty())
        writer->puts(GetUsage().c_str());
      return;
      }
    if (argc>1)
      {
      cmd->Execute(verbosity,writer,argc-1,++argv);
      }
    else
      {
      cmd->Execute(verbosity,writer,0,NULL);
      }
    }
  }

OvmsCommand* OvmsCommand::GetParent()
  {
  return m_parent;
  }

OvmsCommand* OvmsCommand::FindCommand(std::string name)
  {
  return m_children.FindUniquePrefix(name);
  }

void help(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("This isn't really much help, is it?");
  }

void Exit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->Exit();
  }

void level(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* tag = "*";
  if (argc > 0)
    tag = argv[0];
  const std::string& title = cmd->GetTitle();
  esp_log_level_t level_num = (esp_log_level_t)atoi(title.substr(title.size()-2, 1).c_str());
  esp_log_level_set(tag, level_num);
  writer->printf("Logging level for %s set to %s\n",tag,cmd->GetName().c_str());
  }

OvmsCommandApp::OvmsCommandApp()
  {
  ESP_LOGI(TAG, "Initialising COMMAND (1000)");

  m_root.RegisterCommand("help", "Ask for help", help, "", 0, 0);
  m_root.RegisterCommand("exit", "End console session", Exit , "", 0, 0);
  OvmsCommand* level_cmd = m_root.RegisterCommand("level", "Set logging level", NULL, "<$C> [tag]");
  level_cmd->RegisterCommand("verbose", "Log at the VERBOSE level (5)", level , "[tag]", 0, 1);
  level_cmd->RegisterCommand("debug", "Log at the DEBUG level (4)", level , "[tag]", 0, 1);
  level_cmd->RegisterCommand("info", "Log at the INFO level (3)", level , "[tag]", 0, 1);
  level_cmd->RegisterCommand("warn", "Log at the WARN level (2)", level , "[tag]", 0, 1);
  level_cmd->RegisterCommand("error", "Log at the ERROR level (1)", level , "[tag]", 0, 1);
  level_cmd->RegisterCommand("none", "No logging (0)", level , "[tag]", 0, 1);
  }

OvmsCommandApp::~OvmsCommandApp()
  {
  }

OvmsCommand* OvmsCommandApp::RegisterCommand(std::string name, std::string title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                             const char *usage, int min, int max)
  {
  return m_root.RegisterCommand(name, title, execute, usage, min, max);
  }

OvmsCommand* OvmsCommandApp::FindCommand(std::string name)
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
  va_list args;
  va_start(args, fmt);
  size_t ret = lb->append(fmt, args);
  va_end(args);
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
  size_t ret = lb->append(fmt, args);
  va_end(args);
  return ret;
  }

int OvmsCommandApp::HexDump(const char* prefix, const char* data, size_t length)
  {
  char buffer[128]; // Plenty of space for 16x3 + 2 + 16 + 1(\0)
  const char *s = data;
  int rlength = (int)length;

  while (rlength>0)
    {
    char *p = buffer;
    const char *os = s;
    for (int k=0;k<16;k++)
      {
      if (k<rlength)
        {
        sprintf(p,"%2.2x ",*s);
        s++;
        }
      else
        {
        sprintf(p,"   ");
        }
      p+=3;
      }
    sprintf(p,"  ");
    s = os;
    for (int k=0;k<16;k++)
      {
      if (k<rlength)
        {
        if (isprint((int)*s))
          { *p = *s; }
        else
          { *p = '.'; }
        s++;
        }
      else
        {
        *p = ' ';
        }
      p++;
    }
    *p = 0;
    Log("%s %s\n",prefix,buffer);
    rlength -= 16;
    }

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
    m_root.Execute(verbosity, writer, argc, argv);
    }
  }
