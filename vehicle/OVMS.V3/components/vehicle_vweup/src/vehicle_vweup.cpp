/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX

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

/*
;    Subproject:    Integration of support for the VW e-UP
;    Date:          18th February 2020
;
;    Changes:
;    0.1.0  Initial code
:           Code frame with correct TAG and vehicle/can registration
;
;    (C) 2020       Chris van der Meijden
*/

#include "ovms_log.h"
static const char *TAG = "v-vweup";

#include <stdio.h>
#include "vehicle_vweup.h"

OvmsVehicleVWeUP::OvmsVehicleVWeUP()
  {
  ESP_LOGI(TAG, "Start VW e-Up vehicle module");

  RegisterCanBus(3,CAN_MODE_ACTIVE,CAN_SPEED_100KBPS);
  }

OvmsVehicleVWeUP::~OvmsVehicleVWeUP()
  {
  ESP_LOGI(TAG, "Stop VW e-Up vehicle module");
  }

void OvmsVehicleVWeUP::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicleVWeUP::Ticker1(uint32_t ticker)
  {
  }

class OvmsVehicleVWeUPInit
  {
  public: OvmsVehicleVWeUPInit();
} OvmsVehicleVWeUPInit  __attribute__ ((init_priority (9000)));

OvmsVehicleVWeUPInit::OvmsVehicleVWeUPInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: VW e-Up (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeUP>("VWUP","VW e-Up");
  }
