/*
    Project:       Open Vehicle Monitor System
    Module:        Vehicle Toyota bZ4X
    Date:          27th May 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#ifndef __VEHICLE_TOYOTA_BZ4X_H__
#define __VEHICLE_TOYOTA_BZ4X_H__

#include "../../vehicle_toyota_etnga/src/vehicle_toyota_etnga.h"  // Include the Toyota ETNGA vehicle module header

class OvmsVehicleToyotaBz4x : public OvmsVehicleToyotaETNGA
  {
  public:
    OvmsVehicleToyotaBz4x();  // Constructor for the Toyota Bz4x vehicle module
    ~OvmsVehicleToyotaBz4x();  // Destructor for the Toyota Bz4x vehicle module
    static constexpr const char* TAG = "v-toyota-bz4x";

  };

#endif //#ifndef __VEHICLE_TOYOTA_BZ4X_H__
