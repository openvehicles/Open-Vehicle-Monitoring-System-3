/*
;    Project:       Open Vehicle Monitor System
;    Module:        Renault Zoe Ph2 - Vehicle API Commands
;
;    (C) 2022-2026  Carsten Schmiemann
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software;
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "vehicle_renaultzoe_ph2.h"

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandClimateControl(bool climatecontrolon)
{
    OvmsVehicle::vehicle_command_t res;
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    ESP_LOGI(TAG, "Request CommandClimateControl %s", climatecontrolon ? "ON" : "OFF");

    if (climatecontrolon)
    {
        if (!m_vcan_enabled)
        {
            ESP_LOGW(TAG, "CommandClimateControl not possible, because V-CAN is not enabled");
            MyNotify.NotifyString("error", "climate.control", "V-CAN is not enabled");
            res = Fail;
            return res;
        }
        // Check if Zoe is locked, Zoe ECUs can be alive or not, but must be locked to successful trigger pre-heat
        if (!StandardMetrics.ms_v_env_locked->AsBool())
        {
            ESP_LOGW(TAG, "CommandClimateControl not possible, because vehicle is not locked");
            MyNotify.NotifyString("error", "climate.control", "Vehicle is not locked");
            res = Fail;
            return res;
        }
        // Check if hvac is already running
        else if (StandardMetrics.ms_v_env_hvac->AsBool())
        {
            ESP_LOGW(TAG, "CommandClimateControl not possible, because HVAC is already running");
            MyNotify.NotifyString("error", "climate.control", "HVAC is already running");
            res = Fail;
            return res;
        }
        else if (StandardMetrics.ms_v_bat_soc->AsInt() < 15)
        {
            ESP_LOGW(TAG, "CommandClimateControl not possible, because SoC is too low");
            MyNotify.NotifyString("error", "climate.control", "Battery SoC is too low");
            res = Fail;
            return res;
        }
        else
        {
            // HFM to BCM wake up car and start preheat on V-CAN - all in one
            uint8_t data[8] = {0x74, 0x15, 0xFF, 0x00, 0xA4, 0x84, 0x07, 0x11};
            int tx_success_count = 0;
            int tx_attempt_count = 0;
            const int min_successes = 3;   // Need at least 3 successful transmissions
            const int max_attempts = 15;   // Maximum attempts to get those successes

            // Send multiple messages to wake up bus and trigger climate control
            // Stop early if we get enough successful transmissions
            for (int i = 0; i < max_attempts && tx_success_count < min_successes; i++)
            {
                // Clear success flag before sending
                m_tx_success_flag.store(false);

                // Send the frame
                m_can1->WriteStandard(HFM_A1_ID, 8, data);
                tx_attempt_count++;

                // Wait for transmission and callback
                vTaskDelay(100 / portTICK_PERIOD_MS);

                // Check if this specific frame was successfully transmitted
                if (m_tx_success_flag.load() && m_tx_success_id.load() == HFM_A1_ID)
                {
                    tx_success_count++;
                    ESP_LOGI(TAG, "Pre-heat command transmitted successfully (%d/%d successes, attempt %d)",
                             tx_success_count, min_successes, tx_attempt_count);
                }
                else
                {
                    ESP_LOGW(TAG, "Pre-heat command transmission failed (attempt %d)", tx_attempt_count);
                }

                // Add delay between attempts (total ~250ms per iteration)
                vTaskDelay(150 / portTICK_PERIOD_MS);
            }

            if (tx_success_count == 0)
            {
                ESP_LOGE(TAG, "ClimateControl failed - no messages were successfully transmitted after %d attempts", tx_attempt_count);
                MyNotify.NotifyString("error", "climate.control", "Failed to transmit commands to V-CAN");
                return Fail;
            }

            ESP_LOGI(TAG, "ClimateControl: %d/%d messages transmitted successfully", tx_success_count, tx_attempt_count);

            // We know the car will wake up and switch on DC-DC converter, so we can start polling
            if (!rz2->mt_bus_awake->AsBool())
            {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                if (rz2->m_vcan_enabled == false)
                {
                    rz2->ZoeWakeUp();
                }
            }

            StandardMetrics.ms_v_env_hvac->SetValue(true);

            // Disable after-ignition DCDC if active (preconditioning will run DCDC)
            if (rz2->m_dcdc_after_ignition_active)
            {
                ESP_LOGI(TAG, "ClimateControl: Disabling after-ignition DCDC (preconditioning will handle power)");
                rz2->DisableDCDC();
            }

            res = Success;
        }
    }
    else
    {
        // There is a equvivalent command to stop pre-conditioning but HVAC wont stop, so probably not coded in the ECUs
        ESP_LOGW(TAG, "Stop CommandClimateControl not possible, Zoe doesnt support it");
        MyNotify.NotifyString("error", "climate.control", "Stop CommandClimateControl not possible, Zoe doesnt support it");
        res = Fail;
    }

    return res;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandWakeup()
{
    if (!m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandWakeup not possible, because V-CAN is not enabled");
        MyNotify.NotifyString("error", "wakeup.control", "V-CAN is not enabled");
        return Fail;
    }
    else
    {
        // Sending idle HFM commands, HFM, BCM and EVC are waking up for about 20 secs wihout DC-DC, enough to get all metrics
        uint8_t wake_cmd1[8] = {0x64, 0x78, 0x60, 0x00, 0x48, 0x84, 0x07, 0x10};
        uint8_t wake_cmd2[8] = {0x74, 0x47, 0x95, 0x00, 0x48, 0x84, 0x07, 0x10};
        uint8_t wake_cmd3[8] = {0x84, 0x47, 0x95, 0x00, 0x48, 0x84, 0x07, 0x10};
        uint8_t wake_cmd4[8] = {0x94, 0x90, 0x9A, 0x00, 0x44, 0x84, 0x07, 0x10};

        ESP_LOGI(TAG, "Wake up V-CAN...");

        for (int i = 0; i < 3; i++)
        {
            m_can1->WriteStandard(HFM_A1_ID, 8, wake_cmd1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 3; i++)
        {
            m_can1->WriteStandard(HFM_A1_ID, 8, wake_cmd2);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 3; i++)
        {
            m_can1->WriteStandard(HFM_A1_ID, 8, wake_cmd3);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 3; i++)
        {
            m_can1->WriteStandard(HFM_A1_ID, 8, wake_cmd4);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        return Success;
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandLock(const char *pin)
{
    if (!PinCheck(pin))
    {
        ESP_LOGW(TAG, "CommandLock: PIN check failed");
        MyNotify.NotifyString("error", "lock.control", "PIN check failed");
        return Fail;
    }

    if (!m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandLock not possible, because V-CAN is not enabled");
        MyNotify.NotifyString("error", "lock.control", "V-CAN is not enabled");
        return Fail;
    }
    else
    {
        uint8_t lock[8] = {0x44, 0xE3, 0x4F, 0x10, 0x04, 0x84, 0x07, 0x10};
        bool bus_was_sleeping = !mt_bus_awake->AsBool();

        // Send BCM wakeup frame if CAN is sleeping
        if (bus_was_sleeping)
        {
            ESP_LOGI(TAG, "CommandLock: CAN is sleeping, sending BCM wakeup frame");
            uint8_t wakeup[2] = {0x83, 0xC0};
            int wakeup_success_count = 0;
            const int required_successes = 3;
            const int max_wakeup_attempts = 10;

            // Try to send wakeup frame and verify transmission - need 3 successful transmissions
            for (int i = 0; i < max_wakeup_attempts && wakeup_success_count < required_successes; i++)
            {
                m_tx_success_flag.store(false);
                m_can1->WriteStandard(0x682, 2, wakeup);
                vTaskDelay(100 / portTICK_PERIOD_MS);

                if (m_tx_success_flag.load() && m_tx_success_id.load() == 0x682)
                {
                    wakeup_success_count++;
                    ESP_LOGI(TAG, "BCM wakeup frame transmitted successfully (%d/%d successes, attempt %d)",
                             wakeup_success_count, required_successes, i + 1);
                }
                else
                {
                    ESP_LOGW(TAG, "BCM wakeup frame transmission failed (attempt %d)", i + 1);
                }
            }

            if (wakeup_success_count < required_successes)
            {
                ESP_LOGE(TAG, "CommandLock: Failed to send BCM wakeup frame - only %d/%d successful",
                         wakeup_success_count, required_successes);
                MyNotify.NotifyString("error", "lock.control", "Failed to wake up CAN bus");
                return Fail;
            }

            // Wait for bus to wake up
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }

        // Determine how many times to send the command
        int send_count = bus_was_sleeping ? 2 : 1;
        int successful_sends = 0;

        for (int send = 0; send < send_count; send++)
        {
            bool tx_success = false;
            int max_attempts = 10;

            // Send frames until we get actual transmission confirmation from TxCallback
            for (int i = 0; i < max_attempts && !tx_success; i++)
            {
                // Clear success flag before sending
                m_tx_success_flag.store(false);

                // Send the frame
                m_can1->WriteStandard(HFM_A1_ID, 8, lock);

                // Wait for transmission and callback
                vTaskDelay(100 / portTICK_PERIOD_MS);

                // Check if this specific frame was successfully transmitted
                if (m_tx_success_flag.load() && m_tx_success_id.load() == HFM_A1_ID)
                {
                    tx_success = true;
                    ESP_LOGI(TAG, "Lock command transmitted successfully (send %d/%d, attempt %d)",
                             send + 1, send_count, i + 1);
                }
                else
                {
                    ESP_LOGW(TAG, "Lock command transmission failed (send %d/%d, attempt %d/%d)",
                             send + 1, send_count, i + 1, max_attempts);
                }
            }

            if (tx_success)
            {
                successful_sends++;
                // Add small delay between commands when bus was sleeping
                if (bus_was_sleeping && send < send_count - 1)
                {
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Lock command failed on send %d/%d after %d attempts",
                         send + 1, send_count, max_attempts);
            }
        }

        if (successful_sends > 0)
        {
            ESP_LOGI(TAG, "Lock command completed: %d/%d successful sends", successful_sends, send_count);
            return Success;
        }
        else
        {
            ESP_LOGE(TAG, "Lock command failed - no successful transmissions");
            return Fail;
        }
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandUnlock(const char *pin)
{
    if (!PinCheck(pin))
    {
        ESP_LOGW(TAG, "CommandUnlock: PIN check failed");
        MyNotify.NotifyString("error", "lock.control", "PIN check failed");
        return Fail;
    }

    if (!m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandUnlock not possible, because V-CAN is not enabled");
        MyNotify.NotifyString("error", "lock.control", "V-CAN is not enabled");
        return Fail;
    }
    else
    {
        // Standard unlock command, or alternative if configured
        uint8_t unlock[8];
        if (m_alt_unlock_cmd)
        {
            // Alternative unlock command for vehicles that don't respond to standard (they use a different Crypto Key)
            uint8_t alt_unlock[8] = {0xA4, 0x0A, 0xFF, 0x18, 0x04, 0x84, 0x07, 0x30};
            memcpy(unlock, alt_unlock, 8);
        }
        else
        {
            uint8_t std_unlock[8] = {0x44, 0xE3, 0x4F, 0x18, 0x04, 0x84, 0x07, 0x10};
            memcpy(unlock, std_unlock, 8);
        }
        bool bus_was_sleeping = !mt_bus_awake->AsBool();

        // Send BCM wakeup frame if CAN is sleeping
        if (bus_was_sleeping)
        {
            ESP_LOGI(TAG, "CommandUnlock: CAN is sleeping, sending BCM wakeup frame");
            uint8_t wakeup[2] = {0x83, 0xC0};
            int wakeup_success_count = 0;
            const int required_successes = 3;
            const int max_wakeup_attempts = 10;

            // Try to send wakeup frame and verify transmission - need 3 successful transmissions
            for (int i = 0; i < max_wakeup_attempts && wakeup_success_count < required_successes; i++)
            {
                m_tx_success_flag.store(false);
                m_can1->WriteStandard(0x682, 2, wakeup);
                vTaskDelay(100 / portTICK_PERIOD_MS);

                if (m_tx_success_flag.load() && m_tx_success_id.load() == 0x682)
                {
                    wakeup_success_count++;
                    ESP_LOGI(TAG, "BCM wakeup frame transmitted successfully (%d/%d successes, attempt %d)",
                             wakeup_success_count, required_successes, i + 1);
                }
                else
                {
                    ESP_LOGW(TAG, "BCM wakeup frame transmission failed (attempt %d)", i + 1);
                }
            }

            if (wakeup_success_count < required_successes)
            {
                ESP_LOGE(TAG, "CommandUnlock: Failed to send BCM wakeup frame - only %d/%d successful",
                         wakeup_success_count, required_successes);
                MyNotify.NotifyString("error", "lock.control", "Failed to wake up CAN bus");
                return Fail;
            }

            // Wait for bus to wake up
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }

        // Determine how many times to send the command
        int send_count = bus_was_sleeping ? 2 : 1;
        int successful_sends = 0;

        for (int send = 0; send < send_count; send++)
        {
            bool tx_success = false;
            int max_attempts = 10;

            // Send frames until we get actual transmission confirmation from TxCallback
            for (int i = 0; i < max_attempts && !tx_success; i++)
            {
                // Clear success flag before sending
                m_tx_success_flag.store(false);

                // Send the frame
                m_can1->WriteStandard(HFM_A1_ID, 8, unlock);

                // Wait for transmission and callback
                vTaskDelay(100 / portTICK_PERIOD_MS);

                // Check if this specific frame was successfully transmitted
                if (m_tx_success_flag.load() && m_tx_success_id.load() == HFM_A1_ID)
                {
                    tx_success = true;
                    ESP_LOGI(TAG, "Unlock command transmitted successfully (send %d/%d, attempt %d)",
                             send + 1, send_count, i + 1);
                }
                else
                {
                    ESP_LOGW(TAG, "Unlock command transmission failed (send %d/%d, attempt %d/%d)",
                             send + 1, send_count, i + 1, max_attempts);
                }
            }

            if (tx_success)
            {
                successful_sends++;
                // Add small delay between commands when bus was sleeping
                if (bus_was_sleeping && send < send_count - 1)
                {
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Unlock command failed on send %d/%d after %d attempts",
                         send + 1, send_count, max_attempts);
            }
        }

        if (successful_sends > 0)
        {
            ESP_LOGI(TAG, "Unlock command completed: %d/%d successful sends", successful_sends, send_count);
            return Success;
        }
        else
        {
            ESP_LOGE(TAG, "Unlock command failed - no successful transmissions");
            return Fail;
        }
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandUnlockTrunk(const char *pin)
{
    if (!m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandUnlockTrunk not possible, because V-CAN is not enabled");
        MyNotify.NotifyString("error", "trunk.control", "V-CAN is not enabled");
        return Fail;
    }
    else
    {
        uint8_t unlock_trunk[8] = {0x94, 0xE3, 0x4F, 0x20, 0x24, 0x84, 0x07, 0x10};
        int max_attempts = (!mt_bus_awake->AsBool()) ? 10 : 3;
        bool tx_success = false;

        // Send frames until we get actual transmission confirmation from TxCallback
        for (int i = 0; i < max_attempts && !tx_success; i++)
        {
            // Clear success flag before sending
            m_tx_success_flag.store(false);

            // Send the frame
            m_can1->WriteStandard(HFM_A1_ID, 8, unlock_trunk);

            // Wait for transmission and callback
            vTaskDelay(100 / portTICK_PERIOD_MS);

            // Check if this specific frame was successfully transmitted
            if (m_tx_success_flag.load() && m_tx_success_id.load() == HFM_A1_ID)
            {
                tx_success = true;
                ESP_LOGI(TAG, "Unlock trunk command transmitted successfully on attempt %d", i + 1);
            }
            else
            {
                ESP_LOGW(TAG, "Unlock trunk command transmission failed (attempt %d/%d)", i + 1, max_attempts);
            }
        }

        if (tx_success)
        {
            return Success;
        }
        else
        {
            ESP_LOGE(TAG, "Unlock trunk command failed - no successful transmission after %d attempts", max_attempts);
            return Fail;
        }
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandHomelink(int button, int durationms)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    ESP_LOGI(TAG, "CommandHomelink button=%d durationms=%d", button, durationms);

    OvmsVehicle::vehicle_command_t res;
    if (button == 0)
    {
        // ClimateControl workaround because no AC button for Zoe Ph2 in old OVMS app
        res = CommandClimateControl(true);
    }
    else if (button == 1)
    {
        // Start CommandClimateControl and set LongPreCondCounter to three, to retrigger preconditioning if stopped
        // If HVAC is already running, just enable LongPreCond mode without retriggering
        if (StandardMetrics.ms_v_env_hvac->AsBool())
        {
            ESP_LOGI(TAG, "HVAC already running, enabling LongPreCond mode");
            rz2->LongPreCond = true;
            rz2->LongPreCondCounter = 3;
            rz2->LongPreCondLastTrigger = xTaskGetTickCount() * portTICK_PERIOD_MS;
            rz2->LongPreCondWaitingForHvacOff = false;  // HVAC is already on, not waiting

            // Disable after-ignition DCDC if active (preconditioning will run DCDC)
            if (rz2->m_dcdc_after_ignition_active)
            {
                ESP_LOGI(TAG, "LongPreCond: Disabling after-ignition DCDC (preconditioning will handle power)");
                rz2->DisableDCDC();
            }

            res = Success;
        }
        else
        {
            res = CommandClimateControl(true);

            // Wait for HVAC to actually turn on (with 30 second timeout)
            if (res == Success)
            {
                int wait_count = 0;
                const int max_wait = 30;  // 30 seconds timeout
                while (!StandardMetrics.ms_v_env_hvac->AsBool() && wait_count < max_wait)
                {
                    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Check every second
                    wait_count++;
                }

                if (StandardMetrics.ms_v_env_hvac->AsBool())
                {
                    ESP_LOGI(TAG, "HVAC turned on after %d seconds, enabling LongPreCond mode", wait_count);
                    rz2->LongPreCond = true;
                    rz2->LongPreCondCounter = 3;
                    rz2->LongPreCondLastTrigger = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    rz2->LongPreCondWaitingForHvacOff = false;  // HVAC is on, not waiting

                    // Disable after-ignition DCDC if active (preconditioning will run DCDC)
                    if (rz2->m_dcdc_after_ignition_active)
                    {
                        ESP_LOGI(TAG, "LongPreCond: Disabling after-ignition DCDC (preconditioning will handle power)");
                        rz2->DisableDCDC();
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "HVAC did not turn on after %d seconds, LongPreCond mode not enabled", max_wait);
                    res = Fail;
                }
            }
        }
    }
    else if (button == 2)
    {
        // Abort LongPreConditioning
        if (LongPreCond)
        {
            rz2->LongPreCond = false;
            rz2->LongPreCondCounter = 0;
            rz2->LongPreCondLastTrigger = 0;
            rz2->LongPreCondWaitingForHvacOff = false;
            res = Success;
        }
        else
        {
            res = Fail;
        }
    }
    else
    {
        res = NotImplemented;
    }

    return res;
}
