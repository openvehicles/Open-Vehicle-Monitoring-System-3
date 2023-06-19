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
    uint8_t* data = p_frame->data.u8;

    ESP_LOGV(TAG, "CAN2 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
             p_frame->MsgID, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    // Check if the message is the awake message
    if (p_frame->MsgID == 0x000004e0)
    {
        SetAwake(true);
    }
}
