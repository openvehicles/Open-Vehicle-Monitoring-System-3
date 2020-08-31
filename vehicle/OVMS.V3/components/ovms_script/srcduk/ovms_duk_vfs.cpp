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
static const char *TAG = "ovms-duk-vfs";

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
#include "ovms_tls.h"

////////////////////////////////////////////////////////////////////////////////
// DuktapeVFSLoad: load a file asynchronously
//
// This is following a high-level approach, limitation is the file contents needs to fit in RAM
// twice, as the read buffer is converted into a JS string/buffer.
// Partial loading / block operations may be added later as needed, e.g. based on arguments.
//
// Notes:
// - Conditional synchronous operation doesn't make sense; async overhead is minimal
//   and the stat() on "/sd" already needs at least 45 ms.
// - On /store a Load() takes ~ 25 ms base +  5 ms per KB.
// - On /sd    a Load() takes ~ 60 ms base + 30 ms per KB.
//
// Javascript API:
//   var request = VFS.Load({
//     path: "…",
//     [binary: false,]
//     done: function(data) {},      // data: string|u8buf
//     fail: function(error) {},
//     always: function() {},
//   });

class DuktapeVFSLoad : public DuktapeObject
  {
  public:
    DuktapeVFSLoad(duk_context *ctx, int obj_idx);
    ~DuktapeVFSLoad() { /*ESP_LOGD(TAG, "~DuktapeVFSLoad");*/ }
    static duk_ret_t Create(duk_context *ctx);

  public:
    duk_ret_t CallMethod(duk_context *ctx, const char* method, void* data=NULL);

  protected:
    static void LoadTask(void *param);
    void Load();

  protected:
    std::ifstream m_file;
    std::string m_path;
    struct stat m_stat;
    extram::string m_data;
    std::string m_error;
    bool m_binary = false;
  };

duk_ret_t DuktapeVFSLoad::Create(duk_context *ctx)
  {
  // var request = VFS.Load({ args })
  DuktapeVFSLoad *request = new DuktapeVFSLoad(ctx, 0);
  request->Push(ctx);
  return 1;
  }

DuktapeVFSLoad::DuktapeVFSLoad(duk_context *ctx, int obj_idx)
  : DuktapeObject(ctx, obj_idx)
  {
  // get args:
  duk_require_stack(ctx, 5);
  if (duk_get_prop_string(ctx, 0, "path"))
    m_path = duk_to_string(ctx, -1);
  duk_pop(ctx);
  if (duk_get_prop_string(ctx, 0, "binary"))
    m_binary = duk_to_boolean(ctx, -1);
  duk_pop(ctx);

  // check path:
  if (m_path.empty())
    {
    m_error = "no path";
    CallMethod(ctx, "fail");
    return;
    }
  if (MyConfig.ProtectedPath(m_path))
    {
    m_error = "protected path";
    CallMethod(ctx, "fail");
    return;
    }

  // start loader:
  Ref();
  Register(ctx);
  if (xTaskCreatePinnedToCore(LoadTask, "DuktapeVFSLoad", 5*512, this,
      CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_PRIORITY-1, NULL, CORE(1)) != pdPASS)
    {
    Deregister(ctx);
    Unref();
    m_error = "cannot start task";
    CallMethod(ctx, "fail");
    return;
    }
  //ESP_LOGD(TAG, "DuktapeVFSLoad('%s'): started", m_path.c_str());
  }

void DuktapeVFSLoad::LoadTask(void *param)
  {
  DuktapeVFSLoad *me = (DuktapeVFSLoad*)param;
  me->Load();
  me->Unref();
  vTaskDelete(NULL);
  }

void DuktapeVFSLoad::Load()
  {
  // check file & size:
  if (stat(m_path.c_str(), &m_stat) != 0)
    {
    m_error = "file not found";
    RequestCallback("fail");
    return;
    }
  if (S_ISDIR(m_stat.st_mode))
    {
    m_error = "not a file";
    RequestCallback("fail");
    return;
    }
  if (m_stat.st_size > heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM))
    {
    m_error = "file too large";
    RequestCallback("fail");
    return;
    }
  if (m_stat.st_size == 0)
    {
    m_error = "";
    RequestCallback("done");
    return;
    }

  // load data:
  m_file.open(m_path, std::ios::in | std::ios::binary);
  if (m_file.fail())
    {
    m_error = "cannot open file";
    RequestCallback("fail");
    return;
    }
  m_data.resize(m_stat.st_size, '\0');
  m_file.read(&m_data[0], m_stat.st_size);
  bool fail = m_file.fail();
  m_file.close();

  if (fail)
    {
    m_error = "read error";
    RequestCallback("fail");
    }
  else
    {
    m_error = "";
    RequestCallback("done");
    }
  }

duk_ret_t DuktapeVFSLoad::CallMethod(duk_context *ctx, const char* method, void* data /*=NULL*/)
  {
  if (!ctx)
    {
    RequestCallback(method, data);
    return 0;
    }
  OvmsRecMutexLock lock(&m_mutex);
  if (!IsCoupled()) return 0;

  duk_require_stack(ctx, 3);
  int entry_top = duk_get_top(ctx);

  bool deregister = false;

  while (method)
    {
    const char* followup_method = NULL;

    // check method:
    int obj_idx = Push(ctx);
    duk_get_prop_string(ctx, obj_idx, method);
    bool callable = duk_is_callable(ctx, -1);
    duk_pop(ctx);
    if (callable) duk_push_string(ctx, method);
    int nargs = 0;

    // create results & method arguments:
    if (strcmp(method, "done") == 0)
      {
      // done(data):
      followup_method = "always";
      //ESP_LOGD(TAG, "DuktapeVFSLoad('%s'): done size=%d", m_path.c_str(), m_data.size());

      // clear request.error:
      duk_push_string(ctx, "");
      duk_put_prop_string(ctx, obj_idx, "error");

      // set data:
      if (m_binary)
        {
        void* p = duk_push_fixed_buffer(ctx, m_data.size());
        memcpy(p, m_data.data(), m_data.size());
        duk_dup(ctx, -1);
        duk_put_prop_string(ctx, obj_idx, "data");
        }
      else
        {
        duk_push_lstring(ctx, m_data.data(), m_data.size());
        duk_dup(ctx, -1);
        duk_put_prop_string(ctx, obj_idx, "data");
        }
      m_data.clear();
      m_data.shrink_to_fit();
      nargs++;
      }
    else if (strcmp(method, "fail") == 0)
      {
      // fail(error):
      followup_method = "always";
      //ESP_LOGD(TAG, "DuktapeVFSLoad('%s'): failed error='%s'", m_path.c_str(), m_error.c_str());

      // set request.error:
      duk_push_string(ctx, m_error.c_str());
      duk_dup(ctx, -1);
      duk_put_prop_string(ctx, obj_idx, "error");
      nargs++;
      }
    else if (strcmp(method, "always") == 0)
      {
      // always():
      deregister = true;
      }

    // call method:
    if (callable)
      {
      //ESP_LOGD(TAG, "DuktapeVFSLoad: calling method '%s' nargs=%d", method, nargs);
      if (duk_pcall_prop(ctx, obj_idx, nargs) != 0)
        {
        DukOvmsErrorHandler(ctx, -1);
        }
      }

    // clear stack:
    duk_pop_n(ctx, duk_get_top(ctx) - entry_top);

    // followup call:
    method = followup_method;
    } // while (method)

  // allow GC:
  if (deregister) Deregister(ctx);
  return 0;
  }

////////////////////////////////////////////////////////////////////////////////
// DuktapeVFSSave: save a file asynchronously
//
// This is following a high-level approach, limitation is the file contents needs to fit in RAM
// twice, as the JS buffer is copied for the saver.
// Partial saving / block operations may be added later as needed, e.g. based on arguments.
//
// Javascript API:
//   var request = VFS.Save({
//     data: string|u8buf
//     path: "…",                 // missing directories will be created automatically
//     [append: false,]
//     done: function() {},
//     fail: function(error) {},
//     always: function() {},
//   });

class DuktapeVFSSave : public DuktapeObject
  {
  public:
    DuktapeVFSSave(duk_context *ctx, int obj_idx);
    ~DuktapeVFSSave() { /*ESP_LOGD(TAG, "~DuktapeVFSSave");*/ }
    static duk_ret_t Create(duk_context *ctx);

  public:
    duk_ret_t CallMethod(duk_context *ctx, const char* method, void* data=NULL);

  protected:
    static void SaveTask(void *param);
    void Save();

  protected:
    std::ofstream m_file;
    std::string m_path;
    extram::string m_data;
    std::string m_error;
    bool m_append = false;
  };

duk_ret_t DuktapeVFSSave::Create(duk_context *ctx)
  {
  // var request = VFS.Save({ args })
  DuktapeVFSSave *request = new DuktapeVFSSave(ctx, 0);
  request->Push(ctx);
  return 1;
  }

DuktapeVFSSave::DuktapeVFSSave(duk_context *ctx, int obj_idx)
  : DuktapeObject(ctx, obj_idx)
  {
  // get args:
  duk_require_stack(ctx, 5);
  if (duk_get_prop_string(ctx, 0, "path"))
    m_path = duk_to_string(ctx, -1);
  duk_pop(ctx);
  if (duk_get_prop_string(ctx, 0, "append"))
    m_append = duk_to_boolean(ctx, -1);
  duk_pop(ctx);

  if (duk_get_prop_string(ctx, 0, "data"))
    {
    if (duk_is_buffer_data(ctx, -1))
      {
      size_t size;
      void *data = duk_get_buffer_data(ctx, -1, &size);
      m_data.resize(size, '\0');
      memcpy(&m_data[0], data, size);
      }
    else
      {
      m_data = duk_to_string(ctx, -1);
      }
    duk_pop(ctx);
    }
  else
    {
    duk_pop(ctx);
    m_error = "no data";
    CallMethod(ctx, "fail");
    return;
    }

  // check path:
  if (m_path.empty())
    {
    m_error = "no path";
    CallMethod(ctx, "fail");
    return;
    }
  if (MyConfig.ProtectedPath(m_path))
    {
    m_error = "protected path";
    CallMethod(ctx, "fail");
    return;
    }

  // start saver:
  Ref();
  Register(ctx);
  if (xTaskCreatePinnedToCore(SaveTask, "DuktapeVFSSave", 5*512, this,
      CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_PRIORITY-1, NULL, CORE(1)) != pdPASS)
    {
    Deregister(ctx);
    Unref();
    m_error = "cannot start task";
    CallMethod(ctx, "fail");
    return;
    }
  //ESP_LOGD(TAG, "DuktapeVFSSave('%s'): started", m_path.c_str());
  }

void DuktapeVFSSave::SaveTask(void *param)
  {
  DuktapeVFSSave *me = (DuktapeVFSSave*)param;
  me->Save();
  me->Unref();
  vTaskDelete(NULL);
  }

void DuktapeVFSSave::Save()
  {
  // create path:
  size_t n = m_path.rfind('/');
  if (n != 0 && n != std::string::npos)
    {
    std::string dir = m_path.substr(0, n);
    if (!path_exists(dir) && mkpath(dir) != 0)
      {
      m_error = "cannot create path";
      RequestCallback("fail");
      return;
      }
    }

  // save data:
  if (m_append)
    m_file.open(m_path, std::ios::out | std::ios::binary | std::ios::app);
  else
    m_file.open(m_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (m_file.fail())
    {
    m_error = "cannot open file";
    RequestCallback("fail");
    return;
    }
  m_file.write(&m_data[0], m_data.size());
  bool wfail = m_file.fail();
  m_file.close();

  // free buffer:
  m_data.clear();
  m_data.shrink_to_fit();

  if (wfail || m_file.fail())
    {
    m_error = "write error";
    RequestCallback("fail");
    }
  else
    {
    m_error = "";
    RequestCallback("done");
    }
  }

duk_ret_t DuktapeVFSSave::CallMethod(duk_context *ctx, const char* method, void* data /*=NULL*/)
  {
  if (!ctx)
    {
    RequestCallback(method, data);
    return 0;
    }
  OvmsRecMutexLock lock(&m_mutex);
  if (!IsCoupled()) return 0;

  duk_require_stack(ctx, 3);
  int entry_top = duk_get_top(ctx);

  bool deregister = false;

  while (method)
    {
    const char* followup_method = NULL;

    // check method:
    int obj_idx = Push(ctx);
    duk_get_prop_string(ctx, obj_idx, method);
    bool callable = duk_is_callable(ctx, -1);
    duk_pop(ctx);
    if (callable) duk_push_string(ctx, method);
    int nargs = 0;

    // create results & method arguments:
    if (strcmp(method, "done") == 0)
      {
      // done():
      followup_method = "always";
      //ESP_LOGD(TAG, "DuktapeVFSSave('%s'): done", m_path.c_str());

      // clear request.error:
      duk_push_string(ctx, "");
      duk_put_prop_string(ctx, obj_idx, "error");
      }
    else if (strcmp(method, "fail") == 0)
      {
      // fail(error):
      followup_method = "always";
      //ESP_LOGD(TAG, "DuktapeVFSSave('%s'): failed error='%s'", m_path.c_str(), m_error.c_str());

      // set request.error:
      duk_push_string(ctx, m_error.c_str());
      duk_dup(ctx, -1);
      duk_put_prop_string(ctx, obj_idx, "error");
      nargs++;
      }
    else if (strcmp(method, "always") == 0)
      {
      // always():
      deregister = true;
      }

    // call method:
    if (callable)
      {
      //ESP_LOGD(TAG, "DuktapeVFSSave: calling method '%s' nargs=%d", method, nargs);
      if (duk_pcall_prop(ctx, obj_idx, nargs) != 0)
        {
        DukOvmsErrorHandler(ctx, -1);
        }
      }

    // clear stack:
    duk_pop_n(ctx, duk_get_top(ctx) - entry_top);

    // followup call:
    method = followup_method;
    } // while (method)

  // allow GC:
  if (deregister) Deregister(ctx);
  return 0;
  }

////////////////////////////////////////////////////////////////////////////////
// DuktapeVFSInit registration

class DuktapeVFSInit
  {
  public: DuktapeVFSInit();
} MyDuktapeVFSInit  __attribute__ ((init_priority (1700)));

DuktapeVFSInit::DuktapeVFSInit()
  {
  ESP_LOGI(TAG, "Installing DUKTAPE VFS (1710)");
  DuktapeObjectRegistration* dt_vfs = new DuktapeObjectRegistration("VFS");
  dt_vfs->RegisterDuktapeFunction(DuktapeVFSLoad::Create, 1, "Load");
  dt_vfs->RegisterDuktapeFunction(DuktapeVFSSave::Create, 1, "Save");
  MyDuktape.RegisterDuktapeObject(dt_vfs);
  }
