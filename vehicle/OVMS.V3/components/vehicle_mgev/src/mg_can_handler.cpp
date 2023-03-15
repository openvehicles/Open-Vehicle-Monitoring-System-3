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

static const char *TAG = "v-mgev";

#include "vehicle_mgev.h"

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
        ESP_LOGV(
            TAG, "Wake up message (length %d): %02x %02x %02x %02x %02x %02x %02x",
            frame->FIR.B.DLC, frame->data.u8[0], frame->data.u8[1], frame->data.u8[2],
            frame->data.u8[3], frame->data.u8[4], frame->data.u8[5], frame->data.u8[6]
        );
        // If we were off, or woken by this message then wake up
        // if (m_gwmState == AllowToSleep || (m_gwmState == Awake && monotonictime == m_afterRunTicker))
        // {
        //     // If we get a 70a and the CAN is off, then we'll need to wake the CAN
        //     ESP_LOGI(TAG, "Wake state from %d to %d", Off, Diagnostic);
        //     AttemptDiagnostic();
        // }
    }

    uint8_t frameType = frame->data.u8[0] >> 4;
    uint16_t frameLength = 0;
    uint8_t* data = &frame->data.u8[1];

    switch (frameType)
    {
        case ISOTP_FT_SINGLE:
        {
            frameLength = frame->data.u8[0] & 0x0f;
            break;
        }
        case ISOTP_FT_FIRST:
        {
            frameLength = (frame->data.u8[0] & 0x0f) << 8 | frame->data.u8[1];
            data = &frame->data.u8[2];
            break;         
        }
    }
    uint8_t serviceId = data[0] == 0x7f ? 0x7f : data[0] & 0xbfu; //If negative response keep as 7f, otherwise mask out reply flag
    uint16_t responsePid = data[1] << 8 | data[2];    

    // if (frame->MsgID - rxFlag >= 0x700u && frame->MsgID <= 0x7ffu)
    // {
    //     ESP_LOGI(TAG, "Incoming CAN frame from ECU %03x: %02x %02x %02x %02x %02x %02x %02x %02x",
    //         frame->MsgID, 
    //         frame->data.u8[0], frame->data.u8[1], frame->data.u8[2], frame->data.u8[3], 
    //         frame->data.u8[4], frame->data.u8[5], frame->data.u8[6], frame->data.u8[7]
    //     );  
    // }  
    switch (frame->MsgID - rxFlag)
    {
        case gwmId:
        {
            // ESP_LOGI(TAG, "Got a GWM response: %02x %02x %02x %02x %02x %02x %02x %02x",
            //     frame->data.u8[0], frame->data.u8[1], frame->data.u8[2], frame->data.u8[3], 
            //     frame->data.u8[4], frame->data.u8[5], frame->data.u8[6], frame->data.u8[7]
            // );            
            switch (static_cast<GWMTasks>(m_gwm_task->AsInt()))
            {
                case GWMTasks::SoftwareVersion:
                {
                    IncomingSoftwareVersionFrame(frame, frameType, frameLength, responsePid, data);
                    break;
                }
                case GWMTasks::Authentication:
                {
                    IncomingGWMAuthFrame(frame, serviceId, data);
                    break;                      
                }
                default: 
                {
                    IncomingGWMFrame(frame, frameType, frameLength, serviceId, responsePid, data);
                    break;
                }
            }            
            break;
        }
        case bcmId:
        {
            // ESP_LOGI(TAG, "Got a BCM response: %02x %02x %02x %02x %02x %02x %02x %02x",
            //     frame->data.u8[0], frame->data.u8[1], frame->data.u8[2], frame->data.u8[3], 
            //     frame->data.u8[4], frame->data.u8[5], frame->data.u8[6], frame->data.u8[7]
            // );              
            switch (static_cast<BCMTasks>(m_bcm_task->AsInt()))
            {
                case BCMTasks::SoftwareVersion:
                {
                    IncomingSoftwareVersionFrame(frame, frameType, frameLength, responsePid, data);
                    break;
                }
                case BCMTasks::Authentication:
                {
                    IncomingBCMAuthFrame(frame, serviceId, data);
                    break;                      
                }
                case BCMTasks::DRL:
                {
                    IncomingBCMDRLFrame(frame, frameType, serviceId, responsePid, data);
                    break;
                }
                default: 
                {
                    IncomingBCMFrame(frame, frameType, frameLength, serviceId, responsePid, data);
                    break;
                }
            }     
            break;       
        }
        default:
        {
            if (frame->MsgID >= 0x700u && frame->MsgID <= 0x7ffu && m_GettingSoftwareVersions)
            {
                IncomingSoftwareVersionFrame(frame, frameType, frameLength, responsePid, data);
            }              
        }
    }
}

void OvmsVehicleMgEv::IncomingPollReply(
        canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length,
        uint16_t remain)
{
    ESP_LOGV(
        TAG,
        "%03" PRIx32 " TYPE:%" PRIx16 " PID:%02" PRIx16 " Length:%" PRIx8 " Data:%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
        m_poll_moduleid_low,
        type,
        pid,
        length,
        data[0], data[1], data[2], data[3]
    );

    switch (m_poll_moduleid_low)
    {
        case (bmsId | rxFlag):
            IncomingBmsPoll(pid, data, length, remain);
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
        case (evccId | rxFlag):
            IncomingEvccPoll(pid, data, length);
            break;
        case (bcmId | rxFlag):
            IncomingBcmPoll(pid, data, length);
            break;            
    }
}