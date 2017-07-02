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
#include <unistd.h>
#include <string.h>
#include <cstdarg>
#include "rom/uart.h"
#include "command.h"
#include "console_async.h"
#include "microrl.h"

void ConsoleAsyncPrint(microrl_t* rl, const char * str)
  {
  fwrite(str,strlen(str),1,stdout);
  fflush(stdout);
  }

int ConsoleAsyncExecute (microrl_t* rl, int argc, const char * const * argv )
  {
  MyCommandApp.Execute(COMMAND_RESULT_VERBOSE, (OvmsWriter*)rl->userdata, argc, argv);
  return 0;
  }

void ConsoleAsyncTask(void *pvParameters)
  {
  ConsoleAsync* me = (ConsoleAsync*)pvParameters;

  vTaskDelay(50 / portTICK_PERIOD_MS);

	while (1) {
		// put received char from stdin to microrl lib
		char ch = uart_rx_one_char_block();
		microrl_insert_char (me->GetRL(), ch);
	  }
  }

ConsoleAsync::ConsoleAsync()
  {
  puts("\n\033[32mWelcome to the Open Vehicle Monitoring System (OVMS) - async console\033[0m");

  microrl_init (&m_rl, ConsoleAsyncPrint);
  m_rl.userdata = (void*)this;
	microrl_set_execute_callback (&m_rl, ConsoleAsyncExecute);

  xTaskCreatePinnedToCore(ConsoleAsyncTask, "ConsoleAsyncTask", 4096, (void*)this, 5, &m_taskid, 1);
  }

ConsoleAsync::~ConsoleAsync()
  {
  }

microrl_t* ConsoleAsync::GetRL()
  {
  return &m_rl;
  }

int ConsoleAsync::puts(const char* s)
  {
  return ::puts(s);
  }

int ConsoleAsync::printf(const char* fmt, ...)
  {
  va_list args;
  va_start(args,fmt);
  vprintf(fmt,args);
  va_end(args);
  fflush(stdout);
  return 0;
  }

ssize_t ConsoleAsync::write(const void *buf, size_t nbyte)
  {
  ssize_t n = fwrite(buf, nbyte, 1, stdout);
  fflush(stdout);
  return n;
  }

void ConsoleAsync::finalise()
  {
  }
