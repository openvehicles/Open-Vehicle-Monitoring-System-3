/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Subaru Solterra
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#include "ovms_log.h"
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
  ESP_LOGI(OvmsVehicleSubaruSolterra::TAG, "Registering Vehicle: Subaru Solterra (9000)");  // Log an informational message

  MyVehicleFactory.RegisterVehicle<OvmsVehicleSubaruSolterra>("SUBSOL","Subaru Solterra");  // Register the OvmsVehicleSubaruSolterra class with a vehicle factory
  }
