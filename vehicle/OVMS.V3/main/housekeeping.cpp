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

#include <stdio.h>
#include <string.h>
#include <esp_ota_ops.h>
#include "ovms.h"
#include "housekeeping.h"
#include "peripherals.h"
#include "metrics.h"
#include "metrics_standard.h"

#include "esp_heap_alloc_caps.h"

Housekeeping MyHousekeeping __attribute__ ((init_priority (9999)));

void HousekeepingTask(void *pvParameters)
  {
  vTaskDelay(50 / portTICK_PERIOD_MS);

  MyHousekeeping.version();

  while (1)
    {
    MyHousekeeping.metrics();

    // Test CAN
    CAN_frame_t c;
    memset(&c,0,sizeof(CAN_frame_t));
    c.origin = NULL;
    c.MsgID = 0x42;
    c.FIR.B.DLC = 7;
    c.data.u8[0] = 'E';
    c.data.u8[1] = 'S';
    c.data.u8[2] = 'P';
    c.data.u8[3] = 'O';
    c.data.u8[4] = 'V';
    c.data.u8[5] = 'M';
    c.data.u8[6] = 'S';
    MyPeripherals.m_mcp2515_1->Write(&c);

#ifdef CONFIG_OVMS_CONSOLE_LOG_STATUS
    MyCommandApp.Log("SHOULD NOT BE SEEN\r");
    uint32_t caps = MALLOC_CAP_8BIT;
    MyCommandApp.Log("Free %zu  ", xPortGetFreeHeapSizeCaps(caps));
    MyCommandApp.Log("Tasks %u  ", uxTaskGetNumberOfTasks());
    MyCommandApp.Log("Housekeeping 12V %f\r\n", MyPeripherals.m_esp32adc->read() / 194);
#endif

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
  }

Housekeeping::Housekeeping()
  {
  puts("Initialising HOUSEKEEPING Framework...");

  xTaskCreatePinnedToCore(HousekeepingTask, "HousekeepingTask", 4096, (void*)this, 5, &m_taskid, 1);
  }

Housekeeping::~Housekeeping()
  {
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

  version.append(" build ");
  version.append(__DATE__);
  version.append(" ");
  version.append(__TIME__);
  MyMetrics.Set(MS_M_VERSION, version.c_str());
  }

void Housekeeping::metrics()
  {
  OvmsMetricFloat* m1 = (OvmsMetricFloat*)MyMetrics.Find(MS_V_BAT_12V);
  if (m1 == NULL)
    return;

  float v = MyPeripherals.m_esp32adc->read() / 194;
  m1->SetValue(v);

  OvmsMetricInt* m2 = (OvmsMetricInt*)MyMetrics.Find(MS_M_TASKS);
  if (m2 == NULL)
    return;

  uint32_t caps = MALLOC_CAP_8BIT;
  m2->SetValue(uxTaskGetNumberOfTasks());

  OvmsMetricInt* m3 = (OvmsMetricInt*)MyMetrics.Find(MS_M_FREERAM);
  if (m3 == NULL)
    return;

  m3->SetValue(xPortGetFreeHeapSizeCaps(caps));
  }

