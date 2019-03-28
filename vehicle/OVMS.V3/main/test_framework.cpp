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
#include <stdlib.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_sleep.h"
#include "test_framework.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "ovms_script.h"
#include "metrics_standard.h"
#include "ovms_config.h"
#include "can.h"
#include "strverscmp.h"

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
  writer->printf("Javascript 1+2=%d\n", MyScripts.DuktapeEvalIntResult("1+2"));
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

#ifdef CONFIG_OVMS_COMP_SDCARD
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
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

// Spew lines of the ASCII printable characters in the style of RFC 864.
void test_chargen(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int numlines = 1000;
  int delay = 0;
  if (argc>=1)
    {
    numlines = atoi(argv[0]);
    }
  if (argc>=2)
    {
    delay = atoi(argv[1]);
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
    if (writer->write(buf, 73) <= 0)
      break;
    if (delay)
      vTaskDelay(delay/portTICK_PERIOD_MS);
    if (++start == 0x7F)
        start = ' ';
    }
  }

bool test_echo_insert(OvmsWriter* writer, void* ctx, char ch)
  {
  if (ch == '\n')
    return false;
  writer->write(&ch, 1);
  return true;
  }

void test_echo(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Type characters to be echoed, end with newline.");
  writer->RegisterInsertCallback(test_echo_insert, NULL);
  }

void test_watchdog(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Spinning now (watchdog should fire in a few minutes)");
  for (;;) {}
  writer->puts("Error: We should never get here");
  }

void test_realloc(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  void* buf;
  void *interfere = NULL;

  writer->puts("First check heap integrity...");
  heap_caps_check_integrity_all(true);

  writer->puts("Now allocate 4KB RAM...");
  buf = ExternalRamMalloc(4096);

  writer->puts("Check heap integrity...");
  heap_caps_check_integrity_all(true);

  writer->puts("Now re-allocate bigger, 1,000 times...");
  for (int k=1; k<1001; k++)
    {
    buf = ExternalRamRealloc(buf, 4096+k);
    if (interfere == NULL)
      {
      interfere = ExternalRamMalloc(1024);
      }
    else
      {
      free(interfere);
      interfere = NULL;
      }
    }

  writer->puts("Check heap integrity...");
  heap_caps_check_integrity_all(true);

  writer->puts("Now re-allocate smaller, 1,000 times...");
  for (int k=1001; k>0; k--)
    {
    buf = ExternalRamRealloc(buf, 4096+k);
    if (interfere == NULL)
      {
      interfere = ExternalRamMalloc(1024);
      }
    else
      {
      free(interfere);
      interfere = NULL;
      }
    }

  writer->puts("Check heap integrity...");
  heap_caps_check_integrity_all(true);

  writer->puts("And free the buffer...");
  free(buf);
  if (interfere != NULL) free(interfere);

  writer->puts("Final check of heap integrity...");
  heap_caps_check_integrity_all(true);
  }

void test_spiram(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Metrics (%p) are in %s RAM (%d bytes for a base metric)\n",
    StandardMetrics.ms_m_version,
    (((unsigned int)StandardMetrics.ms_m_version >= 0x3f800000)&&
     ((unsigned int)StandardMetrics.ms_m_version <= 0x3fbfffff))?
     "SPI":"INTERNAL",
     sizeof(OvmsMetric));
  }

void test_strverscmp(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int c = strverscmp(argv[0],argv[1]);
  writer->printf("%s %s %s\n",
    argv[0],
    (c<0)?"<":((c==0)?"=":">"),
    argv[1]
    );
  }

void test_can(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int64_t started = esp_timer_get_time();
  int64_t elapsed;
  bool tx = (strcmp(cmd->GetName(), "cantx")==0);

  int frames = 1000;
  if (argc>1) frames = atoi(argv[1]);

  canbus *can;
  if (argc>0)
    can = (canbus*)MyPcpApp.FindDeviceByName(argv[0]);
  else
    can = (canbus*)MyPcpApp.FindDeviceByName("can1");
  if (can == NULL)
    {
    writer->puts("Error: Cannot find specified can bus");
    return;
    }

  writer->printf("Testing %d frames on %s\n",frames,can->GetName());

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));
  frame.origin = can;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;

  for (int k=0;k<frames;k++)
    {
    frame.MsgID = (rand()%64)+256;
    if (tx)
      can->Write(&frame, pdMS_TO_TICKS(10));
    else
      MyCan.IncomingFrame(&frame);
    }

  elapsed = esp_timer_get_time() - started;
  int uspt = elapsed / frames;
  writer->printf("Transmitted %d frames in %lld.%06llds = %dus/frame\n",
    frames, elapsed / 1000000, elapsed % 1000000, uspt);
  }

void test_mkstemp(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int fd1, e1, fd2, e2;
  char tn1[100], tn2[100];
  snprintf(tn1, sizeof(tn1), "%s.XXXXXX", argv[0]);
  snprintf(tn2, sizeof(tn2), "%s.XXXXXX", argv[0]);
  errno = 0;
  fd1 = mkstemp(tn1); e1 = errno;
  fd2 = mkstemp(tn2); e2 = errno;
  writer->printf("tempfile 1: '%s' => fd=%d error='%s'\n", tn1, fd1, (fd1<0) ? strerror(e1) : "-");
  writer->printf("tempfile 2: '%s' => fd=%d error='%s'\n", tn2, fd2, (fd2<0) ? strerror(e2) : "-");
  if (fd1 >= 0) { close(fd1); unlink(tn1); }
  if (fd2 >= 0) { close(fd2); unlink(tn2); }
  }

void test_string(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int loopcnt = atoi(argv[0]);
  int mode = (argc > 1) ? atoi(argv[1]) : 1;
  int stdlen = 0, errcnt = 0;
  size_t pos;
  OvmsMetric* m;
  const char* t5 = "0123456789-abcdefghijk-";
  std::string t6(t5);

  for (int j = 1; j <= loopcnt; j++)
    {
    std::string msg;
    msg.reserve(2048);
    for (m = MyMetrics.m_first; msg.size() < 1024; m = m->m_next ? m->m_next : MyMetrics.m_first)
      {
      if (mode == 1)
        msg += m->AsJSON();
      else if (mode == 2)
        msg += m->AsString();
      else if (mode == 3)
        msg += m->m_name;
      else if (mode == 4)
        msg += MyConfig.GetParamValue("module", "cfgversion", m->m_name);
      else if (mode == 5)
        msg += t5;
      else if (mode == 6)
        msg += t6;

      // check for NUL bytes:
      if ((pos = msg.rfind('\0')) != std::string::npos)
        {
        writer->printf("loop #%d: corruption detected at pos=%u size=%u\n%s\n\n", j, pos, msg.size(), msg.c_str());
        if (++errcnt == 20)
          {
          writer->puts("too many errors, abort");
          return;
          }
        break;
        }
      }

    // display reference:
    if (msg.size() > stdlen)
      {
      stdlen = msg.size();
      writer->printf("#%d: stdlen = %d\n%s\n\n", j, stdlen, msg.c_str());
      }
    }
  writer->puts("finished");
  }

class TestFrameworkInit
  {
  public: TestFrameworkInit();
} MyTestFrameworkInit  __attribute__ ((init_priority (5000)));

TestFrameworkInit::TestFrameworkInit()
  {
  ESP_LOGI(TAG, "Initialising TEST (5000)");

  OvmsCommand* cmd_test = MyCommandApp.RegisterCommand("test","Test framework");
  cmd_test->RegisterCommand("sleep","Test Deep Sleep",test_deepsleep,"[<seconds>]",0,1);
#ifdef CONFIG_OVMS_COMP_SDCARD
  cmd_test->RegisterCommand("sdcard","Test CD CARD",test_sdcard);
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD
  cmd_test->RegisterCommand("javascript","Test Javascript",test_javascript);
  cmd_test->RegisterCommand("chargen","Character generator",test_chargen,"[<#lines>] [<delay_ms>]",0,2);
  cmd_test->RegisterCommand("echo", "Test getchar", test_echo);
  cmd_test->RegisterCommand("watchdog", "Test task spinning (and watchdog firing)", test_watchdog);
  cmd_test->RegisterCommand("realloc", "Test memory re-allocations", test_realloc);
  cmd_test->RegisterCommand("spiram", "Test SPI RAM memory usage", test_spiram);
  cmd_test->RegisterCommand("strverscmp", "Test strverscmp function", test_strverscmp, "", 2, 2);
  cmd_test->RegisterCommand("cantx", "Test CAN bus transmission", test_can, "[<port>] [<number>]", 0, 2);
  cmd_test->RegisterCommand("canrx", "Test CAN bus reception", test_can, "[<port>] [<number>]", 0, 2);
  cmd_test->RegisterCommand("mkstemp", "Test mkstemp function", test_mkstemp, "<file>", 1, 1);
  cmd_test->RegisterCommand("string", "Test std::string memory corruption", test_string, "<loopcnt> <mode>\n"
    "mode: 1=m.AsJSON, 2=m.AsString, 3=m.name, 4=const cfg string, 5=const local cstr, 6=const local string", 2, 2);
  }
