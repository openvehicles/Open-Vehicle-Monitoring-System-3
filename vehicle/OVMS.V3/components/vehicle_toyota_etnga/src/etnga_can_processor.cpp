/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

   Code adapted from Michael Stegen, Mark Webb-Johnson, Sonny Chen, Stephen Davies vehicle modules

*/

#include "ovms_log.h"
#include "vehicle_toyota_etnga.h"

void OvmsVehicleToyotaETNGA::IncomingFrameCan2(CAN_frame_t* p_frame)
{
    const uint8_t* data = p_frame->data.u8;

    ESP_LOGV(TAG, "CAN2 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
             p_frame->MsgID, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    if (!m_allow_wake) {
        // Verbose: this fires for EVERY frame received during a cooldown window — at
        // parked-bus broadcast rates an INFO message here flooded the log.
        ESP_LOGV(TAG, "OBD message ignored during cooldown");
    } else {
        // Bus-liveness only; v.e.awake is now driven by poll state in SetPollState().
        m_last_can2_time = (uint32_t) StandardMetrics.ms_m_monotonic->AsInt();
    }
}
