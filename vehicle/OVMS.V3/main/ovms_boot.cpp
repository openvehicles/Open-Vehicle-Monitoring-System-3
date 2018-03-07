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

#include "ovms.h"
#include "ovms_boot.h"
#include "ovms_command.h"
#include <string.h>

boot_data_t __attribute__((section(".rtc.noload"))) boot_data;

Boot MyBoot __attribute__ ((init_priority (1100)));

void boot_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Last boot was %d second(s) ago\n",monotonictime);
  writer->printf("  This is reset #%d since last power cycle\n",boot_data.boot_count);
  writer->printf("  CPU#0 boot reason was %d\n",boot_data.bootreason_cpu0);
  writer->printf("  CPU#1 boot reason was %d\n",boot_data.bootreason_cpu1);
  }

Boot::Boot()
  {
  ESP_LOGI(TAG, "Initialising BOOT (1100)");

  RESET_REASON cpu0 = rtc_get_reset_reason(0);
  RESET_REASON cpu1 = rtc_get_reset_reason(1);

  if (cpu0 == POWERON_RESET)
    {
    memset(&boot_data,0,sizeof(boot_data_t));
    ESP_LOGI(TAG, "Power cycle reset detected");
    }
  else
    {
    boot_data.boot_count++;
    ESP_LOGI(TAG, "Boot #%d reasons for CPU0=%d and CPU1=%d",boot_data.boot_count,cpu0,cpu1);
    }

  boot_data.bootreason_cpu0 = cpu0;
  boot_data.bootreason_cpu1 = cpu1;

  // Register our commands
  OvmsCommand* cmd_boot = MyCommandApp.RegisterCommand("boot","BOOT framework",NULL, "", 1);
  cmd_boot->RegisterCommand("status","Show boot system status",boot_status,"", 0, 0, false);
  }

Boot::~Boot()
  {
  }
