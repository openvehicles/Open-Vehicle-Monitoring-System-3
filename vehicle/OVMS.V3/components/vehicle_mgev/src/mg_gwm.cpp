/*
;    Project:       Open Vehicle Monitor System
;    Date:          25th February 2021
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Chris Staite
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

bool OvmsVehicleMgEv::AuthenticateGWM(canbus* currentBus)
{
    if (currentBus == nullptr)
    {
        ESP_LOGE(TAG, "No CAN configured for the car");
        return false;
    }    
    
    //Wait for GWM to be free
    uint8_t a = 0;
    while (a < MANUAL_COMMAND_TIMEOUT && m_gwm_task->AsInt() != static_cast<int>(GWMTasks::None))
    {
        ESP_LOGI(TAG, "GWM is still busy, waiting %dms (%d)", MANUAL_COMMAND_WAIT_TICK_PERIOD, a+1);
        vTaskDelay(MANUAL_COMMAND_WAIT_TICK_PERIOD / portTICK_PERIOD_MS);
        a++;
    }
    if (m_gwm_task->AsInt() == static_cast<int>(GWMTasks::None))
    {     
        ESP_LOGI(TAG, "Starting GWM authentication");
        ESP_LOGI(TAG, "GWM auth: sending 1003");

        CAN_frame_t authStart = {
            currentBus,
            nullptr,
            { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
            gwmId,
            { .u8 = {
                (ISOTP_FT_SINGLE << 4) + 2, VEHICLE_POLL_TYPE_OBDIISESSION, 3, 0, 0, 0, 0, 0
            } }
        };

        m_gwm_task->SetValue(static_cast<int>(GWMTasks::Authentication));

        if (currentBus->Write(&authStart) == ESP_FAIL) {
            ESP_LOGE(TAG, "Error writing GWM authentication start frame");
            m_gwm_task->SetValue(static_cast<int>(GWMTasks::None));
            return false;            
        }    

        //Wait for task to complete
        a = 0;
        while (a < MANUAL_COMMAND_TIMEOUT && m_gwm_task->AsInt() == static_cast<int>(GWMTasks::Authentication))
        {
            ESP_LOGI(TAG, "Waiting %dms for GWM auth to complete (%d)", MANUAL_COMMAND_WAIT_TICK_PERIOD, a+1);
            vTaskDelay(MANUAL_COMMAND_WAIT_TICK_PERIOD / portTICK_PERIOD_MS);
            a++;
        } 
        switch (static_cast<GWMTasks>(m_gwm_task->AsInt()))       
        {
            case GWMTasks::Authentication:
            {
                ESP_LOGI(TAG, "GWM auth timeout");
                m_gwm_task->SetValue(static_cast<int>(GWMTasks::None));                       
                break;
            }
            case GWMTasks::AuthenticationDone:
            {
                ESP_LOGI(TAG, "GWM auth done");
                m_gwm_task->SetValue(static_cast<int>(GWMTasks::None));  
                return true;                     
                // break;
            }
            case GWMTasks::AuthenticationError:
            {
                ESP_LOGI(TAG, "GWM auth error");
                m_gwm_task->SetValue(static_cast<int>(GWMTasks::None));
                break;
            }            
            default: {}
        }
    }
    else
    {
        ESP_LOGI(TAG, "GWM is busy, please try again later");
    }
    return false;    
}

void OvmsVehicleMgEv::IncomingGWMAuthFrame(CAN_frame_t* frame, uint8_t serviceId, uint8_t* data)
{
    if (serviceId == UDS_RESP_TYPE_NRC)
    {
        ESP_LOGI(
            TAG, "Got a GWM negative response (length %d): %02x %02x %02x %02x %02x %02x %02x %02x",
            frame->FIR.B.DLC, 
            frame->data.u8[0], frame->data.u8[1], frame->data.u8[2], frame->data.u8[3], 
            frame->data.u8[4], frame->data.u8[5], frame->data.u8[6], frame->data.u8[7]
        );
        if (m_gwm_task->AsInt() == static_cast<int>(GWMTasks::Authentication))
        {
            ESP_LOGI(TAG, "Abandoning GWM authentication");
            m_gwm_task->SetValue(static_cast<int>(GWMTasks::AuthenticationError));
        }        
        return;
    }   

    CAN_frame_t nextFrame = {
        frame->origin,
        nullptr,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        gwmId,
        { .u8 = {
            0, 0, 0, 0, 0, 0, 0, 0
        } }
    };

    switch (serviceId)
    {
        case VEHICLE_POLL_TYPE_OBDIISESSION:
        {
            // Request seed1
            ESP_LOGI(TAG, "GWM auth: requesting seed1");
            nextFrame.data.u8[0] = (ISOTP_FT_SINGLE << 4) | 6;
            nextFrame.data.u8[1] = VEHICLE_POLL_TYPE_SECACCESS;
            nextFrame.data.u8[2] = 0x41u;
            nextFrame.data.u8[3] = 0x3eu;
            nextFrame.data.u8[4] = 0xabu;
            nextFrame.data.u8[5] = 0x00u;
            nextFrame.data.u8[6] = 0x0du;
            break;            
        }
        case VEHICLE_POLL_TYPE_SECACCESS:
        {
            switch (data[1])
            {
                case 0x41:
                {
                    // Seed1 response
                    uint32_t seed = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
                    uint32_t key = MgEvPasscode::GWMKey1(seed);
                    ESP_LOGI(TAG, "GWM auth: seed1 received %08x. Replying with key1 %08x", seed, key);
                    nextFrame.data.u8[0] = (ISOTP_FT_SINGLE << 4) | 6;
                    nextFrame.data.u8[1] = VEHICLE_POLL_TYPE_SECACCESS;            
                    nextFrame.data.u8[2] = 0x42u;
                    nextFrame.data.u8[3] = key >> 24u;
                    nextFrame.data.u8[4] = key >> 16u;
                    nextFrame.data.u8[5] = key >> 8u;
                    nextFrame.data.u8[6] = key;
                    break;
                }
                case 0x42:
                {
                    // Key1 accept, request seed2
                    ESP_LOGI(TAG, "GWM auth: key1 accepted, requesting seed2");
                    nextFrame.data.u8[0] = (ISOTP_FT_SINGLE << 4) | 2;
                    nextFrame.data.u8[1] = VEHICLE_POLL_TYPE_SECACCESS;
                    nextFrame.data.u8[2] = 0x01u;                    
                    break;
                }
                case 0x01:
                {
                    // Seed2 response
                    uint32_t seed = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
                    uint32_t key = MgEvPasscode::GWMKey2(seed);
                    ESP_LOGI(TAG, "GWM auth: seed2 received %08x. Replying with key2 %08x", seed, key);
                    nextFrame.data.u8[0] = (ISOTP_FT_SINGLE << 4) | 6;
                    nextFrame.data.u8[1] = VEHICLE_POLL_TYPE_SECACCESS;            
                    nextFrame.data.u8[2] = 0x02u;
                    nextFrame.data.u8[3] = key >> 24u;
                    nextFrame.data.u8[4] = key >> 16u;
                    nextFrame.data.u8[5] = key >> 8u;
                    nextFrame.data.u8[6] = key; 
                    break;                   
                }
                case 0x02:
                {
                    // Key2 accept, starting routine
                    ESP_LOGI(TAG, "GWM auth: key2 accepted, starting routine");
                    nextFrame.data.u8[0] = (ISOTP_FT_SINGLE << 4) | 5;
                    nextFrame.data.u8[1] = VEHICLE_POLL_TYPE_ROUTINECONTROL;
                    nextFrame.data.u8[2] = 0x01u;
                    nextFrame.data.u8[3] = 0xaau;
                    nextFrame.data.u8[4] = 0xffu;
                    nextFrame.data.u8[5] = 0x00u;         
                    break;           
                }
            }
            break;
        }
        case VEHICLE_POLL_TYPE_ROUTINECONTROL:
        {
            switch (data[1])
            {
                case 0x01:
                {
                    // Routine started, request routine control
                    ESP_LOGI(TAG, "GWM auth: Routine started, requesting routine control");
                    nextFrame.data.u8[0] = (ISOTP_FT_SINGLE << 4) | 4;
                    nextFrame.data.u8[1] = VEHICLE_POLL_TYPE_ROUTINECONTROL;
                    nextFrame.data.u8[2] = 0x03u;
                    nextFrame.data.u8[3] = 0xaau;
                    nextFrame.data.u8[4] = 0xffu;
                    break;
                }
                case 0x03:
                {
                    // Routine control accepted
                    ESP_LOGI(TAG, "GWM auth: Requested routine control.");
                    ESP_LOGI(TAG, "Gateway authentication complete");
                    m_gwm_task->SetValue(static_cast<int>(GWMTasks::AuthenticationDone));
                    break;                  
                }
            }
            break;
        }
        default:
        {
            ESP_LOGI(TAG, "Unexpected service ID %02x for gateway authentication", serviceId);
            return;
        }
    }

    if (static_cast<GWMTasks>(m_gwm_task->AsInt()) != GWMTasks::AuthenticationDone)
    {
        if (frame->origin->Write(&nextFrame) == ESP_FAIL) {
            ESP_LOGE(TAG, "Error writing GWM authentication frame");
            m_gwm_task->SetValue(static_cast<int>(GWMTasks::AuthenticationError));         
        }
    }
}

void OvmsVehicleMgEv::IncomingGWMFrame(CAN_frame_t* frame, uint8_t frameType, uint16_t frameLength, uint8_t serviceId, uint16_t responsePid, uint8_t* data)
{
    switch (frameType)
    {
        case ISOTP_FT_SINGLE:
        {
            switch (serviceId)
            {
                case VEHICLE_POLL_TYPE_TESTERPRESENT:
                {
                    if (frameLength == 2 && data[1] == 0)
                    {
                        ESP_LOGV(TAG, "GWM has responded to tester present");
                        m_WaitingGWMTesterPresentResponse = false;
                    }
                    break;
                }
                case UDS_RESP_TYPE_NRC:
                {
                    ESP_LOGI(TAG, "GWM received negative response");
                    break;
                }
            }
            break;
        }
    }
}