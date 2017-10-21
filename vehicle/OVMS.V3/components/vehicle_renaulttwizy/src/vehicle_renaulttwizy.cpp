/*
;    Project:       Open Vehicle Monitor System
;    Date:          2017-10-21
;
;    Changes:
;    1.0  Initial commit (stub)
;
;    (c) 2017  Michael Balzer <dexter@dexters-web.de>
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
static const char *TAG = "v-renaulttwizy";

#include <stdio.h>
#include "vehicle_renaulttwizy.h"

OvmsVehicleRenaultTwizy::OvmsVehicleRenaultTwizy()
  {
  ESP_LOGI(TAG, "Renault Twizy vehicle module");
  }

OvmsVehicleRenaultTwizy::~OvmsVehicleRenaultTwizy()
  {
  ESP_LOGI(TAG, "Shutdown Renault Twizy vehicle module");
  }

const std::string OvmsVehicleRenaultTwizy::VehicleName()
  {
  return std::string("Renault Twizy");
  }

class OvmsVehicleRenaultTwizyInit
  {
  public: OvmsVehicleRenaultTwizyInit();
} MyOvmsVehicleRenaultTwizyInit  __attribute__ ((init_priority (9000)));

OvmsVehicleRenaultTwizyInit::OvmsVehicleRenaultTwizyInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Renault Twizy (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleRenaultTwizy>("Renault Twizy");
  }

