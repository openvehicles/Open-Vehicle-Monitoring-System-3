#ifndef __OBD_EUP_H__
#define __OBD_EUP_H__

#include "vehicle_vweup.h"

// Car (poll) states
#define VWUP_OFF_TICKER_THRESHOLD 10
#define VWUP_OFF 0
#define VWUP_ON 1
#define VWUP_CHARGING 2

// ECUs TX/RX
#define VWUP_BAT_MGMT_TX 0x7E5
#define VWUP_BAT_MGMT_RX 0x7ED

// PIDs of ECUs
#define VWUP_BAT_MGMT_SOC 0x028C
#define VWUP_BAT_MGMT_U 0x1E3B
#define VWUP_BAT_MGMT_I 0x1E3D

class VWeUpObd : public OvmsVehicle
{
public:
  VWeUpObd();
  ~VWeUpObd();

  void IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain);

protected:
  virtual void Ticker1(uint32_t ticker);
  virtual void Ticker10(uint32_t ticker);

private:
  uint8_t CarOffTicker = 0;

  bool IsOff() { return m_poll_state == VWUP_OFF; }
  bool IsOn() { return m_poll_state == VWUP_ON; }
  bool IsCharging() { return m_poll_state == VWUP_CHARGING; }
};

#endif //#ifndef __OBD_EUP_H__
