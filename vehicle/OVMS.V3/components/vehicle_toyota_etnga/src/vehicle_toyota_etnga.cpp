/*
;    Project:       Open Vehicle Monitor System
;    Module:        Vehicle Toyota e-TNGA platform
;    Date:          4th June 2023
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2023       Jerry Kezar <solterra@kezarnet.com>
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
static const char *TAG = "v-toyota-etnga";

#include "vehicle_toyota_etnga.h"

/**
 * Toyota e-TNGA constructor
 */
OvmsVehicleToyotaETNGA::OvmsVehicleToyotaETNGA()
  {
  ESP_LOGI(TAG, "Toyota e-TNGA vehicle module");

  // Init CAN:
   RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
 
  }

/**
 * Toyota e-TNGA destructor
 */
OvmsVehicleToyotaETNGA::~OvmsVehicleToyotaETNGA()
  {
  ESP_LOGI(TAG, "Shutdown Toyota e-TNGA platform vehicle module");
  }

void OvmsVehicleToyotaETNGA::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
  // Implement how to handle incoming poll replies specific to the Toyota e-TNGA platform
  // Extract relevant data from the reply, update metrics, and perform any required actions
}
