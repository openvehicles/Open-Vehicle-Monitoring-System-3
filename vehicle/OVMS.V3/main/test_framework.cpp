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
static const char *TAG = "test";

#include <stdio.h>
#include "freertos/FreeRTOSConfig.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_deep_sleep.h"
#include "test_framework.h"
#include "ovms_command.h"

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

void test_alerts(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  extern int TestAlerts;
  TestAlerts = !TestAlerts;
  }

void test_memory(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Implementation removed\n");
  }

void test_tasks(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#if configUSE_TRACE_FACILITY
#ifndef CONFIG_FREERTOS_ASSERT_ON_UNTESTED_FUNCTION
  TaskStatus_t tasks[20];
  writer->printf("Number of Tasks = %u\n", uxTaskGetNumberOfTasks());
  UBaseType_t n = uxTaskGetSystemState(tasks, 20, NULL);
  for (UBaseType_t i = 0; i < n; ++i)
    {
    writer->printf("Task %08X %2u %-15s  Max Stack %5u\n", tasks[i].xHandle, tasks[i].xTaskNumber,
      tasks[i].pcTaskName, tasks[i].usStackHighWaterMark);
    }
#else
  writer->printf("Must not set CONFIG_FREERTOS_ASSERT_ON_UNTESTED_FUNCTION\n");
#endif
#else
  writer->printf("Must set configUSE_TRACE_FACILITY=1 in FreeRTOSConfig.h\n");
#endif
  }

class TestFrameworkInit
  {
  public: TestFrameworkInit();
} MyTestFrameworkInit  __attribute__ ((init_priority (5000)));

TestFrameworkInit::TestFrameworkInit()
  {
  ESP_LOGI(TAG, "Initialising TEST (5000)");

  OvmsCommand* cmd_test = MyCommandApp.RegisterCommand("test","Test framework",NULL);
  cmd_test->RegisterCommand("sleep","Test Deep Sleep",test_deepsleep,"[seconds]",0,1);
  cmd_test->RegisterCommand("alerts","Toggle testing alerts in Housekeeping",test_alerts,"",0,0);
  cmd_test->RegisterCommand("memory","Show allocated memory",test_memory,"",0,0);
  cmd_test->RegisterCommand("tasks","Show list of tasks",test_tasks,"",0,0);
  }
