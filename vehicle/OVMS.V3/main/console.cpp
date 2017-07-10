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

OvmsConsole::OvmsConsole()
  {
  }

OvmsConsole::~OvmsConsole()
  {
  }

void OvmsConsole::Initialize(const char* console)
  {
  printf("\n\033[32mWelcome to the Open Vehicle Monitoring System (OVMS) - %s\033[0m", console);
  microrl_init (&m_rl, Print);
  m_rl.userdata = (void*)this;
  microrl_set_complete_callback(&m_rl, Complete);
  microrl_set_execute_callback(&m_rl, Execute);
  ProcessChar('\n');
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
