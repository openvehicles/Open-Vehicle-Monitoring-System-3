/*
;    Project:       Open Vehicle Monitor System
;	 Subproject:    Integrate VW e-Golf
;
;    Changes:
;    Sept 6, 2023: stub made
;
;    (C) 2023  Jared Greco <jaredgreco@gmail.com>
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
static const char *TAG = "v-vwegolf";

#include <stdio.h>
#include "vehicle_vwegolf.h"

OvmsVehicleVWeGolf::OvmsVehicleVWeGolf()
  {
	ESP_LOGI(TAG, "Start vehicle module: VW e-Golf");

	// Start can1 (OBD)
    //RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  }

OvmsVehicleVWeGolf::~OvmsVehicleVWeGolf()
  {
	ESP_LOGI(TAG, "Stop vehicle module: VW e-Golf");
  }

class OvmsVehicleVWeGolfInit
  {
  public: OvmsVehicleVWeGolfInit();
} MyOvmsVehicleVWeGolfInit  __attribute__ ((init_priority (9000)));

OvmsVehicleVWeGolfInit::OvmsVehicleVWeGolfInit()
  {
	ESP_LOGI(TAG, "Registering Vehicle: VW e-Golf (9000)");

	MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeGolf>("VWEG","VW e-Golf");
  }

