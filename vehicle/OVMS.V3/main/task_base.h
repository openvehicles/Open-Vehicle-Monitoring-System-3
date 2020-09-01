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
#ifndef __TASK_BASE_H__
#define __TASK_BASE_H__

#include <forward_list>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <freertos/semphr.h>

#define DEFAULT_STACK 4096
#define DEFAULT_PRIORITY 5

class TaskBase;

class Parent
  {
  public:
    typedef std::forward_list<TaskBase*> Children;

  public:
    Parent();
    ~Parent();

  protected:
    bool AddChild(TaskBase* child);
    void DeleteChild(TaskBase* child);
    void DeleteChildren();

  public:
    bool RemoveChild(TaskBase* child);

  private:
    SemaphoreHandle_t m_mutex;
    Children m_children;
  };
  
class TaskBase
  {
  public:
    TaskBase(const char* name, int stack = DEFAULT_STACK, UBaseType_t priority = DEFAULT_PRIORITY, Parent* parent = NULL);
    Parent* parent() { return m_parent; }

  protected:
    virtual ~TaskBase();

  protected:
    friend class Parent;
    virtual bool Instantiate();
    BaseType_t CreateTask(const char* name, int stack = DEFAULT_STACK,
      UBaseType_t priority = DEFAULT_PRIORITY);
    BaseType_t CreateTaskPinned(const BaseType_t core, const char* name, int stack = DEFAULT_STACK,
      UBaseType_t priority = DEFAULT_PRIORITY);
    static void Task(void *object);
    virtual void Service() = 0;
    virtual void Cleanup();
    void DeleteFromParent();

  private:
    friend class Parent;
    void DeleteTask();

  private:
    TaskHandle_t m_taskid;
    const char* m_name;
    int m_stack;
    UBaseType_t m_priority;
    Parent* m_parent;
  };

#endif //#ifndef __TASK_BASE_H__
