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

#ifndef __OVMS_BOOT_H__
#define __OVMS_BOOT_H__

#include <string>

#include "rom/rtc.h"
#include "rom/crc.h"
#include "esp_system.h"
#include "ovms_events.h"
#include "ovms_mutex.h"

typedef enum
  {
    BR_PowerOn = 0,                 // standard power on / hard reset
    BR_Wakeup,                      // reboot from deep sleep
    BR_SoftReset,                   // user requested reset ("module reset")
    BR_FirmwareUpdate,              // Firmware update reset
    BR_EarlyCrash,                  // crash during boot/init phase
    BR_Crash,                       // crash after reaching stable state
  } bootreason_t;

#if (ESP_IDF_VERSION_MAJOR < 4) || CONFIG_IDF_TARGET_ARCH_XTENSA
  #define __ARCH_NB_REGS 24
  #define __ARCH_REG_OFFSET_IN_FRAME 1
#elif CONFIG_IDF_TARGET_ARCH_RISCV
  #define __ARCH_NB_REGS 37
  #define __ARCH_REG_OFFSET_IN_FRAME 0
#else
  #error "Unknown architecture, please fix ovms_boot.h"
#endif

#define OVMS_BT_LEVELS 32
typedef struct
  {
  int core_id;
  bool is_abort;
  uint32_t reg[__ARCH_NB_REGS];
  struct
    {
    uint32_t pc;
    // possibly add stack info later on
    } bt[OVMS_BT_LEVELS];
  } crash_data_t;

typedef struct
  {
  char name[16];
  uint32_t stackfree;
  } task_info_t;

typedef struct
  {
  // data consistency:
  uint32_t crc;
  uint32_t calc_crc() { return crc32_le(0, (uint8_t*)this+sizeof(crc), sizeof(*this)-sizeof(crc)); }

  // payload:
  unsigned int boot_count;          // Number of times system has rebooted (not power on)
  RESET_REASON bootreason_cpu0;     // Reason for last boot on CPU#0
  RESET_REASON bootreason_cpu1;     // Reason for last boot on CPU#1
  float adc1_factor;                // 12V battery ADC calibration factor
  float min_12v_level;              // 12V battery minimum voltage level to allow boot
  int wakeup_interval;              // Wakeup interval in seconds for 12V restoration check
  bool soft_reset;                  // true = user requested reset ("module reset")
  bool firmware_update;             // true = firmware update restart
  bool stable_reached;              // true = system has reached stable state (see housekeeping)
  unsigned int crash_count_early;   // Number of times system has crashed before reaching stable state
  unsigned int crash_count_total;   // Total number of times system has crashed since power on
  crash_data_t crash_data;          // Register dump & backtrace info
  esp_reset_reason_t reset_hint;    // Copy of RTC_RESET_CAUSE_REG
  char curr_event_name[32];         // Copy of MyEvents.m_current_event
  char curr_event_handler[16];      // … MyEvents.m_current_callback->m_caller
  uint16_t curr_event_runtime;      // … monotonictime-MyEvents.m_current_started
  char wdt_tasknames[32];           // Pipe (|) separated list of the tasks that triggered the TWDT
  bool stack_overflow;
  char stack_overflow_taskname[16];
  task_info_t curr_task[portNUM_PROCESSORS];
  } boot_data_t;

extern boot_data_t boot_data;

class Boot
  {
  public:
    Boot();
    virtual ~Boot();
    void Init();

  public:
    bootreason_t GetBootReason() { return m_bootreason; }
    const char* GetBootReasonName();
    esp_reset_reason_t GetResetReason() { return m_resetreason; }
    const char* GetResetReasonName();
    unsigned int GetCrashCount() { return boot_data.crash_count_total; }
    unsigned int GetEarlyCrashCount() { return m_crash_count_early; }
    bool GetStable() { return boot_data.stable_reached; }
    void SetStable();

  public:
    void SetSoftReset();
    void SetFirmwareUpdate();
    void SetMin12VLevel(float min_12v_level);
    float GetMin12VLevel() { return boot_data.min_12v_level; }
    void Restart(bool hard=false);
    void DeepSleep(unsigned int seconds = 60);
    void DeepSleep(time_t waketime);
    void ShutdownPending(const char* tag);
    void ShutdownReady(const char* tag);
    bool IsShuttingDown();
    void Ticker1(std::string event, void* data);
    void UpdateConfig(std::string event, void* data);

  public:
    OvmsMutex m_shutdown_mutex;
    unsigned int m_shutdown_timer;
    unsigned int m_shutdown_pending;
    bool m_shutdown_deepsleep;
    unsigned int m_shutdown_deepsleep_seconds;
    time_t m_shutdown_deepsleep_waketime;
    bool m_shutting_down;
    bool m_min_12v_level_override;

  public:
#if ESP_IDF_VERSION_MAJOR < 4
    static void ErrorCallback(XtExcFrame *f, int core_id, bool is_abort);
#else
    static void ErrorCallback(const void *f, int core_id, bool is_abort, esp_reset_reason_t reset_hint);
#endif
    void NotifyDebugCrash();

  protected:
    bootreason_t m_bootreason;
    esp_reset_reason_t m_resetreason;
    unsigned int m_crash_count_early;
    bool m_stack_overflow;
  };

extern Boot MyBoot;

void OnBoot();

#endif //#ifndef __OVMS_BOOT_H__
