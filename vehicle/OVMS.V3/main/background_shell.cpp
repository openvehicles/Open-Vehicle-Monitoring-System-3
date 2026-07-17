/*
;    Project:       Open Vehicle Monitor System
;    Date:          7th March 2026
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2026       Michael Balzer
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
static const char *TAG = "bgshell";

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "background_shell.h"
#include "ovms_notify.h"


BackgroundShell::BackgroundShell(std::string taskname,
    int verbosity /*=COMMAND_RESULT_VERBOSE*/,
    int stack /*=CONFIG_OVMS_SYS_COMMAND_STACK_SIZE*/,
    UBaseType_t priority /*=CONFIG_OVMS_SYS_COMMAND_PRIORITY*/,
    int queuesize /*=25*/
  )
  : OvmsShell(verbosity), TaskBase(TAG, stack, priority)
  {
  m_taskname = taskname.substr(0,15);
  m_name = m_taskname.c_str();
  m_queuesize = queuesize;
  m_queue = 0;
  m_exit = false;
  }

BackgroundShell::~BackgroundShell()
  {
  // cleanup & free queue:
  if (m_queue)
    {
    char* input;
    while (xQueueReceive(m_queue, &input, 0) == pdTRUE)
      free(input);
    vQueueDelete(m_queue);
    }
  }

bool BackgroundShell::Init()
  {
  // shell/microrl init, no prompt:
  Initialize(false);
  ProcessChar('\n');

  // create input queue:
  m_queue = xQueueCreate(m_queuesize, sizeof(const char*));
  if (!m_queue) return false;

  // init output buffer:
  m_output.reserve(2048);

  // create task:
  return Instantiate();
  }

void BackgroundShell::Service()
  {
  ESP_LOGI(TAG, "Task %p running", m_taskid);

  char* input;
  while (!m_exit)
    {
    if (xQueueReceive(m_queue, &input, pdMS_TO_TICKS(1000)) == pdTRUE)
      {
      // pass input to microrl:
      ProcessChars(input, strlen(input));
      free(input);
      }
    }
  }

void BackgroundShell::Exit()
  {
  m_exit = true;
  }

void BackgroundShell::Cleanup()
  {
  ESP_LOGI(TAG, "Task %p terminated", m_taskid);
  }

void BackgroundShell::QueueChar(char c)
  {
  char buf[2] = { c, 0 };
  QueueChars(buf, 1);
  }

void BackgroundShell::QueueChars(const char* buf, int len /*=-1*/)
  {
  if (len == -1) len = strlen(buf);
  char* input = strndup(buf, len);
  if (input)
    {
    if (xQueueSend(m_queue, &input, portMAX_DELAY) != pdTRUE)
      free(input);
    }
  }

void BackgroundShell::SetCommand(OvmsCommand* cmd)
  {
  m_command = cmd;

  if (!m_command)
    {
    m_cmd_fullname = "[none]";
    m_cmd_subtype = "cmd.none";
    }
  else
    {
    // get full command name:
    m_cmd_fullname = m_command->GetFullName();
    // expand alias "." → "script run"
    if (startsWith(m_cmd_fullname, "."))
      m_cmd_fullname.replace(0, 1, "script run");

    // derive notification subtype:
    m_cmd_subtype = "cmd.";
    m_cmd_subtype.append(m_cmd_fullname);
    std::replace(m_cmd_subtype.begin(), m_cmd_subtype.end(), ' ', '.');

    ESP_LOGI(TAG, "Executing command: %s", m_cmd_fullname.c_str());
    }
  }

void BackgroundShell::Execute(int argc, const char * const * argv)
  {
  // reset output:
  m_output.clear();
  m_linestart = 0;
  
  // execute command:
  uint32_t t_start = esp_log_timestamp();
  MyCommandApp.Execute(m_verbosity, this, argc, argv);
  uint32_t t_done = esp_log_timestamp();
  
  // send command output:
  if (!m_exit)
    {
    // process remaining lines:
    ProcessOutputLines(true);
    
    // send info notification:
    if (m_notify_mode == 'i')
      {
      MyNotify.NotifyString("info", m_cmd_subtype.c_str(), m_output.empty() ? "(done, no output)" : m_output.c_str());
      }
    }

  // log execution stats:
  uint32_t minstackfree = uxTaskGetStackHighWaterMark(NULL);
  ESP_LOGD(TAG, "Command '%s' done: %.1f seconds run time, output size %u bytes, min stack free %u bytes",
    m_cmd_fullname.c_str(), (float)(t_done-t_start)/1000, m_output.size(), minstackfree);
  }

void BackgroundShell::ProcessOutputLines(bool finish)
  {
  extram::string line;
  
  // process full lines:
  extram::string::size_type eol, eoc;
  while ((eol = m_output.find('\n', m_linestart)) != extram::string::npos)
    {
    if (eol - m_linestart > 0) // skip empty lines
      {
      eoc = eol-1;
      if (eoc > m_linestart && m_output[eoc-1] == '\r')
        eoc--; // skip CR
      if (eoc > m_linestart)
        {
        // log:
        line = m_output.substr(m_linestart, eoc-m_linestart+1).c_str();
        ESP_LOGV(TAG, "[%s] %s", m_cmd_fullname.c_str(), line.c_str());

        // send stream notification:
        if (m_notify_mode == 's')
          {
          MyNotify.NotifyString("stream", m_cmd_subtype.c_str(), line.c_str());
          }
        }
      }
    m_linestart = eol + 1;
    }

  // process remaining buffer:
  if (finish && m_linestart < m_output.size())
    {
    // log:
    line = m_output.substr(m_linestart).c_str();
    ESP_LOGV(TAG, "[%s] %s", m_cmd_fullname.c_str(), line.c_str());

    // send stream notification:
    if (m_notify_mode == 's')
      {
      MyNotify.NotifyString("stream", m_cmd_subtype.c_str(), line.c_str());
      }
    }
  }

int BackgroundShell::puts(const char* s)
  {
  write(s, strlen(s));
  write("\n", 1);
  return 0;
  }

int BackgroundShell::printf(const char* fmt, ...)
  {
  char *buffer = NULL;
  va_list args;
  va_start(args, fmt);
  int ret = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (ret >= 0)
    {
    write(buffer, ret);
    free(buffer);
    }
  return ret;
  }

ssize_t BackgroundShell::write(const void *buf, size_t nbyte)
  {
  // append to output buffer:
  m_output.append((const char*)buf, nbyte);

  // process full lines:
  ProcessOutputLines(false);

  return (ssize_t) nbyte;
  }
