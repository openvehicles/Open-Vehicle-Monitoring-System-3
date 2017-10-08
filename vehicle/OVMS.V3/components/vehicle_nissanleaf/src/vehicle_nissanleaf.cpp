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

#define GEN_1_NEW_CAR_GIDS 281l
#define GEN_1_NEW_CAR_GIDS_S "281"
#define GEN_1_NEW_CAR_RANGE_MILES 84

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
  m_can1->Start(CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  m_can2 = (canbus*)MyPcpApp.FindDeviceByName("can2");
  m_can2->SetPowerMode(On);
  m_can2->Start(CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  m_can3 = (canbus*)MyPcpApp.FindDeviceByName("can3");
  m_can3->SetPowerMode(On);
  m_can3->Start(CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

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

void OvmsVehicleNissanLeaf::IncomingFrameEVBus(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  uint16_t nl_gids;
  uint16_t nl_max_gids = GEN_1_NEW_CAR_GIDS;

  switch (p_frame->MsgID)
    {
    case 0x1db:
    {
      // sent by the LBC, measured inside the battery box
      // current is 11 bit twos complement big endian starting at bit 0
      int16_t nl_battery_current = ((int16_t) d[0] << 3) | (d[1] & 0xe0) >> 5;
      if (nl_battery_current & 0x0400)
        {
        // negative so extend the sign bit
        nl_battery_current |= 0xf800;
        }
      StandardMetrics.ms_v_bat_current->SetValue(nl_battery_current / 2.0f);

      // voltage is 10 bits unsigned big endian starting at bit 16
      int16_t nl_battery_voltage = ((uint16_t) d[2] << 2) | (d[3] & 0xc0) >> 6;
      StandardMetrics.ms_v_bat_voltage->SetValue(nl_battery_voltage / 2.0f);
      break;
    }
    case 0x284:
    {
      // TODO
      //vehicle_nissanleaf_car_on(TRUE);

      uint16_t car_speed16 = d[4];
      car_speed16 = car_speed16 << 8;
      car_speed16 = car_speed16 | d[5];
      // this ratio determined by comparing with the dashboard speedometer
      // it is approximately correct and converts to km/h on my car with km/h speedo
      StandardMetrics.ms_v_pos_speed->SetValue(car_speed16 / 92);
      break;
    }
    case 0x54c:
    {
      if (d[6] == 0xff)
        {
        break;
        }
      // TODO this temperature isn't quite right
      int8_t ambient_temp = d[6] - 56; // Fahrenheit
      ambient_temp = (ambient_temp - 32) / 1.8f; // Celsius
      StandardMetrics.ms_v_temp_ambient->SetValue(ambient_temp);
      break;
    }
    case 0x5bc:
    {
      uint16_t nl_gids_candidate = ((uint16_t) d[0] << 2) | ((d[1] & 0xc0) >> 6);
      if (nl_gids_candidate == 1023)
        {
        // ignore invalid data seen during startup
        break;
        }
      nl_gids = nl_gids_candidate;
      StandardMetrics.ms_v_bat_soc->SetValue((nl_gids * 100 + (nl_max_gids / 2)) / nl_max_gids);
      StandardMetrics.ms_v_bat_range_ideal->SetValue((nl_gids * GEN_1_NEW_CAR_RANGE_MILES + (GEN_1_NEW_CAR_GIDS / 2)) / GEN_1_NEW_CAR_GIDS);
    }
      break;
    case 0x5c0:
      if (d[0] == 0x40)
        {
        StandardMetrics.ms_v_temp_battery->SetValue(d[2] / 2 - 40);
        }
      break;
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrame(CAN_frame_t* p_frame)
  {
  if (p_frame->origin == m_can1)
    {
    IncomingFrameEVBus(p_frame);
    }
  // TODO replace can bus testing with actual can bus decoding:
  if (p_frame->origin == m_can2)
    {
    ESP_LOGI(TAG, "Can 2 Frame");
    m_can2 = NULL;
    }
  if (p_frame->origin == m_can3)
    {
    ESP_LOGI(TAG, "Can 3 Frame");
    m_can3 = NULL;
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
