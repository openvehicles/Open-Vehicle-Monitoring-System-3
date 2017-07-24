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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"
#include "esp_log.h"

static const char *TAG = "Console";
static char CRbuf[1] = { '\r' };
static char NLbuf[1] = { '\n' };
static char ctrlRbuf[1] = { 'R'-0100 };

OvmsConsole::OvmsConsole()
  {
  m_ready = false;
  }

OvmsConsole::~OvmsConsole()
  {
  m_ready = false;
  MyCommandApp.DeregisterConsole(this);
  vTaskDelete(m_taskid);
  }

void OvmsConsole::Initialize(const char* console)
  {
  std::string task = "Console";
  task += console;
  task += "Task";
  xTaskCreatePinnedToCore(ConsoleTask, task.c_str(), 4096, (void*)this, 5, &m_taskid, 1);

  printf("\n\033[32mWelcome to the Open Vehicle Monitoring System (OVMS) - %s Console\033[0m", console);
  microrl_init (&m_rl, Print);
  m_rl.userdata = (void*)this;
  microrl_set_complete_callback(&m_rl, Complete);
  microrl_set_execute_callback(&m_rl, Execute);
  ProcessChar('\n');

  MyCommandApp.RegisterConsole(this);
  m_ready = true;
  }

void OvmsConsole::ProcessChar(const char c)
  {
  // put received char ino microrl lib
  microrl_insert_char(&m_rl, c);
  }

void OvmsConsole::ProcessChars(const char* buf, int len)
  {
  for (int i = 0; i < len; ++i)
    {
    // put received char from UART to microrl lib
    microrl_insert_char(&m_rl, *buf++);
    }
  }

char ** OvmsConsole::GetCompletion(OvmsCommandMap& children, const char* token)
  {
  unsigned int index = 0;
  if (token)
    {
    for (OvmsCommandMap::iterator it = children.begin(); it != children.end(); ++it)
      {
      const char *key = it->first.c_str();
      if (strncmp(key, token, strlen(token)) == 0)
        {
        if (index < COMPLETION_MAX_TOKENS+1)
          {
          if (index == COMPLETION_MAX_TOKENS)
            {
            key = "...";
            }
          strncpy(m_space[index], key, TOKEN_MAX_LENGTH-1);
          m_space[index][TOKEN_MAX_LENGTH-1] = '\0';
          m_completions[index] = m_space[index];
          ++index;
          }
        }
      }
    }
  m_completions[index] = NULL;
  return m_completions;
  }

void OvmsConsole::Print(microrl_t* rl, const char * str)
  {
  ((OvmsWriter*)rl->userdata)->write(str,strlen(str));
  }

char ** OvmsConsole::Complete (microrl_t* rl, int argc, const char * const * argv )
  {
  return MyCommandApp.Complete((OvmsWriter*)rl->userdata, argc, argv);
  }

int OvmsConsole::Execute (microrl_t* rl, int argc, const char * const * argv )
  {
  MyCommandApp.Execute(COMMAND_RESULT_VERBOSE, (OvmsWriter*)rl->userdata, argc, argv);
  return 0;
  }

void OvmsConsole::Log(char* message)
  {
  if (!m_ready)
    {
    free(message);
    return;
    }
  Event event;
  event.type = ALERT;
  event.buffer = message;
  BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
  if (ret != pdPASS)
    {
    free(message);
    ESP_LOGI(TAG, "Timeout queueing message in Console::Log\n");
    }
  }

typedef enum
  {
  AT_PROMPT,
  AWAITING_NL,
  NO_NL
  } DisplayState;

void OvmsConsole::ConsoleTask(void *pvParameters)
  {
  ((OvmsConsole*)pvParameters)->EventLoop();
  }

void OvmsConsole::EventLoop()
  {
  DisplayState state = AT_PROMPT;
  portTickType ticks = portMAX_DELAY;
  Event event;

  vTaskDelay(50 / portTICK_PERIOD_MS);

  for (;;)
    {
    // Waiting for UART RX event or async log message event
    if (xQueueReceive(m_queue, (void*)&event, ticks))
      {
      if (event.type == ALERT)
        {
        // We remove the newline from the end of a log message so that we can later
        // output a newline as part of restoring the command prompt and its line
        // without leaving a blank line above it.  So before we display a new log
        // message we need to output a newline if the last action was displaying a
        // log message, or output a carriage return to back over the prompt.
        if (state == AWAITING_NL)
          write(NLbuf, 1);
        else if (state == AT_PROMPT)
          write(CRbuf, 1);
        size_t len = strlen(event.buffer);
        if (event.buffer[len-1] == '\n')
          {
          event.buffer[--len] = '\0';
          if (event.buffer[len-1] == '\r')  // Remove CR, too, in case of \r\n
            event.buffer[--len] = '\0';
          state = AWAITING_NL;
          write(event.buffer, len);
          }
        else
          {
          state = NO_NL;
          write(event.buffer, len);
          }
        free(event.buffer);
        ticks = 200 / portTICK_PERIOD_MS;
        }
      else
        {
        if (state != AT_PROMPT)
          ProcessChars(ctrlRbuf, 1);    // Restore the prompt plus any type-in on a new line
        state = AT_PROMPT;
        HandleDeviceEvent(&event);
        }
      }
    else
      {
      // Timeout indicates the queue is empty
      if (state != AT_PROMPT)
        ProcessChars(ctrlRbuf, 1);    // Restore the prompt plus any type-in on a new line
      state = AT_PROMPT;
      ticks = portMAX_DELAY;
      }
    }
  }
