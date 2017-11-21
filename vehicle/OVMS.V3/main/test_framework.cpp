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
static const char *TAG = "test";

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_deep_sleep.h"
#include "test_framework.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "ovms_script.h"

void test_deepsleep(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int sleeptime = 60;
  if (argc==1)
    {
    sleeptime = atoi(argv[0]);
    }
  writer->puts("Entering deep sleep...");
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  esp_deep_sleep(1000000LL * sleeptime);
  }

void test_javascript(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE
  writer->puts("No javascript engine enabled");
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  duk_context *ctx = MyScripts.Duktape();
  duk_eval_string(ctx, "1+2");
  writer->printf("Javascript 1+2=%d\n", (int) duk_get_int(ctx, -1));
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

void test_sdcard(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  sdcard *sd = MyPeripherals->m_sdcard;

  if (!sd->isinserted())
    {
    writer->puts("Error: No SD CARD inserted");
    return;
    }
  if (!sd->ismounted())
    {
    writer->puts("Error: SD CARD not mounted");
    return;
    }

  unlink("/sd/ovmstest.txt");
  char buffer[512];
  memset(buffer,'A',sizeof(buffer));

  FILE *fd = fopen("/sd/ovmstest.txt","w");
  if (fd == NULL)
    {
    writer->puts("Error: /sd/ovmstest.txt could not be opened for writing");
    return;
    }

  writer->puts("SD CARD test starts...");
  for (int k=0;k<2048;k++)
    {
    fwrite(buffer, sizeof(buffer), 1, fd);
    if ((k % 128)==0)
      writer->printf("SD CARD written %d/%d\n",k,2048);
    }
  fclose(fd);

  writer->puts("Cleaning up");
  unlink("/sd/ovmstest.txt");

  writer->puts("SD CARD test completes");
  }

// Spew lines of the ASCII printable characters in the style of RFC 864.
void test_chargen(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int numlines = 1000;
  if (argc==1)
    {
    numlines = atoi(argv[0]);
    }
  char buf[74];
  buf[72] = '\n';
  buf[73] = '\0';
  char start = '!';
  for (int line = 0; line < numlines; ++line)
    {
    char ch = start;
    for (int col = 0; col < 72; ++col)
      {
      buf[col] = ch;
      if (++ch == 0x7F)
        ch = ' ';
      }
    writer->write(buf, 73);
    if (++start == 0x7F)
        start = ' ';
    }
  }

class TestFrameworkInit
  {
  public: TestFrameworkInit();
} MyTestFrameworkInit  __attribute__ ((init_priority (5000)));

TestFrameworkInit::TestFrameworkInit()
  {
  ESP_LOGI(TAG, "Initialising TEST (5000)");

  OvmsCommand* cmd_test = MyCommandApp.RegisterCommand("test","Test framework",NULL,"",0,0,true);
  cmd_test->RegisterCommand("sleep","Test Deep Sleep",test_deepsleep,"[<seconds>]",0,1,true);
  cmd_test->RegisterCommand("sdcard","Test CD CARD",test_sdcard,"",0,0,true);
  cmd_test->RegisterCommand("javascript","Test Javascript",test_javascript,"",0,0,true);
  cmd_test->RegisterCommand("chargen","Character generator [<#lines>]",test_chargen,"",0,1,false);
  }
