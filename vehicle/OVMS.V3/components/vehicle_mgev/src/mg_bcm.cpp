/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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
#include "mg_auth.h"

bool OvmsVehicleMgEv::AuthenticateBCM(canbus* currentBus)
{
    if (currentBus == nullptr)
    {
        ESP_LOGE(TAG, "No CAN configured for the car");
        return false;
    }    

    //Wait for BCM to be free
    uint8_t a = 0;
    while (a < MANUAL_COMMAND_TIMEOUT && m_bcm_task->AsInt() != static_cast<int>(BCMTasks::None))
    {
        ESP_LOGI(TAG, "BCM is still busy, waiting %dms (%d)", MANUAL_COMMAND_WAIT_TICK_PERIOD, a+1);
        vTaskDelay(MANUAL_COMMAND_WAIT_TICK_PERIOD / portTICK_PERIOD_MS);
        a++;
    }
    if (m_bcm_task->AsInt() == static_cast<int>(BCMTasks::None))
    {        
        ESP_LOGI(TAG, "Starting BCM authentication");
        ESP_LOGI(TAG, "BCM auth: sending 1003");

        CAN_frame_t authStart = {
            currentBus,
            nullptr,
            { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
            bcmId,
            { .u8 = {
                (ISOTP_FT_SINGLE << 4) + 2, VEHICLE_POLL_TYPE_OBDIISESSION, 3, 0, 0, 0, 0, 0
            } }
        };

        m_bcm_task->SetValue(static_cast<int>(BCMTasks::Authentication));   

        if (currentBus->Write(&authStart) == ESP_FAIL) {
            ESP_LOGE(TAG, "Error writing BCM authentication start frame");
            m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));  
            return false;
        }   

        //Wait for task to complete
        a = 0;
        while (a < MANUAL_COMMAND_TIMEOUT && m_bcm_task->AsInt() == static_cast<int>(BCMTasks::Authentication))
        {
            ESP_LOGI(TAG, "Waiting %dms for BCM auth to complete (%d)", MANUAL_COMMAND_WAIT_TICK_PERIOD, a+1);
            vTaskDelay(MANUAL_COMMAND_WAIT_TICK_PERIOD / portTICK_PERIOD_MS);
            a++;
        }        
        switch (static_cast<BCMTasks>(m_bcm_task->AsInt()))       
        {
            case BCMTasks::Authentication:
            {
                ESP_LOGI(TAG, "BCM auth timeout");
                m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));                       
                break;
            }
            case BCMTasks::AuthenticationDone:
            {
                ESP_LOGI(TAG, "BCM auth done");
                m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));  
                return true;                     
                // break;
            }
            case BCMTasks::AuthenticationError:
            {
                ESP_LOGI(TAG, "BCM auth error");
                m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));
                break;
            }
            default: {}
        }
    }
    else
    {
        ESP_LOGI(TAG, "BCM is busy, please try again later");
    }
    return false;
}

void OvmsVehicleMgEv::IncomingBCMAuthFrame(CAN_frame_t* frame, uint8_t serviceId, uint8_t* data)
{
    if (serviceId == UDS_RESP_TYPE_NRC)
    {
        ESP_LOGI(
            TAG, "Got a BCM negative response (length %d): %02x %02x %02x %02x %02x %02x %02x %02x",
            frame->FIR.B.DLC, 
            frame->data.u8[0], frame->data.u8[1], frame->data.u8[2], frame->data.u8[3], 
            frame->data.u8[4], frame->data.u8[5], frame->data.u8[6], frame->data.u8[7]
        );
        if (m_bcm_task->AsInt() == static_cast<int>(BCMTasks::Authentication))
        {
            ESP_LOGI(TAG, "Abandoning BCM authentication");
            m_bcm_task->SetValue(static_cast<int>(BCMTasks::AuthenticationError));
        }
        return;
    }  

    CAN_frame_t nextFrame = {
        frame->origin,
        nullptr,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        bcmId,
        { .u8 = {
            0, 0, 0, 0, 0, 0, 0, 0
        } }
    };

    switch (serviceId)
    {
        case VEHICLE_POLL_TYPE_OBDIISESSION:
        {
            // Request seed
            ESP_LOGI(TAG, "BCM auth: requesting seed");
            nextFrame.data.u8[0] = (ISOTP_FT_SINGLE << 4) | 2;
            nextFrame.data.u8[1] = VEHICLE_POLL_TYPE_SECACCESS;
            nextFrame.data.u8[2] = 0x01u;
            break;  
        }
        case VEHICLE_POLL_TYPE_SECACCESS:
        {
            switch (data[1])
            {
                case 0x01:
                {
                    // Seed response
                    uint32_t seed = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
                    if (seed == 0)
                    {
                        ESP_LOGI(TAG, "BCM auth: seed received %08x. BCM seems to already have been authenticated", seed);
                        m_bcm_task->SetValue(static_cast<int>(BCMTasks::AuthenticationDone));
                    }
                    else
                    {
                        uint32_t key = MgEvPasscode::BCMKey(seed);
                        ESP_LOGI(TAG, "BCM auth: seed received %08x. Replying with key %08x", seed, key);
                        nextFrame.data.u8[0] = (ISOTP_FT_SINGLE << 4) | 6;
                        nextFrame.data.u8[1] = VEHICLE_POLL_TYPE_SECACCESS;
                        nextFrame.data.u8[2] = 0x02u;
                        nextFrame.data.u8[3] = key >> 24u;
                        nextFrame.data.u8[4] = key >> 16u;
                        nextFrame.data.u8[5] = key >> 8u;
                        nextFrame.data.u8[6] = key; 
                    }
                    break;                  
                }
                case 0x02:
                {
                    // Seed accept
                    ESP_LOGI(TAG, "BCM auth: key accepted");
                    ESP_LOGI(TAG, "BCM authentication complete");
                    m_bcm_task->SetValue(static_cast<int>(BCMTasks::AuthenticationDone));              
                    break;
                }
            }
            break;
        }
        default:
        {
            ESP_LOGI(TAG, "Unexpected service ID %02x for BCM authentication", serviceId);
            return;
        }
    }

    if (static_cast<BCMTasks>(m_bcm_task->AsInt()) != BCMTasks::AuthenticationDone)
    {
        if (frame->origin->Write(&nextFrame) == ESP_FAIL) {
            ESP_LOGE(TAG, "Error writing BCM authentication frame");
            m_bcm_task->SetValue(static_cast<int>(BCMTasks::AuthenticationError));        
        }
    }
}

/**
 * @param TurnOn true = on, false = off
 */
void OvmsVehicleMgEv::DRLCommand(OvmsWriter* writer, canbus* currentBus, bool TurnOn)
{
    if (currentBus == nullptr)
    {
        writer->puts("No CAN configured for the car");
        return;
    }    

    //Wait for BCM to be free
    uint8_t a = 0;
    while (a < MANUAL_COMMAND_TIMEOUT && m_bcm_task->AsInt() != static_cast<int>(BCMTasks::None))
    {
        ESP_LOGI(TAG, "BCM is still busy, waiting %dms (%d)", MANUAL_COMMAND_WAIT_TICK_PERIOD, a+1);
        vTaskDelay(MANUAL_COMMAND_WAIT_TICK_PERIOD / portTICK_PERIOD_MS);
        a++;
    }
    if (m_bcm_task->AsInt() == static_cast<int>(BCMTasks::None))
    { 
        ESP_LOGI(TAG, "Turning %s daytime running light", TurnOn ? "on" : "off");
      
        CAN_frame_t Command = {
            currentBus,
            &DRLFirstFrameSentCallback,
            { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
            bcmId,
            { .u8 = {
                (ISOTP_FT_FIRST << 4) + 0, 0x0c, VEHICLE_POLL_TYPE_IOCONTROL, bcmDrlPid >> 8, bcmDrlPid & 0x00ff, 0x03, static_cast<uint8_t>(TurnOn ? 0x04 : 0), 0
            } }
        };
          
        m_bcm_task->SetValue(static_cast<int>(BCMTasks::DRL));
        // Pause the poller so we're not being interrupted
        {
            OvmsRecMutexLock lock(&m_poll_mutex);
            m_poll_plist = nullptr;
        }             

        if (currentBus->Write(&Command) == ESP_FAIL) {
            ESP_LOGE(TAG, "Error writing DRL command frame");
            m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));
            // Re-start polling
            m_poll_plist = m_pollData;        
            return;      
        }

        //Wait for task to complete
        a = 0;
        while (a < MANUAL_COMMAND_TIMEOUT && m_bcm_task->AsInt() == static_cast<int>(BCMTasks::DRL))
        {
            ESP_LOGI(TAG, "Waiting %dms for DRL command to complete (%d)", MANUAL_COMMAND_WAIT_TICK_PERIOD, a+1);
            vTaskDelay(MANUAL_COMMAND_WAIT_TICK_PERIOD / portTICK_PERIOD_MS);
            a++;
        }        
        switch (static_cast<BCMTasks>(m_bcm_task->AsInt()))
        {
            case BCMTasks::DRL:
            {
                writer->printf("DRL %s command timeout", TurnOn ? "on" : "off");
                m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));    
                break;
            }
            case BCMTasks::DRLDone:
            {
                const char* DRLStateResponse = (m_DRLResponse[1] & 0x0f) == 0x04 ? "on" : "off";
                if ((m_DRLResponse[2] == 0x20) && (m_DRLResponse[3] == 0) && (m_DRLResponse[4] == 0))
                {
                    writer->printf("DRL %s command successful", DRLStateResponse);
                }  
                else if (((m_DRLResponse[2] & 0x0f) == 0xeu) && (m_DRLResponse[4] == 0x22))
                {
                    //Success but user won't see difference. Lights selector must be in AUTO mode with main lights off. DRL will normally be on and bright in this mode. 
                    //If selector is not in AUTO mode, even with main lights off, DRL will be on but dim and the DRL commands will not succeed.
                    if (m_DRLResponse[1] >> 4 == 8)
                    {
                        //Main lights on
                        writer->printf("DRL %s command successful. Turn the lights selector to AUTO mode with main lights off to see result.", DRLStateResponse);
                    }
                    else if (m_DRLResponse[1] >> 4 == 0)
                    {
                        //Main lights off
                        writer->printf("DRL %s command successful. Turn the lights selector to AUTO mode with main lights off to see result.", DRLStateResponse);
                    }
                }
                else
                {
                    writer->printf("DRL %s command failed", TurnOn ? "on" : "off");
                }                
                m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));
                break;
            }
            case BCMTasks::DRLError:
            {
                writer->printf("DRL %s command failed", TurnOn ? "on" : "off");
                m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));    
                break;
            }
            default: {}
        }
        // Re-start polling
        m_poll_plist = m_pollData;                
    }
    else
    {
        writer->puts("BCM is busy, please try again later");
    }
}

void OvmsVehicleMgEv::DRLFirstFrameSent(const CAN_frame_t* p_frame, bool success)
{
    if (success)
    {
        m_DRLConsecutiveFrameTimer = esp_timer_get_time();
        ESP_LOGI(TAG, "DRL first frame sent @ %" PRId64, m_DRLConsecutiveFrameTimer);
    }
    else
    {
        ESP_LOGE(TAG, "DRL first frame sent failed");
    }
}

void OvmsVehicleMgEv::IncomingBCMDRLFrame(CAN_frame_t* frame, uint8_t frameType, uint8_t serviceId, uint16_t responsePid, uint8_t* data)
{
    if (serviceId == UDS_RESP_TYPE_NRC)
    {
        ESP_LOGI(
            TAG, "Got a BCM negative response (length %d): %02x %02x %02x %02x %02x %02x %02x %02x",
            frame->FIR.B.DLC, 
            frame->data.u8[0], frame->data.u8[1], frame->data.u8[2], frame->data.u8[3], 
            frame->data.u8[4], frame->data.u8[5], frame->data.u8[6], frame->data.u8[7]
        );
        if (m_bcm_task->AsInt() == static_cast<int>(BCMTasks::DRL))
        {
            ESP_LOGI(TAG, "Abandoning DRL command");
            m_bcm_task->SetValue(static_cast<int>(BCMTasks::DRLError));
        }
        return;
    } 

    CAN_frame_t nextFrame = {
        frame->origin,
        nullptr,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        bcmId,
        { .u8 = {
            0, 0, 0, 0, 0, 0, 0, 0
        } }
    };

    switch (frameType)
    {
        case ISOTP_FT_FIRST:
        {
            if ((serviceId == VEHICLE_POLL_TYPE_IOCONTROL) && (responsePid == bcmDrlPid))
            {
                // Send flow control and save first half of response
                ESP_LOGI(TAG, "DRL: first multi-frame response received, sending flow control");
                nextFrame.data.u8[0] = (ISOTP_FT_FLOWCTRL << 4) | 0;
                m_DRLResponse[0] = data[3];
                m_DRLResponse[1] = data[4];
                m_DRLResponse[2] = data[5];
            }
            else
            {
                ESP_LOGI(TAG, "DRL: unexpected serviceId %02x or responsePid %04x", serviceId, responsePid);
                return;
            }
            break;
        }
        case ISOTP_FT_CONSECUTIVE:
        {
            // Save second half of response
            m_DRLResponse[3] = data[0];
            m_DRLResponse[4] = data[1];
            ESP_LOGI(TAG, "DRL: second multi-frame response received. Total DRL response = %02x %02x %02x %02x %02x", m_DRLResponse[0], m_DRLResponse[1], m_DRLResponse[2], m_DRLResponse[3], m_DRLResponse[4]);
            m_bcm_task->SetValue(static_cast<int>(BCMTasks::DRLDone));                          
            break;
        }
        case ISOTP_FT_FLOWCTRL:
        {
            uint8_t DesiredSeparationTime = data[1]; //in milliseconds
            int64_t CurrentTime = esp_timer_get_time();
            int64_t TimeElapsedSinceFirstFrameSent = CurrentTime - m_DRLConsecutiveFrameTimer; //in microseconds
            int64_t TimeLeftToWait = DesiredSeparationTime*1000 - TimeElapsedSinceFirstFrameSent; //in microseconds
            ESP_LOGI(TAG, "DRL: flow control received. Current time = %" PRId64 ". Desired separation time = %dus. Time elapsed since first frame sent = %" PRId64 "us. Time left to wait = %" PRId64 "us.", CurrentTime, DesiredSeparationTime*1000, TimeElapsedSinceFirstFrameSent, std::max(TimeLeftToWait, (int64_t)0)); 
            if (TimeLeftToWait > 0)
            {
                int64_t TicksToWait = TimeLeftToWait / portTICK_PERIOD_US + (TimeLeftToWait % portTICK_PERIOD_US > 0 ? 1 : 0); //TicksToWait = ceil(TimeLeftToWait/portTICK_PERIOD_US)
                ESP_LOGI(TAG, "Waiting %" PRId64 "us (%" PRId64 " ticks) before sending next frame", TimeLeftToWait, TicksToWait);
                vTaskDelay(TicksToWait);
                ESP_LOGI(TAG, "(%" PRId64 ") Wait completed", esp_timer_get_time());
            }
            ESP_LOGI(TAG, "Sending next frame");
            nextFrame.data.u8[0] = (ISOTP_FT_CONSECUTIVE << 4) | 1;
            nextFrame.data.u8[3] = 0x04;           
            break;
        }
    }


    if (static_cast<BCMTasks>(m_bcm_task->AsInt()) != BCMTasks::DRLDone)
    {
        if (frame->origin->Write(&nextFrame) == ESP_FAIL) {
            ESP_LOGE(TAG, "Error writing DRL frame");
            m_bcm_task->SetValue(static_cast<int>(BCMTasks::DRLError));        
        }
    }
}

void OvmsVehicleMgEv::IncomingBCMFrame(CAN_frame_t* frame, uint8_t frameType, uint16_t frameLength, uint8_t serviceId, uint16_t responsePid, uint8_t* data)
{
    switch (serviceId)
    {
        case VEHICLE_POLL_TYPE_READDATA:
        {
            if (frameType == ISOTP_FT_SINGLE)
            {
                data = &data[3];
                uint8_t dataLength = frameLength - 3;
                IncomingBcmPoll(responsePid, data, dataLength);
            }                
            break;
        }
        case UDS_RESP_TYPE_NRC:
        {
            ESP_LOGI(TAG, "BCM received negative response");
            break;
        }
    }
}