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
static const char *TAG = "ovms-duk-util";

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

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM
  #include "umm_malloc_cfg.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// DukOvmsPrint

static duk_ret_t DukOvmsPrint(duk_context *ctx)
  {
  const char *output = duk_safe_to_string(ctx,0);

  OvmsWriter* duktapewriter = MyDuktape.GetDuktapeWriter();

  if (output != NULL)
    {
    if (duktapewriter != NULL)
      {
      duktapewriter->printf("%s",output);
      }
    else
      {
      std::string filename, function;
      int linenumber = 0;
      MyDuktape.DukGetCallInfo(ctx, &filename, &linenumber, &function);
      if (function == "eval")
        ESP_LOGI(TAG, "[%s:%d] %s", filename.c_str(), linenumber, output);
      else
        ESP_LOGI(TAG, "[%s:%d:%s] %s", filename.c_str(), linenumber, function.c_str(), output);
      }
    }
  return 0;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // DukOvmsWrite

static duk_ret_t DukOvmsWrite(duk_context *ctx)
  {
  OvmsWriter* duktapewriter = MyDuktape.GetDuktapeWriter();

  if (!duktapewriter) return 0;

  size_t size;
  const void *data;

  if (duk_is_buffer_data(ctx, 0))
   data = duk_get_buffer_data(ctx, 0, &size);
  else
   data = duk_to_lstring(ctx, 0, &size);

  if (data)
   duktapewriter->write(data, size);

  return 0;
  }

////////////////////////////////////////////////////////////////////////////////
// DukOvmsAssert

static duk_ret_t DukOvmsAssert(duk_context *ctx)
  {
  if (duk_to_boolean(ctx, 0))
    {
    return 0;
    }
  duk_error(ctx, DUK_ERR_ERROR, "assertion failed: %s", duk_safe_to_string(ctx, 1));
  return 0;
  }

////////////////////////////////////////////////////////////////////////////////
// DukOvmsMemInfo

static duk_ret_t DukOvmsMemInfo(duk_context *ctx)
  {
  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM
    // Using umm_malloc:
    umm_info(NULL, false);
    DukContext dc(ctx);
    duk_idx_t obj_idx = dc.PushObject();

    // Standard info:
    dc.Push(ummHeapInfo.totalBlocks * CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM_BLOCKSIZE);
                                                  dc.PutProp(obj_idx, "totalBytes");
    dc.Push(ummHeapInfo.usedBlocks * CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM_BLOCKSIZE);
                                                  dc.PutProp(obj_idx, "usedBytes");
    dc.Push(ummHeapInfo.freeBlocks * CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM_BLOCKSIZE);
                                                  dc.PutProp(obj_idx, "freeBytes");
    dc.Push(ummHeapInfo.maxFreeContiguousBlocks * CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM_BLOCKSIZE);
                                                  dc.PutProp(obj_idx, "largestFreeBytes");

    // Allocator specific info:
    dc.Push("umm");                               dc.PutProp(obj_idx, "memlib");
    dc.Push(ummHeapInfo.totalEntries);            dc.PutProp(obj_idx, "ummTotalEntries");
    dc.Push(ummHeapInfo.usedEntries);             dc.PutProp(obj_idx, "ummUsedEntries");
    dc.Push(ummHeapInfo.freeEntries);             dc.PutProp(obj_idx, "ummFreeEntries");
    dc.Push(ummHeapInfo.totalBlocks);             dc.PutProp(obj_idx, "ummTotalBlocks");
    dc.Push(ummHeapInfo.usedBlocks);              dc.PutProp(obj_idx, "ummUsedBlocks");
    dc.Push(ummHeapInfo.freeBlocks);              dc.PutProp(obj_idx, "ummFreeBlocks");
    dc.Push(ummHeapInfo.maxFreeContiguousBlocks); dc.PutProp(obj_idx, "ummMaxFreeContiguousBlocks");
    dc.Push(ummHeapInfo.usage_metric);            dc.PutProp(obj_idx, "ummUsageMetric");
    dc.Push(ummHeapInfo.fragmentation_metric);    dc.PutProp(obj_idx, "ummFragmentationMetric");
  #else
    // Using system default allocator:
    multi_heap_info_t heapinfo;
    heap_caps_get_info(&heapinfo, MALLOC_CAP_SPIRAM);
    DukContext dc(ctx);
    duk_idx_t obj_idx = dc.PushObject();

    // Standard info:
    dc.Push(heapinfo.total_free_bytes + heapinfo.total_allocated_bytes);
                                                  dc.PutProp(obj_idx, "totalBytes");
    dc.Push(heapinfo.total_allocated_bytes);      dc.PutProp(obj_idx, "usedBytes");
    dc.Push(heapinfo.total_free_bytes);           dc.PutProp(obj_idx, "freeBytes");
    dc.Push(heapinfo.largest_free_block);         dc.PutProp(obj_idx, "largestFreeBytes");

    // Allocator specific info:
    dc.Push("sys");                               dc.PutProp(obj_idx, "memlib");
    dc.Push(heapinfo.minimum_free_bytes);         dc.PutProp(obj_idx, "sysMinimumFreeBytes");
    dc.Push(heapinfo.allocated_blocks);           dc.PutProp(obj_idx, "sysAllocatedBlocks");
    dc.Push(heapinfo.free_blocks);                dc.PutProp(obj_idx, "sysFreeBlocks");
    dc.Push(heapinfo.total_blocks);               dc.PutProp(obj_idx, "sysTotalBlocks");
  #endif

  return 1;
  }

////////////////////////////////////////////////////////////////////////////////
// DuktapeHTTPInit registration

class DuktapeUtilInit
  {
  public: DuktapeUtilInit();
} MyDuktapeUtilInit  __attribute__ ((init_priority (1700)));

DuktapeUtilInit::DuktapeUtilInit()
  {
  ESP_LOGI(TAG, "Installing DUKTAPE Utilities (1710)");
  MyDuktape.RegisterDuktapeFunction(DukOvmsPrint, 1, "print");
  MyDuktape.RegisterDuktapeFunction(DukOvmsWrite, 1, "write");
  MyDuktape.RegisterDuktapeFunction(DukOvmsAssert, 2, "assert");
  MyDuktape.RegisterDuktapeFunction(DukOvmsMemInfo, 0, "meminfo");
  }
