/*
    Project:       Open Vehicle Monitor System
    Module:        Vehicle Toyota bZ4X
    Date:          27th May 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "ovms_log.h"
#include "vehicle_toyota_bz4x.h"

// Constructor for the OvmsVehicleToyotaBz4x class
OvmsVehicleToyotaBz4x::OvmsVehicleToyotaBz4x()
{
  ESP_LOGI(TAG, "Toyota bZ4X vehicle module");  // Log an informational message
}

// Destructor for the OvmsVehicleToyotaBz4x class
OvmsVehicleToyotaBz4x::~OvmsVehicleToyotaBz4x()
{
  ESP_LOGI(TAG, "Shutdown Toyota bZ4X vehicle module");  // Log an informational message
}

// Initialization class for the Toyota bZ4X vehicle module
class OvmsVehicleToyotaBz4xInit
{
  public: 
    OvmsVehicleToyotaBz4xInit();
} OvmsVehicleToyotaBz4xInit  __attribute__ ((init_priority (9000)));

// Constructor for the OvmsVehicleToyotaBz4xInit class
OvmsVehicleToyotaBz4xInit::OvmsVehicleToyotaBz4xInit()
  {
  ESP_LOGI(OvmsVehicleToyotaBz4x::TAG, "Registering Vehicle: Toyota bZ4X (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleToyotaBz4x>("TOYBZ4X","Toyota bZ4X");
  }
