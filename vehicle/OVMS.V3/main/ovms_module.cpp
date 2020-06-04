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
static const char *TAG = "module";

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/FreeRTOSConfig.h"
#include "esp_heap_caps.h"
#include <esp_system.h>
#include "ovms_module.h"
#include "ovms_peripherals.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#ifdef CONFIG_HEAP_TASK_TRACKING
#include "esp_heap_task_info.h"
#endif
#include "ovms_boot.h"
#include "ovms_mutex.h"
#include "ovms_notify.h"
#include "string_writer.h"

#define MAX_TASKS 30
#define DUMPSIZE 1000
#define NUMTASKS 32
#define NAMELEN 16
#define TASKLIST 10
#define NOT_FOUND (TaskHandle_t)0xFFFFFFFF

#ifndef CONFIG_HEAP_TASK_TRACKING
#define NOGO 1
#endif
#if configUSE_TRACE_FACILITY==0
#define NOGO 1
#endif
#ifdef CONFIG_FREERTOS_ASSERT_ON_UNTESTED_FUNCTION
#define NOGO 1
#endif

#ifdef NOGO
static void must(OvmsWriter* writer)
  {
  writer->printf("To use these debugging tools, must set CONFIG_HEAP_TASK_TRACKING=y\n");
  writer->printf("and have updated openvehicles/esp-idf\n");
  }

static void module_memory(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  must(writer);
  }

static void module_tasks(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  must(writer);
  }

static void module_tasks_data(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  must(writer);
  }

static void module_check(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  must(writer);
  }

void AddTaskToMap(TaskHandle_t task) {}

#else

enum region_types {
    DRAM = 0,
    D_IRAM = 1,
    IRAM = 2,
    SPIRAM = 3,
    NUM_REGIONS = 4
};
#if NUM_HEAP_TASK_CAPS < NUM_REGIONS
#error NUM_HEAP_TASK_CAPS must be >= NUM_REGIONS
#endif

class HeapTotals;

static heap_task_info_params_t* params = NULL;
static TaskHandle_t* tasklist = NULL;
static TaskStatus_t* taskstatus = NULL;
static uint32_t taskstatus_cnt = 0;
static uint32_t taskstatus_time = 0;
static uint32_t totalruntime = 0;
static OvmsMutex taskstatus_mutex;
static heap_task_block_t* before = NULL;
static heap_task_block_t* after = NULL;
static size_t numbefore = 0, numafter = 0;
static HeapTotals* changes = NULL;
static const char* states[] = {"Run", "Rdy", "Blk", "Sus", "Del"};


class Name
  {
  public:
    inline Name() {}
    inline Name(const char *name)
      {
      for (int i = 0; i < NAMELEN/4; ++i)
        words[i] = 0;
      strncpy(bytes, name, NAMELEN-1);
      }
    inline Name(const Name& name)
      {
      for (int i = 0; i < NAMELEN/4; ++i)
        words[i] = name.words[i];
      bytes[NAMELEN-1] = '\0';
      }
    inline bool operator==(Name& a)
      {
      for (int i = 0; i < NAMELEN/4 - 1; ++i)
        if (a.words[i] != words[i]) return false;
      if (a.words[NAMELEN/4-1] != (words[NAMELEN/4-1] & 0x7FFFFFFF)) return false;
      return true;
      }
    inline char operator[](int i) { return (words[i/4] >> ((i & 3) * 8)) & 0xFF; }
    inline void set(int i, char c)
      {
      words[i/4] &= ~(0xFF << ((i & 3) * 8));
      words[i/4] |= c << ((i & 3) * 8);
      }
  public:
    union
      {
      char bytes[NAMELEN];
      int words[NAMELEN/4];
      };
  };


class TaskMap
  {
  private:
    TaskMap() : count(0) {}
    typedef struct
      {
      TaskHandle_t id;
      Name name;
      } TaskPair;

  public:
    inline static TaskMap* instance()
      {
      if (!taskmap)
        {
        void* p = heap_caps_malloc(sizeof(TaskMap), MALLOC_CAP_32BIT);
        if (!p)
          return NULL;
        taskmap = new(p) TaskMap;
        }
      return taskmap;
      }
    bool insert(TaskHandle_t taskid, const char* name)
      {
      int i;
      for (i = 0; i < count; ++i)
        {
        if (map[i].id == taskid)
          break;
        }
      if (i == count)
        {
        if (count == NUMTASKS)
          return false;
        ++count;
        }
      map[i].id = taskid;
      map[i].name = Name(name);
      return true;
      }
    bool find(TaskHandle_t taskid, Name& name)
      {
      for (int i = 0; i < count; ++i)
        {
        if (map[i].id == taskid)
          {
          name = map[i].name;
          return true;
          }
        }
      sprintf(name.bytes, "%08X*", (unsigned int)taskid);
      name.bytes[NAMELEN-1] = 0x80;
      return false;
      }
    TaskHandle_t find(Name& name)
      {
      for (int i = 0; i < count; ++i)
        {
        if (map[i].name == name)
          return map[i].id;
        }
      char* end;
      TaskHandle_t task = (TaskHandle_t)strtoul(name.bytes, &end, 16);
      if (*end == '\0')
        return task;
      return NOT_FOUND;
      }
    UBaseType_t populate()
      {
      for (int i = 0; i < count; ++i)
        {
        int j = 0;
        for ( ; j < NAMELEN-2; ++j)
          {
          if (map[i].name[j] == '*' || map[i].name[j] == '\0')
            break;
          }
        map[i].name.set(j++, '*');
        map[i].name.set(j, '\0');
        map[i].name.words[NAMELEN/4-1] |= 0x80000000;
        }
      taskstatus_cnt = uxTaskGetSystemState(taskstatus, MAX_TASKS, &totalruntime);
      taskstatus_time = xTaskGetTickCount() / configTICK_RATE_HZ;
      for (UBaseType_t i = 0; i < taskstatus_cnt; ++i)
        {
        insert(taskstatus[i].xHandle, taskstatus[i].pcTaskName);
        }
      return taskstatus_cnt;
      }
    bool zero(TaskHandle_t taskid)
      {
      for (int i = 0; i < count; ++i)
        {
        if (map[i].id == taskid)
          {
          if (map[i].name.words[NAMELEN/4-1] > 0)
            return false;
          for (++i ; i < count; ++i)
            {
            map[i-1] = map[i];
            }
          --count;
          return true;
          }
        }
      return false;
      }
    void dump()
      {
      for (int i = 0; i < count; ++i)
        {
        Name name = map[i].name;
        ::printf("taskmap %d %p %.15s\n", i, map[i].id, name.bytes);
        }
      }

  private:
    static TaskMap* taskmap;
    int count;
    TaskPair map[NUMTASKS];
  };
TaskMap* TaskMap::taskmap = NULL;


class FreeHeap
  {
  public:
    inline FreeHeap()
      {
      m_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
      m_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT|MALLOC_CAP_INTERNAL) - m_free_8bit;
      m_free_spi = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
      if (!m_total_8bit)
        {
        m_total_8bit = TotalSize(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
        m_total_32bit = TotalSize(MALLOC_CAP_32BIT|MALLOC_CAP_INTERNAL) - FreeHeap::m_total_8bit;
        m_total_spi = TotalSize(MALLOC_CAP_SPIRAM);
        }
      }
    size_t Free8bit() { return m_free_8bit; }
    size_t Free32bit() { return m_free_32bit; }
    size_t FreeSPI() { return m_free_spi; }
    size_t Total8bit() { return m_total_8bit; }
    size_t Total32bit() { return m_total_32bit; }
    size_t TotalSPI() { return m_total_spi; }

  private:
    size_t TotalSize(uint32_t caps)
    {
    multi_heap_info_t info;
    heap_caps_get_info(&info, caps);
    size_t total = info.total_free_bytes + info.total_allocated_bytes +
      info.allocated_blocks*20 + info.free_blocks*4;
    return total;
    }

  private:
    size_t m_free_8bit;
    size_t m_free_32bit;
    size_t m_free_spi;
    static size_t m_total_8bit;
    static size_t m_total_32bit;
    static size_t m_total_spi;
  };
size_t FreeHeap::m_total_8bit = 0;
size_t FreeHeap::m_total_32bit = 0;
size_t FreeHeap::m_total_spi = 0;


class HeapTask
  {
  public:
    HeapTask()
      {
      totals.task = 0;
      for (int i = 0; i < NUM_HEAP_TASK_CAPS; ++i)
        totals.size[i] = 0;
      }
    heap_task_totals_t totals;
  };


class HeapTotals
  {
  public:
    HeapTotals() : count(0) {}
    int begin() { return 0; }
    int end() { return count; }
    heap_task_totals_t* array() { return &after[0].totals; }
    size_t* size() { return (size_t*)&count; }
    TaskHandle_t Task(int task) { return after[task].totals.task; }
    int Before(int task, int type) { return before[task].totals.size[type]; }
    int After(int task, int type) { return after[task].totals.size[type]; }
    void transfer()
      {
      TaskMap* tm = TaskMap::instance();
      int k = 0;
      for (int i = 0; i < count; ++i)
        {
        if (tm && After(i, DRAM) == 0 && After(i, D_IRAM) == 0 &&
          After(i, IRAM) == 0 && After(i, SPIRAM) == 0)
          {
          tm->zero(Task(i));
          continue;
          }
        before[k].totals.task = after[i].totals.task;
        for (int j = 0; j < NUM_HEAP_TASK_CAPS; ++j)
          before[k].totals.size[j] = after[i].totals.size[j];
        ++k;
        }
      count = k;
      }
    int find(TaskHandle_t task)
      {
      for (int i = 0; i < count; ++i)
        if (task == after[i].totals.task)
          return i;
      return -1;
      }
    void append(HeapTask& t)
      {
      if (count < NUMTASKS)
        {
        after[count] = t;
        ++count;
        }
      }

  private:
    HeapTask before[NUMTASKS];
    HeapTask after[NUMTASKS];
    int count;
  };


void AddTaskToMap(TaskHandle_t task)
  {
  TaskMap::instance()->insert(task, pcTaskGetTaskName(task));
  }


static void print_blocks(OvmsWriter* writer, TaskHandle_t task, bool leaks)
  {
  int count = 0, total = 0, size = 0;;
  bool separate = false;
  Name name;
  TaskMap* tm = TaskMap::instance();
  char pbuf[32];
  tm->find(task, name);
  for (int i = 0; i < numbefore; ++i)
    {
    if (before[i].task != task)
      continue;
    int j = 0;
    for ( ; j < numafter; ++j)
      if (before[i].address == after[j].address && before[i].size == after[j].size)
        break;
    if (j == numafter)
      {
      writer->printf("- t=%.15s s=%4d a=%p\n", name.bytes,
        before[i].size, before[i].address);
      ++count;
      }
    }
  if (count)
    separate = true;
  total += count;
  size = 0;
  count = 0;
  for (int i = 0; i < numafter; ++i)
    {
    if (after[i].task != task)
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
      if (!leaks)
        {
        for (int pi=0; pi<32; pi++)
          pbuf[pi] = isprint(((char*)p)[pi]) ? ((char*)p)[pi] : '.';
        writer->printf("  t=%.15s s=%4d a=%p  %08X %08X %08X %08X %08X %08X %08X %08X | %-32.32s\n",
          name.bytes, after[i].size, p, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], pbuf);
        }
      size += after[i].size;
      ++count;
      }
    }
  if (count)
    {
    separate = true;
    if (leaks)
      writer->printf("  t=%.15s s=%4d in %d blocks still allocated\n", name.bytes, size, count);
    }
  total += count;
  size = 0;
  count = 0;
  for (int i = 0; i < numafter; ++i)
    {
    if (after[i].task != task)
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
      if (numbefore > 0 || !leaks)
        {
        for (int pi=0; pi<32; pi++)
          pbuf[pi] = isprint(((char*)p)[pi]) ? ((char*)p)[pi] : '.';
        writer->printf("+ t=%.15s s=%4d a=%p  %08X %08X %08X %08X %08X %08X %08X %08X | %-32.32s\n",
          name.bytes, after[i].size, p, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], pbuf);
        }
      size += after[i].size;
      ++count;
      }
    }
  if (numbefore == 0 && leaks && count)
    writer->printf("  t=%.15s s=%d in %d blocks allocated\n", name.bytes, size, count);
  total += count;
  if (total)
    writer->printf("============================\n");
  }


static bool allocate()
  {
  if (!before)
    {
    before = (heap_task_block_t*)heap_caps_malloc(sizeof(heap_task_block_t)*DUMPSIZE, MALLOC_CAP_32BIT);
    after = (heap_task_block_t*)heap_caps_malloc(sizeof(heap_task_block_t)*DUMPSIZE, MALLOC_CAP_32BIT);
    taskstatus = (TaskStatus_t*)heap_caps_malloc(sizeof(TaskStatus_t)*MAX_TASKS, MALLOC_CAP_32BIT);
    tasklist = (TaskHandle_t*)heap_caps_malloc(sizeof(TaskHandle_t)*TASKLIST, MALLOC_CAP_32BIT);
    params = (heap_task_info_params_t*)heap_caps_malloc(sizeof(heap_task_info_params_t), MALLOC_CAP_32BIT);
    void* p = heap_caps_malloc(sizeof(HeapTotals), MALLOC_CAP_32BIT);
    changes = new(p) HeapTotals();
    if (!before || !after || !taskstatus || !tasklist || !params || !changes)
      {
      if (before)
        free(before);
      if (after)
        free(after);
      if (taskstatus)
        free(taskstatus);
      if (tasklist)
        free(tasklist);
      if (params)
        free(params);
      if (changes)
        delete(changes);
      return false;
      }
    }
  return true;
  }


static UBaseType_t get_tasks()
  {
  TaskMap* tm = TaskMap::instance();
  UBaseType_t numtasks = 0;
  if (tm)
    numtasks = tm->populate();
  return numtasks;
  }


static void get_memory(TaskHandle_t* tasks, size_t taskslen)
  {
  params->mask[DRAM] = MALLOC_CAP_8BIT | MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL;
  params->caps[DRAM] = MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL;
  params->mask[D_IRAM] = MALLOC_CAP_8BIT | MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL;
  params->caps[D_IRAM] = MALLOC_CAP_8BIT | MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL;
  params->mask[IRAM] = MALLOC_CAP_8BIT | MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL;
  params->caps[IRAM] = MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL;
  params->mask[SPIRAM] = MALLOC_CAP_SPIRAM;
  params->caps[SPIRAM] = MALLOC_CAP_SPIRAM;
  params->tasks = tasks;
  params->num_tasks = taskslen;
  params->totals = changes->array();
  params->num_totals = changes->size();
  params->max_totals = NUMTASKS;
  params->blocks = after;
  params->max_blocks = DUMPSIZE;
  numafter = heap_caps_get_per_task_info(params);
  }


static void module_memory(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsMutexLock lock(&taskstatus_mutex);
  static const char* ctasks[] = {}; // { "tiT", "wifi"};
  const char* const* tasks = ctasks;
  bool all = false;
  bool leaks = *(cmd->GetName()) == 'l';
  if (argc > 0)
    {
    if (**argv == '*')
      tasks = NULL;
    else if (**argv == '=')
      {
      all = true;
      argc = sizeof(ctasks) / sizeof(char*);
      }
    else
      tasks = argv;
    }
  else
    argc = sizeof(ctasks) / sizeof(char*);
  if (!allocate())
    {
    writer->printf("Can't allocate storage for memory diagnostics\n");
    return;
    }
  UBaseType_t n = get_tasks();
  TaskMap* tm = TaskMap::instance();
  TaskHandle_t* tl = NULL;
  size_t tln = 0;
  if (tasks)
    {
    tl = tasklist;
    for ( ; argc > 0; --argc, ++tasks)
      {
      Name name(*tasks);
      TaskHandle_t tk = tm->find(name);
      if (tk != NOT_FOUND)
        {
        *tl++ = tk;
        if (++tln == TASKLIST)
          break;
        }
      else
        writer->printf("Task %s unknown\n", *tasks);
      }
    tl = tasklist;
    }
  get_memory(tl, tln);

  FreeHeap now;
  writer->printf("Free 8-bit %zu/%zu, 32-bit %zu/%zu, SPIRAM %zu/%zu\n",
    now.Free8bit(), now.Total8bit(), now.Free32bit(), now.Total32bit(),
    now.FreeSPI(), now.TotalSPI());
  bool first = true;
  UBaseType_t num = 0;
  for (int done = changes->begin(); done < changes->end(); ++num)
    {
    TaskHandle_t taskid = 0;
    if (num != 0)
      {
      UBaseType_t t;
      for (t = 0; t < n; ++t)
        if (taskstatus[t].xTaskNumber == num)
          break;
      if (t == n)
        continue;
      taskid = taskstatus[t].xHandle;
      }
    for (int i = changes->begin(); i < changes->end(); ++i)
      {
      if (num == 0)
        {
        UBaseType_t t;
        for (t = 0; t < n; ++t)
          if (taskstatus[t].xHandle == (*changes).Task(i))
            break;
        if (t < n)
          continue;
        }
      else if ((*changes).Task(i) != taskid)
        continue;
      int change[NUM_HEAP_TASK_CAPS];
      bool any = false;
      for (int j = 0; j < NUM_HEAP_TASK_CAPS; ++j)
        {
        change[j] = (*changes).After(i, j) - (*changes).Before(i, j);
        if (change[j])
          any = true;
        }
      if (any || all)
        {
        if (first)
          {
          writer->printf("--Task--     Total DRAM D/IRAM   IRAM SPIRAM"
            "   +/- DRAM D/IRAM   IRAM SPIRAM\n");
          first = false;
          }
        Name name("NoTaskMap");
        if (tm)
          tm->find((*changes).Task(i), name);
        writer->printf("%-15.15s %7d%7d%7d%7d    %+7d%+7d%+7d%+7d\n", name.bytes,
          (*changes).After(i, DRAM), (*changes).After(i, D_IRAM),
          (*changes).After(i, IRAM), (*changes).After(i, SPIRAM),
          change[DRAM], change[D_IRAM], change[IRAM], change[SPIRAM]);
        }
      ++done;
      if (num > 0)
        break;
      }
    }

  if (tln)
    {
    writer->printf("============================ blocks dumped = %d%s\n",
      numafter, numafter < DUMPSIZE ? "" : " (limited)");
    tl = tasklist;
    for (int i = 0; i < tln; ++i, ++tl)
      print_blocks(writer, *tl, leaks);
    }
  else if (!tasks)
    {
    bool headline = true;
    for (int i = changes->begin(); i < changes->end(); ++i)
      {
      bool any = false;
      for (int j = 0; j < NUM_HEAP_TASK_CAPS; ++j)
        {
        if ((*changes).After(i, j) - (*changes).Before(i, j) != 0)
          any = true;
        }
      if (any)
        {
        if (headline)
          {
          writer->printf("============================ blocks dumped = %d%s\n",
            numafter, numafter < DUMPSIZE ? "" : " (limited)");
          headline = false;
          }
        print_blocks(writer, (*changes).Task(i), leaks);
        }
      }
    }

  changes->transfer();
  for (int i = 0; i < numafter; ++i)
    {
    before[i] = after[i];
    }
  numbefore = numafter;
  }


static void module_tasks(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsMutexLock lock(&taskstatus_mutex);
  UBaseType_t num = uxTaskGetNumberOfTasks();
  writer->printf("Number of Tasks =%3u%s    Stack:  Now   Max Total    Heap 32-bit SPIRAM C# PRI CPU%%\n", num,
    num > MAX_TASKS ? ">max" : "    ");
  if (!allocate())
    {
    writer->printf("Can't allocate storage for task diagnostics\n");
    return;
    }
  bool showStack = (strcmp(cmd->GetName(),"stack") == 0);

  // sample current task activity if last taskstatus fetched is too old:
  uint32_t now = xTaskGetTickCount() / configTICK_RATE_HZ;
  if (now - taskstatus_time > 3600)
    {
    get_tasks();
    vTaskDelay(pdMS_TO_TICKS(2000));
    }

  // copy last taskstatus for comparison:
  std::map<UBaseType_t, uint32_t> last_runtime;
  for (int i = 0; i < taskstatus_cnt; i++)
    last_runtime[taskstatus[i].xTaskNumber] = taskstatus[i].ulRunTimeCounter;
  uint32_t last_totalruntime = totalruntime;

  // update taskstatus:
  UBaseType_t n = get_tasks();
  get_memory(tasklist, 0);
  uint32_t diff_totalruntime = totalruntime - last_totalruntime;

  // output task info sorted by xTaskNumber:
  num = 0;
  for (UBaseType_t j = 0; j < n; )
    {
    for (UBaseType_t i = 0; i < n; ++i)
      {
      if (taskstatus[i].xTaskNumber == num)
        {
        int k = changes->find(taskstatus[i].xHandle);
        int heaptotal = 0, heap32bit = 0, heapspi = 0;
        if (k >= 0)
          {
          heaptotal = (*changes).After(k, DRAM) + (*changes).After(k, D_IRAM);
          heap32bit = (*changes).After(k, IRAM);
          heapspi = (*changes).After(k, SPIRAM);
          }
        uint32_t total = (uint32_t)taskstatus[i].pxStackBase >> 16;
        uint32_t used = total - ((uint32_t)taskstatus[i].pxStackBase & 0xFFFF);
        int core = xTaskGetAffinity(taskstatus[i].xHandle);
        uint32_t runtime = taskstatus[i].ulRunTimeCounter - last_runtime[taskstatus[i].xTaskNumber];
        writer->printf("%08X %4u %s %-15s %5u %5u %5u %7u%7u%7u  %c %3d %3.0f%%\n", taskstatus[i].xHandle,
          taskstatus[i].xTaskNumber, states[taskstatus[i].eCurrentState], taskstatus[i].pcTaskName,
          used, total - taskstatus[i].usStackHighWaterMark, total, heaptotal, heap32bit, heapspi,
          (core == tskNO_AFFINITY) ? '*' : '0'+core, taskstatus[i].uxCurrentPriority,
          diff_totalruntime ? ((float) runtime / diff_totalruntime * 100) : 0.0f);
        if (showStack)
          {
          uint32_t* stack = (uint32_t*)(pxTaskGetStackStart(taskstatus[i].xHandle) + total);
          uint32_t* topstack = (uint32_t*)((uint8_t*)stack - used);
          while (topstack < stack)
            {
            uint32_t word = *topstack++;
            if ((word & 0xFF000000) == 0x80000000)
              {
              writer->printf("  %p", word - 0x40000000);
              }
            }
          writer->printf("\n");
          }
        ++j;
        break;
        }
      }
    ++num;
    }
  }

static void module_tasks_data(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsMutexLock lock(&taskstatus_mutex);
  StringWriter buf;
  buf.reserve(2048);

  UBaseType_t num = uxTaskGetNumberOfTasks();
  if (!allocate())
    {
    if (writer)
      writer->printf("Can't allocate storage for task diagnostics\n");
    return;
    }

  // sample current task activity if last taskstatus fetched is too old:
  uint32_t now = xTaskGetTickCount() / configTICK_RATE_HZ;
  if (now - taskstatus_time > 3600)
    {
    get_tasks();
    vTaskDelay(pdMS_TO_TICKS(2000));
    }

  // copy last taskstatus for comparison:
  std::map<UBaseType_t, uint32_t> last_runtime;
  for (int i = 0; i < taskstatus_cnt; i++)
    last_runtime[taskstatus[i].xTaskNumber] = taskstatus[i].ulRunTimeCounter;
  uint32_t last_totalruntime = totalruntime;

  // update taskstatus:
  UBaseType_t n = get_tasks();
  get_memory(tasklist, 0);
  uint32_t diff_totalruntime = totalruntime - last_totalruntime;

  // output task count & total runtime diff:
  buf.printf("*-OVM-DebugTasks,1,86400,%u,%u", n, diff_totalruntime);

  // output tasks sorted by xTaskNumber:
  num = 0;
  for (UBaseType_t j = 0; j < n; )
    {
    for (UBaseType_t i = 0; i < n; ++i)
      {
      if (taskstatus[i].xTaskNumber == num)
        {
        int k = changes->find(taskstatus[i].xHandle);
        int heaptotal = 0, heap32bit = 0, heapspi = 0;
        if (k >= 0)
          {
          heaptotal = (*changes).After(k, DRAM) + (*changes).After(k, D_IRAM);
          heap32bit = (*changes).After(k, IRAM);
          heapspi = (*changes).After(k, SPIRAM);
          }
        uint32_t total = (uint32_t)taskstatus[i].pxStackBase >> 16;
        uint32_t used = total - ((uint32_t)taskstatus[i].pxStackBase & 0xFFFF);
        uint32_t runtime = taskstatus[i].ulRunTimeCounter - last_runtime[taskstatus[i].xTaskNumber];

        // output task info block:
        //  ,<num>,<name>,<state>
        //  ,<stack_now>,<stack_max>,<stack_total>
        //  ,<heaptotal>,<heap32bit>,<heapspi>
        //  ,<runtime>
        buf.printf(",%u,%-.15s,%s,%u,%u,%u,%u,%u,%u,%u",
          taskstatus[i].xTaskNumber, taskstatus[i].pcTaskName, states[taskstatus[i].eCurrentState],
          used, total - taskstatus[i].usStackHighWaterMark, total,
          heaptotal, heap32bit, heapspi, runtime);

        ++j;
        break;
        }
      }
    ++num;
    }

  if (writer)
    {
    writer->puts(buf.substr(25).c_str());
    }
  else
    {
    ESP_LOGD(TAG, "Task stats: %s", buf.substr(25).c_str());
    MyNotify.NotifyString("data", "debug.tasks", buf.c_str());
    }
  }

static void module_check(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  heap_caps_check_integrity_all(true);
  }
#endif // NOGO

static void module_fault(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  ESP_LOGI(TAG,"Abort faulting module (on command)");
  abort();
  }

static void module_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Resetting system...");
  vTaskDelay(1000/portTICK_PERIOD_MS);
  MyBoot.Restart();
  }

static void module_perform_factoryreset(OvmsWriter* writer)
  {
  const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "store");
  if (p == NULL)
    {
    if (writer)
      writer->puts("DATA/FAT Partition 'store' could not be found - factory reset aborted");
    else
      ESP_LOGE(TAG, "DATA/FAT Partition 'store' could not be found - factory reset aborted");
    return;
    }

  if (writer)
    {
    writer->printf("Store partition is at %08x size %08x\n", p->address, p->size);
    writer->puts("Unmounting configuration store...");
    }
  else
    {
    ESP_LOGI(TAG, "Store partition is at %08x size %08x", p->address, p->size);
    ESP_LOGI(TAG, "Unmounting configuration store...");
    }
  MyConfig.unmount();

  if (writer)
    writer->printf("Erasing %d bytes of flash...\n",p->size);
  else
    ESP_LOGI(TAG, "Erasing %d bytes of flash...", p->size);
  spi_flash_erase_range(p->address, p->size);

  if (writer)
    writer->puts("Factory reset of configuration store complete and reboot now...");
  else
    ESP_LOGW(TAG, "Factory reset of configuration store complete and reboot now...");
  vTaskDelay(1000/portTICK_PERIOD_MS);
  MyBoot.Restart();
  }

bool module_factory_reset_yesno(OvmsWriter* writer, void* ctx, char ch)
  {
  writer->printf("%c\n",ch);

  if (ch != 'y')
    {
    writer->puts("Factory reset aborted");
    return false;
    }

  module_perform_factoryreset(writer);

  // not reached, just for gcc:
  return false;
  }

static void module_factory_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 1 && strcmp(argv[0], "-noconfirm") == 0)
    {
    module_perform_factoryreset(writer);
    }
  else
    {
    writer->printf("Reset configuration store to factory defaults, and lose all configuration data (y/n): ");
    writer->RegisterInsertCallback(module_factory_reset_yesno, NULL);
    }
  }

static void module_eventhandler(std::string event, void* data)
  {
  if (event == "ticker.300")
    {
    if (MyConfig.GetParamValueBool("module", "debug.tasks", false))
      module_tasks_data(0, NULL, NULL, 0, NULL);
    }

#ifdef CONFIG_OVMS_COMP_SDCARD
  else if (event == "sd.mounted")
    {
    if (unlink("/sd/factoryreset.txt") == 0)
      {
      ESP_LOGW(TAG, "Found file '/sd/factoryreset.txt' => performing factory reset");
      module_perform_factoryreset(NULL);
      }
    }
  else if (event == "ticker.1")
    {
    static int module_sw2_pushcnt = 0;
    // SW2 is connected to SD_DATA0 so can be 0 due to SD card activity.
    // Is there a clean way to detect if SD transmissions are currently running?
    // The sdmmc semaphore and status variables are all static…
    // Workaround: only watch SW2 if no SD card is mounted:
    if (MyPeripherals->m_sdcard->isinserted() || gpio_get_level((gpio_num_t)MODULE_GPIO_SW2) != 0)
      {
      module_sw2_pushcnt = 0;
      return;
      }
    module_sw2_pushcnt++;
    if (module_sw2_pushcnt < 10)
      ESP_LOGW(TAG, "SW2 pushed, factory reset in %d seconds", 10-module_sw2_pushcnt);
    else
      {
      ESP_LOGW(TAG, "SW2 held for 10 seconds => performing factory reset");
      module_perform_factoryreset(NULL);
      }
    }
#endif
  }

static void module_summary(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("OVMS MODULE SUMMARY");

  writer->puts("\nModule");
  writer->printf("  Version:  %s\n",StandardMetrics.ms_m_version->AsString().c_str());
  writer->printf("  Hardware: %s\n",StandardMetrics.ms_m_hardware->AsString().c_str());
  writer->printf("  12v:      %0.1fv\n",StandardMetrics.ms_v_bat_12v_voltage->AsFloat());

#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
  MyPeripherals->m_simcom->SupportSummary(writer);
#endif // #ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM

  MyConfig.SupportSummary(writer);

  writer->puts("\nREPORT ENDS");
  }

class OvmsModuleInit
  {
  public:
  OvmsModuleInit()
    {
    ESP_LOGI(TAG, "Initialising MODULE (5100)");
#ifndef NOGO
    TaskMap::instance()->insert(0x00000000, "no task");
#endif

#ifdef CONFIG_OVMS_COMP_SDCARD
    MyEvents.RegisterEvent(TAG, "sd.mounted", module_eventhandler);
    MyEvents.RegisterEvent(TAG, "ticker.1", module_eventhandler);
#endif //CONFIG_OVMS_COMP_SDCARD
    MyEvents.RegisterEvent(TAG, "ticker.300", module_eventhandler);

    OvmsCommand* cmd_module = MyCommandApp.RegisterCommand("module","MODULE framework");
    cmd_module->RegisterCommand("memory","Show module memory usage",module_memory,"[<task names or ids>|*|=]",0,TASKLIST);
    cmd_module->RegisterCommand("leaks","Show module memory changes",module_memory,"[<task names or ids>|*|=]",0,TASKLIST);
    OvmsCommand* cmd_tasks = cmd_module->RegisterCommand("tasks","Show module task usage",module_tasks,"[stack]",0,1);
    cmd_tasks->RegisterCommand("stack","Show module task usage with stack",module_tasks);
    cmd_tasks->RegisterCommand("data","Output module task stats record",module_tasks_data);
    cmd_module->RegisterCommand("fault","Abort fault the module",module_fault);
    cmd_module->RegisterCommand("reset","Reset module",module_reset);
    cmd_module->RegisterCommand("check","Check heap integrity",module_check);
    cmd_module->RegisterCommand("summary","Show module summary",module_summary);
    OvmsCommand* cmd_factory = cmd_module->RegisterCommand("factory","MODULE FACTORY framework");
    cmd_factory->RegisterCommand("reset","Factory Reset module",module_factory_reset,"[-noconfirm]",0,1);
    }
  } MyOvmsModuleInit  __attribute__ ((init_priority (5100)));

// Returns the value of the stack pointer in the calling function.
// The stack frame for this function is 32 bytes, hence the add.

void* stack() {
    __asm__(
"   addi a2, sp, 32\n"
"   retw.n\n"
    );
    return 0;
}
