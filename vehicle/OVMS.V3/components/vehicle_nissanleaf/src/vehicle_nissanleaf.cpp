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

typedef enum
  {
  CHARGER_STATUS_IDLE,
  CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT,
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_QUICK_CHARGING,
  CHARGER_STATUS_FINISHED
  } ChargerStatus;

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

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_car_on()
// Takes care of setting all the state appropriate when the car is on
// or off. Centralized so we can more easily make on and off mirror
// images.
//

void vehicle_nissanleaf_car_on(bool isOn)
  {
  StandardMetrics.ms_v_env_on->SetValue(isOn);
  StandardMetrics.ms_v_env_awake->SetValue(isOn);
  // We don't know if handbrake is on or off, but it's usually off when the car is on.
  StandardMetrics.ms_v_env_handbrake->SetValue(isOn);
  if (isOn)
    {
    StandardMetrics.ms_v_door_chargeport->SetValue(false);
    StandardMetrics.ms_v_charge_pilot->SetValue(false);
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    // TODO
    // car_chargestate = 0;
    // car_chargesubstate = 0;
    }
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_charger_status()
// Takes care of setting all the charger state bit when the charger
// switches on or off. Separate from vehicle_nissanleaf_poll1() to make
// it clearer what is going on.
//

void vehicle_nissanleaf_charger_status(ChargerStatus status)
  {
  switch (status)
    {
    case CHARGER_STATUS_IDLE:
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      // TODO
      // car_chargestate = 0;
      // car_chargesubstate = 0;
      break;
    case CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT:
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      // TODO
      // car_chargestate = 0;
      // car_chargesubstate = 0;
      break;
    case CHARGER_STATUS_QUICK_CHARGING:
    case CHARGER_STATUS_CHARGING:
      if (!StandardMetrics.ms_v_charge_inprogress->AsBool())
        {
        StandardMetrics.ms_v_charge_kwh->SetValue(0); // Reset charge kWh
        }
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      // TODO
      // car_chargestate = 1;
      // car_chargesubstate = 3; // Charging by request

      // TODO only use battery current for Quick Charging, for regular charging
      // we should return AC line current and voltage, not battery
      // TODO does the leaf know the AC line current and voltage?
      // TODO v3 supports negative values here, what happens if we send a negative charge current to a v2 client?
      if (StandardMetrics.ms_v_bat_current->AsFloat() < 0)
        {
        // battery current can go negative when climate control draws more than
        // is available from the charger. We're abusing the line current which
        // is unsigned, so don't underflow it
        //
        // TODO quick charging can draw current from the vehicle
        StandardMetrics.ms_v_charge_current->SetValue(0);
        }
      else
        {
        StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsFloat());
        }
      StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
      break;
    case CHARGER_STATUS_FINISHED:
      // Charging finished
      StandardMetrics.ms_v_charge_current->SetValue(0);
      // TODO set this in ovms v2
      // TODO the charger probably knows the line voltage, when we find where it's
      // coded, don't zero it out when we're plugged in but not charging
      StandardMetrics.ms_v_charge_voltage->SetValue(0);
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      // TODO
      // car_chargestate = 4; // Charge DONE
      // car_chargesubstate = 3; // Charging by request
      break;
    }
  if (status != CHARGER_STATUS_CHARGING && status != CHARGER_STATUS_QUICK_CHARGING)
    {
    StandardMetrics.ms_v_charge_current->SetValue(0);
    // TODO the charger probably knows the line voltage, when we find where it's
    // coded, don't zero it out when we're plugged in but not charging
    StandardMetrics.ms_v_charge_voltage->SetValue(0);
    }
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
    }
      break;
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
    }
      break;
    case 0x54b:
    {
      bool hvac_candidate;
      // this might be a bit field? So far these 6 values indicate HVAC on
      hvac_candidate = 
        d[1] == 0x0a || // Gen 1 Remote
        d[1] == 0x48 || // Manual Heating or Fan Only
        d[1] == 0x4b || // Gen 2 Remote Heating
        d[1] == 0x71 || // Gen 2 Remote Cooling
        d[1] == 0x76 || // Auto
        d[1] == 0x78;   // Manual A/C on
      StandardMetrics.ms_v_env_hvac->SetValue(hvac_candidate);
    }
      break;
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
    case 0x5bf:
      if (d[4] == 0xb0)
        {
        // Quick Charging
        // TODO enum?
        StandardMetrics.ms_v_charge_type->SetValue("CHAdeMO");
        StandardMetrics.ms_v_charge_climit->SetValue(120);
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_door_chargeport->SetValue(true);
        }
      else
        {
        // Maybe J1772 is connected
        // can_databuffer[2] is the J1772 maximum available current, 0 if we're not plugged in
        // TODO enum?
        StandardMetrics.ms_v_charge_type->SetValue("J1772");
        uint8_t current_limit = d[2] / 5;
        StandardMetrics.ms_v_charge_climit->SetValue(current_limit);
        StandardMetrics.ms_v_charge_pilot->SetValue(current_limit != 0);
        StandardMetrics.ms_v_door_chargeport->SetValue(current_limit != 0);
        }

      switch (d[4])
        {
        case 0x28:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x30:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        case 0xb0: // Quick Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          break;
        case 0x60: // L2 Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x40:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_FINISHED);
          break;
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
