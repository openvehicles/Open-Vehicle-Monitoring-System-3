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
static const char *TAG = "boot";

#include "freertos/FreeRTOS.h"
#include "freertos/xtensa_api.h"
#include "rom/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_system.h"
#include "esp_panic.h"
#include "esp_task_wdt.h"

#include "ovms.h"
#include "ovms_boot.h"
#include "ovms_command.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"
#include "ovms_config.h"
#include "metrics_standard.h"
#include "string_writer.h"
#include <string.h>

boot_data_t __attribute__((section(".rtc.noload"))) boot_data;

Boot MyBoot __attribute__ ((init_priority (1100)));

extern void xt_unhandled_exception(XtExcFrame *frame);

// Exception descriptions copied from esp32/panic.c:
static const char *edesc[] = {
    "IllegalInstruction", "Syscall", "InstructionFetchError", "LoadStoreError",
    "Level1Interrupt", "Alloca", "IntegerDivideByZero", "PCValue",
    "Privileged", "LoadStoreAlignment", "res", "res",
    "InstrPDAddrError", "LoadStorePIFDataError", "InstrPIFAddrError", "LoadStorePIFAddrError",
    "InstTLBMiss", "InstTLBMultiHit", "InstFetchPrivilege", "res",
    "InstrFetchProhibited", "res", "res", "res",
    "LoadStoreTLBMiss", "LoadStoreTLBMultihit", "LoadStorePrivilege", "res",
    "LoadProhibited", "StoreProhibited", "res", "res",
    "Cp0Dis", "Cp1Dis", "Cp2Dis", "Cp3Dis",
    "Cp4Dis", "Cp5Dis", "Cp6Dis", "Cp7Dis"
};
#define NUM_EDESCS (sizeof(edesc) / sizeof(char *))

// Register names copied from esp32/panic.c:
static const char *sdesc[] = {
    "PC      ", "PS      ", "A0      ", "A1      ", "A2      ", "A3      ", "A4      ", "A5      ",
    "A6      ", "A7      ", "A8      ", "A9      ", "A10     ", "A11     ", "A12     ", "A13     ",
    "A14     ", "A15     ", "SAR     ", "EXCCAUSE", "EXCVADDR", "LBEG    ", "LEND    ", "LCOUNT  "
};

// Boot reasons [bootreason_t]
static const char* const bootreason_name[] = {
    "PowerOn",
    "SoftReset",
    "FirmwareUpdate",
    "EarlyCrash",
    "Crash",
};
#define NUM_BOOTREASONS (sizeof(bootreason_name) / sizeof(char *))

// Reset reasons [esp_reset_reason_t]
static const char *resetreason_name[] = {
    "Unknown/unset",
    "Power-on event",
    "External pin",
    "esp_restart",
    "Exception/panic",
    "Interrupt watchdog",
    "Task watchdog",
    "Other watchdogs",
    "Exiting deep sleep",
    "Brownout",
    "SDIO",
};
#define NUM_RESETREASONS (sizeof(resetreason_name) / sizeof(char *))


/**
 * ovms_reset_reason_get_hint()
 * 
 * This is a copy of esp_reset_reason_get_hint() from esp32/reset_reason.c. Because the
 * default esp32 init only checks the reason for CPU0 and then clears the reason register,
 * we need to store a copy in our boot_data record, but esp_reset_reason_get_hint()
 * is an internal method.
 */

#define RST_REASON_BIT  0x80000000
#define RST_REASON_MASK 0x7FFF
#define RST_REASON_SHIFT 16

/* in IRAM, can be called from panic handler */
static esp_reset_reason_t IRAM_ATTR ovms_reset_reason_get_hint(void)
{
    uint32_t reset_reason_hint = REG_READ(RTC_RESET_CAUSE_REG);
    uint32_t high = (reset_reason_hint >> RST_REASON_SHIFT) & RST_REASON_MASK;
    uint32_t low = reset_reason_hint & RST_REASON_MASK;
    if ((reset_reason_hint & RST_REASON_BIT) == 0 || high != low) {
        return ESP_RST_UNKNOWN;
    }
    return (esp_reset_reason_t) low;
}


void boot_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  
  time_t rawtime;
  struct tm* tml;
  char tb[32];
  
  writer->printf("Last boot was %d second(s) ago\n",monotonictime);
  
  time(&rawtime);
  rawtime = rawtime-(time_t)monotonictime;
  tml = localtime(&rawtime);
  if ((strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S %Z", tml) > 0) && rawtime > 0)
    writer->printf("Time at boot: %s\n", tb);

  writer->printf("  This is reset #%d since last power cycle\n",boot_data.boot_count);
  writer->printf("  Detected boot reason: %s (%d/%d)\n",MyBoot.GetBootReasonName(),boot_data.bootreason_cpu0,boot_data.bootreason_cpu1);
  writer->printf("  Reset reason: %s (%d)\n",MyBoot.GetResetReasonName(),MyBoot.GetResetReason());
  writer->printf("  Crash counters: %d total, %d early\n",MyBoot.GetCrashCount(),MyBoot.GetEarlyCrashCount());

  if (MyBoot.m_restart_timer>0)
    {
    writer->printf("\nRestart in progress (%d secs, waiting for %d tasks)\n",
      MyBoot.m_restart_timer, MyBoot.m_restart_pending);
    }

  if (MyBoot.GetCrashCount() > 0)
    {
    // output data of last crash:
    writer->printf("\nLast crash: ");
    if (boot_data.crash_data.is_abort)
      {
      writer->printf("abort() was called on core %d\n", boot_data.crash_data.core_id);
      }
    else
      {
      int exccause = boot_data.crash_data.reg[19];
      writer->printf("%s exception on core %d\n",
        (exccause < NUM_EDESCS) ? edesc[exccause] : "Unknown", boot_data.crash_data.core_id);
      writer->printf("  Registers:\n");
      for (int i=0; i<24; i++)
        writer->printf("  %s: 0x%08lx%s", sdesc[i], boot_data.crash_data.reg[i], ((i+1)%4) ? "" : "\n");
      }
    writer->printf("  Backtrace:\n ");
    for (int i=0; i<OVMS_BT_LEVELS && boot_data.crash_data.bt[i].pc; i++)
      writer->printf(" 0x%08lx", boot_data.crash_data.bt[i].pc);
    if (boot_data.curr_event_name[0])
      {
      writer->printf("\n  Event: %s@%s %u secs", boot_data.curr_event_name, boot_data.curr_event_handler,
        boot_data.curr_event_runtime);
      }
    if (MyBoot.GetResetReason() == ESP_RST_TASK_WDT)
      {
      writer->printf("\n  WDT tasks: %s", boot_data.wdt_tasknames);
      }
    writer->printf("\n  Version: %s\n", StdMetrics.ms_m_version->AsString("").c_str());
    }
  }

void boot_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  memset(&boot_data,0,sizeof(boot_data_t));
  boot_data.crc = boot_data.calc_crc();
  writer->puts("Boot status data has been cleared.");
  }

Boot::Boot()
  {
  ESP_LOGI(TAG, "Initialising BOOT (1100)");

  RESET_REASON cpu0 = rtc_get_reset_reason(0);
  RESET_REASON cpu1 = rtc_get_reset_reason(1);

  m_restart_timer = 0;
  m_restart_pending = 0;

  m_resetreason = esp_reset_reason(); // Note: necessary to link reset_reason module

  if (cpu0 == POWERON_RESET)
    {
    memset(&boot_data,0,sizeof(boot_data_t));
    m_bootreason = BR_PowerOn;
    ESP_LOGI(TAG, "Power cycle reset detected");
    }
  else if (boot_data.crc != boot_data.calc_crc())
    {
    memset(&boot_data,0,sizeof(boot_data_t));
    m_bootreason = BR_PowerOn;
    ESP_LOGW(TAG, "Boot data corruption detected, data cleared");
    }
  else
    {
    boot_data.boot_count++;
    ESP_LOGI(TAG, "Boot #%d reasons for CPU0=%d and CPU1=%d",boot_data.boot_count,cpu0,cpu1);

    m_resetreason = boot_data.reset_hint;
    ESP_LOGI(TAG, "Reset reason %s (%d)", GetResetReasonName(), GetResetReason());

    if (boot_data.soft_reset)
      {
      boot_data.crash_count_total = 0;
      boot_data.crash_count_early = 0;
      m_bootreason = BR_SoftReset;
      ESP_LOGI(TAG, "Soft reset by user");
      }
    else if (boot_data.firmware_update)
      {
      boot_data.crash_count_total = 0;
      boot_data.crash_count_early = 0;
      m_bootreason = BR_FirmwareUpdate;
      ESP_LOGI(TAG, "Firmware update reset");
      }
    else if (!boot_data.stable_reached)
      {
      boot_data.crash_count_total++;
      boot_data.crash_count_early++;
      m_bootreason = BR_EarlyCrash;
      ESP_LOGE(TAG, "Early crash #%d detected", boot_data.crash_count_early);
      }
    else
      {
      boot_data.crash_count_total++;
      m_bootreason = BR_Crash;
      ESP_LOGE(TAG, "Crash #%d detected", boot_data.crash_count_total);
      }
    }

  m_crash_count_early = boot_data.crash_count_early;

  boot_data.bootreason_cpu0 = cpu0;
  boot_data.bootreason_cpu1 = cpu1;

  // reset flags:
  boot_data.soft_reset = false;
  boot_data.firmware_update = false;
  boot_data.stable_reached = false;

  boot_data.crc = boot_data.calc_crc();

  // install error handler:
  xt_set_error_handler_callback(ErrorCallback);

  // Register our commands
  OvmsCommand* cmd_boot = MyCommandApp.RegisterCommand("boot","BOOT framework",boot_status, "", 0, 0, false);
  cmd_boot->RegisterCommand("status","Show boot system status",boot_status,"", 0, 0, false);
  cmd_boot->RegisterCommand("clear","Clear/reset boot system status",boot_clear,"", 0, 0, false);
  }

Boot::~Boot()
  {
  }

void Boot::SetStable()
  {
  boot_data.stable_reached = true;
  boot_data.crash_count_early = 0;
  boot_data.crc = boot_data.calc_crc();
  }

void Boot::SetSoftReset()
  {
  boot_data.soft_reset = true;
  boot_data.crc = boot_data.calc_crc();
  }

void Boot::SetFirmwareUpdate()
  {
  boot_data.soft_reset = false;
  boot_data.firmware_update = true;
  boot_data.crc = boot_data.calc_crc();
  }

const char* Boot::GetBootReasonName()
  {
  return (m_bootreason >= 0 && m_bootreason < NUM_BOOTREASONS)
    ? bootreason_name[m_bootreason]
    : "Unknown boot reason";
  }

const char* Boot::GetResetReasonName()
  {
  return (m_resetreason >= 0 && m_resetreason < NUM_RESETREASONS)
    ? resetreason_name[m_resetreason]
    : "Unknown reset reason";
  }

static void boot_shutdown_done(const char* event, void* data)
  {
  MyConfig.unmount();
  esp_restart();
  }

static void boot_shuttingdown_done(const char* event, void* data)
  {
  if (MyBoot.m_restart_pending == 0)
    MyBoot.m_restart_timer = 2;
  }

void Boot::Restart(bool hard)
  {
  SetSoftReset();

  if (hard)
    {
    esp_restart();
    return;
    }

  ESP_LOGI(TAG,"Shutting down for restart...");
  OvmsMutexLock lock(&m_restart_mutex);
  m_restart_pending = 0;
  m_restart_timer = 60; // Give them 60 seconds to shutdown
  MyEvents.SignalEvent("system.shuttingdown", NULL, boot_shuttingdown_done);

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&Boot::Ticker1, this, _1, _2));
  }

void Boot::RestartPending(const char* tag)
  {
  OvmsMutexLock lock(&m_restart_mutex);
  m_restart_pending++;
  }

void Boot::RestartReady(const char* tag)
  {
  OvmsMutexLock lock(&m_restart_mutex);
  m_restart_pending--;
  if (m_restart_pending == 0)
    m_restart_timer = 2;
  }

void Boot::Ticker1(std::string event, void* data)
  {
  if (m_restart_timer > 0)
    {
    OvmsMutexLock lock(&m_restart_mutex);
    m_restart_timer--;
    if (m_restart_timer == 1)
      {
      ESP_LOGI(TAG,"Restart now");
      }
    else if (m_restart_timer == 0)
      {
      MyEvents.SignalEvent("system.shutdown", NULL, boot_shutdown_done);
      return;
      }
    else if ((m_restart_timer % 5)==0)
      ESP_LOGI(TAG,"Restart in %d seconds (%d pending)...",m_restart_timer,m_restart_pending);
    }
  }

bool Boot::IsShuttingDown()
  {
  return (m_restart_timer > 0);
  }

void Boot::ErrorCallback(XtExcFrame *frame, int core_id, bool is_abort)
  {
  boot_data.reset_hint = ovms_reset_reason_get_hint();
  boot_data.crash_data.core_id = core_id;
  boot_data.crash_data.is_abort = is_abort;

  // Save registers:
  for (int i=0; i<24; i++)
    boot_data.crash_data.reg[i] = ((uint32_t*)frame)[i+1];

  // Save backtrace:
  // (see panic.c::doBacktrace() for code template)
  #define _adjusted_pc(pc) (((pc) & 0x80000000) ? (((pc) & 0x3fffffff) | 0x40000000) : (pc))
  uint32_t i = 0, pc = frame->pc, sp = frame->a1;
  boot_data.crash_data.bt[i++].pc = _adjusted_pc(pc);
  pc = frame->a0;
  while (i < OVMS_BT_LEVELS)
    {
    uint32_t psp = sp;
    if (!esp_stack_ptr_is_sane(sp))
        break;
    sp = *((uint32_t *) (sp - 0x10 + 4));
    boot_data.crash_data.bt[i++].pc = _adjusted_pc(pc - 3);
    pc = *((uint32_t *) (psp - 0x10));
    if (pc < 0x40000000)
        break;
    }
  while (i < OVMS_BT_LEVELS)
    boot_data.crash_data.bt[i++].pc = 0;

  // Save Event debug info:
  if (!MyEvents.m_current_event.empty())
    {
    strlcpy(boot_data.curr_event_name, MyEvents.m_current_event.c_str(), sizeof(boot_data.curr_event_name));
    if (MyEvents.m_current_callback)
      strlcpy(boot_data.curr_event_handler, MyEvents.m_current_callback->m_caller.c_str(), sizeof(boot_data.curr_event_handler));
    else
      strlcpy(boot_data.curr_event_handler, "EventScript", sizeof(boot_data.curr_event_handler));
    boot_data.curr_event_runtime = monotonictime - MyEvents.m_current_started;
    }
  else
    {
    boot_data.curr_event_name[0] = 0;
    boot_data.curr_event_handler[0] = 0;
    boot_data.curr_event_runtime = 0;
    }

  // Save TWDT task info:
  esp_task_wdt_get_trigger_tasknames(boot_data.wdt_tasknames, sizeof(boot_data.wdt_tasknames));

  boot_data.crc = boot_data.calc_crc();
  }

void Boot::NotifyDebugCrash()
  {
  if (GetCrashCount() > 0)
    {
    // Send crash data notification:
    // H type "*-OVM-DebugCrash"
    //  ,<firmware_version>
    //  ,<bootcount>,<bootreason_name>,<bootreason_cpu0>,<bootreason_cpu1>
    //  ,<crashcnt>,<earlycrashcnt>,<crashtype>,<crashcore>,<registers>,<backtrace>
    //  ,<resetreason>,<resetreason_name>
    //  ,<curr_event_name>,<curr_event_handler>,<curr_event_runtime>
    //  ,<wdt_tasknames>

    StringWriter buf;
    buf.reserve(1024);
    buf.append("*-OVM-DebugCrash,0,2592000,");
    buf.append(StdMetrics.ms_m_version->AsString(""));
    buf.printf(",%d,%s,%d,%d,%d,%d"
      , boot_data.boot_count, GetBootReasonName(), boot_data.bootreason_cpu0, boot_data.bootreason_cpu1
      , GetCrashCount(), GetEarlyCrashCount());

    // type, core, registers:
    if (boot_data.crash_data.is_abort)
      {
      buf.printf(",abort(),%d,", boot_data.crash_data.core_id);
      }
    else
      {
      int exccause = boot_data.crash_data.reg[19];
      buf.printf(",%s,%d,",
        (exccause < NUM_EDESCS) ? edesc[exccause] : "Unknown", boot_data.crash_data.core_id);
      for (int i=0; i<24; i++)
        buf.printf("0x%08lx ", boot_data.crash_data.reg[i]);
      }

    // backtrace:
    buf.append(",");
    for (int i=0; i<OVMS_BT_LEVELS && boot_data.crash_data.bt[i].pc; i++)
      buf.printf("0x%08lx ", boot_data.crash_data.bt[i].pc);

    // Reset reason:
    buf.printf(",%d,%s", GetResetReason(), GetResetReasonName());

    // Event debug info:
    buf.printf(",%s,%s,%u", boot_data.curr_event_name, boot_data.curr_event_handler, boot_data.curr_event_runtime);

    // TWDT debug info:
    buf.printf(",%s", boot_data.wdt_tasknames);

    MyNotify.NotifyString("data", "debug.crash", buf.c_str());
    }
  }
