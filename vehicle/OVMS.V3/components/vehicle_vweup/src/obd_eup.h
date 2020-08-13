#ifndef __OBD_EUP_H__
#define __OBD_EUP_H__

#include "vehicle_vweup.h"
#include "poll_reply_helper.h"

// So I can easily swap between logging all Values as Info or as Verbos
#define VALUE_LOG(t, d, v) (ESP_LOGI(t, d, v))
//#define VALUE_LOG(t,d,v)    (ESP_LOGV(t, d, v))

// Car (poll) states
#define VWUP_OFF 0
#define VWUP_ON 1
#define VWUP_CHARGING 2

// ECUs TX/RX
#define VWUP_BAT_MGMT_TX 0x7E5
#define VWUP_BAT_MGMT_RX 0x7ED
#define VWUP_CHARGER_TX 0x744
#define VWUP_CHARGER_RX 0x7AE

// PIDs of ECUs
#define VWUP_BAT_MGMT_SOC 0x028C
#define VWUP_BAT_MGMT_U 0x1E3B
#define VWUP_BAT_MGMT_I 0x1E3D
#define VWUP_CHARGER_EXTDIAG 0x03
#define VWUP_CHARGER_POWER_EFF 0x15D6
#define VWUP_CHARGER_POWER_LOSS 0x15E1
#define VWUP_CHARGER_AC_U 0x41FC
#define VWUP_CHARGER_AC_I 0x41FB
#define VWUP_CHARGER_DC_U 0x41F8
#define VWUP_CHARGER_DC_I 0x41F9

#define VWUP_CHARGER_EFF_CALC_POLL_TIME 5

class VWeUpObd : public OvmsVehicle
{
public:
  VWeUpObd();
  ~VWeUpObd();

  OvmsMetricFloat *ChargerPowerEff;  // Efficiency of the Charger [%]
  OvmsMetricFloat *ChargerPowerLoss; // Power loss of Charger [W]
  OvmsMetricFloat *ChargerAC1U;      // AC Voltage Phase 1
  OvmsMetricFloat *ChargerAC2U;      // AC Voltage Phase 2
  OvmsMetricFloat *ChargerAC1I;      // AC Current Phase 1
  OvmsMetricFloat *ChargerAC2I;      // AC Current Phase 2
  OvmsMetricFloat *ChargerDC1U;      // DC Voltage 1
  OvmsMetricFloat *ChargerDC2U;      // DC Voltage 2
  OvmsMetricFloat *ChargerDC1I;      // DC Current 1
  OvmsMetricFloat *ChargerDC2I;      // DC Current 2

  void IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain);

protected:
  virtual void Ticker1(uint32_t ticker);

private:

  PollReplyHelper PollReply;

  void CheckCarState();

  bool IsOff() { return m_poll_state == VWUP_OFF; }
  bool IsOn() { return m_poll_state == VWUP_ON; }
  bool IsCharging() { return m_poll_state == VWUP_CHARGING; }
};

#endif //#ifndef __OBD_EUP_H__
