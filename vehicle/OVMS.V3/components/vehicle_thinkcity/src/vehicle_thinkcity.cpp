/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
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
static const char *TAG = "v-thinkcity";

#include <stdio.h>
#include "vehicle_thinkcity.h"

OvmsVehicleThinkCity::OvmsVehicleThinkCity()
  {
  ESP_LOGI(TAG, "Start Think City vehicle module");

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  // require GPS:
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);
  }

OvmsVehicleThinkCity::~OvmsVehicleThinkCity()
  {
  ESP_LOGI(TAG, "Stop Think City vehicle module");
  }

void OvmsVehicleThinkCity::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x301:
      {
      float soc = 100.0 - ((((unsigned int)d[4]<<8) + d[5])/10);
      StandardMetrics.ms_v_bat_current->SetValue( (((int) d[0] << 8) + d[1]) / 10 );
      StandardMetrics.ms_v_bat_voltage->SetValue( (((unsigned int) d[2] << 8) + d[3]) / 10 );
      StandardMetrics.ms_v_bat_soc->SetValue(soc);
      StandardMetrics.ms_v_bat_temp->SetValue( (((signed int)d[6]<<8) + d[7])/10 );
      StandardMetrics.ms_v_bat_range_ideal->SetValue(111.958773 * soc / 100);
      StandardMetrics.ms_v_bat_range_est->SetValue(93.205678 * soc / 100);
      break;
      }
    default:
      break;
    }
  }

void OvmsVehicleThinkCity::Ticker1(uint32_t ticker)
  {
  }

void OvmsVehicleThinkCity::Ticker10(uint32_t ticker)
  {
  }

OvmsVehicle::vehicle_command_t OvmsVehicleThinkCity::CommandLock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleThinkCity::CommandUnlock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleThinkCity::CommandActivateValet(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleThinkCity::CommandDeactivateValet(const char* pin)
  {
  return NotImplemented;
  }

class OvmsVehicleThinkCityInit
  {
  public: OvmsVehicleThinkCityInit();
} OvmsVehicleThinkCityInit  __attribute__ ((init_priority (9000)));

OvmsVehicleThinkCityInit::OvmsVehicleThinkCityInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: THINK CITY (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleThinkCity>("TGTC","Think City");
  }
