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
static const char *TAG = "v-subaru-solterra";  // Logging tag for this module

#include <stdio.h>
#include "vehicle_subaru_solterra.h"

// Constructor for the OvmsVehicleSubaruSolterra class
OvmsVehicleSubaruSolterra::OvmsVehicleSubaruSolterra()
{
  ESP_LOGI(TAG, "Subaru Solterra vehicle module");  // Log an informational message
}

// Destructor for the OvmsVehicleSubaruSolterra class
OvmsVehicleSubaruSolterra::~OvmsVehicleSubaruSolterra()
{
  ESP_LOGI(TAG, "Shutdown Subaru Solterra vehicle module");  // Log an informational message
}

// Initialization class for the Subaru Solterra vehicle module
class OvmsVehicleSubaruSolterraInit
{
  public: 
    OvmsVehicleSubaruSolterraInit();
} MyOvmsVehicleSubaruSolterraInit  __attribute__ ((init_priority (9000)));

// Constructor for the OvmsVehicleSubaruSolterraInit class
OvmsVehicleSubaruSolterraInit::OvmsVehicleSubaruSolterraInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Subaru Solterra (9000)");  // Log an informational message

  MyVehicleFactory.RegisterVehicle<OvmsVehicleSubaruSolterra>("SUBSOL","Subaru Solterra");  // Register the OvmsVehicleSubaruSolterra class with a vehicle factory
  }
