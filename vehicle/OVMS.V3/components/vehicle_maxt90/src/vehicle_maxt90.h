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

  // Handle raw incoming CAN frames (for lock/unlock & other BCM signals)
  void IncomingFrameCan1(CAN_frame_t* p_frame) override;

private:
  // ─────────────────────────────────────────────
  // Custom metrics
  // ─────────────────────────────────────────────
  //  - xmt.v.hvac.temp   : HVAC / coolant temperature (°C)
  //  - xmt.b.capacity    : Nominal pack capacity (kWh)
  //  - xmt.v.locked      : Door lock state (true = locked)
  OvmsMetricFloat* m_hvac_temp_c       = nullptr; // xmt.v.hvac.temp
  OvmsMetricFloat* m_pack_capacity_kwh = nullptr; // xmt.b.capacity
  OvmsMetricBool*  m_lock_state        = nullptr; // xmt.v.locked

  // ─────────────────────────────────────────────
  // Helpers
  // ─────────────────────────────────────────────
  static inline uint16_t u16be(const uint8_t* p)
  {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
  }
};

#endif // __VEHICLE_MAXT90_H__
