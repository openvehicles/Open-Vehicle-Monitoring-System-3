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

#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include "ovms_command.h"
#include "ovms_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
#include "duktape.h"

typedef enum
  {
  DUKTAPE_none = 0,             // Do nothing
  DUKTAPE_register,             // Register extension function
  DUKTAPE_reload,               // Reload DukTape engine
  DUKTAPE_compact,              // Compact DukTape memory
  DUKTAPE_event,                // Event
  DUKTAPE_autoinit,             // Auto init
  DUKTAPE_evalnoresult,         // Execute script text (without result)
  DUKTAPE_evalfloatresult,      // Execute script text (float result)
  DUKTAPE_evalintresult         // Execute script text (int result)
  } duktape_msg_t;

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

class DuktapeObjectRegistration
  {
  public:
    DuktapeObjectRegistration(const char* name);
    ~DuktapeObjectRegistration();

  public:
    const char* GetName();
    void RegisterDuktapeFunction(duk_c_function func, duk_idx_t nargs, const char* name);

  public:
    void RegisterWithDuktape(duk_context* ctx);

  protected:
    const char* m_name;
    DuktapeFunctionMap m_fnmap;
  };

typedef std::map<const char*, DuktapeObjectRegistration*, CmpStrOp> DuktapeObjectMap;

typedef struct
  {
  union
    {
    struct
      {
      duk_c_function func;
      duk_idx_t nargs;
      const char* name;
      } dt_register;
    struct
      {
      const char* name;
      void* data;
      } dt_event;
    struct
      {
      const char* text;
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
    } body;
  duktape_msg_t type;
  QueueHandle_t waitcompletion;
  OvmsWriter* writer;
  } duktape_queue_t;

#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

class OvmsScripts
  {
  public:
    OvmsScripts();
    ~OvmsScripts();

  public:
    void EventScript(std::string event, void* data);
    void AllScripts(std::string path);

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  public:
    void RegisterDuktapeFunction(duk_c_function func, duk_idx_t nargs, const char* name);
    void RegisterDuktapeModule(const char* start, size_t length, const char* name);
    duktape_registermodule_t* FindDuktapeModule(const char* name);
    void RegisterDuktapeObject(DuktapeObjectRegistration* ob);
    void AutoInitDuktape();

  protected:
    void DuktapeDispatch(duktape_queue_t* msg);
    void DuktapeDispatchWait(duktape_queue_t* msg);

  public:
    void  DuktapeEvalNoResult(const char* text, OvmsWriter* writer=NULL);
    float DuktapeEvalFloatResult(const char* text, OvmsWriter* writer=NULL);
    int   DuktapeEvalIntResult(const char* text, OvmsWriter* writer=NULL);
    void  DuktapeReload();
    void  DuktapeCompact();

  public:
    void DukTapeInit();
    void DukTapeTask();

  protected:
    duk_context* m_dukctx;
    TaskHandle_t m_duktaskid;
    QueueHandle_t m_duktaskqueue;
    DuktapeFunctionMap m_fnmap;
    DuktapeModuleMap m_modmap;
    DuktapeObjectMap m_obmap;
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  };

extern OvmsScripts MyScripts;

#endif //#ifndef __SCRIPT_H__
