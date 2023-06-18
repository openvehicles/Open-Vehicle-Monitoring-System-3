/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota Reverse Engineering tool
   Date:          18th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#ifndef __VEHICLE_TOYOTA_RE_H__
#define __VEHICLE_TOYOTA_RE_H__

#include "vehicle.h"

class OvmsVehicleToyotaRE : public OvmsVehicle
  {
  private:
    uint16_t pid_;      // Variable to store the captured PID
    bool watchMessage_; // Flag to indicate when to start watching for the follow-up message

  public:
    OvmsVehicleToyotaRE();
    ~OvmsVehicleToyotaRE();

    void IncomingFrameCan2(CAN_frame_t* p_frame);

  };

#endif // __VEHICLE_TOYOTA_RE_H__
