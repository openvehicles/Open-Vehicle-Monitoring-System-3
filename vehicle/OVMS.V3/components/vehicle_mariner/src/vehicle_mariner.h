#ifndef __VEHICLE_MARINER_H__
#define __VEHICLE_MARINER_H__

#include "ovms_vehicle.h"

void nmeaserver_task(void *pvParameters);

class OvmsVehicleMariner : public OvmsVehicle
{
  public:
    OvmsVehicleMariner();
    ~OvmsVehicleMariner();

  public:
    void IncomingFrame(CAN_frame_t* frame) override;
};

// Mocked Test Vehicle Class
class OvmsVehicleMarinerTest : public OvmsVehicle
{
  public:
    OvmsVehicleMarinerTest();
    ~OvmsVehicleMarinerTest();

  public:
    void IncomingFrame(CAN_frame_t* frame) override;
};

#endif // __VEHICLE_MARINER_H__