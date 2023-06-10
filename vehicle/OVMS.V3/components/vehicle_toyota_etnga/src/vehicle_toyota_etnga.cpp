/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#include "ovms_log.h"
static const char *TAG = "v-toyota-etnga";

#include "vehicle_toyota_etnga.h"

// Poll state descriptions:
//    POLLSTATE_SLEEP (0)     : Vehicle is sleeping; no activity on the can bus. We are listening only.
//    POLLSTATE_ACTIVE (1)    : Vehicle is alive; activity on CAN bus
//    POLLSTATE_READY (2)     : Vehicle is "Ready" to drive or being driven
//    POLLSTATE_CHARGING (3)  : Vehicle is charging

static const OvmsVehicle::poll_pid_t obdii_polls[] = {
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_VOLTAGE_AND_CURRENT, { 0, 0, 0, 0}, 0, ISOTP_STD },
    POLL_LIST_END
  };

OvmsVehicleToyotaETNGA::OvmsVehicleToyotaETNGA()
  {
  ESP_LOGI(TAG, "Toyota eTNGA platform module");

  // Init CAN:
   RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // Set polling PID list
  PollSetPidList(m_can2,obdii_polls);

  // Set polling state
  PollSetState(POLLSTATE_SLEEP);

  }

void OvmsVehicleToyotaETNGA::Ticker1(uint32_t ticker)
  {

  }

void OvmsVehicleToyotaETNGA::Ticker10(uint32_t ticker)
  {

    if (StandardMetrics.ms_v_vin->AsString().empty()) {
      RequestVIN();
    }

  }

void OvmsVehicleToyotaETNGA::Ticker3600(uint32_t ticker)
  {
  }

OvmsVehicleToyotaETNGA::~OvmsVehicleToyotaETNGA()
  {
  ESP_LOGI(TAG, "Shutdown Toyota eTNGA platform module");
  }

void OvmsVehicleToyotaETNGA::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
    uint8_t *d = p_frame->data.u8;

    // Process the incoming message
    switch (p_frame->MsgID) {
    
    case 0x45a: {
      // I'm not sure what this message is...
      break;
    }

    case 0x4e0: {
      // I'm not sure what this message is...
      break;
    }

    default:
      // Unknown frame. Log for evaluation
      ESP_LOGV(TAG,"CAN2 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
        p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
        break;
    }
  }
