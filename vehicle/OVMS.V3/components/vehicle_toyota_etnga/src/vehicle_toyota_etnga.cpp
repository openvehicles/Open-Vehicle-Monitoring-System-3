/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#include "ovms_log.h"
#include "vehicle_toyota_etnga.h"

// Poll state descriptions:
//    POLLSTATE_SLEEP (0)     : Vehicle is sleeping; no activity on the can bus. We are listening only.
//    POLLSTATE_ACTIVE (1)    : Vehicle is alive; activity on CAN bus
//    POLLSTATE_READY (2)     : Vehicle is "Ready" to drive or being driven
//    POLLSTATE_CHARGING (3)  : Vehicle is charging

static const OvmsVehicle::poll_pid_t obdii_polls[] = {
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_VOLTAGE_AND_CURRENT, { 0, 0, 2, 2}, 0, ISOTP_STD },
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_READY_SIGNAL, { 0, 15, 2, 0}, 0, ISOTP_STD },
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGING_LIDS_SWITCH, { 0, 15, 0, 2}, 0, ISOTP_STD },  
    POLL_LIST_END
  };

OvmsVehicleToyotaETNGA::OvmsVehicleToyotaETNGA()
  {
  ESP_LOGI(TAG, "Toyota eTNGA platform module");

  // Init CAN
  RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // Set polling PID list
  PollSetPidList(m_can2,obdii_polls);

  // Set polling state
  PollSetState(POLLSTATE_SLEEP);
  SetReadyStatus(false);

  }

OvmsVehicleToyotaETNGA::~OvmsVehicleToyotaETNGA()
  {
  ESP_LOGI(TAG, "Shutdown Toyota eTNGA platform module");
  }

void OvmsVehicleToyotaETNGA::Ticker1(uint32_t ticker)
{
  ++tickerCount;
  ESP_LOGV(TAG, "Tick! tickerCount=%d frameCount=%d replyCount=%d pollerstate=%d", tickerCount, frameCount, replyCount, m_poll_state);
//  ESP_LOGV(TAG, "Environment On: %d, Charge Port Door: %d", StdMetrics.ms_v_env_on->AsBool(), StdMetrics.ms_v_door_chargeport->AsBool());

  switch (m_poll_state) {
    case POLLSTATE_SLEEP:
      handleSleepState();
      break;

    case POLLSTATE_ACTIVE:
      handleActiveState();
      break;

    case POLLSTATE_READY:
      handleReadyState();
      break;

    case POLLSTATE_CHARGING:
      handleChargingState();
      break;

    default:
      ESP_LOGE(TAG, "Invalid poll state: %d", m_poll_state);
      break;
  }

  if (frameCount > 0) {
      // CAN communication. Reset counters.
      frameCount = 0;
      tickerCount = 0;
  }

}

void OvmsVehicleToyotaETNGA::Ticker60(uint32_t ticker)
  {
    // Request VIN if not already set
    if (StandardMetrics.ms_v_vin->AsString().empty()) {
      RequestVIN();
    }

  }

void OvmsVehicleToyotaETNGA::Ticker3600(uint32_t ticker)
  {
  }
