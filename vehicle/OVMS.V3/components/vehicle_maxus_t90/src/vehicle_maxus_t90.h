#pragma once
#include "vehicle.h"

class OvmsVehicleMaxusT90 : public OvmsVehicle
{
public:
  OvmsVehicleMaxusT90();
  ~OvmsVehicleMaxusT90();
  const char* VehicleShortName();
  void IncomingPollReply(CAN_frame_t* frame, uint16_t pid, uint8_t* data,
                         uint8_t length, uint16_t mlremain) override;
};
