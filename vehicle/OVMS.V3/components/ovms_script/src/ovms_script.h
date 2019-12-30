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
#include <list>
#include <utility>

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
class DuktapeObject;
class OvmsScripts;

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
      void* data;
      } dt_callback;
    } body;
  duktape_msg_t type;
  QueueHandle_t waitcompletion;
  OvmsWriter* writer;
  } duktape_queue_t;


/***************************************************************************************************
 * DuktapeObject: coupled C++ / JS object
 * 
 *  Intended for API methods to attach internal API state to a JS object and provide
 *    a standard callback invocation interface for JS objects in local scopes.
 *  
 *  - Override CallMethod() to implement specific method calls
 *  - Override Finalize() for specific destruction in JS context (garbage collection)
 *  - call Register() to prevent normal garbage collection (but not heap destruction)
 *  - call Ref() to protect against deletion (reference count)
 *  - call Lock() to protect concurrent access (recursive mutex)
 *  
 *  - GetInstance() retrieves the DuktapeObject associated with a JS object if any
 *  - Push() pushes the JS object onto the Duktape stack
 *  
 *  Note: the DuktapeObject may persist after the JS object has been finalized, e.g.
 *    if some callbacks are pending after the Duktape heap has been destroyed.
 *    Use IsCoupled() to check if the JS object is still available.
 *  
 *  Ref/Unref:
 *    Normal life cycle is from construction to finalization. Pending callbacks extend
 *    the life until the last callback has been processed. A subclass may extend the life
 *    by calling Ref(), which increases the reference count. Unref() deletes the instance
 *    if no references are left.
 */

class DuktapeObject
  {
  friend OvmsScripts;
  public:
    DuktapeObject();
    DuktapeObject(duk_context *ctx, int obj_idx);
    virtual ~DuktapeObject();

  public:
    bool Lock(TickType_t timeout = portMAX_DELAY) { return m_mutex.Lock(timeout); }
    void Unlock() { m_mutex.Unlock(); }

  public:
    void Ref();
    bool Unref();

  public:
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
    void RequestCallback(const char* method, void* data=NULL);
    duk_ret_t DuktapeCallback(duk_context *ctx, duktape_queue_t &msg);
    virtual duk_ret_t CallMethod(duk_context *ctx, const char* method, void* data=NULL);

  public:
    duk_idx_t Push(duk_context *ctx);

  protected:
    OvmsRecMutex m_mutex;               // concurrency lock
    uint32_t m_refcnt = 0;              // references to this object (1→0 = deletion)
    void* m_object = NULL;              // heapptr to JS object
    bool m_registered = false;          // registration state (true = GC prevented)
    void* m_registry = NULL;            // optional heapptr to specific registry JS object
  };


/***************************************************************************************************
 * DuktapeHTTPRequest: perform asynchronous HTTP request
 *  - uses GET/POST (if post data is given)
 *  - follows 301/302 redirects automatically (max 5 hops)
 *  - automatically prevents garbage collection while active
 *  - Note: any valid server response is considered a success (= triggers done callback)
 *  - TODO: implement request.abort() method
 *  - TODO: implement digest authentication
 * 
 * Javascript API:
 *  create:
 *     var request = HTTP.request({
 *       url: "…",
 *       [headers: [{ "key": "value", … }, …]]    // Note: array members may contain multiple headers
 *       [post: "foo=bar&…",]                     // assumed x-www-form-urlencoded w/o Content-Type
 *       [timeout: ms,]                           // default: 120 seconds
 *       [binary: bool,]                          // default: false
 *       [done: function(response){},]
 *       [fail: function(error){},]
 *     });
 *  
 *  done(): (this = request, response = request.response)
 *    request.response = {
 *      statusCode: e.g. 200,
 *      statusText: e.g. "OK",
 *      [body: <string>,]                         // if binary=false
 *      [data: <Uint8Array>,]                     // if binary=true
 *      headers: [{ key: value }, …],
 *    }
 *  
 *  fail(): (this = request, error = request.error)
 *    request.error = "error description"
 *  
 *  also:
 *    request.url = last URL used if redirected
 *    request.redirectCount = number of redirects
 */

class DuktapeHTTPRequest : public DuktapeObject
  {
  public:
    DuktapeHTTPRequest(duk_context *ctx, int obj_idx);
    ~DuktapeHTTPRequest();
    static duk_ret_t Create(duk_context *ctx);

  protected:
    bool StartRequest(duk_context *ctx=NULL);

  public:
    static void MongooseCallbackEntry(struct mg_connection *nc, int ev, void *ev_data);
    void MongooseCallback(struct mg_connection *nc, int ev, void *ev_data);

  public:
    duk_ret_t CallMethod(duk_context *ctx, const char* method, void* data=NULL);

  protected:
    extram::string m_url;
    int m_redirectcnt = 0;
    bool m_ispost = false;
    extram::string m_post;
    int m_timeout = 120*1000;
    bool m_binary = false;
    extram::string m_headers;
    extram::string m_error;
    struct mg_connection *m_mgconn = NULL;
    int m_response_status = 0;
    extram::string m_response_statusmsg;
    extram::string m_response_body;
    std::list<std::pair<extram::string, extram::string>> m_response_headers;
  };


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
    void  DuktapeRequestCallback(DuktapeObject* instance, const char* method, void* data);

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
