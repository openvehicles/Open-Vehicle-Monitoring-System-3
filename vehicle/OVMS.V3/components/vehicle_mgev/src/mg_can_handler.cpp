/*
;    Project:       Open Vehicle Monitor System
;    Date:          11th September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris Staite
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
static const char *TAG = "v-mgev";

#include "vehicle_mgev.h"
#include "mg_obd_pids.h"

void OvmsVehicleMgEv::IncomingFrameCan1(CAN_frame_t* p_frame)
{
    if (m_poll_bus_default != m_can1)
    {
        return;
    }
    IncomingPollFrame(p_frame);
}

void OvmsVehicleMgEv::IncomingFrameCan2(CAN_frame_t* p_frame)
{
    if (m_poll_bus_default != m_can2)
    {
        return;
    }
    IncomingPollFrame(p_frame);
}

void OvmsVehicleMgEv::IncomingFrameCan3(CAN_frame_t* p_frame)
{
    if (m_poll_bus_default != m_can3)
    {
        return;
    }
    IncomingPollFrame(p_frame);
}

void OvmsVehicleMgEv::IncomingFrameCan4(CAN_frame_t* p_frame)
{
    if (m_poll_bus_default != m_can4)
    {
        return;
    }
    IncomingPollFrame(p_frame);
}

void OvmsVehicleMgEv::IncomingPollFrame(CAN_frame_t* frame)
{
    if (frame->MsgID == 0x70au)
    {
        ESP_LOGI(
            TAG, "Wake up message (length %d): %02x %02x %02x %02x %02x %02x %02x",
            frame->FIR.B.DLC, frame->data.u8[0], frame->data.u8[1], frame->data.u8[2],
            frame->data.u8[3], frame->data.u8[4], frame->data.u8[5], frame->data.u8[6]
        );
    }

    uint8_t frameType = frame->data.u8[0] >> 4;
    uint8_t frameLength = frame->data.u8[0] & 0x0f;
    uint8_t* data = &frame->data.u8[1];
    uint8_t dataLength = frameLength;

    if (frameType == ISOTP_FT_SINGLE)
    {
        if ((data[0] & 0x40) == 0)
        {
            // Unsuccessful response, ignore it
            return;
        }
        if ((data[0] - 0x40) == VEHICLE_POLL_TYPE_TESTERPRESENT)
        {
            // Tester present response, can do something else now
            if (frame->MsgID == (bcmId | rxFlag))
            {
                // BCM has replied to tester present, send queries to it
                ESP_LOGV(TAG, "Keep-alive sent, sending alarm sensitive messages");
                SendAlarmSensitive(frame->origin);
            }
            else if (m_wakeState == Waking && frame->MsgID == (gwmId | rxFlag))
            {
                ESP_LOGV(TAG, "Got response from gateway, wake up complete");
                m_wakeState = Tester;
                // If we are currently unlocked, then send to BCM
                if (StandardMetrics.ms_v_env_locked->AsBool() == false &&
                    monotonictime - StandardMetrics.ms_v_env_locked->LastModified() <= 2)
                {
                    SendKeepAliveTo(frame->origin, bcmId);
                }
            }
            return;
        }
    }
    if (frameType == ISOTP_FT_SINGLE || frameType == ISOTP_FT_FIRST)
    {
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
        if (frameType == ISOTP_FT_SINGLE && frame->MsgID == (bcmId | rxFlag))
        {
            IncomingBcmPoll(responsePid, data, dataLength);
        }

        if (responsePid == softwarePid)
        {
            std::vector<char> version(dataLength + 1, '0');
            version.back() = '\0';
            std::copy(
                data,
                &data[frameType == ISOTP_FT_SINGLE ? dataLength : 3],
                version.begin()
            );
            if (frameType == ISOTP_FT_FIRST)
            {
                CAN_frame_t flowControl = {
                    frame->origin,
                    nullptr,
                    { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
                    frame->MsgID - rxFlag,
                    { .u8 = { 0x30, 0, 25, 0, 0, 0, 0, 0 } }
                };
                frame->origin->Write(&flowControl);
            }
            m_versions.push_back(std::make_pair(
                frame->MsgID - rxFlag, std::move(version)
            ));
        }
    }
    // Handle multi-line version responses
    else if (m_poll_plist == nullptr && frameType == ISOTP_FT_CONSECUTIVE)
    {
        auto start = ((dataLength - 1) * 7) + 3;
        for (auto& version : m_versions)
        {
            if (version.first == frame->MsgID - rxFlag)
            {
                auto end = version.second.size() - 1;
                std::copy(
                    data,
                    &data[end - start > 7 ? 7 : (end - start)],
                    version.second.begin() + start
                );
            }
        }
    }
}

bool OvmsVehicleMgEv::SendPollMessage(
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

void OvmsVehicleMgEv::IncomingPollReply(
        canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length,
        uint16_t remain)
{
    ESP_LOGV(
        TAG,
        "%03x TYPE:%x PID:%02x Length:%x Data:%02x %02x %02x %02x",
        m_poll_moduleid_low,
        type,
        pid,
        length,
        data[0], data[1], data[2], data[3]
    );

    switch (m_poll_moduleid_low)
    {
        case (bmsId | rxFlag):
            IncomingBmsPoll(pid, data, length);
            break;
        case (dcdcId | rxFlag):
            IncomingDcdcPoll(pid, data, length);
            break;
        case (vcuId | rxFlag):
            IncomingVcuPoll(pid, data, length, remain);
            break;
        case (atcId | rxFlag):
            IncomingAtcPoll(pid, data, length);
            break;
        case (tpmsId | rxFlag):
            IncomingTpmsPoll(pid, data, length);
            break;
        case (pepsId | rxFlag):
            IncomingPepsPoll(pid, data, length);
            break;
        // BCM poll type handled in IncomingPollFrame
    }
}
