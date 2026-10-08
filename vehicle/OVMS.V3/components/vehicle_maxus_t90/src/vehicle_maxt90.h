#ifndef __VEHICLE_MAXT90_H__
#define __VEHICLE_MAXT90_H__

#include <string>
#include "vehicle_obdii.h"
#include "ovms_metrics.h"

class OvmsVehicleMaxt90 : public OvmsVehicleOBDII
{
public:
  OvmsVehicleMaxt90();
  ~OvmsVehicleMaxt90();

protected:
  // OBDII poll replies:
  void IncomingPollReply(const OvmsPoller::poll_job_t& job,
                         uint8_t* data, uint8_t length) override;

  // Raw CAN1 frames (lock 0x281, doors/lights 0x375, HVAC 0x510, odometer 0x540):
  void IncomingFrameCan1(CAN_frame_t* p_frame) override;

  // Expires a charge state the car can no longer confirm (it went to sleep):
  void Ticker10(uint32_t ticker) override;

private:
  // Custom metrics:
  //  - xmt.v.hvac.temp  : HVAC / coolant temperature (°C)
  //  - xmt.b.capacity   : Nominal pack capacity (kWh)
  OvmsMetricFloat* m_hvac_temp_c       = nullptr; // xmt.v.hvac.temp
  OvmsMetricFloat* m_pack_capacity_kwh = nullptr; // xmt.b.capacity

  // Vehicle state, as last reported by the car:
  bool     m_ready        = false;   // E015 non-zero: ignition on
  bool     m_charging     = false;   // E018 active while E015 (ignition) is zero
  bool     m_was_charging = false;   // charged since the car was last driven
  int      m_e015         = -1;      // VCU ignition state, -1 = not read yet
  int      m_e018         = -1;      // VCU HV-active state, -1 = not read yet
  uint32_t m_charge_seen  = 0;       // seconds since boot of the last E015/E018 reply
  uint32_t m_pow_time     = 0;       // seconds since boot of the last pack-power sample
  bool     m_pow_have     = false;   // m_pow_time holds a sample from this charge

  // BMS SOC (0.1 % resolution) takes precedence over the VCU's integer SOC:
  uint32_t m_bms_soc_time = 0;
  bool     m_bms_soc_seen = false;

  // Reassembly of multi-frame replies. The poller hands the callback one
  // chunk per CAN frame, so anything above 4 data bytes arrives in pieces.
  std::string m_rxbuf;
  uint16_t    m_rxpid  = 0;
  uint32_t    m_rxtime = 0;
  bool Collect(uint16_t pid, const uint8_t* data, uint8_t length, size_t want);

  void UpdateChargeState();
  void UpdatePollState();

  // Helpers:
  static inline uint16_t u16be(const uint8_t* p)
  {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
  }
};

#endif // __VEHICLE_MAXT90_H__
