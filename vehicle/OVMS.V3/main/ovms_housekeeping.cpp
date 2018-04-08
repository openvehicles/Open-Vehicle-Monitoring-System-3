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
static const char *TAG = "housekeeping";

#include <stdio.h>
#include <string.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include "ovms.h"
#include "ovms_housekeeping.h"
#include "ovms_peripherals.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "ovms_config.h"
#include "console_async.h"
#include "ovms_module.h"
#include "ovms_boot.h"
#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_SERVER_V2
#include "ovms_server_v2.h"
#endif
#ifdef CONFIG_OVMS_COMP_SERVER_V3
#include "ovms_server_v3.h"
#endif
#include "rom/rtc.h"
#ifdef CONFIG_OVMS_COMP_OBD2ECU
#include "obd2ecu.h"
#endif

#define AUTO_INIT_STABLE_TIME           10      // seconds after which an auto init boot is considered stable
                                                // (Note: resolution = 10 seconds)
#define AUTO_INIT_INHIBIT_CRASHCOUNT    5

static int tick = 0;

void HousekeepingTicker1( TimerHandle_t timer )
  {
  monotonictime++;
  StandardMetrics.ms_m_monotonic->SetValue((int)monotonictime);

  MyEvents.SignalEvent("ticker.1", NULL);

  tick++;
  if ((tick % 10)==0) MyEvents.SignalEvent("ticker.10", NULL);
  if ((tick % 60)==0) MyEvents.SignalEvent("ticker.60", NULL);
  if ((tick % 300)==0) MyEvents.SignalEvent("ticker.300", NULL);
  if ((tick % 600)==0) MyEvents.SignalEvent("ticker.600", NULL);
  if ((tick % 3600)==0)
    {
    tick = 0;
    MyEvents.SignalEvent("ticker.3600", NULL);
    }
  }

Housekeeping::Housekeeping()
  {
  ESP_LOGI(TAG, "Initialising HOUSEKEEPING Framework...");

  MyConfig.RegisterParam("system.adc", "ADC configuration", true, true);
  MyConfig.RegisterParam("auto", "Auto init configuration", true, true);

  // Register our events
  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"housekeeping.init", std::bind(&Housekeeping::Init, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.10", std::bind(&Housekeeping::Metrics, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.600", std::bind(&Housekeeping::TimeLogger, this, _1, _2));

  // Fire off the event that causes us to be called back in Events tasks context
  MyEvents.SignalEvent("housekeeping.init", NULL);
  }

Housekeeping::~Housekeeping()
  {
  }

void Housekeeping::Init(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Executing on CPU core %d",xPortGetCoreID());
  ESP_LOGI(TAG, "reset_reason: cpu0=%d, cpu1=%d", rtc_get_reset_reason(0), rtc_get_reset_reason(1));

  tick = 0;
  m_timer1 = xTimerCreate("Housekeep ticker",1000 / portTICK_PERIOD_MS,pdTRUE,this,HousekeepingTicker1);
  xTimerStart(m_timer1, 0);

  ESP_LOGI(TAG, "Initialising WATCHDOG...");
  esp_task_wdt_init(120, true);

  ESP_LOGI(TAG, "Starting PERIPHERALS...");
  MyPeripherals = new Peripherals();

#ifdef CONFIG_OVMS_COMP_ESP32CAN
  MyPeripherals->m_esp32can->SetPowerMode(Off);
#endif // #ifdef CONFIG_OVMS_COMP_ESP32CAN

#ifdef CONFIG_OVMS_COMP_EXT12V
  MyPeripherals->m_ext12v->SetPowerMode(Off);
#endif // #ifdef CONFIG_OVMS_COMP_EXT12V

  // component auto init:
  if (!MyConfig.GetParamValueBool("auto", "init", true))
    {
    ESP_LOGW(TAG, "Auto init disabled (enable: config set auto init yes)");
    }
  else if (MyBoot.GetEarlyCrashCount() >= AUTO_INIT_INHIBIT_CRASHCOUNT)
    {
    ESP_LOGE(TAG, "Auto init inhibited: too many early crashes (%d)", MyBoot.GetEarlyCrashCount());
    }
  else
    {
#ifdef CONFIG_OVMS_COMP_EXT12V
    ESP_LOGI(TAG, "Auto init ext12v (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    MyPeripherals->m_ext12v->AutoInit();
#endif // CONFIG_OVMS_COMP_EXT12V

#ifdef CONFIG_OVMS_COMP_WIFI
    ESP_LOGI(TAG, "Auto init wifi (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    MyPeripherals->m_esp32wifi->AutoInit();
#endif // CONFIG_OVMS_COMP_WIFI

#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
    ESP_LOGI(TAG, "Auto init modem (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    MyPeripherals->m_simcom->AutoInit();
#endif // #ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM

    ESP_LOGI(TAG, "Auto init vehicle (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    MyVehicleFactory.AutoInit();

#ifdef CONFIG_OVMS_COMP_OBD2ECU
    ESP_LOGI(TAG, "Auto init obd2ecu (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    obd2ecuInit.AutoInit();
#endif // CONFIG_OVMS_COMP_OBD2ECU

#ifdef CONFIG_OVMS_COMP_SERVER
#ifdef CONFIG_OVMS_COMP_SERVER_V2
    ESP_LOGI(TAG, "Auto init server v2 (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    MyOvmsServerV2Init.AutoInit();
#endif // CONFIG_OVMS_COMP_SERVER_V2
#ifdef CONFIG_OVMS_COMP_SERVER_V3
    ESP_LOGI(TAG, "Auto init server v3 (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    MyOvmsServerV3Init.AutoInit();
#endif // CONFIG_OVMS_COMP_SERVER_V3
#endif // CONFIG_OVMS_COMP_SERVER

    ESP_LOGI(TAG, "Auto init done (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    }

  ESP_LOGI(TAG, "Starting USB console...");
  ConsoleAsync::Instance();

  MyEvents.SignalEvent("system.start",NULL);

  Metrics(event,data); // Causes the metrics to be produced
  }

void Housekeeping::Metrics(std::string event, void* data)
  {
#ifdef CONFIG_OVMS_COMP_ADC
  OvmsMetricFloat* m1 = StandardMetrics.ms_v_bat_12v_voltage;
  if (m1 == NULL)
    return;

  // Allow the user to adjust the ADC conversion factor
  float f = MyConfig.GetParamValueFloat("system.adc","factor12v");
  if (f == 0) f = 182;
  float v = (float)MyPeripherals->m_esp32adc->read() / f;
  m1->SetValue(v);
#endif // #ifdef CONFIG_OVMS_COMP_ADC

  OvmsMetricInt* m2 = StandardMetrics.ms_m_tasks;
  if (m2 == NULL)
    return;

  m2->SetValue(uxTaskGetNumberOfTasks());

  OvmsMetricInt* m3 = StandardMetrics.ms_m_freeram;
  if (m3 == NULL)
    return;
  uint32_t caps = MALLOC_CAP_8BIT;
  size_t free = heap_caps_get_free_size(caps);
  m3->SetValue(free);

  // set boot stable flag after some seconds uptime:
  if (!MyBoot.GetStable() && monotonictime >= AUTO_INIT_STABLE_TIME)
    {
    ESP_LOGI(TAG, "System considered stable (free: %d bytes)", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    MyBoot.SetStable();
    // …and send debug crash data as necessary:
    MyBoot.NotifyDebugCrash();
    }
  }

void Housekeeping::TimeLogger(std::string event, void* data)
  {
  time_t rawtime;
  time ( &rawtime );
  struct tm* tmu = localtime(&rawtime);
  ESP_LOGI(TAG, "Local time: %.24s", asctime(tmu));
  }
