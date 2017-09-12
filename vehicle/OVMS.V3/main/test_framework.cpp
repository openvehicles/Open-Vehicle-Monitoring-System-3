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
#include <string.h>
#include "freertos/FreeRTOSConfig.h"
#include "freertos/heap_regions_debug.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_deep_sleep.h"
#include "esp_heap_alloc_caps.h"
#include "test_framework.h"
#include "ovms_command.h"
#include "duktape.h"

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

#ifdef configENABLE_MEMORY_DEBUG_DUMP
#define DUMPSIZE 400
#define TOTALSIZE 32

typedef struct
  {
  char task[4];
  int before;
  int after;
  } HeapTask;

class HeapTotals
  {
  public:
    HeapTotals() : count(0) {}
    int begin() { return 0; }
    int end() { return count; }
    HeapTask& operator[](size_t index) { return tasks[index]; };
    void clear() { count = 0; }
    int find(char* task)
    {
    for (int i = 0; i < count; ++i)
      if (strcmp(task, tasks[i].task) == 0)
        return i;
    return -1;
    }
    void append(HeapTask& t)
    {
    if (count < TOTALSIZE)
      {
      tasks[count] = t;
      ++count;
      }
    }
    
  private:
    HeapTask tasks[TOTALSIZE];
    int count;
  };

static mem_dump_block_t before[DUMPSIZE], after[DUMPSIZE];
static size_t numbefore = 0, numafter = 0;
static HeapTotals changes;

static void print_blocks(OvmsWriter* writer, const char* task)
  {
  int count = 0, total = 0;
  bool separate = false;
  for (int i = 0; i < numbefore; ++i)
    {
    if (strcmp(before[i].task, task) != 0)
      continue;
    int j = 0;
    for ( ; j < numafter; ++j)
      if (before[i].address == after[j].address && before[i].size == after[j].size)
        break;
    if (j == numafter)
      {
      writer->printf("- t=%s s=%4d a=%p\n", before[i].task, before[i].size, before[i].address);
      ++count;
      }
    }
  if (count)
    separate = true;
  total += count;
  count = 0;
  for (int i = 0; i < numafter; ++i)
    {
    if (strcmp(after[i].task, task) != 0)
      continue;
    int j = 0;
    for ( ; j < numbefore; ++j)
      if (after[i].address == before[j].address && after[i].size == before[j].size)
        break;
    if (j < numbefore)
      {
      int* p = (int*)after[i].address;
      if (separate)
        {
        writer->printf("----------------------------\n");
        separate = false;
        }
      writer->printf("  t=%s s=%4d a=%p  %08X %08X %08X %08X %08X %08X %08X %08X\n",
        after[i].task, after[i].size, p, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
      ++count;
      }
    }
  if (count)
    separate = true;
  total += count;
  for (int i = 0; i < numafter; ++i)
    {
    if (strcmp(after[i].task, task) != 0)
      continue;
    int j = 0;
    for ( ; j < numbefore; ++j)
      if (after[i].address == before[j].address && after[i].size == before[j].size)
        break;
    if (j == numbefore)
      {
      int* p = (int*)after[i].address;
      if (separate)
        {
        writer->printf("----------------------------\n");
        separate = false;
        }
      writer->printf("+ t=%s s=%4d a=%p  %08X %08X %08X %08X %08X %08X %08X %08X\n",
        after[i].task, after[i].size, p, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
      ++total;
      }
    }
  if (total)
    writer->printf("============================\n");
  }
#endif

void test_memory(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef configENABLE_MEMORY_DEBUG_DUMP
  static const char *ctasks[] = { "TST", "CTe", "CAs", "TRT", "tiT", "wif", 0};
  const char* const* tasks = ctasks;
  if (argc > 0)
    {
    if (**argv == '*')
      tasks = NULL;
    else
      tasks = argv;
    }

  numafter = mem_debug_malloc_dump(0, after, DUMPSIZE);

  changes.clear();
  for (int i = 0; i < numbefore; ++i)
    {
    int index = changes.find(before[i].task);
    if (index >= 0)
      {
      changes[index].before += before[i].size;
      }
    else
      {
      HeapTask task;
      *(int*)task.task = *(int*)before[i].task;
      task.before = before[i].size;
      task.after = 0;
      changes.append(task);
      }
    }
  for (int i = 0; i < numafter; ++i)
    {
    int index = changes.find(after[i].task);
    if (index >= 0)
      {
      changes[index].after += after[i].size;
      }
    else
      {
      HeapTask task;
      *(int*)task.task = *(int*)after[i].task;
      task.before = 0;
      task.after = after[i].size;
      changes.append(task);
      }
    }

  writer->printf("============================\n");
  uint32_t caps = MALLOC_CAP_8BIT;
  writer->printf("Free %zu,  numafter = %d\n", xPortGetFreeHeapSizeCaps(caps), numafter);
  for (int i = changes.begin(); i < changes.end(); ++i)
    {
    int change = changes[i].after - changes[i].before;
    if (change)
      writer->printf("task=%s total=%7d change=%+d\n", changes[i].task, changes[i].after, change);
    }

  writer->printf("============================\n");
  if (tasks)
    {
    while (*tasks)
      {
      print_blocks(writer, *tasks);
      ++tasks;
      }
    }
  else
    {
    for (int i = changes.begin(); i < changes.end(); ++i)
      {
      int change = changes[i].after - changes[i].before;
      if (change)
        print_blocks(writer, changes[i].task);
      }
    }

  for (int i = 0; i < numafter; ++i)
    {
    before[i] = after[i];
    }
  numbefore = numafter;
#else
  writer->printf("Must set CONFIG_ENABLE_MEMORY_DEBUG and have updated IDF with mem_debug_malloc_dump()\n");
#endif
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

void test_javascript(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  duk_context *ctx = duk_create_heap_default();
  duk_eval_string(ctx, "1+2");
  writer->printf("Javascript 1+2=%d\n", (int) duk_get_int(ctx, -1));
  duk_destroy_heap(ctx);
  }

void test_abort(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef configENABLE_MEMORY_DEBUG_DUMP
#if configENABLE_MEMORY_DEBUG_ABORT
  int size = 0;
  int count;
  char task[4] = {0};
  strncpy(task, argv[0], 3);
  count = atoi(argv[1]);
  if (argc == 3)
    {
    size = atoi(argv[2]);
    }
  mem_malloc_set_abort(*(int*)task, size, count);
#else
  writer->printf("Must set configENABLE_MEMORY_DEBUG_ABORT\n");
#endif
#else
  writer->printf("Must set CONFIG_ENABLE_MEMORY_DEBUG and have updated IDF with mem_malloc_set_abort()\n");
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
  cmd_test->RegisterCommand("housekeeping","Toggle testing alerts in Housekeeping",test_alerts,"",0,0);
  cmd_test->RegisterCommand("memory","Show allocated memory",test_memory,"",0);
  cmd_test->RegisterCommand("tasks","Show list of tasks",test_tasks,"",0,0);
  cmd_test->RegisterCommand("javascript","Test Javascript",test_javascript,"",0,0);
  cmd_test->RegisterCommand("abort","Set trap to abort on malloc",test_abort,"<task> <count> [size]",2,3);
  }
