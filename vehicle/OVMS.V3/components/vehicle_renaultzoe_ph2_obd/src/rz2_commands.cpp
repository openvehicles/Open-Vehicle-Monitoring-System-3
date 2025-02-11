/*
;    Project:       Open Vehicle Monitor System
;    Date:          9th Feb 2025
;
;    (C) 2025       Carsten Schmiemann
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

#include "vehicle_renaultzoe_ph2_obd.h"

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2OBD::CommandClimateControl(bool climatecontrolon)
{
    OvmsVehicle::vehicle_command_t res;
    ESP_LOGI(TAG, "Request CommandClimateControl %s", climatecontrolon ? "ON" : "OFF");

    if (climatecontrolon)
    {
        // Check if Zoe is locked, Zoe ECUs can be alive or not, but must be locked to successful trigger pre-heat
        if (!StandardMetrics.ms_v_env_locked->AsBool())
        {
            ESP_LOGW(TAG, "CommandClimateControl not possible, because vehicle is not locked");
            res = Fail;
            return res;
        }
        // Check if hvac is already running
        else if (StandardMetrics.ms_v_env_hvac->AsBool())
        {
            ESP_LOGW(TAG, "CommandClimateControl not possible, because HVAC is already running");
            res = Fail;
            return res;
        }
        else
        {
            // BCM wake up car and start preheat on V-CAN (OVMS CAN2) - all in one
            uint8_t data[8] = {0x74, 0x15, 0xFF, 0x00, 0xA4, 0x84, 0x07, 0x11};

            // Enable CAN2, because we only need it for sending pre-heat command
            m_can2->Start(CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
            m_can2->SetPowerMode(On);

            for (int i = 0; i < 11; i++)
            {
                m_can2->WriteStandard(0x46F, 8, data);
                vTaskDelay(250 / portTICK_PERIOD_MS);
                ESP_LOGI(TAG, "Send wake-up and pre-heat command to V-CAN (CAN2) (%d/10) ...", i);
            }

            // Disable CAN2 interface, to save OVMS resources
            m_can2->SetPowerMode(Off);
            m_can2->Stop();
            res = Success;
        }
    }
    else
    {
        ESP_LOGI(TAG, "CommandClimateControl stop request currently not implemented.");
        res = NotImplemented;
    }

    return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2OBD::CommandHomelink(int button, int durationms)
{
    // ClimateControl workaround because no AC button for Zoe Ph2 in OVMS app
    ESP_LOGI(TAG, "CommandHomelink button=%d durationms=%d", button, durationms);

    OvmsVehicle::vehicle_command_t res;
    if (button == 0)
    {
        res = CommandClimateControl(true);
    }
    else
    {
        res = NotImplemented;
    }

    return res;
}