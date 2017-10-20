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
static const char *TAG = "ovms-module";

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOSConfig.h"
#include "freertos/heap_regions_debug.h"
#include "esp_heap_alloc_caps.h"
#include "ovms_module.h"
#include "ovms_command.h"

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

void module_memory(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef configENABLE_MEMORY_DEBUG_DUMP
  static const char *ctasks[] = {0}; //{ "TST", "CTe", "CAs", "TRT", "tiT", "wif", 0};
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

void module_tasks(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#if configUSE_TRACE_FACILITY
#ifndef CONFIG_FREERTOS_ASSERT_ON_UNTESTED_FUNCTION
#define MAX_TASKS 30
  TaskStatus_t tasks[MAX_TASKS];
  bzero(tasks, sizeof(tasks));
  UBaseType_t num = uxTaskGetNumberOfTasks();
  writer->printf("Number of Tasks =%3u%s   Stack:  Now   Max Total\n", num,
    num > MAX_TASKS ? ">max" : "    ");
    
  UBaseType_t n = uxTaskGetSystemState(tasks, MAX_TASKS, NULL);
  num = 0;
  for (UBaseType_t j = 0; j < n; )
    {
    for (UBaseType_t i = 0; i < n; ++i)
      {
      if (tasks[i].xTaskNumber == num)
        {
        uint32_t total = (uint32_t)tasks[i].pxStackBase >> 16;
        writer->printf("Task %08X %2u %-15s %5u %5u %5u\n", tasks[i].xHandle, tasks[i].xTaskNumber,
          tasks[i].pcTaskName, total - ((uint32_t)tasks[i].pxStackBase & 0xFFFF),
          total - tasks[i].usStackHighWaterMark, total);
        ++j;
        break;
        }
      }
    ++num;
    }
#else
  writer->printf("Must not set CONFIG_FREERTOS_ASSERT_ON_UNTESTED_FUNCTION\n");
#endif
#else
  writer->printf("Must set configUSE_TRACE_FACILITY=1 in FreeRTOSConfig.h\n");
#endif
  }

void module_abort(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void module_task(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  }

class OvmsModuleInit
  {
  public: OvmsModuleInit();
} MyOvmsModuleInit  __attribute__ ((init_priority (5100)));

OvmsModuleInit::OvmsModuleInit()
  {
  ESP_LOGI(TAG, "Initialising MODULE (5100)");

  OvmsCommand* cmd_module = MyCommandApp.RegisterCommand("module","Module framework",NULL);
  cmd_module->RegisterCommand("memory","Show module memory usage",module_memory,"",0,0,true);
  cmd_module->RegisterCommand("tasks","Show module task usage",module_tasks,"",0,0,true);
  //cmd_module->RegisterCommand("task","Show module task detail",module_task,"<taskid>",1,1,true);
  cmd_module->RegisterCommand("abort","Set trap to abort on malloc",module_abort,"<task> <count> [<size>]",2,3,true);
  }

// Returns the value of the stack pointer in the calling function.
// The stack frame for this function is 32 bytes, hence the add.

void* stack() {
    __asm__(
"   addi a2, sp, 32\n"
"   retw.n\n"
    );
    return 0;
}
