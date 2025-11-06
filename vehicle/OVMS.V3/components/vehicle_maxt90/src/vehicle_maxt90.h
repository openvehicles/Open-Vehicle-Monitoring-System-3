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
  // Correct override signature for OBDII poll replies:
  void IncomingPollReply(const OvmsPoller::poll_job_t& job,
                         uint8_t* data, uint8_t length) override;

  // Handle raw CAN frames on bus 1 (for lock state on ID 0x281):
  void IncomingFrameCan1(CAN_frame_t* p_frame) override;

private:
  // Custom metrics:
  OvmsMetricFloat* m_hvac_temp_c = nullptr;       // x.maxt90.hvac_temp_c
  OvmsMetricFloat* m_pack_capacity_kwh = nullptr; // x.maxt90.capacity_kwh (static 88.5)

  // Helpers:
  static inline uint16_t u16be(const uint8_t* p)
  {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
  }
};

#endif // __VEHICLE_MAXT90_H__
