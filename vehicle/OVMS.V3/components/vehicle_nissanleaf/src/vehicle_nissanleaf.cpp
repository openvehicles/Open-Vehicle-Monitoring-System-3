/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th September 2017
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
static const char *TAG = "v-nissanleaf";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_nissanleaf.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

static void NL_rxtask(void *pvParameters)
  {
  OvmsVehicleNissanLeaf *me = (OvmsVehicleNissanLeaf*)pvParameters;
  CAN_frame_t frame;

  while(1)
    {
    if (xQueueReceive(me->m_rxqueue, &frame, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      if (me->m_can1 == frame.origin) me->IncomingFrame(&frame);
      }
    }
  }

OvmsVehicleNissanLeaf::OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Nissan Leaf v3.0 vehicle module");

  m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));
  xTaskCreatePinnedToCore(NL_rxtask, "v_NL Rx Task", 4096, (void*)this, 5, &m_rxtask, 1);

  m_can1 = (canbus*)MyPcpApp.FindDeviceByName("can1");
  m_can1->SetPowerMode(On);
  m_can1->Start(CAN_MODE_ACTIVE,CAN_SPEED_1000KBPS);

  MyCan.RegisterListener(m_rxqueue);
  }

OvmsVehicleNissanLeaf::~OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Shutdown Nissan Leaf vehicle module");

  m_can1->SetPowerMode(Off);
  MyCan.DeregisterListener(m_rxqueue);

  vQueueDelete(m_rxqueue);
  vTaskDelete(m_rxtask);
  }

const std::string OvmsVehicleNissanLeaf::VehicleName()
  {
  return std::string("Nissan Leaf");
  }

void OvmsVehicleNissanLeaf::IncomingFrame(CAN_frame_t* p_frame)
  {
  if (p_frame->origin != m_can1) return; // Only handle CAN1

  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    // todo
    }
  }

class OvmsVehicleNissanLeafInit
  {
  public: OvmsVehicleNissanLeafInit();
  } MyOvmsVehicleNissanLeafInit  __attribute__ ((init_priority (9000)));

OvmsVehicleNissanLeafInit::OvmsVehicleNissanLeafInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Nissan Leaf (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleNissanLeaf>("NL");
  }
