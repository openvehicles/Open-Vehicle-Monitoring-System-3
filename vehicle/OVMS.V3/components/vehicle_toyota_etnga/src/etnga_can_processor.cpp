/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.

   Code adapted from Michael Stegen, Mark Webb-Johnson, Sonny Chen, Stephen Davies vehicle modules

*/

#include "ovms_log.h"
#include "vehicle_toyota_etnga.h"

void OvmsVehicleToyotaETNGA::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
    // Count the arriving frames.  The ticker uses this to see if anything is arriving ie in the OBD actually alive
    ++frameCount;

    uint8_t *d = p_frame->data.u8;

    ESP_LOGV(TAG,"Unknown CAN2 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
      p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );

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

    case HYBRID_BATTERY_SYSTEM_RX: {
      break;
    }

    case HYBRID_CONTROL_SYSTEM_RX: {
      break;
    }

    case PLUG_IN_CONTROL_SYSTEM_RX: {
      break;
    }

    default:
      // Unknown frame. Log for evaluation
      ESP_LOGD(TAG,"Unknown CAN2 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
        p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
        break;
    }
  }
