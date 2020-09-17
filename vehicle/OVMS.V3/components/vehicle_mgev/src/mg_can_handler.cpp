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

namespace {

/// The maximum number of seconds between the asleep packets before we decide that it's
/// no longer in zombie state
constexpr uint32_t NOT_ZOMBIE_TIME = 5u;

}  // anon namespace

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
    // TODO: Should check for tester present confirmation (02 3e+40)

    uint8_t frameType = frame->data.u8[0] >> 4;
    if (frameType != ISOTP_FT_SINGLE)
    {
        // Support for single frame types only
        return;
    }
    uint8_t frameLength = frame->data.u8[0] & 0x0f;
    uint8_t* data = &frame->data.u8[1];
    uint8_t dataLength = frameLength;
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
            m_wakeState = Zombie;
        }
        return;
    }
    if (!POLL_TYPE_HAS_16BIT_PID(data[0] - 0x40))
    {
        // Only support 16-bit PIDs as in SendAlarmSensitive
        return;
    }
    uint16_t responsePid = data[1] << 8 | data[2];
    data = &data[3];
    dataLength -= 3;
    if (frame->MsgID == (bcmId | rxFlag))
    {
        IncomingBcmPoll(responsePid, data, dataLength);
    }
    else if (m_poll_state == PollStateAsleep)
    {
        if (frame->MsgID == m_sleepPollsItem->rxmoduleid &&
                m_sleepPollsItem->pid == responsePid)
        {
            if (monotonictime - m_sleepPacketTime <= NOT_ZOMBIE_TIME)
            {
                m_wakeState = Awake;
            }
            m_sleepPacketTime = monotonictime;
            IncomingPollReply(
                frame->origin, frameType, responsePid, data, dataLength, 0
            );
            ++m_sleepPollsItem;
            if (m_sleepPollsItem->txmoduleid == 0)
            {
                m_sleepPollsItem = m_sleepPolls;
            }
        }
    }
}

void OvmsVehicleMgEv::SendSleepPoll(canbus* currentBus)
{
    const auto pollItem = m_sleepPollsItem;
    if (pollItem->txmoduleid == 0)
    {
        return;
    }
    SendPollMessage(
        currentBus, pollItem->txmoduleid, pollItem->type, pollItem->pid
    );
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
