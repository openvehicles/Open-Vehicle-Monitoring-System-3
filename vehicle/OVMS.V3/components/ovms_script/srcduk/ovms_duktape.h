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

#ifndef __OVMS_DUKTAPE_H__
#define __OVMS_DUKTAPE_H__

#include "ovms_command.h"
#include "ovms_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "duktape.h"
#include <list>
#include <utility>

////////////////////////////////////////////////////////////////////////////////
// DukContext: C++ wrapper for duk_context

class DukContext
  {
  public:
    DukContext(duk_context *ctx) { m_ctx = ctx; }
    ~DukContext() {}

    void Push(bool val)                   { duk_push_boolean(m_ctx, val); }
    void Push(int val)                    { duk_push_number(m_ctx, val); }
    void Push(short val)                  { duk_push_number(m_ctx, val); }
    void Push(long val)                   { duk_push_number(m_ctx, val); }
    void Push(unsigned int val)           { duk_push_uint(m_ctx, val); }
    void Push(unsigned short val)         { duk_push_uint(m_ctx, val); }
    void Push(unsigned long val)          { duk_push_uint(m_ctx, val); }
    void Push(float val)                  { duk_push_number(m_ctx, float2double(val)); }
    void Push(double val)                 { duk_push_number(m_ctx, val); }
    void Push(const char* val)            { duk_push_string(m_ctx, val); }
    void Push(const std::string &val)     { duk_push_lstring(m_ctx, val.data(), val.size()); }
    void Push(const extram::string &val)  { duk_push_lstring(m_ctx, val.data(), val.size()); }

    // Push string as Uint8Array:
    void PushBinary(const std::string &val)
      {
      void* p = duk_push_fixed_buffer(m_ctx, val.size());
      if (p) memcpy(p, val.data(), val.size());
      }
    void PushBinary(const extram::string &val)
      {
      void* p = duk_push_fixed_buffer(m_ctx, val.size());
      if (p) memcpy(p, val.data(), val.size());
      }

    duk_idx_t PushArray()                 { return duk_push_array(m_ctx); }
    duk_idx_t PushObject()                { return duk_push_object(m_ctx); }
    duk_bool_t PutProp(duk_idx_t obj_idx, duk_uarridx_t arr_idx)
      { return duk_put_prop_index(m_ctx, obj_idx, arr_idx); }
    duk_bool_t PutProp(duk_idx_t obj_idx, const char* prop_name)
      { return duk_put_prop_string(m_ctx, obj_idx, prop_name); }

  public:
    duk_context *m_ctx;
  };

////////////////////////////////////////////////////////////////////////////////
// Duktape Task Commands

typedef enum
  {
  DUKTAPE_none = 0,             // Do nothing
  DUKTAPE_reload,               // Reload DukTape engine
  DUKTAPE_compact,              // Compact DukTape memory
  DUKTAPE_event,                // Event
  DUKTAPE_autoinit,             // Auto init
  DUKTAPE_evalnoresult,         // Execute script text (without result)
  DUKTAPE_evalfloatresult,      // Execute script text (float result)
  DUKTAPE_evalintresult,        // Execute script text (int result)
  DUKTAPE_callback,             // DuktapeObject callback
  DUKTAPE_command,              // Duktape command
  DUKTAPE_shutdown,             // Shutdown Duktape
  DUKTAPE_deleteobject          // Delete/uncouple/deregister object in duktape context.
  } duktape_msg_t;

////////////////////////////////////////////////////////////////////////////////
// DuktapeObjectRegistration

typedef struct
  {
  duk_c_function func;
  duk_idx_t nargs;
  } duktape_registerfunction_t;

typedef std::map<const char*, duktape_registerfunction_t*, CmpStrOp> DuktapeFunctionMap;

typedef struct
  {
  const char* start;
  size_t length;
  } duktape_registermodule_t;

typedef std::map<const char*, duktape_registermodule_t*, CmpStrOp> DuktapeModuleMap;

class DuktapeObjectRegistration;

typedef std::map<const char *, DuktapeObjectRegistration*, CmpStrOp> DuktapeObjectMap;

class DuktapeObjectRegistration
  {
  public:
    DuktapeObjectRegistration(const char* name);
    ~DuktapeObjectRegistration();

  public:
    const char* GetName();
    void RegisterDuktapeFunction(duk_c_function func, duk_idx_t nargs, const char* name);

    void RegisterDuktapeObject(DuktapeObjectRegistration* obj, const char *name);

  public:
    void RegisterWithDuktape(duk_context* ctx);

  protected:
    void PushThisAsObject(duk_context* ctx);
    const char* m_name;
    DuktapeFunctionMap m_fnmap;
    DuktapeObjectMap m_obmap;
  };

typedef std::map<const char*, DuktapeObjectRegistration*, CmpStrOp> DuktapeObjectMap;
class DuktapeObject;
class OvmsScripts;
class DuktapeConsoleCommand;

class DuktapeCallbackParameter
  {
  public:
    virtual ~DuktapeCallbackParameter() { }
    /** Push the parameter onto the stack.
     */
    virtual int Push(duk_context *ctx) = 0;
  };

typedef struct
  {
  union
    {
    struct
      {
      const char* name;
      void* data;
      } dt_event;
    struct
      {
      const char* text;
      const char* filename;
      } dt_evalnoresult;
    struct
      {
      const char* text;
      float* result;
      } dt_evalfloatresult;
    struct
      {
      const char* text;
      int* result;
      } dt_evalintresult;
    struct
      {
      DuktapeObject* instance;
      const char* method;
      DuktapeCallbackParameter* params;
      } dt_callback;
    struct
      {
      const char *command;
      DuktapeConsoleCommand* dcc;
      int argc;
      const char * const *argv;
      } dt_command;
    struct
      {
      DuktapeObject *instance;
      bool decouple:1;
      bool deregister:1;
      } dt_deleteobject;
    } body;
  duktape_msg_t type;
  QueueHandle_t waitcompletion;
  OvmsWriter* writer;
  } duktape_queue_t;


////////////////////////////////////////////////////////////////////////////////
// DuktapeObject: coupled C++ / JS object
//
//  Intended for API methods to attach internal API state to a JS object and provide
//    a standard callback invocation interface for JS objects in local scopes.
//
//  - Override CallMethod() to implement specific method calls
//  - Override Finalize() for specific destruction in JS context (garbage collection)
//  - call Register() to prevent normal garbage collection (but not heap destruction)
//  - call Ref() to protect against deletion (reference count)
//  - call Lock() to protect concurrent access (recursive mutex)
//
//  - GetInstance() retrieves the DuktapeObject associated with a JS object if any
//  - Push() pushes the JS object onto the Duktape stack
//
//  Note: the DuktapeObject may persist after the JS object has been finalized, e.g.
//    if some callbacks are pending after the Duktape heap has been destroyed.
//    Use IsCoupled() to check if the JS object is still available.
//
//  Ref/Unref:
//    Normal life cycle is from construction to finalization. Pending callbacks extend
//    the life until the last callback has been processed. A subclass may extend the life
//    by calling Ref(), which increases the reference count. Unref() deletes the instance
//    if no references are left.

class DuktapeObject
  {
  friend OvmsScripts;
  public:
    DuktapeObject();
    DuktapeObject(duk_context *ctx, int obj_idx);
  protected:
    // Don't delete. Decouple or Unref()
    virtual ~DuktapeObject();

  public:
    bool Lock(TickType_t timeout = portMAX_DELAY) { return m_mutex.Lock(timeout); }
    void Unlock() { m_mutex.Unlock(); }

  public:
    void Ref();
    bool Unref();

  public:
    virtual void AfterCouple(duk_context *ctx, int obj_idx);
    virtual void BeforeDecouple(duk_context *ctx);
    bool Couple(duk_context *ctx, int obj_idx);
    void Decouple(duk_context *ctx);
    bool IsCoupled() { return m_object != NULL; }
    static DuktapeObject* GetInstance(duk_context *ctx, int obj_idx);

  protected:
    static duk_ret_t DuktapeFinalizer(duk_context *ctx);
    virtual void Finalize(duk_context *ctx, bool heapDestruct);

  public:
    void Register(duk_context *ctx);
    void Register(duk_context *ctx, int obj_idx);
    void Deregister(duk_context *ctx);
    bool IsRegistered() { return m_registered; }

  public:
    bool RequestCallback(const char* method, DuktapeCallbackParameter* params=nullptr);
    duk_ret_t DuktapeCallback(duk_context *ctx, duktape_queue_t &msg);
    virtual duk_ret_t CallMethod(duk_context *ctx, const char* method, DuktapeCallbackParameter* params=nullptr);

  public:
    duk_idx_t Push(duk_context *ctx);

  protected:
    OvmsRecMutex m_mutex;               // concurrency lock
    uint32_t m_refcnt = 0;              // references to this object (1→0 = deletion)
    void* m_object = NULL;              // heapptr to JS object
    bool m_registered = false;          // registration state (true = GC prevented)
    void* m_registry = NULL;            // optional heapptr to specific registry JS object
  };

////////////////////////////////////////////////////////////////////////////////
// DuktapeConsoleCommand

class DuktapeConsoleCommand : public DuktapeObject
  {
  public:
    // Construct then call Couple() separately to ensure the correct AfterCouple()
    // is called.
    DuktapeConsoleCommand(OvmsCommand* cmd, const char* module);

    void AfterCouple(duk_context *ctx, int obj_idx) override;
    void BeforeDecouple(duk_context *ctx) override;
  protected:
    ~DuktapeConsoleCommand();
  public:
    OvmsCommand* m_cmd;
    std::string m_module;
  };

////////////////////////////////////////////////////////////////////////////////
// OvmsDuktape
//
// Container for object registration and the main Duktape task

class OvmsDuktape
  {
  public:
    OvmsDuktape();
    ~OvmsDuktape();

  public:
    void RegisterDuktapeFunction(duk_c_function func, duk_idx_t nargs, const char* name);
    void RegisterDuktapeModule(const char* start, size_t length, const char* name);
    duktape_registermodule_t* FindDuktapeModule(const char* name);
    void RegisterDuktapeObject(DuktapeObjectRegistration* ob);
    void AutoInitDuktape();

  protected:
    bool DuktapeDispatch(duktape_queue_t* msg, TickType_t queuewait=portMAX_DELAY);
    void DuktapeDispatchWait(duktape_queue_t* msg);

    void DukOvmsCommandRun(OvmsWriter* writer, const char * command, DuktapeConsoleCommand* dcc, int argc, const char* const* argv);

    void DukTapeUnload(bool unload_modules = true);

    void EventSystemShuttingDown(std::string event, void* data);

    void Ticker1_Shutdown(std::string event, void* data);
  public:
    void  DuktapeEvalNoResult(const char* text, OvmsWriter* writer=NULL, const char* filename=NULL);
    float DuktapeEvalFloatResult(const char* text, OvmsWriter* writer=NULL);
    int   DuktapeEvalIntResult(const char* text, OvmsWriter* writer=NULL);
    void DuktapeEvalCommand(OvmsWriter* writer, const char *command, DuktapeConsoleCommand* dcc, int argc, const char* const* argv);
    void  DuktapeReload();
    void  DuktapeCompact(bool wait=true);
    bool  DuktapeRequestCallback(DuktapeObject* instance, const char* method, DuktapeCallbackParameter* params);
    OvmsWriter* GetDuktapeWriter();
    void DukGetCallInfo(duk_context *ctx, std::string *filename, int *linenumber, std::string *function);

    bool DuktapeRequestDelete(DuktapeObject* instance, bool decouple, bool deregister);

  protected:
    void NotifyDuktapeModuleUnloadAll(duk_context *ctx);
  public:
    void NotifyDuktapeModuleLoad(const char* filename);
    void NotifyDuktapeModuleUnload(const char* filename);
    bool RegisterDuktapeConsoleCommand(duk_context *ctx, int obj_idx,
                                       const char* filename,
                                       const char* parent,
                                       const char* name, const char* title,
                                       const char *usage = "", int min = 0, int max = 0);

  public:
    void DukTapeInit();
    void DukTapeTask();
    void ProcessJob(duktape_queue_t& msg);
    bool DukTapeAvailable() { return m_dukctx != NULL; }
    bool InDukTapeTask() { return xTaskGetCurrentTaskHandle() == m_duktaskid; }
    duk_context* DukTapeContext() { return m_dukctx; }
    void EventScript(std::string event, void* data);

  protected:
    duk_context* m_dukctx;
    TaskHandle_t m_duktaskid;
    QueueHandle_t m_duktaskqueue;
    DuktapeFunctionMap m_fnmap;
    DuktapeModuleMap m_modmap;
    DuktapeObjectMap m_obmap;

  public:
    typedef std::map<OvmsCommand*, DuktapeConsoleCommand*> DuktapeCommandMap;
    DuktapeCommandMap m_cmdmap;
  };

extern void DukOvmsErrorHandler(duk_context *ctx, duk_idx_t err_idx, OvmsWriter *writer=NULL, const char *filename=NULL);

extern OvmsDuktape MyDuktape;

#endif //#ifndef __OVMS_DUKTAPE_H__
