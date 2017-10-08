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

#include "esp_log.h"
static const char *TAG = "housekeeping";

#include <stdio.h>
#include <string.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include "ovms.h"
#include "ovms_housekeeping.h"
#include "ovms_peripherals.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "ovms_config.h"

#include "esp_heap_alloc_caps.h"
//extern "C" {
//#include "esp_heap_caps.h"
//}

ConsoleAsync *MyUsbConsole;

int console_logger(const char *format, va_list arg)
  {
  char *buffer = (char*)malloc(512);
  vsnprintf(buffer, 512, format, arg);
  int k = strlen(buffer);
  MyUsbConsole->Log(buffer);
  return k;
  }

#ifdef CONFIG_OVMS_DEV_LOGSTATUS
int TestAlerts = true;
#else
int TestAlerts = false;
#endif

void HousekeepingTicker1( TimerHandle_t timer )
  {
  Housekeeping* h = (Housekeeping*)pvTimerGetTimerID(timer);
  h->Ticker1();
  }

void HousekeepingTask(void *pvParameters)
  {
  Housekeeping* me = (Housekeeping*)pvParameters;

  vTaskDelay(50 / portTICK_PERIOD_MS);

  me->init();

  me->version();

  while (1)
    {
    me->metrics();

    if (TestAlerts)
      {
      MyCommandApp.LogPartial("SHOULD NOT BE SEEN\r");
      uint32_t caps = MALLOC_CAP_8BIT;
      size_t free = xPortGetFreeHeapSizeCaps(caps);
//      size_t free = heap_caps_get_free_size(caps);
      MyCommandApp.LogPartial("Free %zu  ",free);
      MyCommandApp.Log("Tasks %u\r\n", uxTaskGetNumberOfTasks());
      }

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
  }

Housekeeping::Housekeeping()
  {
  ESP_LOGI(TAG, "Initialising HOUSEKEEPING Framework...");

  MyConfig.RegisterParam("system.adc", "ADC configuration", true, true);

  xTaskCreatePinnedToCore(HousekeepingTask, "HousekeepingTask", 4096, (void*)this, 5, &m_taskid, 1);
  }

Housekeeping::~Housekeeping()
  {
  }

void Housekeeping::init()
  {
  ESP_LOGI(TAG, "Executing on CPU core %d",xPortGetCoreID());

  m_tick = 0;
  m_timer1 = xTimerCreate("Housekeep ticker",1000 / portTICK_PERIOD_MS,pdTRUE,this,HousekeepingTicker1);
  xTimerStart(m_timer1, 0);

  ESP_LOGI(TAG, "Starting PERIPHERALS...");
  MyPeripherals = new Peripherals();

  MyPeripherals->m_esp32can->SetPowerMode(Off);
  MyPeripherals->m_ext12v->SetPowerMode(Off);

  ESP_LOGI(TAG, "Starting USB console...");
  MyUsbConsole = new ConsoleAsync();

  esp_log_set_vprintf(console_logger);

  MyEvents.SignalEvent("system.start",NULL);
  }

void Housekeeping::version()
  {
  std::string version(OVMS_VERSION);

  const esp_partition_t *p = esp_ota_get_running_partition();
  if (p != NULL)
    {
    version.append("/");
    version.append(p->label);
    }
  version.append("/");
  version.append(CONFIG_OVMS_VERSION_TAG);

  version.append(" build (idf ");
  version.append(esp_get_idf_version());
  version.append(") ");
  version.append(__DATE__);
  version.append(" ");
  version.append(__TIME__);
  StandardMetrics.ms_m_version->SetValue(version.c_str());

  std::string hardware("OVMS ");
  esp_chip_info_t chip;
  esp_chip_info(&chip);
  if (chip.features & CHIP_FEATURE_EMB_FLASH) hardware.append("EMBFLASH ");
  if (chip.features & CHIP_FEATURE_WIFI_BGN) hardware.append("WIFI ");
  if (chip.features & CHIP_FEATURE_BLE) hardware.append("BLE ");
  if (chip.features & CHIP_FEATURE_BT) hardware.append("BT ");
  char buf[32]; sprintf(buf,"cores=%d ",chip.cores); hardware.append(buf);
  sprintf(buf,"rev=ESP32/%d",chip.revision); hardware.append(buf);
  StandardMetrics.ms_m_hardware->SetValue(hardware.c_str());

  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  sprintf(buf,"%02x:%02x:%02x:%02x:%02x:%02x",
          mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  StandardMetrics.ms_m_serial->SetValue(buf);
  }

void Housekeeping::metrics()
  {
  OvmsMetricFloat* m1 = StandardMetrics.ms_v_bat_12v;
  if (m1 == NULL)
    return;

  // Allow the user to adjust the ADC conversion factor
  float f = MyConfig.GetParamValueFloat("system.adc","factor12v");
  if (f == 0) f = 182;
  float v = (float)MyPeripherals->m_esp32adc->read() / f;
  m1->SetValue(v);

  OvmsMetricInt* m2 = StandardMetrics.ms_m_tasks;
  if (m2 == NULL)
    return;

  m2->SetValue(uxTaskGetNumberOfTasks());

  OvmsMetricInt* m3 = StandardMetrics.ms_m_freeram;
  if (m3 == NULL)
    return;
  uint32_t caps = MALLOC_CAP_8BIT;
  size_t free = xPortGetFreeHeapSizeCaps(caps);
//  size_t free = heap_caps_get_free_size(caps);
  m3->SetValue(free);
  }

void Housekeeping::Ticker1()
  {
  monotonictime++;
  StandardMetrics.ms_m_monotonic->SetValue((int)monotonictime);

  MyEvents.SignalEvent("ticker.1", NULL);

  m_tick++;
  if ((m_tick % 10)==0) MyEvents.SignalEvent("ticker.10", NULL);
  if ((m_tick % 60)==0) MyEvents.SignalEvent("ticker.60", NULL);
  if ((m_tick % 300)==0) MyEvents.SignalEvent("ticker.300", NULL);
  if ((m_tick % 600)==0) MyEvents.SignalEvent("ticker.600", NULL);
  if ((m_tick % 3600)==0)
    {
    m_tick = 0;
    MyEvents.SignalEvent("ticker.3600", NULL);
    }
  }
