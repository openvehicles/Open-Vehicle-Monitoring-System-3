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
static const char *TAG = "ovms-duktape";

#include <string>
#include <fstream>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <esp_task_wdt.h>
#include "ovms_malloc.h"
#include "ovms_module.h"
#include "ovms_duktape.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "console_async.h"
#include "buffered_shell.h"
#include "ovms_netmanager.h"
#include "ovms_boot.h"

#ifdef CONFIG_OVMS_COMP_PLUGINS
#include "ovms_plugins.h"
#endif // #ifdef CONFIG_OVMS_COMP_PLUGINS

using namespace std::placeholders;

OvmsDuktape MyDuktape __attribute__ ((init_priority (1000)));
OvmsWriter* duktapewriter = NULL;

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM
  #include "umm_malloc.c"
  static void *umm_memory = NULL;
#endif

////////////////////////////////////////////////////////////////////////////////
// Duktape utility functions

static duk_int_t duk__eval_module_source(duk_context *ctx, void *udata);
static void duk__push_module_object(duk_context *ctx, const char *id, duk_bool_t main);

static duk_bool_t duk__get_cached_module(duk_context *ctx, const char *id)
  {
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	if (duk_get_prop_string(ctx, -1, id))
    {
		duk_remove(ctx, -2);
		duk_remove(ctx, -2);
		return 1;
  	}
  else
    {
		duk_pop_3(ctx);
		return 0;
	  }
  }

/* Place a `module` object on the top of the value stack into the require cache
 * based on its `.id` property.  As a convenience to the caller, leave the
 * object on top of the value stack afterwards.
 */
static void duk__put_cached_module(duk_context *ctx)
  {
	/* [ ... module ] */

	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_dup(ctx, -3);

	/* [ ... module stash req_cache module ] */

	(void) duk_get_prop_string(ctx, -1, "id");
	duk_dup(ctx, -2);
	duk_put_prop(ctx, -4);

	duk_pop_3(ctx);  /* [ ... module ] */
  }

static void duk__del_cached_module(duk_context *ctx, const char *id)
  {
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_del_prop_string(ctx, -1, id);
	duk_pop_2(ctx);
  }

static duk_ret_t duk__handle_require(duk_context *ctx)
  {
	/*
	 *  Value stack handling here is a bit sloppy but should be correct.
	 *  Call handling will clean up any extra garbage for us.
	 */

	const char *id;
	const char *parent_id;
	duk_idx_t module_idx;
	duk_idx_t stash_idx;
	duk_int_t ret;

	duk_push_global_stash(ctx);
	stash_idx = duk_normalize_index(ctx, -1);

	duk_push_current_function(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "moduleId");
	parent_id = duk_require_string(ctx, -1);
	(void) parent_id;  /* not used directly; suppress warning */

	/* [ id stash require parent_id ] */

	id = duk_require_string(ctx, 0);

	(void) duk_get_prop_string(ctx, stash_idx, "\xff" "modResolve");
	duk_dup(ctx, 0);   /* module ID */
	duk_dup(ctx, -3);  /* parent ID */
	duk_call(ctx, 2);

	/* [ ... stash ... resolved_id ] */

	id = duk_require_string(ctx, -1);

	if (duk__get_cached_module(ctx, id))
    {
		goto have_module;  /* use the cached module */
	  }

	duk__push_module_object(ctx, id, 0 /*main*/);
	duk__put_cached_module(ctx);  /* module remains on stack */

	/*
	 *  From here on out, we have to be careful not to throw.  If it can't be
	 *  avoided, the error must be caught and the module removed from the
	 *  require cache before rethrowing.  This allows the application to
	 *  reattempt loading the module.
	 */

	module_idx = duk_normalize_index(ctx, -1);

	/* [ ... stash ... resolved_id module ] */

	(void) duk_get_prop_string(ctx, stash_idx, "\xff" "modLoad");
	duk_dup(ctx, -3);  /* resolved ID */
	(void) duk_get_prop_string(ctx, module_idx, "exports");
	duk_dup(ctx, module_idx);
	ret = duk_pcall(ctx, 3);
	if (ret != DUK_EXEC_SUCCESS)
    {
		duk__del_cached_module(ctx, id);
		duk_throw(ctx);  /* rethrow */
	  }

	if (duk_is_string(ctx, -1))
    {
		duk_int_t ret;

		/* [ ... module source ] */
		ret = duk_safe_call(ctx, duk__eval_module_source, NULL, 2, 1);
		if (ret != DUK_EXEC_SUCCESS)
      {
			duk__del_cached_module(ctx, id);
			duk_throw(ctx);  /* rethrow */
		  }
	  }
  else if (duk_is_undefined(ctx, -1))
    {
		duk_pop(ctx);
  	}
  else
    {
		duk__del_cached_module(ctx, id);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "invalid module load callback return value");
	  }

	/* fall through */

 have_module:
	/* [ ... module ] */

	(void) duk_get_prop_string(ctx, -1, "exports");
	return 1;
  }

static void duk__push_require_function(duk_context *ctx, const char *id)
  {
	duk_push_c_function(ctx, duk__handle_require, 1);
	duk_push_string(ctx, "name");
	duk_push_string(ctx, "require");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
	duk_push_string(ctx, id);
	duk_put_prop_string(ctx, -2, "\xff" "moduleId");

	/* require.cache */
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_put_prop_string(ctx, -3, "cache");
	duk_pop(ctx);

	/* require.main */
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "mainModule");
	duk_put_prop_string(ctx, -3, "main");
	duk_pop(ctx);
  }

static void duk__push_module_object(duk_context *ctx, const char *id, duk_bool_t main)
  {
	duk_push_object(ctx);

	/* Set this as the main module, if requested */
	if (main)
    {
		duk_push_global_stash(ctx);
		duk_dup(ctx, -2);
		duk_put_prop_string(ctx, -2, "\xff" "mainModule");
		duk_pop(ctx);
	  }

	/* Node.js uses the canonicalized filename of a module for both module.id
	 * and module.filename.  We have no concept of a file system here, so just
	 * use the module ID for both values.
	 */
	duk_push_string(ctx, id);
	duk_dup(ctx, -1);
	duk_put_prop_string(ctx, -3, "filename");
	duk_put_prop_string(ctx, -2, "id");

	/* module.exports = {} */
	duk_push_object(ctx);
	duk_put_prop_string(ctx, -2, "exports");

	/* module.loaded = false */
	duk_push_false(ctx);
	duk_put_prop_string(ctx, -2, "loaded");

	/* module.require */
	duk__push_require_function(ctx, id);
	duk_put_prop_string(ctx, -2, "require");
  }

static duk_int_t duk__eval_module_source(duk_context *ctx, void *udata)
  {
	const char *src;

	/*
	 *  Stack: [ ... module source ]
	 */

	(void) udata;

	/* Wrap the module code in a function expression.  This is the simplest
	 * way to implement CommonJS closure semantics and matches the behavior of
	 * e.g. Node.js.
	 */
	duk_push_string(ctx, "(function(exports,require,module,__filename,__dirname){");
	src = duk_require_string(ctx, -2);
	duk_push_string(ctx, (src[0] == '#' && src[1] == '!') ? "//" : "");  /* Shebang support. */
	duk_dup(ctx, -3);  /* source */
	duk_push_string(ctx, "\n})");  /* Newline allows module last line to contain a // comment. */
	duk_concat(ctx, 4);

	/* [ ... module source func_src ] */

	(void) duk_get_prop_string(ctx, -3, "filename");
	duk_compile(ctx, DUK_COMPILE_EVAL);
	duk_call(ctx, 0);

	/* [ ... module source func ] */

	/* Set name for the wrapper function. */
	duk_push_string(ctx, "name");
	duk_push_string(ctx, "main");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);

	/* call the function wrapper */
	(void) duk_get_prop_string(ctx, -3, "exports");   /* exports */
	(void) duk_get_prop_string(ctx, -4, "require");   /* require */
	duk_dup(ctx, -5);                                 /* module */
	(void) duk_get_prop_string(ctx, -6, "filename");  /* __filename */
	duk_push_undefined(ctx);                          /* __dirname */
	duk_call(ctx, 5);

	/* [ ... module source result(ignore) ] */

	/* module.loaded = true */
	duk_push_true(ctx);
	duk_put_prop_string(ctx, -4, "loaded");

	/* [ ... module source retval ] */

	duk_pop_2(ctx);

	/* [ ... module ] */

	return 1;
  }

/* Load a module as the 'main' module. */
duk_ret_t duk_module_node_peval_main(duk_context *ctx, const char *path)
  {
	/*
	 *  Stack: [ ... source ]
	 */

	duk__push_module_object(ctx, path, 1 /*main*/);
	/* [ ... source module ] */

	duk_dup(ctx, 0);
	/* [ ... source module source ] */

	return duk_safe_call(ctx, duk__eval_module_source, NULL, 2, 1);
  }

void duk_module_node_init(duk_context *ctx)
  {
	/*
	 *  Stack: [ ... options ] => [ ... ]
	 */

	duk_idx_t options_idx;

	duk_require_object_coercible(ctx, -1);  /* error before setting up requireCache */
	options_idx = duk_require_normalize_index(ctx, -1);

	/* Initialize the require cache to a fresh object. */
	duk_push_global_stash(ctx);
	duk_push_bare_object(ctx);
	duk_put_prop_string(ctx, -2, "\xff" "requireCache");
	duk_pop(ctx);

	/* Stash callbacks for later use.  User code can overwrite them later
	 * on directly by accessing the global stash.
	 */
	duk_push_global_stash(ctx);
	duk_get_prop_string(ctx, options_idx, "resolve");
	duk_require_function(ctx, -1);
	duk_put_prop_string(ctx, -2, "\xff" "modResolve");
	duk_get_prop_string(ctx, options_idx, "load");
	duk_require_function(ctx, -1);
	duk_put_prop_string(ctx, -2, "\xff" "modLoad");
	duk_pop(ctx);

	/* Stash main module. */
	duk_push_global_stash(ctx);
	duk_push_undefined(ctx);
	duk_put_prop_string(ctx, -2, "\xff" "mainModule");
	duk_pop(ctx);

	/* register `require` as a global function. */
	duk_push_global_object(ctx);
	duk_push_string(ctx, "require");
	duk__push_require_function(ctx, "");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE |
	                      DUK_DEFPROP_SET_WRITABLE |
	                      DUK_DEFPROP_SET_CONFIGURABLE);
	duk_pop(ctx);

	duk_pop(ctx);  /* pop argument */
  }

////////////////////////////////////////////////////////////////////////////////
// Duktape engine task

void DukTapeLaunchTask(void *pvParameters)
  {
  OvmsDuktape* me = (OvmsDuktape*)pvParameters;

  me->DukTapeTask();
  }

void* DukOvmsAlloc(void *udata, duk_size_t size)
  {
  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM
    return umm_malloc(size);
  #else
    return ExternalRamMalloc(size);
  #endif
  }

void* DukOvmsRealloc(void *udata, void *ptr, duk_size_t size)
  {
  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM
    return umm_realloc(ptr, size);
  #else
    return ExternalRamRealloc(ptr, size);
  #endif
  }

void DukOvmsFree(void *udata, void *ptr)
  {
  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM
    umm_free(ptr);
  #else
    free(ptr);
  #endif
  }

void DukOvmsFatalHandler(void *udata, const char *msg)
  {
  ESP_LOGE(TAG, "Duktape fatal error: %s",msg);
  abort();
  }

void DukOvmsErrorHandler(duk_context *ctx, duk_idx_t err_idx, OvmsWriter *writer, const char *filename)
  {
  const char *error, *stack = NULL;
  int linenumber = 0;
  if (!filename) filename = "eval";

  duk_require_stack(ctx, 1);

  if (duk_is_error(ctx, err_idx))
    {
    duk_get_prop_string(ctx, err_idx, "fileName");
    if (duk_is_string(ctx, -1))
      filename = duk_get_string(ctx, -1);
    duk_pop(ctx);
    duk_get_prop_string(ctx, err_idx, "lineNumber");
    linenumber = duk_get_number_default(ctx, -1, 0);
    duk_pop(ctx);
    duk_get_prop_string(ctx, err_idx, "stack");
    if (duk_is_string(ctx, -1))
      stack = duk_get_string(ctx, -1);
    duk_pop(ctx);
    }

  error = stack ? stack : duk_safe_to_string(ctx, err_idx);

  if (writer)
    {
    // some errors have line numbers
    if (linenumber == 0 || strstr(error, "(line "))
      writer->printf("ERROR: %s\n", error);
    else
      writer->printf("ERROR: %s (line %d)\n", error, linenumber);
    }
  else
    {
    ESP_LOGE(TAG, "[%s:%d] %s", filename, linenumber, error);
    }
  }

static duk_ret_t DukOvmsResolveModule(duk_context *ctx)
  {
  const char *module_id;
  const char *parent_id;

  module_id = duk_require_string(ctx, 0);
  parent_id = duk_require_string(ctx, 1);

  duk_push_sprintf(ctx, "%s.js", module_id);
  ESP_LOGD(TAG,"resolve_cb: id:'%s', parent-id:'%s', resolve-to:'%s'",
                  module_id, parent_id, duk_get_string(ctx, -1));
  return 1;
  }

static duk_ret_t DukOvmsLoadModule(duk_context *ctx)
  {
  const char *filename;
  const char *module_id;

  module_id = duk_require_string(ctx, 0);
  duk_get_prop_string(ctx, 2, "filename");
  filename = duk_require_string(ctx, -1);

  if (strncmp(module_id,"int/",4)==0)
    {
    // Load internal module
    std::string name(module_id+4,strlen(module_id)-7);
    duktape_registermodule_t* mod = MyDuktape.FindDuktapeModule(name.c_str());
    if (mod == NULL)
      {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "load_cb: cannot find internal module: %s", module_id);
      return 0;
      }
    else
      {
      ESP_LOGD(TAG,"load_cb: id:'%s' internally provided %s (%d bytes)", module_id, filename, mod->length);
      duk_push_lstring(ctx, mod->start, mod->length);
      MyDuktape.NotifyDuktapeModuleLoad(filename);
      return 1;
      }
    }

  ESP_LOGD(TAG,"load_cb: id:'%s', filename:'%s'", module_id, filename);

  // Resolve the module path
  std::string path;
  FILE* sf;
  if (strncmp(filename,"plugin/",7)==0)
    {
    path = std::string("/store/plugins/");
    path.append(filename+7);
    sf = fopen(path.c_str(), "r");
    }
  else
    {
    path = std::string("/store/scripts/");
    path.append(filename);
    sf = fopen(path.c_str(), "r");
    if (sf == NULL)
      {
      path = std::string("/sd/scripts/");
      path.append(filename);
      sf = fopen(path.c_str(), "r");
      }
    }

  // Go ahead and load the module
  if (sf == NULL)
    {
    duk_error(ctx, DUK_ERR_TYPE_ERROR, "load_cb: cannot find module: %s", module_id);
    return 0;
    }
  else
    {
    fseek(sf,0,SEEK_END);
    long slen = ftell(sf);
    fseek(sf,0,SEEK_SET);
    char *script = new char[slen+1];
    memset(script,0,slen+1);
    fread(script,1,slen,sf);
    duk_push_string(ctx, script);
    delete [] script;
    fclose(sf);
    ESP_LOGD(TAG,"load_cb: id:'%s' vfs provided %s (%lu bytes)", module_id, filename, slen);
    MyDuktape.NotifyDuktapeModuleLoad(filename);
    }

  return 1;
  }

////////////////////////////////////////////////////////////////////////////////
// DuktapeObject

/**
 * Construct empty (uncoupled) DuktapeObject
 */
DuktapeObject::DuktapeObject()
  {
  }

/**
 * Construct DuktapeObject coupled to stack object
 *  Stack: unchanged
 */
DuktapeObject::DuktapeObject(duk_context *ctx, int obj_idx)
  {
  Couple(ctx, obj_idx);
  assert(IsCoupled());
  }

/**
 * Destruct DuktapeObject
 *  Note: no JS context here, override Finalize() for JS destructor
 */
DuktapeObject::~DuktapeObject()
  {
  }

/**
 * Ref: increment reference count
 */
void DuktapeObject::Ref()
  {
  OvmsRecMutexLock lock(&m_mutex);
  m_refcnt++;
  // ESP_LOGD(TAG, "DuktapeObject::Ref cnt=%d", m_refcnt);
  }

/**
 * Unref: decrement reference count, delete self if no references left
 *  Returns true if instance has been deleted
 */
bool DuktapeObject::Unref()
  {
  Lock();
  assert(m_refcnt > 0);
  m_refcnt--;
  // ESP_LOGD(TAG, "DuktapeObject::Unref cnt=%d", m_refcnt);
  if (m_refcnt == 0)
    {
    Unlock();
    delete this;
    return true;
    }
  Unlock();
  return false;
  }

void DuktapeObject::AfterCouple(duk_context *ctx, int obj_idx)
  {
  }

/**
 * Couple: set hidden instance pointer & finalizer on JS object
 *  Stack: unchanged
 */
bool DuktapeObject::Couple(duk_context *ctx, int obj_idx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  duk_require_object(ctx, obj_idx);
  m_object = duk_get_heapptr(ctx, obj_idx);
  if (!m_object) return false;
  duk_require_stack(ctx, 3);
  duk_push_heapptr(ctx, m_object);
  // set instance pointer:
  duk_push_pointer(ctx, this);
  duk_put_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("duktapeObject"));
  // set finalizer:
  duk_push_c_function(ctx, DuktapeFinalizer, 2 /*(object,heapDestruct)*/);
  duk_set_finalizer(ctx, -2);
  duk_pop(ctx);
  Ref();
  AfterCouple(ctx, obj_idx);
  return true;
  }

 void DuktapeObject::BeforeDecouple(duk_context *ctx)
  {
  }

/**
 * Decouple: clear hidden instance pointer & finalizer on JS object
 *  Stack: unchanged
 */
void DuktapeObject::Decouple(duk_context *ctx)
  {
    {
    OvmsRecMutexLock lock(&m_mutex);
    if (!m_object) return;

    BeforeDecouple(ctx);

    duk_require_stack(ctx, 3);
    duk_push_heapptr(ctx, m_object);
    // clear instance pointer:
    duk_del_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL("duktapeObject"));
    // clear finalizer:
    duk_push_undefined(ctx);
    duk_set_finalizer(ctx, -2);
    duk_pop(ctx);
    m_object = NULL;
    }
  Unref();
  }

/**
 * GetInstance: get hidden instance pointer from JS object
 *  Returns null if no instance is attached
 *  Stack: unchanged
 */
DuktapeObject* DuktapeObject::GetInstance(duk_context *ctx, int obj_idx)
  {
  DuktapeObject* instance = NULL;
  duk_require_stack(ctx, 2);
  if (duk_get_prop_string(ctx, obj_idx, DUK_HIDDEN_SYMBOL("duktapeObject")))
    instance = (DuktapeObject*)duk_get_pointer(ctx, -1);
  duk_pop(ctx);
  return instance;
  }

/**
 * DuktapeFinalizer: Duktape finalizer (destructor) entry
 */
duk_ret_t DuktapeObject::DuktapeFinalizer(duk_context *ctx)
  {
  DuktapeObject* me = GetInstance(ctx, 0);
  bool heapDestruct = duk_opt_boolean(ctx, 1, false);
  if (me) me->Finalize(ctx, heapDestruct);
  return 0;
  }

/**
 * Finalize: JS destructor, called by garbage collection
 */
void DuktapeObject::Finalize(duk_context *ctx, bool heapDestruct)
  {
  // ESP_LOGD(TAG, "DuktapeObject::Finalize heapDestruct=%d", heapDestruct);
  Decouple(ctx); // deletes self if no reference left
  }

/**
 * Push: push coupled JS object onto stack
 *  Stack: [ … ] → [ … obj ]
 */
duk_idx_t DuktapeObject::Push(duk_context *ctx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!m_object) return DUK_INVALID_INDEX;
  duk_require_stack(ctx, 1);
  return duk_push_heapptr(ctx, m_object);
  }

/**
 * Register object with the global stash to prevent garbage collection
 *  Stack: unchanged
 */
void DuktapeObject::Register(duk_context *ctx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!m_object || m_registered) return;
  // ESP_LOGD(TAG, "DuktapeObject::Register @globalStash");
  duk_require_stack(ctx, 3);

  // get registry:
  duk_push_global_stash(ctx);
  if (!duk_get_prop_string(ctx, -1, "duktapeObjectRegistry"))
    {
    // create registry:
    duk_pop(ctx);
    duk_push_object(ctx);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "duktapeObjectRegistry");
    }

  // register using instance address as key:
  duk_push_pointer(ctx, this);
  duk_push_heapptr(ctx, m_object);
  duk_put_prop(ctx, -3);

  duk_pop_2(ctx); // registry & global stash

  m_registry = NULL;
  m_registered = true;
  }

/**
 * Register with a specific registry object on the stack
 *  Note: you need to ensure the object is stable until deregistration!
 *  Stack: unchanged
 */
void DuktapeObject::Register(duk_context *ctx, int obj_idx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!m_object || m_registered) return;
  // ESP_LOGD(TAG, "DuktapeObject::Register @object");
  duk_require_object(ctx, obj_idx);
  duk_require_stack(ctx, 3);

  // register using instance address as key:
  duk_dup(ctx, obj_idx);
  duk_push_pointer(ctx, this);
  duk_push_heapptr(ctx, m_object);
  duk_put_prop(ctx, -3);
  duk_pop(ctx);

  m_registry = duk_get_heapptr(ctx, obj_idx);
  m_registered = true;
  }

/**
 * Deregister:
 *  Stack: unchanged
 */
void DuktapeObject::Deregister(duk_context *ctx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!m_object || !m_registered) return;
  // ESP_LOGD(TAG, "DuktapeObject::Deregister");
  duk_require_stack(ctx, 3);

  if (m_registry)
    {
    duk_push_heapptr(ctx, m_registry);
    duk_push_pointer(ctx, this);
    duk_del_prop(ctx, -2);
    duk_pop(ctx); // registry
    }
  else
    {
    duk_push_global_stash(ctx);
    if (duk_get_prop_string(ctx, -1, "duktapeObjectRegistry"))
      {
      duk_push_pointer(ctx, this);
      duk_del_prop(ctx, -2);
      }
    duk_pop_2(ctx); // registry & global stash
    }

  m_registry = NULL;
  m_registered = false;
  }

/**
 * RequestCallback: request object method call by Duktape context
 * \param method The name of the method to call on the object.
 * \param data The parameter pusher. (Deleted after call succeeds/push fails)
 * \return true if succeeded in requesting the callback.
 */
bool DuktapeObject::RequestCallback(const char* method, DuktapeCallbackParameter* params /*=NULL*/)
  {
  if (MyDuktape.DukTapeAvailable())
    {
    Ref();
    bool res = MyDuktape.DuktapeRequestCallback(this, method, params);

    if (!res)
      Unref();

    return res;
    }
  else
    {
    ESP_LOGE(TAG, "DuktapeObject::RequestCallback(%s) failed: Duktape not available", method);
    if (params != nullptr)
      delete params;
    return false;
    }
  }

/**
 * DuktapeCallback: call object method in Duktape context
 */
duk_ret_t DuktapeObject::DuktapeCallback(duk_context *ctx, duktape_queue_t &msg)
  {
  duk_ret_t res = -1;
  if (ctx)
    res = CallMethod(ctx, msg.body.dt_callback.method, msg.body.dt_callback.params);
  else
    ESP_LOGE(TAG, "DuktapeObject::DuktapeCallback(%s) failed: Duktape shutdown", msg.body.dt_callback.method);
  Unref();
  return res;
  }

/**
 * CallMethod: execute object method in Duktape context
 * \param ctx Duktape context.
 * \param method Method to call.
 * \param params Parameter pusher. Deleted after call.
 */
duk_ret_t DuktapeObject::CallMethod(duk_context *ctx, const char* method, DuktapeCallbackParameter* params)
  {
  if (!ctx)
    {
    // data paramter ownership delegated.
    RequestCallback(method, params);
    return 0;
    }
  OvmsRecMutexLock lock(&m_mutex);
  if (!IsCoupled())
    {
    if (params != nullptr)
      delete params;
    return 0;
    }
  int entry_top = duk_get_top(ctx);
  int paramcount = 0;
  duk_idx_t objidx = Push(ctx);

  duk_push_string(ctx, method);
  if (params != nullptr)
    paramcount = params->Push(ctx);
  int call_res = duk_pcall_prop(ctx, objidx, paramcount);
  if (call_res != 0)
    {
    DukOvmsErrorHandler(ctx, call_res);
    }
  // discard return values if any + this:
  duk_pop_n(ctx, duk_get_top(ctx) - entry_top);
  if (params != nullptr)
    delete params;
  return 0;
  }

////////////////////////////////////////////////////////////////////////////////
// DuktapeObjectRegistration
//
// An object to record a Duktape object registration for OVMS

DuktapeObjectRegistration::DuktapeObjectRegistration(const char* name)
  {
  m_name = name;
  }

DuktapeObjectRegistration::~DuktapeObjectRegistration()
  {
  }

const char* DuktapeObjectRegistration::GetName()
  {
  return m_name;
  }

void DuktapeObjectRegistration::RegisterDuktapeFunction(duk_c_function func, duk_idx_t nargs, const char* name)
  {
  duktape_registerfunction_t* fn = new duktape_registerfunction_t;
  fn->func = func;
  fn->nargs = nargs;
  m_fnmap[name] = fn;
  }
void DuktapeObjectRegistration::RegisterDuktapeObject(DuktapeObjectRegistration* obj, const char *name)
  {
  m_obmap[name] = obj;
  }

void DuktapeObjectRegistration::PushThisAsObject(duk_context* ctx)
  {
  duk_push_object(ctx);

  DuktapeFunctionMap::iterator itm=m_fnmap.begin();
  while (itm!=m_fnmap.end())
    {
    const char* name = itm->first;
    duktape_registerfunction_t* fn = itm->second;
    ESP_LOGD(TAG,"Duktape: Pre-Registered object %s function %s",m_name,name);
    duk_push_c_function(ctx, fn->func, fn->nargs);
    duk_push_string(ctx, "name");
    duk_push_string(ctx, name);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);  /* Improve stacktraces by displaying function name */
    duk_put_prop_string(ctx, -2, name);
    ++itm;
    }
  for ( auto ito = m_obmap.begin(); ito != m_obmap.end(); ++ito )
    {
    ito->second->PushThisAsObject(ctx);
    duk_put_prop_string(ctx, -2, ito->first);
    }
  }

void DuktapeObjectRegistration::RegisterWithDuktape(duk_context* ctx)
  {
  PushThisAsObject(ctx);
  duk_put_global_string(ctx, m_name);
  }

////////////////////////////////////////////////////////////////////////////////
// DuktapeConsoleCommand

DuktapeConsoleCommand::DuktapeConsoleCommand(OvmsCommand* cmd, const char* module)
  : DuktapeObject(),
  m_cmd(cmd),
  m_module(module)
  {
  }

DuktapeConsoleCommand::~DuktapeConsoleCommand()
  {
  }

void DuktapeConsoleCommand::AfterCouple(duk_context *ctx, int obj_idx)
  {
  Register(ctx);
  }

void DuktapeConsoleCommand::BeforeDecouple(duk_context *ctx)
  {
  Deregister(ctx);
  }

////////////////////////////////////////////////////////////////////////////////
// OvmsDuktape
//
// This is the main OVMS Duktape registry and singleton utiltiy object

OvmsDuktape::OvmsDuktape()
  {
  ESP_LOGI(TAG, "Initialising DUKTAPE Registry (1000)");

  m_dukctx = NULL;
  m_duktaskid = NULL;
  m_duktaskqueue = NULL;

  // Register standard modules...
  extern const char mod_pubsub_js_start[]     asm("_binary_pubsub_js_start");
  extern const char mod_pubsub_js_end[]       asm("_binary_pubsub_js_end");
  RegisterDuktapeModule(mod_pubsub_js_start, mod_pubsub_js_end - mod_pubsub_js_start, "PubSub");

  extern const char mod_json_js_start[]     asm("_binary_json_js_start");
  extern const char mod_json_js_end[]       asm("_binary_json_js_end");
  RegisterDuktapeModule(mod_json_js_start, mod_json_js_end - mod_json_js_start, "JSON");

  // Start the DukTape task...
  m_duktaskqueue = xQueueCreate(CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_QUEUE_SIZE,sizeof(duktape_queue_t));
  xTaskCreatePinnedToCore(DukTapeLaunchTask, "OVMS DukTape",
                          CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_STACK, (void*)this,
                          CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_PRIORITY, &m_duktaskid, CORE(1));
  AddTaskToMap(m_duktaskid);
  }

OvmsDuktape::~OvmsDuktape()
  {
  MyEvents.DeregisterEvent(TAG);
  // Just in case. Should be handled by shutdown event.
  DukTapeUnload(false);
  }

void OvmsDuktape::EventSystemShuttingDown(std::string event, void* data)
  {
  if (m_dukctx)
    {
    MyBoot.ShutdownPending(TAG);
    // Register a special shut-down event to check shut-down
    MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsDuktape::Ticker1_Shutdown, this, _1, _2));

    duktape_queue_t dmsg;
    memset(&dmsg, 0, sizeof(dmsg));
    dmsg.type = DUKTAPE_shutdown;
    DuktapeDispatch(&dmsg);
    }
  }

void OvmsDuktape::Ticker1_Shutdown(std::string event, void* data)
  {
  if (!m_dukctx)
    {
    MyEvents.DeregisterEvent(TAG);
    MyBoot.ShutdownReady(TAG);
    }
  }

void OvmsDuktape::RegisterDuktapeFunction(duk_c_function func, duk_idx_t nargs, const char* name)
  {
  duktape_registerfunction_t* fn = new duktape_registerfunction_t;
  fn->func = func;
  fn->nargs = nargs;
  m_fnmap[name] = fn;
  }

void OvmsDuktape::RegisterDuktapeModule(const char* start, size_t length, const char* name)
  {
  duktape_registermodule_t* mn = new duktape_registermodule_t;
  mn->start = start;
  mn->length = length;
  m_modmap[name] = mn;
  }

duktape_registermodule_t* OvmsDuktape::FindDuktapeModule(const char* name)
  {
  auto mod = m_modmap.find(name);
  if (mod == m_modmap.end())
    { return NULL; }
  else
    { return mod->second; }
  }

void OvmsDuktape::RegisterDuktapeObject(DuktapeObjectRegistration* ob)
  {
  m_obmap[ob->GetName()] = ob;
  }

void OvmsDuktape::AutoInitDuktape()
  {
  // Registering the event here rather than the constructor since MyDuktape gets constructed before
  // MyEvents.
  MyEvents.RegisterEvent(TAG,"system.shuttingdown",std::bind(&OvmsDuktape::EventSystemShuttingDown, this, _1, _2));

  if (MyConfig.GetParamValueBool("auto", "scripting", true))
    {
    duktape_queue_t dmsg;
    memset(&dmsg, 0, sizeof(dmsg));
    dmsg.type = DUKTAPE_autoinit;
    DuktapeDispatch(&dmsg);
    }
  }

void OvmsDuktape::EventScript(std::string event, void* data)
  {
  if (!m_dukctx) return;

  // dispatch event to PubSub component:
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_event;
  dmsg.body.dt_event.name = strdup(event.c_str());
  dmsg.body.dt_event.data = NULL; // data unused, may also be invalid in async script execution
  if (!DuktapeDispatch(&dmsg, 0))
    {
    ESP_LOGE(TAG, "EventScript: event '%s' lost (queue overflow)", event.c_str());
    free((void*)dmsg.body.dt_event.name);
    }
  else
   {
   // event processing delayed?
   int qwait = uxQueueMessagesWaiting(m_duktaskqueue);
   if (qwait > 10)
     {
     ESP_LOGW(TAG, "EventScript: event '%s' delayed, queued at position %d/%d", event.c_str(),
       qwait, CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_QUEUE_SIZE);
     }
   }

  if (event == "ticker.60")
    {
    // request garbage collection once per minute:
    DuktapeCompact(false);
    }
  }

bool OvmsDuktape::DuktapeDispatch(duktape_queue_t* msg, TickType_t queuewait /*=portMAX_DELAY*/)
  {
  msg->waitcompletion = NULL;
  if (InDukTapeTask())
    {
    ProcessJob(*msg);
    }
  else if (xQueueSend(m_duktaskqueue, msg, queuewait) != pdPASS)
    {
    ESP_LOGW(TAG, "DuktapeDispatch: msg type %u lost, queue full", msg->type);
    return false;
    }
  return true;
  }

void OvmsDuktape::DuktapeDispatchWait(duktape_queue_t* msg)
  {
  if (InDukTapeTask())
    {
    ProcessJob(*msg);
    }
  else
    {
    msg->waitcompletion = xSemaphoreCreateBinary();
    xQueueSend(m_duktaskqueue, msg, portMAX_DELAY);
    xSemaphoreTake(msg->waitcompletion, portMAX_DELAY);
    vSemaphoreDelete(msg->waitcompletion);
    msg->waitcompletion = NULL;
    }
  }

void OvmsDuktape::DuktapeEvalNoResult(const char* text, OvmsWriter* writer, const char* filename /*=NULL*/)
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_evalnoresult;
  dmsg.writer = writer;
  dmsg.body.dt_evalnoresult.text = text;
  dmsg.body.dt_evalnoresult.filename = filename;
  DuktapeDispatchWait(&dmsg);
  }

float OvmsDuktape::DuktapeEvalFloatResult(const char* text, OvmsWriter* writer)
  {
  float result = 0;
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_evalfloatresult;
  dmsg.writer = writer;
  dmsg.body.dt_evalfloatresult.text = text;
  dmsg.body.dt_evalfloatresult.result = &result;
  DuktapeDispatchWait(&dmsg);
  return result;
  }

int OvmsDuktape::DuktapeEvalIntResult(const char* text, OvmsWriter* writer)
  {
  int result = 0;
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_evalintresult;
  dmsg.writer = writer;
  dmsg.body.dt_evalintresult.text = text;
  dmsg.body.dt_evalintresult.result = &result;
  DuktapeDispatchWait(&dmsg);
  return result;
  }

void OvmsDuktape::DuktapeReload()
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_reload;
  DuktapeDispatchWait(&dmsg);
  }

void OvmsDuktape::DuktapeCompact(bool wait /*=true*/)
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_compact;
  if (wait)
    DuktapeDispatchWait(&dmsg);
  else
    DuktapeDispatch(&dmsg, 0);
  }

bool OvmsDuktape::DuktapeRequestCallback(DuktapeObject* instance, const char* method, DuktapeCallbackParameter* params)
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_callback;
  dmsg.body.dt_callback.instance = instance;
  dmsg.body.dt_callback.method = method;
  dmsg.body.dt_callback.params = params;
  return DuktapeDispatch(&dmsg);
  }

bool OvmsDuktape::DuktapeRequestDelete(DuktapeObject* instance, bool decouple, bool deregister)
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));

  dmsg.type = DUKTAPE_deleteobject;
  dmsg.body.dt_deleteobject.instance = instance;
  dmsg.body.dt_deleteobject.decouple = decouple;
  dmsg.body.dt_deleteobject.deregister = deregister;
  return DuktapeDispatch(&dmsg);
  }

OvmsWriter* OvmsDuktape::GetDuktapeWriter()
  {
  return duktapewriter;
  }

void OvmsDuktape::DukGetCallInfo(duk_context *ctx, std::string *filename, int *linenumber, std::string *function)
  {
  duk_require_stack(ctx, 3);
  for (int i=-2; ; i--)
    {
    *filename = "";
    *function = "";
    duk_inspect_callstack_entry(ctx, i);
    if (duk_is_undefined(ctx, -1))
      {
      duk_pop(ctx);
      break;
      }
    duk_get_prop_string(ctx, -1, "lineNumber");
    *linenumber = duk_get_number_default(ctx, -1, 0);
    duk_pop(ctx);
    if (duk_get_prop_string(ctx, -1, "function"))
      {
      duk_get_prop_string(ctx, -1, "fileName");
      *filename = duk_get_string_default(ctx, -1, "");
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "name");
      *function = duk_get_string_default(ctx, -1, "");
      duk_pop(ctx);
      }
    duk_pop_2(ctx);
    // skip internal modules:
    if (!startsWith(*filename, "int/"))
      break;
    }
  }

void OvmsDuktape::NotifyDuktapeModuleLoad(const char* filename)
  {
  ESP_LOGD(TAG,"Duktape: module load: %s",filename);

  // We don't need to do anything special with this
  }

void OvmsDuktape::NotifyDuktapeModuleUnload(const char* filename)
  {
  ESP_LOGD(TAG,"Duktape: module unload: %s",filename);

  }

void OvmsDuktape::NotifyDuktapeModuleUnloadAll(duk_context *ctx)
  {
  if (!ctx || !InDukTapeTask())
    {
    return;
    }
  ESP_LOGD(TAG,"Duktape: module unload all");

  // We need to unregister all registered duktape commands
  // (for all modules)
  for (auto it = m_cmdmap.begin(); it != m_cmdmap.end(); ++it)
    {
    OvmsCommand* cmd = it->first;
    DuktapeConsoleCommand* dcc = it->second;
    ESP_LOGD(TAG,"Duktape: unregister command %s/%s",
      cmd->GetParent()->GetName(), cmd->GetName());
    dcc->Decouple(ctx);
    cmd->GetParent()->UnregisterCommand(cmd->GetName());
    }
  m_cmdmap.clear();
  }

void DukOvmsCommandRegisterRun(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  ESP_LOGD(TAG, "DukOvmsCommandRegisterRun(%s)",cmd->GetName());
  if (!MyDuktape.DukTapeContext())
    {
    writer->puts("Error: Duktape not running");
    return;
    }

  auto it = MyDuktape.m_cmdmap.find(cmd);
  if (it == MyDuktape.m_cmdmap.end())
    {
    ESP_LOGE(TAG, "Command '%s' cannot be found in registry",cmd->GetName());
    writer->printf("Error: Command '%s' is not in the registry",cmd->GetName());
    return;
    }
  DuktapeConsoleCommand* dcc = it->second;
  std::string name(cmd->GetName());

  for (OvmsCommand *parent = cmd->GetParent(); parent != nullptr; parent = parent->GetParent())
    {
    auto pname = parent->GetName();
    if (pname && *pname)
      {
      name.insert(0,"/");
      name.insert(0,pname);
      }
    }

  MyDuktape.DuktapeEvalCommand(writer, name.c_str(), dcc, argc, argv);
  }

void OvmsDuktape::DuktapeEvalCommand(OvmsWriter* writer, const char *command, DuktapeConsoleCommand* dcc, int argc, const char* const* argv)
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_command;
  dmsg.body.dt_command.command = command;
  dmsg.body.dt_command.dcc = dcc;
  dmsg.body.dt_command.argc = argc;
  dmsg.body.dt_command.argv = argv;
  dmsg.writer = writer;
  DuktapeDispatchWait(&dmsg);
  }

void OvmsDuktape::DukOvmsCommandRun(OvmsWriter* writer, const char * command, DuktapeConsoleCommand* dcc, int argc, const char* const* argv)
  {
  // Perform the callback
  if (!dcc->IsCoupled())
    {
    writer->printf("Error: Command '%s' has been uncoupled.\n", command);
    return;
    }
  else
    {
    duk_context *ctx = DukTapeContext();
    dcc->Push(ctx);

    duk_push_string(ctx, command);

    duk_idx_t arr_idx = duk_push_array(ctx);
    for (int idx = 0; idx < argc; ++idx)
      {
      duk_push_string(ctx, argv[idx]);
      duk_put_prop_index(ctx, arr_idx, idx);
      }
    duk_call(ctx, 2);
    int res = duk_get_int(ctx, -1);
    duk_pop(ctx);
    if (res) // Failed.
      writer->printf("Error: Command '%s' failed\n", command);
    }
  }

bool OvmsDuktape::RegisterDuktapeConsoleCommand(
  duk_context *ctx, int obj_idx,
  const char* filename,
  const char* parent,
  const char* name, const char* title,
  const char *usage, int min, int max)
  {
  ESP_LOGD(TAG,"Duktape: register console command '%s'",name);

  char ch = *name;
  bool isok =
    (ch >= 'a' && ch <= 'z') ||
    (ch >= 'A' && ch <= 'Z');
  if (!isok)
    {
    ESP_LOGE(TAG, "Duktape: Script %s - invalid user command name \"%s\" (must start with an alpha character)",
        filename, name );
    return false;
    }

  bool last_alphanum = false;
  for ( auto it = name+1; *it; ++it)
    {
    char ch = *it;
    bool isok =
      (ch >= 'a' && ch <= 'z') ||
      (ch >= 'A' && ch <= 'Z') ||
      (ch >= '0' && ch <= '9');
    last_alphanum = isok;
    isok = isok || (ch  == '-') || (ch == '_');
    if (!isok)
      {
      ESP_LOGE(TAG, "Duktape: Script %s - invalid user command name \"%s\" (invalid character '%c')",
          filename, name, ch );
      return false;
      }
    }
  if (!last_alphanum)
    {
    ESP_LOGE(TAG, "Duktape: Script %s - invalid user command name \"%s\" (must end with an alphanumeric character)",
        filename, name );
    return false;
    }

  // We need to register the specified console command, to
  // the specified module

  bool update = false;
  OvmsCommand* pcmd = MyCommandApp.FindCommandFullName(parent, true);
  if (pcmd == NULL)
    {
    ESP_LOGE(TAG,"Duktape: Script %s - failed to register user command \"%s\" on unknown parent \"%s\"",
        filename, name, parent );
    return false;
    }
  std::string full_parent_name = pcmd->GetFullName();
  switch (pcmd->GetType())
    {
    case OvmsCommandType::System:
      {
      ESP_LOGE(TAG,"Duktape: Script %s - failed to register user command \"%s\" against internal command \"%s\"",
          filename, name, full_parent_name.c_str());
      return false;
      }
    case OvmsCommandType::SystemAllowUserCmd:
      break;
    case OvmsCommandType::SystemAllowUsrDir:
      {
      // Shouldn't really happen.
      ESP_LOGE(TAG,"Duktape: Script %s - failed to register user command \"%s\" must use \"%s usr\"",
          filename, name, full_parent_name.c_str());
      return false;
      }
    case OvmsCommandType::SystemUsrDir:
    case OvmsCommandType::User:
      break; // OK
    }

  OvmsCommand *cmd = pcmd->FindCommand(name);
  if (cmd)
    {
    // Check if it's a js function which we can Update
    if ( (cmd->GetType() != OvmsCommandType::User) ||
         (m_cmdmap.find(cmd) == m_cmdmap.end()) )
      {
      ESP_LOGE(TAG,"Duktape: Script %s - failed trying to update known command \"%s %s\"",
          filename, full_parent_name.c_str(), name);
      return false;
      }
      cmd->UpdateCommand(title, usage, min, max);
      update = true;
    }
  else
    {
    cmd = pcmd->RegisterCommand(
      name, title, DukOvmsCommandRegisterRun, usage, min, max, true,
      nullptr, OvmsCommandType::User);
    }

  if (cmd != NULL)
    {
    ESP_LOGD(TAG,"Duktape: Script %s - %s Console Command Function for \"%s %s\"",
        filename, update ? "Updating" : "Registering new", full_parent_name.c_str(), name);
    DuktapeConsoleCommand *dcc = new DuktapeConsoleCommand( cmd, filename);
    dcc->Couple(ctx, obj_idx);
    if (update)
      {
      DuktapeConsoleCommand *dcco = m_cmdmap[cmd];
      m_cmdmap[cmd] = dcc;
      if (dcco)
        dcco->Decouple(ctx);
      }
    else
      m_cmdmap[cmd] = dcc;
    return true;
    }
  else
    {
    ESP_LOGE(TAG,"Duktape: Script %s - Unable to register Console command \"%s %s\"",
      filename, full_parent_name.c_str(), name);
    return false;
    }
  }

void OvmsDuktape::DukTapeUnload(bool unload_modules)
  {
  if (m_dukctx != NULL)
    {
    if (unload_modules)
      NotifyDuktapeModuleUnloadAll(m_dukctx);
    ESP_LOGI(TAG,"Duktape: Clearing existing context");
    duk_destroy_heap(m_dukctx);
    #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM
      if (umm_memory != NULL)
        {
        free(umm_memory);
        umm_memory = NULL;
        }
    #endif
    m_dukctx = NULL;
    }
  }

void OvmsDuktape::DukTapeInit()
  {
  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM
    // Allocate dedicated UMM heap space:
    int memsize = MyConfig.GetParamValueInt("module", "duktape.heapsize",
      CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM_DEFAULTSIZE) * 1024;
    if (memsize <= 0)
      memsize = 512 * 1024;
    else if (memsize > UMM_MAX_BLOCKS * UMM_BLOCK_BODY_SIZE)
      memsize = UMM_MAX_BLOCKS * UMM_BLOCK_BODY_SIZE;
    ESP_LOGI(TAG, "Duktape: Creating heap (size: %u bytes)", memsize);
    umm_memory = ExternalRamMalloc(memsize);
    if (!umm_memory)
      {
      ESP_LOGE(TAG, "Duktape: unable to allocate %u bytes for the heap", memsize);
      return;
      }
    umm_init_heap(umm_memory, memsize);
  #else
    ESP_LOGI(TAG, "Duktape: Creating heap");
  #endif

  m_dukctx = duk_create_heap(DukOvmsAlloc,
    DukOvmsRealloc,
    DukOvmsFree,
    this,
    DukOvmsFatalHandler);

  ESP_LOGI(TAG,"Duktape: Initialising module system");
  duk_push_object(m_dukctx);
  duk_push_c_function(m_dukctx, DukOvmsResolveModule, DUK_VARARGS);
  duk_put_prop_string(m_dukctx, -2, "resolve");
  duk_push_c_function(m_dukctx, DukOvmsLoadModule, DUK_VARARGS);
  duk_put_prop_string(m_dukctx, -2, "load");
  duk_module_node_init(m_dukctx);

  if (m_fnmap.size() > 0)
    {
    // We have some functions to register...
    DuktapeFunctionMap::iterator itm=m_fnmap.begin();
    while (itm!=m_fnmap.end())
      {
      const char* name = itm->first;
      duktape_registerfunction_t* fn = itm->second;
      ESP_LOGD(TAG,"Duktape: Pre-Registered function %s",name);
      duk_push_c_function(m_dukctx, fn->func, fn->nargs);
      duk_put_global_string(m_dukctx, name);
      ++itm;
      }
    }

  if (m_obmap.size() > 0)
    {
    // We have some objects to register...
    DuktapeObjectMap::iterator itm=m_obmap.begin();
    while (itm!=m_obmap.end())
      {
      const char* name = itm->first;
      DuktapeObjectRegistration* ob = itm->second;
      ESP_LOGD(TAG,"Duktape: Pre-Registered object %s",name);
      ob->RegisterWithDuktape(m_dukctx);
      ++itm;
      }
    }

  // Load registered modules
  if (m_modmap.size() > 0)
    {
    // We have some modules to register...
    DuktapeModuleMap::iterator itm=m_modmap.begin();
    while (itm!=m_modmap.end())
      {
      const char* name = itm->first;
      ESP_LOGD(TAG,"Duktape: Pre-Registered module %s",name);
      std::string loadfn("(function(){");
      loadfn.append(name);
      loadfn.append("=require(\"int/");
      loadfn.append(name);
      loadfn.append("\");})();");
      duk_push_string(m_dukctx, loadfn.c_str());
      if (duk_peval(m_dukctx) != 0)
        {
        ESP_LOGE(TAG,"Duktape: %s",duk_safe_to_string(m_dukctx, -1));
        }
      duk_pop(m_dukctx);
      ++itm;
      }
    }

  #ifdef CONFIG_OVMS_COMP_PLUGINS
  // Plugins
  MyPluginStore.LoadEnabledModules(EL_MODULE);
  #endif // #ifdef CONFIG_OVMS_COMP_PLUGINS

  // ovmsmain
  FILE* sf = fopen("/store/scripts/ovmsmain.js", "r");
  if (sf != NULL)
    {
    fseek(sf,0,SEEK_END);
    long slen = ftell(sf);
    fseek(sf,0,SEEK_SET);
    char *script = new char[slen+1];
    memset(script,0,slen+1);
    fread(script,1,slen,sf);
    duk_push_string(m_dukctx, script);
    delete [] script;
    fclose(sf);
    ESP_LOGI(TAG,"Duktape: Executing ovmsmain.js");
    NotifyDuktapeModuleLoad("ovmsmain.js");
    duk_module_node_peval_main(m_dukctx, "ovmsmain.js");
    NotifyDuktapeModuleUnload("ovmsmain.js");
    }
  }

void OvmsDuktape::DukTapeTask()
  {
  duktape_queue_t msg;

  ESP_LOGI(TAG,"Duktape: Scripting task is running");
  esp_task_wdt_add(NULL); // WATCHDOG is active for this task

  while(1)
    {
    if (xQueueReceive(m_duktaskqueue, &msg, pdMS_TO_TICKS(5000))==pdTRUE)
      {
      esp_task_wdt_reset(); // Reset WATCHDOG timer for this task
      ProcessJob(msg);
      if (msg.waitcompletion)
        {
        // Signal the completion...
        xSemaphoreGive(msg.waitcompletion);
        }
      }
    esp_task_wdt_reset(); // Reset WATCHDOG timer for this task
    }
  }

void OvmsDuktape::ProcessJob(duktape_queue_t& msg)
  {
  duktapewriter = msg.writer;

  switch(msg.type)
    {
    case DUKTAPE_autoinit:
      {
      // Auto init
      DukTapeInit();
      }
      break;

    case DUKTAPE_reload:
      {
      // Reload DUKTAPE engine
      DukTapeUnload();
      DukTapeInit();
      }
      break;

    case DUKTAPE_compact:
      {
      // Compact DUKTAPE memory
      if (m_dukctx != NULL)
        {
        ESP_LOGV(TAG,"Duktape: Compacting DukTape memory");
        uint32_t ts = esp_log_timestamp();
        duk_gc(m_dukctx, 0);
        duk_gc(m_dukctx, 0);
        ESP_LOGD(TAG, "Duktape: Compacting DukTape memory done in %" PRIu32 " ms", esp_log_timestamp()-ts);
        }
      }
      break;

    case DUKTAPE_event:
      {
      // Event
      if (m_dukctx != NULL)
        {
        // Deliver the event to DUKTAPE
        uint32_t ts = esp_log_timestamp();
        duk_require_stack(m_dukctx, 5);
        duk_get_global_string(m_dukctx, "PubSub");
        duk_get_prop_string(m_dukctx, -1, "publish");
        duk_dup(m_dukctx, -2);  /* this binding = process */
        duk_push_string(m_dukctx, msg.body.dt_event.name);
        duk_push_string(m_dukctx, "");
        if (duk_pcall_method(m_dukctx, 2) != 0)
          {
          DukOvmsErrorHandler(m_dukctx, -1);
          }
        duk_pop_2(m_dukctx);
        ts = esp_log_timestamp() - ts;
        if (ts > 1000)
          ESP_LOGW(TAG, "Duktape: event handling for '%s' took %" PRIu32 " ms", msg.body.dt_event.name, ts);
        }
      }
      free((void*)msg.body.dt_event.name);
      break;

    case DUKTAPE_evalnoresult:
      if (m_dukctx != NULL)
        {
        // Execute script text (without result)
        const char* filename = msg.body.dt_evalnoresult.filename;
        if (!filename) filename = "eval";
        duk_push_string(m_dukctx, msg.body.dt_evalnoresult.text);
        duk_push_string(m_dukctx, filename);
        if (duk_pcompile(m_dukctx, DUK_COMPILE_EVAL) != 0 || duk_pcall(m_dukctx, 0) != 0)
          {
          DukOvmsErrorHandler(m_dukctx, -1, msg.writer, filename);
          }
        duk_pop(m_dukctx);
        }
      else
        {
        if (msg.writer)
          msg.writer->puts("ERROR: Duktape not started");
        else
          ESP_LOGE(TAG, "Duktape not started");
        }
      break;

    case DUKTAPE_evalfloatresult:
      if (m_dukctx != NULL)
        {
        // Execute script text (float result)
        duk_push_string(m_dukctx, msg.body.dt_evalfloatresult.text);
        if (duk_peval(m_dukctx) != 0)
          {
          DukOvmsErrorHandler(m_dukctx, -1, msg.writer);
          *msg.body.dt_evalfloatresult.result = 0;
          }
        else
          {
          *msg.body.dt_evalfloatresult.result = (float)duk_to_number(m_dukctx,-1);
          }
        duk_pop(m_dukctx);
        }
      else
        {
        if (msg.writer)
          msg.writer->puts("ERROR: Duktape not started");
        else
          ESP_LOGE(TAG, "Duktape not started");
        *msg.body.dt_evalfloatresult.result = 0;
        }
      break;

    case DUKTAPE_evalintresult:
      if (m_dukctx != NULL)
        {
        // Execute script text (int result)
        duk_push_string(m_dukctx, msg.body.dt_evalintresult.text);
        if (duk_peval(m_dukctx) != 0)
          {
          DukOvmsErrorHandler(m_dukctx, -1, msg.writer);
          *msg.body.dt_evalintresult.result = 0;
          }
        else
          {
          *msg.body.dt_evalintresult.result = (int)duk_to_int(m_dukctx,-1);
          }
        duk_pop(m_dukctx);
        }
      else
        {
        if (msg.writer)
          msg.writer->puts("ERROR: Duktape not started");
        else
          ESP_LOGE(TAG, "Duktape not started");
        *msg.body.dt_evalintresult.result = 0;
        }
      break;

    case DUKTAPE_callback:
      {
      // DuktapeObject callback (without result)
      DuktapeObject* dto = msg.body.dt_callback.instance;
      dto->DuktapeCallback(m_dukctx, msg);
      }
      break;
    case DUKTAPE_command:
      {
      DukOvmsCommandRun(msg.writer, msg.body.dt_command.command,
          msg.body.dt_command.dcc, msg.body.dt_command.argc, msg.body.dt_command.argv);
      }
      break;
    case DUKTAPE_shutdown:
      {
      DukTapeUnload();
      }
      break;
    case DUKTAPE_deleteobject:
      {
      if (msg.body.dt_deleteobject.deregister)
        msg.body.dt_deleteobject.instance->Deregister(m_dukctx);
      if (msg.body.dt_deleteobject.decouple)
        msg.body.dt_deleteobject.instance->Decouple(m_dukctx);
      msg.body.dt_deleteobject.instance->Unref();
      }break;

    default:
      ESP_LOGE(TAG,"Duktape: Unrecognised msg type 0x%04x",msg.type);
      break;
    }

  duktapewriter = NULL;
  }
