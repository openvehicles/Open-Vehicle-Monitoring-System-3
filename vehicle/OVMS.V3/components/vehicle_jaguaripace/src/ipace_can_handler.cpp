/*
;    Project:       Open Vehicle Monitor System
;    Date:          1st April 2021
;
;    Changes:
;    0.0.1  Initial release
;
;    (C) 2021       Didier Ernotte
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
static const char *TAG = "v-jaguaripace";

#include "vehicle_jaguaripace.h"
#include "ipace_obd_pids.h"

void OvmsVehicleJaguarIpace::IncomingFrameCan1(CAN_frame_t* p_frame)
{
    ESP_LOGD(TAG, "IncomingFrameCan1, ");

    if (m_poll_bus_default != m_can1)
    {
        return;
    }
    IncomingPollFrame(p_frame);
}


void OvmsVehicleJaguarIpace::IncomingPollFrame(CAN_frame_t* frame)
{
    uint8_t frameType = frame->data.u8[0] >> 4;
    uint8_t frameLength = frame->data.u8[0] & 0x0f;
    uint8_t* data = &frame->data.u8[1];
    uint8_t dataLength = frameLength;

    ESP_LOGD(TAG, "IncomingPollFrame, frameType=%d, dataLength=%d", frameType, dataLength);

    if (frameType == ISOTP_FT_SINGLE)
    {
        if ((data[0] & 0x40) == 0)
        {
            // Unsuccessful response, ignore it
            ESP_LOGD(TAG, "IncomingPollFrame, Unsuccessful response data0=%d", data[0]);
            return;
        }
        if ((data[0] - 0x40) == VEHICLE_POLL_TYPE_TESTERPRESENT)
        {
            // Tester present response, can do something else now
            if (frame->MsgID == (bcmId | rxFlag))
            {
                // BCM has replied to tester present, send queries to it
                ESP_LOGD(TAG, "BCM has responed to Tester Present, safe to query Lock/Door State");
                // SendAlarmSensitive(frame->origin);
            }
            // else if (frame->MsgID == (gwmId | rxFlag))
            // {
                // GWM has responsed to Tester present

                //ESP_LOGV(TAG, "Got response from gateway, wake up complete");


                // m_gwmState = Tester;
                // // If we are currently unlocked, then send to BCM
                // if (StandardMetrics.ms_v_env_locked->AsBool() == false &&
                //     monotonictime - StandardMetrics.ms_v_env_locked->LastModified() <= 2)
                // {
                //     // FIXME: Disabled until we are happy that the alarm won't go off
                //     //SendTesterPresentTo(frame->origin, bcmId);
                // }
            // }
            return;
        }
    }
    if (frameType == ISOTP_FT_SINGLE || frameType == ISOTP_FT_FIRST)
    {
        ESP_LOGD(TAG, "IncomingPollFrame, SINGLE or First frameType=%d", frameType);
        if (frameType == ISOTP_FT_FIRST)
        {
            dataLength = (dataLength << 8) | data[0];
            ++data;
        }

        if (!POLL_TYPE_HAS_16BIT_PID(data[0] - 0x40))
        {
            // Only support 16-bit PIDs as in SendAlarmSensitive
            return;
        }

        uint16_t responsePid = data[1] << 8 | data[2];
        data = &data[3];
        dataLength -= 3;
        ESP_LOGD(TAG, "IncomingPollFrame, responsePid=%d, dataLength=%d", responsePid, dataLength);

        if (frameType == ISOTP_FT_SINGLE)
        {
            ESP_LOGD(TAG, "IncomingPollFrame, SINGLE frame->MsgID=%" PRId32, frame->MsgID);
            switch (frame->MsgID) {
                case (becmId | rxFlag):
                    IncomingBecmPoll(responsePid, data, dataLength, 0);
                    break;
            } 
        }

    }
    // Handle multi-line version responses
    else if (m_poll_plist == nullptr && frameType == ISOTP_FT_CONSECUTIVE)
    {
        // auto start = ((dataLength - 1) * 7) + 3;
        // for (auto& version : m_versions)
        // {
        //     if (version.first == frame->MsgID - rxFlag)
        //     {
        //         auto end = version.second.size() - 1;
        //         std::copy(
        //             data,
        //             &data[end - start > 7 ? 7 : (end - start)],
        //             version.second.begin() + start
        //         );
        //     }
        // }
    }
}

bool OvmsVehicleJaguarIpace::SendPollMessage(
        canbus* bus, uint16_t id, uint8_t type, uint16_t pid)
{
    CAN_frame_t sendFrame = {
        bus,
        nullptr,  // send callback (ignore)
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        id,
        // Only support for 16-bit PIDs because that's all we use
        { .u8 = {
            (ISOTP_FT_SINGLE << 4) + 3, static_cast<uint8_t>(type),
            static_cast<uint8_t>(pid >> 8),
            static_cast<uint8_t>(pid & 0xff), 0, 0, 0, 0
        } }
    };
    return bus->Write(&sendFrame) != ESP_FAIL;
}

void OvmsVehicleJaguarIpace::IncomingPollReply(canbus* bus, const OvmsPoller::poll_state_t& state, uint8_t* data, uint8_t length, const OvmsPoller::poll_pid_t &pollentry)
{
    ESP_LOGD(
        TAG,
        "%03" PRIx32 " TYPE:%" PRIx16 " PID:%02" PRIx16 " Length:%" PRIx8 " Data:%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
        m_poll_moduleid_low,
        state.type,
        state.pid,
        length,
        data[0], data[1], data[2], data[3]
    );

    switch (m_poll_moduleid_low)
    {
        case (becmId | rxFlag):
            IncomingBecmPoll(state.pid, data, length, state.mlremain);
            break;
        case (hvacId | rxFlag):
            IncomingHvacPoll(state.pid, data, length, state.mlremain);
            break;
        case (bcmId | rxFlag):
            IncomingBcmPoll(state.pid, data, length, state.mlremain);
            break;
        case (tpmsId | rxFlag):
            IncomingTpmsPoll(state.pid, data, length, state.mlremain);
            break;
        case (tcuId | rxFlag):
            IncomingTcuPoll(state.pid, data, length, state.mlremain);
            break;
    }
}
