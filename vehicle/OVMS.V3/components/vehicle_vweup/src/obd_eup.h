#ifndef __OBD_EUP_H__
#define __OBD_EUP_H__

#include "vehicle_vweup.h"

// ECUs TX/RX
#define VWUP_BAT_MGMT_TX 0x7E5
#define VWUP_BAT_MGMT_RX 0x7ED

// PIDs of ECUs
#define VWUP_BAT_MGMT_SOC 0x028C

class VWeUpObd : public OvmsVehicle
{
public:
  VWeUpObd();
  ~VWeUpObd();
  
  void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);

private:

};

#endif //#ifndef __OBD_EUP_H__
