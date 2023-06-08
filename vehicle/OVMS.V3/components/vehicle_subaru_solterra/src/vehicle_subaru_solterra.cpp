/*
;    Project:       Open Vehicle Monitor System
;    Module:        Vehicle Subaru Solterra
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
static const char *TAG = "v-subaru-solterra";

#include "subaru_solterra.h"

/**
 * Subaru Solterra constructor
 */
OvmsVehicleSubaruSolterra::OvmsVehicleSubaruSolterra()
  : OvmsVehicleToyotaETNGA() // Call the base class constructor
{
  ESP_LOGI(TAG, "Subaru Solterra vehicle module");

  // Initialize any private member variables and perform necessary setup for the Subaru Solterra
}

/**
 * Subaru Solterra destructor
 */
OvmsVehicleSubaruSolterra::~OvmsVehicleSubaruSolterra()
{
  ESP_LOGI(TAG, "Shutdown Subaru Solterra vehicle module");

  // Perform any necessary cleanup and shutdown procedures for the Subaru Solterra
}
