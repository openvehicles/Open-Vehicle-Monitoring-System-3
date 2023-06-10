/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Subaru Solterra
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#ifndef __VEHICLE_SUBARU_SOLTERRA_H__
#define __VEHICLE_SUBARU_SOLTERRA_H__

#include "../../vehicle_toyota_etnga/src/vehicle_toyota_etnga.h"  // Include the Toyota ETNGA vehicle module header

class OvmsVehicleSubaruSolterra : public OvmsVehicleToyotaETNGA
{
public:
  OvmsVehicleSubaruSolterra();  // Constructor for the Subaru Solterra vehicle module
  ~OvmsVehicleSubaruSolterra(); // Destructor for the Subaru Solterra vehicle module
  static constexpr const char* TAG = "v-subaru-solterra";

};

#endif // __VEHICLE_SUBARU_SOLTERRA_H__
