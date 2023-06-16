/*
    Project:       Open Vehicle Monitor System
    Module:        Vehicle Toyota bZ4X
    Date:          27th May 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
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
ESP_LOGI(TAG, "Toyota bZ4X vehicle module");  // Log an informational message
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
