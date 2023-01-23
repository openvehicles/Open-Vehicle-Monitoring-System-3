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
#include "ovms_log.h"
#include "ovms_console.h"
#include "ovms_version.h"
#include "log_buffers.h"

//static const char *TAG = "Console";
static char CRbuf[4] = { '\r', '\033', '[', 'K' };
static char NLbuf[2] = { '\r', '\n' };
static char ctrlRbuf[1] = { 'R'-0100 };

char ** Complete (microrl_t* rl, int argc, const char * const * argv )
  {
  return MyCommandApp.Complete((OvmsWriter*)rl->userdata, argc, argv);
  }

OvmsConsole::OvmsConsole()
  : OvmsShell(COMMAND_RESULT_VERBOSE)
  {
  m_ready = false;
  m_deferred = NULL;
  m_discarded = 0;
  m_state = AT_PROMPT;
  m_lost = m_acked = 0;
  }

OvmsConsole::~OvmsConsole()
  {
  m_ready = false;
  MyCommandApp.DeregisterConsole(this);
  }

void OvmsConsole::Initialize(const char* console)
  {
  OvmsShell::Initialize(console != NULL);
  microrl_set_complete_callback(&m_rl, Complete);
  if (console)
    {
    printf("\nWelcome to the Open Vehicle Monitoring System (OVMS) - %s Console\n", console);
    printf("Firmware: %s\nHardware: %s\n",GetOVMSVersion().c_str(),GetOVMSHardware().c_str());
    ProcessChar('\n');
    MyCommandApp.RegisterConsole(this);
    }
  m_ready = true;
  }

char** OvmsConsole::SetCompletion(int index, const char* token)
  {
  if (index < COMPLETION_MAX_TOKENS+1)
    {
    if (index == COMPLETION_MAX_TOKENS)
      token = "...";
    if (token)
      {
      strncpy(m_space[index], token, TOKEN_MAX_LENGTH-1);
      m_space[index][TOKEN_MAX_LENGTH-1] = '\0';
      m_completions[index] = m_space[index];
      ++index;
      }
    m_completions[index] = NULL;
    }
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
  BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, 0);
  if (ret != pdPASS)
    {
    message->release();
    ++m_lost;
    }
  }

void OvmsConsole::Service()
  {
  vTaskDelay(50 / portTICK_PERIOD_MS);

  for (;;)
    {
    Poll(portMAX_DELAY);
    }
  }

void OvmsConsole::Poll(portTickType ticks, QueueHandle_t queue)
  {
  static Event event;

  if (!queue)
    queue = m_queue;
  for (;;)
    {
    // Waiting for UART RX event or async log message event
    if (xQueueReceive(queue, (void*)&event, ticks))
      {
      if (event.type <= RECV)
        {
        if (m_state != AT_PROMPT)
          ProcessChars(ctrlRbuf, 1);    // Restore the prompt plus any type-in on a new line
        m_state = AT_PROMPT;
        HandleDeviceEvent(&event);
        continue;
        }
      // While a command that takes input is executing, put alert events into a
      // separate "deferred" queue.  If that queue fills, keep only the last N
      // events and count those discarded.
      if (m_insert)
        {
        if (!m_deferred)
          m_deferred = xQueueCreate(CONFIG_OVMS_HW_CONSOLE_QUEUE_SIZE, sizeof(Event));
        if (xQueueSendToBack(m_deferred, (void *)&event, 0) != pdPASS)
          {
          Event discard;
          xQueueReceive(m_deferred, (void*)&discard, 0);
          xQueueSendToBack(m_deferred, (void *)&event, 0);
          if (discard.type == ALERT_MULTI)
            discard.multi->release();
          else
            free(discard.buffer);
          ++m_discarded;
          }
        continue;
        }
      // We remove the newline from the end of a log message so that we can later
      // output a newline as part of restoring the command prompt and its line
      // without leaving a blank line above it.  So before we display a new log
      // message we need to output a newline if the last action was displaying a
      // log message, or output a carriage return to back over the prompt.
      if (m_monitoring &&
        (event.type != ALERT_MULTI || event.multi->begin() != event.multi->end()))
        {
        if (m_state == AWAITING_NL)
          write(NLbuf, 2);
        else if (m_state == AT_PROMPT)
          write(CRbuf, 4);
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
          --len;
          if (buffer[len-1] == '\r')  // Omit CR, too, in case of \r\n
            --len;
          m_state = AWAITING_NL;
          write(buffer, len);
          }
        else
          {
          m_state = NO_NL;
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
      unsigned int lost = m_lost - m_acked;     // Modulo 2^32 arithmetic
      if (lost > 0)
        {
        if (m_state == AWAITING_NL)
          write(NLbuf, 2);
        else if (m_state == AT_PROMPT)
          write(CRbuf, 4);
        printf("\033[33m[%d log messages lost]\033[0m", lost);
        m_state = AWAITING_NL;
        m_acked += lost;
        continue;
        }
      if (m_state != AT_PROMPT)
        ProcessChars(ctrlRbuf, 1);    // Restore the prompt plus any type-in on a new line
      m_state = AT_PROMPT;
      return;
      }
    }
  }

void OvmsConsole::finalise()
  {
  if (m_deferred)
    {
    if (m_discarded)
      {
      printf("\033[33m[%d log messages discarded]\033[0m", m_discarded);
      m_state = AWAITING_NL;
      m_discarded = 0;
      }
    Poll(0, m_deferred);
    vQueueDelete(m_deferred);
    m_deferred = NULL;
    }
  }
