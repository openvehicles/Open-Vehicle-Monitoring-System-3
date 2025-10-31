#ifndef __VEHICLE_MAXT90_H__
#define __VEHICLE_MAXT90_H__

#include "vehicle_obdii.h"
#include "ovms_metrics.h"

class OvmsVehicleMaxt90 : public OvmsVehicleOBDII
{
public:
  OvmsVehicleMaxt90();
  ~OvmsVehicleMaxt90();

protected:
  // Handle decoded poll replies:
  void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid,
                         uint8_t* data, uint8_t length, uint16_t mlremain) override;

private:
  // Custom metrics:
  OvmsMetricFloat* m_hvac_temp_c = nullptr;  // x.maxt90.hvac_temp_c

  // Helpers:
  static inline uint16_t u16be(const uint8_t* p)
  {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
  }
};

#endif // __VEHICLE_MAXT90_H__
