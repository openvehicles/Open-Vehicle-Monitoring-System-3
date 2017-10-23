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
#include "esp_log.h"
#include "ovms_console.h"
#include "log_buffers.h"

//static const char *TAG = "Console";
static char CRbuf[1] = { '\r' };
static char NLbuf[1] = { '\n' };
static char ctrlRbuf[1] = { 'R'-0100 };

char ** Complete (microrl_t* rl, int argc, const char * const * argv )
  {
  return MyCommandApp.Complete((OvmsWriter*)rl->userdata, argc, argv);
  }

OvmsConsole::OvmsConsole(Parent* parent)
  : OvmsShell(COMMAND_RESULT_VERBOSE), TaskBase(parent)
  {
  m_ready = false;
  }

OvmsConsole::~OvmsConsole()
  {
  m_ready = false;
  MyCommandApp.DeregisterConsole(this);
  }

void OvmsConsole::Initialize(const char* console)
  {
  printf("\n\033[32mWelcome to the Open Vehicle Monitoring System (OVMS) - %s Console\033[0m", console);
  OvmsShell::Initialize(true);
  microrl_set_complete_callback(&m_rl, Complete);
  ProcessChar('\n');

  MyCommandApp.RegisterConsole(this);
  m_ready = true;
  }

char ** OvmsConsole::GetCompletion(OvmsCommandMap& children, const char* token)
  {
  unsigned int index = 0;
  if (token)
    {
    for (OvmsCommandMap::iterator it = children.begin(); it != children.end(); ++it)
      {
      const char *key = it->first;
      if (strncmp(key, token, strlen(token)) == 0)
        {
        if (index < COMPLETION_MAX_TOKENS+1)
          {
          if (it->second->IsSecure() && !m_issecure)
            continue;
          if (index == COMPLETION_MAX_TOKENS)
            key = "...";
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

void OvmsConsole::Log(LogBuffers* message)
  {
  if (!m_ready)
    {
    message->release();
    return;
    }
  Event event;
  event.type = ALERT_MULTI;
  event.multi = message;
  BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
  if (ret != pdPASS)
    {
    message->release();
//    ESP_LOGI(TAG, "Timeout queueing message in Console::Log\n");
    }
  }

typedef enum
  {
  AT_PROMPT,
  AWAITING_NL,
  NO_NL
  } DisplayState;

void OvmsConsole::Service()
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
      if (event.type <= RECV)
        {
        if (state != AT_PROMPT)
          ProcessChars(ctrlRbuf, 1);    // Restore the prompt plus any type-in on a new line
        state = AT_PROMPT;
        HandleDeviceEvent(&event);
        continue;
        }
      // We remove the newline from the end of a log message so that we can later
      // output a newline as part of restoring the command prompt and its line
      // without leaving a blank line above it.  So before we display a new log
      // message we need to output a newline if the last action was displaying a
      // log message, or output a carriage return to back over the prompt.
      if (event.type != ALERT_MULTI || event.multi->begin() != event.multi->end())
        {
        if (state == AWAITING_NL)
          write(NLbuf, 1);
        else if (state == AT_PROMPT)
          write(CRbuf, 1);
        char* buffer;
        size_t len;
        if (event.type == ALERT_MULTI)
          {
          LogBuffers::iterator before = event.multi->begin(), after;
          while (true)
            {
            buffer = *before;
            len = strlen(buffer);
            after = before;
            ++after;
            if (after == event.multi->end())
              break;
            write(buffer, len);
            before = after;
            }
          }
        else
          {
          buffer = event.buffer;
          len = strlen(buffer);
          }
        if (buffer[len-1] == '\n')
          {
          buffer[--len] = '\0';
          if (buffer[len-1] == '\r')  // Remove CR, too, in case of \r\n
            buffer[--len] = '\0';
          state = AWAITING_NL;
          write(buffer, len);
          }
        else
          {
          state = NO_NL;
          write(buffer, len);
          }
        }
      if (event.type == ALERT_MULTI)
        event.multi->release();
      else
        free(event.buffer);
      ticks = 200 / portTICK_PERIOD_MS;
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
