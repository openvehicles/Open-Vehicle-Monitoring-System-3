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
#if ESP_IDF_VERSION_MAJOR >= 5
#include "soc/uart_reg.h"
#endif
#include "rom/rtc.h"
#include "rom/uart.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR < 4
#include "esp_panic.h"
#else
#include "esp_private/panic_internal.h"
#include "esp_private/system_internal.h"
#endif
#include "esp_task_wdt.h"
#include <driver/adc.h>

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
    "Wakeup",
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


#if ESP_IDF_VERSION_MAJOR < 4
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

#endif

void boot_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  
  time_t rawtime;
  struct tm timeinfo;
  struct tm* tml;
  char tb[32];
  
  writer->printf("Last boot was %" PRId32 " second(s) ago\n",monotonictime);
  
  time(&rawtime);
  rawtime = rawtime-(time_t)monotonictime;
  tml = localtime_r(&rawtime, &timeinfo);
  if ((strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S %Z", tml) > 0) && rawtime > 0)
    writer->printf("Time at boot: %s\n", tb);

  writer->printf("  This is reset #%d since last power cycle\n",boot_data.boot_count);
  writer->printf("  Detected boot reason: %s (%d/%d)\n",MyBoot.GetBootReasonName(),boot_data.bootreason_cpu0,boot_data.bootreason_cpu1);
  writer->printf("  Reset reason: %s (%d)\n",MyBoot.GetResetReasonName(),MyBoot.GetResetReason());
  writer->printf("  Crash counters: %d total, %d early\n",MyBoot.GetCrashCount(),MyBoot.GetEarlyCrashCount());

  if (MyBoot.m_shutdown_timer>0)
    {
    writer->printf("\nShutdown for %s in progress (%d secs, waiting for %d tasks)\n",
      MyBoot.m_shutdown_deepsleep ? "DeepSleep" : "Restart",
      MyBoot.m_shutdown_timer, MyBoot.m_shutdown_pending);
    }

  if (MyBoot.GetCrashCount() > 0)
    {
    // output data of last crash:
    writer->printf("\nLast crash: ");
    if (boot_data.crash_data.is_abort)
      {
      // Software controlled panic:
      writer->printf("abort() was called on core %d\n", boot_data.crash_data.core_id);
      if (boot_data.stack_overflow_taskname[0])
        writer->printf("  Stack overflow in task %s\n", boot_data.stack_overflow_taskname);
      else if (boot_data.curr_task[0].name[0] && !boot_data.curr_task[0].stackfree)
        writer->printf("  Pending stack overflow in task %s\n", boot_data.curr_task[0].name);
      else if (boot_data.curr_task[1].name[0] && !boot_data.curr_task[1].stackfree)
        writer->printf("  Pending stack overflow in task %s\n", boot_data.curr_task[1].name);
      }
    else
      {
      // Hardware exception:
      int exccause = boot_data.crash_data.reg[19];
      writer->printf("%s exception on core %d\n",
        (exccause < NUM_EDESCS) ? edesc[exccause] : "Unknown", boot_data.crash_data.core_id);
      writer->printf("  Registers:\n");
      for (int i=0; i<24; i++)
        writer->printf("  %s: 0x%08" PRIx32 "%s", sdesc[i], boot_data.crash_data.reg[i], ((i+1)%4) ? "" : "\n");
      }

    for (int core = 0; core < portNUM_PROCESSORS; core++)
      {
      if (boot_data.curr_task[core].name[0])
        writer->printf("  Current task on core %d: %s, %" PRIu32 " stack bytes free\n",
          core, boot_data.curr_task[core].name, boot_data.curr_task[core].stackfree);
      }

    writer->printf("  Backtrace:\n ");
    for (int i=0; i<OVMS_BT_LEVELS && boot_data.crash_data.bt[i].pc; i++)
      writer->printf(" 0x%08" PRIx32, boot_data.crash_data.bt[i].pc);

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
    writer->printf("\n  Hardware: %s\n", StdMetrics.ms_m_hardware->AsString("").c_str());
    }
  }

void boot_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  memset(&boot_data,0,sizeof(boot_data_t));
  boot_data.crc = boot_data.calc_crc();
  writer->puts("Boot status data has been cleared.");
  }

#if ESP_IDF_VERSION_MAJOR >= 4
/*
Error handling for OVMS/ESP-IDF4+ is done differently than the OVMS/ESP-IDF3 versions.
- In OVMS/ESP-IDF3, ESP-IDF was patched to add support for a custom error handler.
- In OVMS/ESP-IDF4+, we take advantage of the ability of the linker to wrap a symbol,
  and we thus wrap the original `esp_panic_handler` (panic.c)

See:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#wrappers-to-redefine-or-extend-existing-functions
- https://github.com/espressif/esp-idf/issues/7681#issuecomment-941957783
*/
extern "C" {
  extern void esp_panic_handler_reconfigure_wdts(void);

  /* Declare the real panic handler function. We'll be able to call it after executing our custom code */
  extern void __real_esp_panic_handler(panic_info_t*);

  /* This function will be considered the esp_panic_handler to call in case a panic occurs */
  void __wrap_esp_panic_handler(panic_info_t* info)
    {
    /* Custom code, count the number of panics or simply print a message */
    esp_rom_printf("Panic has been triggered by the program!\n");

    /*
    This logic comes from the original esp_panic_handler (in panic.c) which is
    used to set the reset reason hint.
    However, our wrapper happens *before* this function, so cannot benefit from
    the hint.
    */
    esp_reset_reason_t reset_hint = esp_reset_reason_get_hint();
    if (ESP_RST_UNKNOWN == reset_hint)
      {
      switch (info->exception)
        {
        case PANIC_EXCEPTION_IWDT:
            reset_hint = ESP_RST_INT_WDT;
            break;
        case PANIC_EXCEPTION_TWDT:
            reset_hint = ESP_RST_TASK_WDT;
            break;
        case PANIC_EXCEPTION_ABORT:
        case PANIC_EXCEPTION_FAULT:
        default:
            reset_hint = ESP_RST_PANIC;
            break;
        }
      }

    /* Call the ErrorCallback from OVMS/ESP-IDF3 - adding the `reset_hint` parameter */
    Boot::ErrorCallback(info->frame, info->core, g_panic_abort, reset_hint);

    esp_panic_handler_reconfigure_wdts();

    /* Call the original panic handler function to finish processing this error (creating a core dump for example...) */
    __real_esp_panic_handler(info);
    }
}
#endif

Boot::Boot()
  {
  ESP_LOGI(TAG, "Initialising BOOT (1100)");

  RESET_REASON cpu0 = rtc_get_reset_reason(0);
  RESET_REASON cpu1 = rtc_get_reset_reason(1);

  m_shutdown_timer = 0;
  m_shutdown_pending = 0;
  m_shutdown_deepsleep = false;
  m_shutdown_deepsleep_seconds = 0;
  m_shutting_down = false;
  m_min_12v_level_override = false;

  m_resetreason = esp_reset_reason(); // Note: necessary to link reset_reason module

  if (cpu0 == POWERON_RESET)
    {
    memset(&boot_data,0,sizeof(boot_data_t));
    m_bootreason = BR_PowerOn;
    ESP_LOGI(TAG, "Power cycle reset detected");
    }
  else if (cpu0 == DEEPSLEEP_RESET)
    {
    m_bootreason = BR_Wakeup;
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "Wakeup from deep sleep detected, wakeup cause %d", wakeup_cause);

    if (boot_data.crc != boot_data.calc_crc())
      {
      memset(&boot_data,0,sizeof(boot_data_t));
      ESP_LOGW(TAG, "Boot data corruption detected, data cleared");
      }

    // There is currently only one deep sleep application: saving the 12V battery
    // from depletion. So we need to check if the voltage level is sufficient for
    // normal operation now. MyPeripherals has not been initialized yet, so we need
    // to read the ADC manually here.
    #ifdef CONFIG_OVMS_COMP_ADC
      // Note: RTC_MODULE nags about a lock release before aquire, this can be ignored
      //  (reason: RTC_MODULE needs FreeRTOS for locking, which hasn't been started yet)
      float min_12v_level = (boot_data.min_12v_level > 0) ? boot_data.min_12v_level : 0;
      if (min_12v_level > 0)
        {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
        uint32_t adc_level = 0;
        for (int i = 0; i < 5; i++)
          {
          ets_delay_us(2000);
          adc_level += adc1_get_raw(ADC1_CHANNEL_0);
          }
        float adc1_factor = (boot_data.adc1_factor > 0) ? boot_data.adc1_factor : 195.7;
        float level_12v = (float) adc_level / 5.0f / adc1_factor;
        ESP_LOGI(TAG, "12V level: ~%.1fV", level_12v);
        if (level_12v >= min_12v_level)
          ESP_LOGI(TAG, "12V level sufficient, proceeding with boot");
        else if (level_12v < 1.0)
          ESP_LOGI(TAG, "Assuming USB powered, proceeding with boot");
        else
          {
          ESP_LOGE(TAG, "12V level insufficient, re-entering deep sleep");
          int wakeup_interval = (boot_data.wakeup_interval > 0) ? boot_data.wakeup_interval : 60;
          esp_deep_sleep(1000000LL * wakeup_interval);
          }
        }
    #else
      ESP_LOGW(TAG, "ADC not available, cannot check 12V level");
    #endif // CONFIG_OVMS_COMP_ADC
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

    if (boot_data.soft_reset)
      {
      boot_data.crash_count_total = 0;
      boot_data.crash_count_early = 0;
      m_bootreason = BR_SoftReset;
      m_resetreason = ESP_RST_SW;
      ESP_LOGI(TAG, "Soft reset by user");
      }
    else if (boot_data.firmware_update)
      {
      boot_data.crash_count_total = 0;
      boot_data.crash_count_early = 0;
      m_bootreason = BR_FirmwareUpdate;
      ESP_LOGI(TAG, "Firmware update reset");
      m_resetreason = ESP_RST_SW;
      }
    else if (!boot_data.stable_reached)
      {
      boot_data.crash_count_total++;
      boot_data.crash_count_early++;
      m_bootreason = BR_EarlyCrash;
      ESP_LOGE(TAG, "Early crash #%d detected", boot_data.crash_count_early);
      m_resetreason = boot_data.reset_hint;
      ESP_LOGI(TAG, "Reset reason %s (%d)", GetResetReasonName(), GetResetReason());
      }
    else
      {
      boot_data.crash_count_total++;
      m_bootreason = BR_Crash;
      ESP_LOGE(TAG, "Crash #%d detected", boot_data.crash_count_total);
      m_resetreason = boot_data.reset_hint;
      ESP_LOGI(TAG, "Reset reason %s (%d)", GetResetReasonName(), GetResetReason());
      }
    }

  m_crash_count_early = boot_data.crash_count_early;
  m_stack_overflow = boot_data.stack_overflow;
  if (!m_stack_overflow)
    boot_data.stack_overflow_taskname[0] = 0;

  boot_data.bootreason_cpu0 = cpu0;
  boot_data.bootreason_cpu1 = cpu1;
  boot_data.reset_hint = ESP_RST_UNKNOWN;

  // reset flags:
  boot_data.soft_reset = false;
  boot_data.firmware_update = false;
  boot_data.stable_reached = false;
  boot_data.stack_overflow = false;

  boot_data.crc = boot_data.calc_crc();

  // install error handler:
#if ESP_IDF_VERSION_MAJOR < 4
  xt_set_error_handler_callback(ErrorCallback);
#endif

  // Register our commands
  OvmsCommand* cmd_boot = MyCommandApp.RegisterCommand("boot","BOOT framework",boot_status, "", 0, 0, false);
  cmd_boot->RegisterCommand("status","Show boot system status",boot_status,"", 0, 0, false);
  cmd_boot->RegisterCommand("clear","Clear/reset boot system status",boot_clear,"", 0, 0, false);
  }

Boot::~Boot()
  {
  }

void Boot::Init()
  {
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&Boot::UpdateConfig, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.mounted", std::bind(&Boot::UpdateConfig, this, _1, _2));
  }

void Boot::UpdateConfig(std::string event, void* data)
  {
  OvmsConfigParam* param = (OvmsConfigParam*) data;
  if (!param || param->GetName() == "system.adc" || param->GetName() == "vehicle")
    {
    boot_data.adc1_factor     = MyConfig.GetParamValueFloat("system.adc", "factor12v", 195.7);
    boot_data.wakeup_interval = MyConfig.GetParamValueInt("vehicle", "12v.wakeup_interval", 60);
    if (!m_min_12v_level_override)
      boot_data.min_12v_level = MyConfig.GetParamValueFloat("vehicle", "12v.wakeup", 0);
    boot_data.crc = boot_data.calc_crc();
    }
  }

void Boot::SetMin12VLevel(float min_12v_level)
  {
  boot_data.min_12v_level = min_12v_level;
  boot_data.crc = boot_data.calc_crc();
  // inhibit update from config:
  m_min_12v_level_override = true;
  ESP_LOGI(TAG, "Minimum 12V boot level set to %.1fV", min_12v_level);
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
  if (m_stack_overflow)
    return "Stack overflow";
  return (m_resetreason >= 0 && m_resetreason < NUM_RESETREASONS)
    ? resetreason_name[m_resetreason]
    : "Unknown reset reason";
  }

static void boot_shutdown_done(const char* event, void* data)
  {
  MyConfig.unmount();

  if (MyBoot.m_shutdown_deepsleep)
    {
    // For consistency with init, instead of calling MyPeripherals->m_esp32->SetPowerMode(DeepSleep):
    if (MyBoot.m_shutdown_deepsleep_waketime)
      {
      // Accomodate for boot time (15 seconds):
      int seconds = MyBoot.m_shutdown_deepsleep_waketime - time(0) - 15;
      if (seconds < 0) seconds = 0;
      esp_deep_sleep(1000000LL * seconds);
      }
    else
      {
      esp_deep_sleep(1000000LL * MyBoot.m_shutdown_deepsleep_seconds);
      }
    }
  else
    {
    esp_restart();
    }
  }

static void boot_shuttingdown_done(const char* event, void* data)
  {
  if (MyBoot.m_shutdown_pending == 0)
    MyBoot.m_shutdown_timer = 2;
  }

void Boot::Restart(bool hard)
  {
  SetSoftReset();

  if (hard)
    {
    esp_restart();
    return;
    }

  ESP_LOGI(TAG,"Shutting down for %s...", m_shutdown_deepsleep ? "DeepSleep" : "Restart");
  OvmsMutexLock lock(&m_shutdown_mutex);
  m_shutting_down = true;
  m_shutdown_pending = 0;
  m_shutdown_timer = 70; // Give them 70 seconds to shutdown
  MyEvents.SignalEvent("system.shuttingdown", NULL, boot_shuttingdown_done);

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&Boot::Ticker1, this, _1, _2));
  }

void Boot::DeepSleep(unsigned int seconds /*=60*/)
  {
  m_shutdown_deepsleep = true;
  m_shutdown_deepsleep_seconds = seconds;
  m_shutdown_deepsleep_waketime = 0;
  Restart(false);
  }

void Boot::DeepSleep(time_t waketime)
  {
  m_shutdown_deepsleep = true;
  m_shutdown_deepsleep_seconds = 0;
  m_shutdown_deepsleep_waketime = waketime;
  Restart(false);
  }

void Boot::ShutdownPending(const char* tag)
  {
  OvmsMutexLock lock(&m_shutdown_mutex);
  m_shutdown_pending++;
  ESP_LOGW(TAG, "Shutdown: %s pending, %d total", tag, m_shutdown_pending);
  }

void Boot::ShutdownReady(const char* tag)
  {
  OvmsMutexLock lock(&m_shutdown_mutex);
  m_shutdown_pending--;
  ESP_LOGI(TAG, "Shutdown: %s ready, %d pending", tag, m_shutdown_pending);
  if (m_shutdown_pending == 0)
    m_shutdown_timer = 2;
  }

void Boot::Ticker1(std::string event, void* data)
  {
  if (m_shutdown_timer > 0)
    {
    OvmsMutexLock lock(&m_shutdown_mutex);
    m_shutdown_timer--;
    if (m_shutdown_timer == 1)
      {
      ESP_LOGI(TAG, "%s now", m_shutdown_deepsleep ? "DeepSleep" : "Restart");
      }
    else if (m_shutdown_timer == 0)
      {
      MyEvents.SignalEvent("system.shutdown", NULL, boot_shutdown_done);
      return;
      }
    else if ((m_shutdown_timer % 5)==0)
      ESP_LOGI(TAG, "%s in %d seconds (%d pending)...",
        m_shutdown_deepsleep ? "DeepSleep" : "Restart", m_shutdown_timer, m_shutdown_pending);
    }
  }

bool Boot::IsShuttingDown()
  {
  return m_shutting_down;
  }


/*
 * Direct UART output utils borrowed from esp32/panic.c
 */
static void panicPutChar(char c)
  {
  while (((READ_PERI_REG(UART_STATUS_REG(CONFIG_CONSOLE_UART_NUM)) >> UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT) >= 126) ;
  WRITE_PERI_REG(UART_FIFO_REG(CONFIG_CONSOLE_UART_NUM), c);
  }

static void panicPutStr(const char *c)
  {
  int x = 0;
  while (c[x] != 0)
    {
    panicPutChar(c[x]);
    x++;
    }
  }

/*
 * This function is called by task_wdt_isr function (ISR for when TWDT times out).
 * It can be redefined in user code to handle twdt events.
 * Note: It has the same limitations as the interrupt function.
 *       Do not use ESP_LOGI functions inside.
 */
extern "C" void esp_task_wdt_isr_user_handler(void)
  {
  panicPutStr("\r\n[OVMS] ***TWDT***\r\n");
  // Save TWDT task info:
#if ESP_IDF_VERSION_MAJOR < 4
  esp_task_wdt_get_trigger_tasknames(boot_data.wdt_tasknames, sizeof(boot_data.wdt_tasknames));
#else
  snprintf(boot_data.wdt_tasknames, sizeof(boot_data.wdt_tasknames), "(unavailable)");
  #warning "(TODO) ESP-IDF >= 4 : list of tasks triggering WDT not available."
#endif
  }

/*
 * This function is called if FreeRTOS detects a stack overflow.
 */
extern "C" void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
  {
  panicPutStr("\r\n[OVMS] ***ERROR*** A stack overflow in task ");
  panicPutStr((char *)pcTaskName);
  panicPutStr(" has been detected.\r\n");
  strlcpy(boot_data.stack_overflow_taskname, (const char*)pcTaskName, sizeof(boot_data.stack_overflow_taskname));
  boot_data.stack_overflow = true;
  abort();
  }

#if ESP_IDF_VERSION_MAJOR < 4
void Boot::ErrorCallback(XtExcFrame *frame, int core_id, bool is_abort)
  {
  boot_data.reset_hint = ovms_reset_reason_get_hint();

#else

void Boot::ErrorCallback(const void *f, int core_id, bool is_abort, esp_reset_reason_t reset_hint)
  {
#if CONFIG_IDF_TARGET_ARCH_XTENSA
  XtExcFrame *frame = (XtExcFrame *) f;
#elif CONFIG_IDF_TARGET_ARCH_RISCV
  RvExcFrame *frame = (RvExcFrame *) f;
#endif

  boot_data.reset_hint = reset_hint;

#endif

  boot_data.crash_data.core_id = core_id;
  boot_data.crash_data.is_abort = is_abort;

  // Save registers:
  for (int i=0; i<__ARCH_NB_REGS; i++)
    {
    boot_data.crash_data.reg[i] = ((uint32_t*)frame)[i + __ARCH_REG_OFFSET_IN_FRAME];
    }

  // Save backtrace:
  uint32_t i = 0;
#if (ESP_IDF_VERSION_MAJOR < 4) || CONFIG_IDF_TARGET_ARCH_XTENSA
  // (see panic.c::doBacktrace() for code template)
  #define _adjusted_pc(pc) (((pc) & 0x80000000) ? (((pc) & 0x3fffffff) | 0x40000000) : (pc))
  uint32_t pc = frame->pc, sp = frame->a1;
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
#else
  #warning "Backtrace not available in crash_data on this architecture - please fix ovms_boot.cpp"
#endif
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

  // Save current tasks:
  panicPutStr("\r\n[OVMS] Current tasks: ");
  for (int core=0; core<portNUM_PROCESSORS; core++)
    {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    TaskHandle_t task = xTaskGetCurrentTaskHandleForCore(core);
#else
    TaskHandle_t task = xTaskGetCurrentTaskHandleForCPU(core);
#endif
    if (task)
      {
      char *name = pcTaskGetTaskName(task);
      uint32_t stackfree = uxTaskGetStackHighWaterMark(task);
      if (core > 0) panicPutChar('|');
      panicPutStr(name);
      strlcpy(boot_data.curr_task[core].name, name, sizeof(boot_data.curr_task[core].name));
      boot_data.curr_task[core].stackfree = stackfree;
      if (!stackfree) boot_data.stack_overflow = true;
      }
    else
      {
      boot_data.curr_task[core].name[0] = 0;
      boot_data.curr_task[core].stackfree = 0;
      }
    }
  panicPutStr("\r\n");

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
    //  ,<hardware_info>
    //  ,<stack_overflow_task>
    //  ,<core0_task>,<core0_stackfree>,<core1_task>,<core1_stackfree>

    StringWriter buf;
    buf.reserve(2048);
    buf.append("*-OVM-DebugCrash,0,2592000,");
    buf.append(mp_encode(StdMetrics.ms_m_version->AsString("")));
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
        buf.printf("0x%08" PRIx32 " ", boot_data.crash_data.reg[i]);
      }

    // backtrace:
    buf.append(",");
    for (int i=0; i<OVMS_BT_LEVELS && boot_data.crash_data.bt[i].pc; i++)
      buf.printf("0x%08" PRIx32 " ", boot_data.crash_data.bt[i].pc);

    // Reset reason:
    buf.printf(",%d,%s", GetResetReason(), GetResetReasonName());

    // Event debug info:
    buf.printf(",%s,%s,%u", boot_data.curr_event_name, boot_data.curr_event_handler, boot_data.curr_event_runtime);

    // TWDT debug info:
    std::string tasknames = boot_data.wdt_tasknames;
    buf.append(",");
    buf.append(mp_encode(tasknames));

    // Hardware info:
    buf.append(",");
    buf.append(mp_encode(StdMetrics.ms_m_hardware->AsString("")));

    // Stack overflow task:
    std::string name = boot_data.stack_overflow_taskname;
    buf.append(",");
    buf.append(mp_encode(name));

    // Current tasks:
    for (int i = 0; i < 2; i++)
      {
      name = boot_data.curr_task[i].name;
      buf.append(",");
      buf.append(mp_encode(name));
      buf.printf(",%" PRIu32, boot_data.curr_task[i].stackfree);
      }

    MyNotify.NotifyString("data", "debug.crash", buf.c_str());
    }
  }
