/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2017  Mark Webb-Johnson
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
static const char *TAG = "task_base";

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ovms.h"
#include "task_base.h"
#include "ovms_module.h"

Parent::Parent()
  {
  m_mutex = xSemaphoreCreateMutex();
  }

Parent::~Parent()
  {
  DeleteChildren();
  vSemaphoreDelete(m_mutex);
  }

bool Parent::AddChild(TaskBase* child)
  {
  if (!child->Instantiate())
    {
    child->Cleanup();
    delete child;
    return false;
    }
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_children.push_front(child);
  xSemaphoreGive(m_mutex);
  return true;
  }

void Parent::DeleteChild(TaskBase* child)
  {
  if (RemoveChild(child))
    child->DeleteTask();
  }

// This function would typically be called from the child's task to divorce
// itself from the parent because it is closing down on its own.
bool Parent::RemoveChild(TaskBase* child)
  {
  bool removed = false;
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  Children::iterator at = m_children.begin(), after = at;
  if (at != m_children.end())
    {
    if (*at == child)
      {
      removed = true;
      m_children.pop_front();
      }
    else
      {
      for (++after; after != m_children.end(); ++at, ++after)
        {
        if (*after == child)
          {
          removed = true;
          m_children.erase_after(at);
          break;
          }
        }
      }
    }
  xSemaphoreGive(m_mutex);
  return removed;
  }

void Parent::DeleteChildren()
  {
  while (true)
    {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (m_children.empty())
      {
      xSemaphoreGive(m_mutex);
      break;
      }
    TaskBase* child = *m_children.begin();
    m_children.pop_front();
    xSemaphoreGive(m_mutex);
    child->DeleteTask();
    }
  }

TaskBase::TaskBase(const char* name,
                   int stack /*=DEFAULT_STACK*/,
                   UBaseType_t priority /*=DEFAULT_PRIORITY*/,
                   Parent* parent /*=NULL*/)
  {
  m_name = name;
  m_stack = stack;
  m_priority = priority;
  m_parent = parent;
  m_taskid = 0;
  }

TaskBase::~TaskBase()
  {
  }

bool TaskBase::Instantiate()
  {
  if (CreateTaskPinned(1, m_name, m_stack, m_priority) != pdPASS)
    {
    // Use printf here because ESP_LOG requires heap allocations
    ::printf("\nInsufficient memory to create %s task\n", m_name);
    return false;
    }
  return true;
  }

BaseType_t TaskBase::CreateTask(const char* name, int stack, UBaseType_t priority)
  {
  BaseType_t task = xTaskCreate(Task, name, stack, (void*)this, priority, &m_taskid);
  AddTaskToMap(m_taskid);
  return task;
  }

BaseType_t TaskBase::CreateTaskPinned(const BaseType_t core, const char* name, int stack, UBaseType_t priority)
  {
  BaseType_t task = xTaskCreatePinnedToCore(Task, name, stack, (void*)this, priority, &m_taskid, CORE(core));
  AddTaskToMap(m_taskid);
  return task;
  }

void TaskBase::Task(void *object)
  {
  TaskBase* me = (TaskBase*)object;
  me->Service();
  ESP_LOGD(TAG, "Task %s finished with %u stack free", me->m_name, uxTaskGetStackHighWaterMark(me->m_taskid));
  me->DeleteFromParent();
  while (true); // Illegal instruction abort occurs if this function returns
  }

// This function should be overridden in the derived class to perform any
// cleanup operations that must be done before the task is deleted.
// This function should NOT be called from the destructor because it is
// called explicitly here before the object is deleted.  That separation
// is needed so DeleteTask can clean up before deleting the task and still
// wait to delete the object until after the task is deleted.
void TaskBase::Cleanup()
  {
  }

// This function must NOT be called from ~TaskBase().
void TaskBase::DeleteFromParent()
  {
  if (!m_parent || m_parent->RemoveChild(this))
    {
    TaskHandle_t taskid = m_taskid;
    Cleanup();
    delete this;
    if (taskid)
      vTaskDelete(taskid);
    }
  }

// This function must NOT be called from the TaskBase object's own task.
// It is only to be called from Parent::DeleteChildren() executing in the
// parent object's task.
void TaskBase::DeleteTask()
  {
  Cleanup();
  if (m_taskid)
    vTaskDelete(m_taskid);
  delete this;
  }
