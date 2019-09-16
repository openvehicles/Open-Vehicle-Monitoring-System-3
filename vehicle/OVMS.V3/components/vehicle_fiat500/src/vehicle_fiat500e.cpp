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
static const char *TAG = "v-fiat500e";

#include <stdio.h>
#include "vehicle_fiat500e.h"

OvmsVehicleFiat500e::OvmsVehicleFiat500e()
  {
  ESP_LOGI(TAG, "Start Fiat 500e vehicle module");

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  }

OvmsVehicleFiat500e::~OvmsVehicleFiat500e()
  {
  ESP_LOGI(TAG, "Stop Fiat 500e vehicle module");
  }

void OvmsVehicleFiat500e::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicleFiat500e::Ticker1(uint32_t ticker)
  {
  }

class OvmsVehicleFiat500eInit
  {
  public: OvmsVehicleFiat500eInit();
} OvmsVehicleFiat500eInit  __attribute__ ((init_priority (9000)));

OvmsVehicleFiat500eInit::OvmsVehicleFiat500eInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: FIAT 500e (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleFiat500e>("FT5E","Fiat 500e");
  }
