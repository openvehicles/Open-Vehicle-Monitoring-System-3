/*
;    Project:       Open Vehicle Monitor System
;    Date:          1st April 2021
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

namespace {

bool StillAwaitingData(std::vector<std::tuple<uint32_t, std::vector<char>, uint16_t>> versions, uint32_t ECUCount)
{
    if (versions.size() < ECUCount)
    {
        //If we haven't received some reply from all ECUs, returns true
        return true; 
    }
    for (auto& version : versions)
    {
        if (get<2>(version) > 0) //If finds an ECU that still has data remaining, returns true
        {
            return true;
        }
    }
    return false;
}

} // anon namespace

void OvmsVehicleMgEv::SoftwareVersions(OvmsWriter* writer)
{
    // Can check car state first to see which ECUs to query.
    // Start with just GWM and TBOX.
    // If m_state_gwm is unlocked (variant B), add everything else except EVCC, FDR and FVCM.
    //      If charger is connected, add EVCC.
    //      If m_ignition_state == 2, add FDR and FVCM.
    // Another possible modification: change data type of m_versions from a vector of a tuple to a vector of a struct so instead of doing get<0>(version), we can do version.ID which is more readable.
    
    constexpr uint32_t ecus[] = {
        bmsId, dcdcId, vcuId, atcId, bcmId, gwmId, tpmsId, pepsId, 0x761u, 0x760u, 0x784u,
        0x776u, 0x771u, 0x782u, 0x734u, 0x733u, 0x732u, 0x730u, 0x723u, 0x721u, 0x720u, 0x711u
    };
    const char *names[] = {
        "BMS", "DCDC", "VCU", "ATC", "BCM", "GWM", "TPMS", "PEPS", "ICE", "IPK", "EVCC",
        "PLC", "SCU", "TC", "FDR", "FVCM", "RDRA", "SRM", "EPB", "EPS", "ABS", "TBOX"
    };
    m_ECUCountToQuerySoftwareVersion = sizeof(ecus) / sizeof(ecus[0]);

    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        writer->puts("No CAN configured for the car");
        return;
    }

    // Wait for GWM and BCM to be free
    uint8_t a = 0;
    while (a < MANUAL_COMMAND_TIMEOUT && !(m_gwm_task->AsInt() == static_cast<int>(GWMTasks::None) && m_bcm_task->AsInt() == static_cast<int>(BCMTasks::None)))
    {
        ESP_LOGI(TAG, "GWM or BCM is still busy, waiting %dms", MANUAL_COMMAND_WAIT_TICK_PERIOD);
        vTaskDelay(MANUAL_COMMAND_WAIT_TICK_PERIOD / portTICK_PERIOD_MS);
        a++;
    }
    if (m_gwm_task->AsInt() == static_cast<int>(GWMTasks::None) && m_bcm_task->AsInt() == static_cast<int>(BCMTasks::None))
    {
        ESP_LOGI(TAG, "Getting software versions");
        m_gwm_task->SetValue(static_cast<int>(GWMTasks::SoftwareVersion));
        m_bcm_task->SetValue(static_cast<int>(BCMTasks::SoftwareVersion));
        m_GettingSoftwareVersions = true;        

        // Pause the poller so we're not being interrupted
        PausePolling();

        // Send the software version requests
        m_versions.clear();
        m_SoftwareVersionResponsesReceived = 0;
        for (size_t i = 0u; i < m_ECUCountToQuerySoftwareVersion; ++i)
        {
            ESP_LOGV(TAG, "Sending query to %03" PRIx32, ecus[i]);
            SendPollMessage(
                currentBus, ecus[i], VEHICLE_POLL_TYPE_OBDIIEXTENDED, softwarePid
            );
        }

        // Wait for the responses
        a = 0;
        while (a < MANUAL_COMMAND_TIMEOUT && StillAwaitingData(m_versions, m_ECUCountToQuerySoftwareVersion))
        {
            ESP_LOGI(TAG, "Waiting %dms for all ECUs to respond (%d)", MANUAL_COMMAND_WAIT_TICK_PERIOD, a+1);
            vTaskDelay(MANUAL_COMMAND_WAIT_TICK_PERIOD / portTICK_PERIOD_MS);
            a++;
        }
        
        ESP_LOGI(TAG, "%d/%" PRId32 " ECUs responded", m_SoftwareVersionResponsesReceived, m_ECUCountToQuerySoftwareVersion);

        m_GettingSoftwareVersions = false;
        //Reset GWM and BCM task back to none (incase we didn't receive a response back)
        if (m_gwm_task->AsInt() == static_cast<int>(GWMTasks::SoftwareVersion) || m_bcm_task->AsInt() == static_cast<int>(BCMTasks::SoftwareVersion))
        {
            m_gwm_task->SetValue(static_cast<int>(GWMTasks::None));
            m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));     
        }  
        // Re-start polling
        ResumePolling();

        // Output responses
        for (const auto& version : m_versions)
        {
            const char* versionString = &get<1>(version).front();
            //Skip null characters
            while (!*versionString)
            {
                ++versionString;
            }
            const char* name = nullptr;
            for (size_t i = 0u; i < sizeof(ecus) / sizeof(ecus[0]); ++i)
            {
                if (ecus[i] == get<0>(version))
                {
                    name = names[i];
                }
            }            
            writer->printf("%s %s\n", name, versionString);
        }
        if (m_versions.empty())
        {
            writer->puts("No ECUs responded");
        }

        // Free memory
        m_versions.clear();        
    }
    else
    {
        writer->puts("GWM or BCM busy, please try again later");
    }
}

void OvmsVehicleMgEv::IncomingSoftwareVersionFrame(const CAN_frame_t* frame, uint8_t frameType, uint16_t frameLength, uint16_t responsePid, const uint8_t* data)
{       
    switch (frameType)
    {
        case ISOTP_FT_SINGLE: case ISOTP_FT_FIRST:
        {
            if (responsePid == softwarePid)
            {        
                //Move data pointer along by 3 bytes so we skip serviceId (1 byte) and responsePid (2 bytes)
                data = &data[3];                
                uint16_t dataLength = frameLength - 3;

                //Initialise software version result as a character vector with '0' characters. Number of 0 characters = data length we are expecting + 1 null character to terminate the string.
                std::vector<char> version(dataLength + 1, '0');
                version.back() = '\0';
                uint16_t BytesToCopy = frameType == ISOTP_FT_SINGLE ? dataLength : 3;
                std::copy(
                    data,
                    &data[BytesToCopy],
                    version.begin()
                );              
                m_versions.push_back(std::make_tuple(
                    frame->MsgID - rxFlag, std::move(version), dataLength - BytesToCopy
                ));                             
                if (frameType == ISOTP_FT_FIRST)
                {
                    //Send flow control
                    CAN_frame_t flowControl = {
                        frame->origin,
                        nullptr,
                        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
                        frame->MsgID - rxFlag,
                        { .u8 = { (ISOTP_FT_FLOWCTRL << 4 | 0), 0, 25, 0, 0, 0, 0, 0 } }
                    };
                    frame->origin->Write(&flowControl);
                    ESP_LOGV(TAG, "First part of software version response from %03" PRIx32 ". %d bytes expected, %d bytes received, %d bytes remaining.", frame->MsgID - rxFlag, dataLength, BytesToCopy, dataLength - BytesToCopy); 
                }  
                else
                {
                    m_SoftwareVersionResponsesReceived++;
                    ESP_LOGI(TAG, "(%d/%" PRId32 ") Received software version response from %03" PRIx32, m_SoftwareVersionResponsesReceived, m_ECUCountToQuerySoftwareVersion, frame->MsgID - rxFlag); 
                }                        
            }
            break;
        }
        case ISOTP_FT_CONSECUTIVE:
        {
            uint8_t frameNumber = frame->data.u8[0] & 0x0f;
            //First of consecutive frames (ISOTP_FT_FIRST) will have had 3 data bytes. Following consecutive frames (ISOTP_FT_CONSECUTIVE) will have 7 data bytes.
            //First consecutive frame will have frameNumber = 1.
            auto start = ((frameNumber - 1) * 7) + 3;
            for (auto& version : m_versions)
            {            
                if (get<0>(version) == frame->MsgID - rxFlag)
                {
                    auto end = get<1>(version).size() - 1; //This is equal to number of expected bytes + 1 since we already initialised the result with '0' characters + null character. Note that std::copy(first, last, result) is inclusive of 'first' but exclusive of 'last' i.e. copies from 'first' up to 1 address before 'last' so having number of expected bytes + 1 is fine, no need to have number of expected bytes.
                    uint16_t BytesToCopy = end - start > 7 ? 7 : (end - start); //If end of frame is beyond this frame, cap copying data to 7 bytes
                    std::copy(
                        data,
                        &data[BytesToCopy],
                        get<1>(version).begin() + start
                    );     
                    get<2>(version) -= BytesToCopy; //Decrement bytes remaining
                    ESP_LOGV(TAG, "Received consecutive frame (%d) of software version response from %03" PRIx32 ". %d bytes received, %d bytes remaining.", frameNumber, frame->MsgID - rxFlag, BytesToCopy, get<2>(version));
                    if (get<2>(version) == 0)
                    {
                        //No more bytes remaining
                        m_SoftwareVersionResponsesReceived++;
                        ESP_LOGI(TAG, "(%d/%" PRId32 ") Received software version response from %03" PRIx32, m_SoftwareVersionResponsesReceived, m_ECUCountToQuerySoftwareVersion, frame->MsgID - rxFlag);
                        switch (frame->MsgID - rxFlag)
                        {
                            case gwmId:
                            {
                                ESP_LOGV(TAG, "Got response from GWM, setting GWM task back to none");
                                m_gwm_task->SetValue(static_cast<int>(GWMTasks::None));
                                break;
                            }
                            case bcmId:
                            {
                                ESP_LOGV(TAG, "Got response from BCM, setting BCM task back to none");
                                m_bcm_task->SetValue(static_cast<int>(BCMTasks::None));
                                break;
                            }
                        }
                    }                                                  
                }                             
            }   
            //break;      
        }
    }
}
