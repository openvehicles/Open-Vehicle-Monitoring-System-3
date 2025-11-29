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
        uint8_t unlock[8] = {0x44, 0xE3, 0x4F, 0x18, 0x04, 0x84, 0x07, 0x10};
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
        res = CommandClimateControl(true);
        vTaskDelay(15000 / portTICK_PERIOD_MS); // Wait for HVAC response
        rz2->LongPreCond = true;
        rz2->LongPreCondCounter = 3;
    }
    else if (button == 2)
    {
        // Abort LongPreConditioning
        if (LongPreCond)
        {
            rz2->LongPreCond = false;
            rz2->LongPreCondCounter = 0;
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

void OvmsVehicleRenaultZoePh2::CommandPollerStartIdle(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    rz2->PollSetState(1);

    if (rz2->m_vcan_enabled)
    {
        rz2->vcan_poller_state = true;
    }

    ESP_LOGI(TAG, "Start Ph2 poller in idle mode");
    writer->puts("Start Ph2 poller in idle mode");
}

void OvmsVehicleRenaultZoePh2::CommandPollerStop(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    rz2->PollSetState(0);

    // We dont know anything about the car if OBD port is used, so assume it will go sleeping
    if (rz2->m_vcan_enabled)
    {
        rz2->vcan_poller_state = false;
    }
    else
    {
        rz2->mt_bus_awake->SetValue(false);
        StandardMetrics.ms_v_env_awake->SetValue(false);
        StandardMetrics.ms_v_env_charging12v->SetValue(false);
        StandardMetrics.ms_v_pos_speed->SetValue(0);
        StandardMetrics.ms_v_bat_12v_current->SetValue(0);
        StandardMetrics.ms_v_charge_12v_current->SetValue(0);
        StandardMetrics.ms_v_bat_current->SetValue(0);
        StandardMetrics.ms_v_env_hvac->SetValue(false);
        StandardMetrics.ms_v_env_aux12v->SetValue(false);
    }

    ESP_LOGI(TAG, "Stop Ph2 poller");
    writer->puts("Stop Ph2 poller");
}

void OvmsVehicleRenaultZoePh2::CommandLighting(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    if (!rz2->m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandLighting not possible, because V-CAN is not enabled");
        writer->puts("CommandLighting not possible, because V-CAN is not enabled");
        return;
    }

    uint8_t lighting_cmd[8] = {0x44, 0xE3, 0x4F, 0x40, 0x04, 0x84, 0x07, 0x10};
    int max_attempts = (!rz2->mt_bus_awake->AsBool()) ? 10 : 3;
    bool tx_success = false;

    // Send frames until we get actual transmission confirmation from TxCallback
    for (int i = 0; i < max_attempts && !tx_success; i++)
    {
        // Clear success flag before sending
        rz2->m_tx_success_flag.store(false);

        // Send the frame
        rz2->m_can1->WriteStandard(HFM_A1_ID, 8, lighting_cmd);

        // Wait for transmission and callback
        vTaskDelay(100 / portTICK_PERIOD_MS);

        // Check if this specific frame was successfully transmitted
        if (rz2->m_tx_success_flag.load() && rz2->m_tx_success_id.load() == HFM_A1_ID)
        {
            tx_success = true;
            ESP_LOGI(TAG, "Lighting command transmitted successfully on attempt %d", i + 1);
        }
        else
        {
            ESP_LOGW(TAG, "Lighting command transmission failed (attempt %d/%d)", i + 1, max_attempts);
        }
    }

    if (tx_success)
    {
        ESP_LOGI(TAG, "Lighting command completed - headlights should turn on for ~30 seconds");
        writer->puts("Lighting command sent - headlights will stay on for ~30 seconds");
    }
    else
    {
        ESP_LOGE(TAG, "Lighting command failed - no successful transmission after %d attempts", max_attempts);
        writer->puts("Lighting command failed - CAN transmission error");
    }
}

void OvmsVehicleRenaultZoePh2::CommandUnlockTrunkShell(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    if (!rz2->m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandUnlockTrunk not possible, because V-CAN is not enabled");
        writer->puts("CommandUnlockTrunk not possible, because V-CAN is not enabled");
        return;
    }

    OvmsVehicle::vehicle_command_t result = rz2->CommandUnlockTrunk(NULL);

    if (result == OvmsVehicle::Success)
    {
        writer->puts("Trunk unlock command sent successfully");
    }
    else
    {
        writer->puts("Trunk unlock command failed");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDebug(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    // Debug 100km rolling consumption calculation
    float batteryConsumption = StandardMetrics.ms_v_bat_consumption->AsFloat(0, kWhP100K);
    float estimatedRange = StandardMetrics.ms_v_bat_range_est->AsFloat(0, Kilometers);
    float totalDistance = rz2->totalDistance;
    // Debug VCAN poller trigger
    float vcan_message_counter = rz2->vcan_message_counter;
    size_t historySize = rz2->consumptionHistory.size();
    // Debug long preconditioning
    bool LongPreCond = rz2->LongPreCond;
    int LongPreCondCounter = rz2->LongPreCondCounter;

    writer->printf("Battery Consumption: %.2f kWh/100km\n", batteryConsumption);
    writer->printf("Estimated Range: %.2f km\n", estimatedRange);
    writer->printf("Total Distance: %.2f km\n", totalDistance);
    writer->printf("VCAN message count: %.0f msgs/s\n", vcan_message_counter);
    writer->printf("Consumption history buffer size: %zu entries\n", historySize);
    writer->printf("LongPreCond active: %s\n", LongPreCond ? "yes" : "no");
    writer->printf("LongPreCond counter: %d\n", LongPreCondCounter);
}

void OvmsVehicleRenaultZoePh2::CommandPollerInhibit(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    rz2->mt_poller_inhibit->SetValue(true);
    rz2->ZoeShutdown();
    ESP_LOGW(TAG, "OVMS poller inhibited, it will not detect Zoe wakeup and sleep, ready for vehicle mantenance");
    writer->printf("OVMS poller inhibited, it will not detect Zoe wakeup and sleep, ready for vehicle mantenance");
}

void OvmsVehicleRenaultZoePh2::CommandPollerResume(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    rz2->mt_poller_inhibit->SetValue(false);
    ESP_LOGI(TAG, "OVMS poller resumed, if OBD port is used, Zoe must sleep before it will detect wakeup again");
    writer->printf("OVMS poller resumed, if OBD port is used, Zoe must sleep before it will detect wakeup again");
}

// ISO TP common function to send ISO TP requests to ECUs connected to V-CAN to change configuration or clear DTCs
OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandDDT(uint32_t txid, uint32_t rxid, bool ext_frame, string request)
{
    uint8_t protocol = ext_frame ? ISOTP_EXTADR : ISOTP_STD;
    int timeout_ms = 1000;  // 1 second timeout for busy CAN bus
    std::string response;

    ESP_LOGD(TAG, "CommandDDT: TX=0x%03" PRIX32 " RX=0x%03" PRIX32 " Protocol=%s Request=%s",
             txid, rxid, ext_frame ? "EXTADR" : "STD", hexencode(request).c_str());

    int err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);

    if (err == POLLSINGLE_OK)
    {
        ESP_LOGD(TAG, "CommandDDT: Success - Response: %s", hexencode(response).c_str());
        return Success;
    }
    else if (err == POLLSINGLE_TXFAILURE)
    {
        ESP_LOGE(TAG, "CommandDDT: Transmission failure (CAN bus error)");
        return Fail;
    }
    else if (err == POLLSINGLE_TIMEOUT)
    {
        ESP_LOGE(TAG, "CommandDDT: Timeout waiting for response");
        return Fail;
    }
    else
    {
        // UDS/ISO-TP error code (positive value)
        ESP_LOGE(TAG, "CommandDDT: Request failed with error code 0x%02X (%s)",
                 err, OvmsPoller::PollResultCodeName(err));
        return Fail;
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacEnableCompressor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (!StandardMetrics.ms_v_env_awake->AsBool() || StandardMetrics.ms_v_pos_speed->AsFloat() != 0)
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
        return;
    }

    ESP_LOGI(TAG, "Try to send HVAC compressor enable command");
    writer->puts("Try to send HVAC compressor enable command");

    const int max_retries = 3;
    OvmsVehicle::vehicle_command_t result;
    bool all_successful = true;

    // Extended session
    int retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("1003"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "Extended session command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "Extended session command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "Extended session command failed after %d attempts", max_retries);
        writer->puts("ERROR: Extended session command failed");
        all_successful = false;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Enable hot loop
    retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("2E422402"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "Hot loop enable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "Hot loop enable command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "Hot loop enable command failed after %d attempts", max_retries);
        writer->puts("ERROR: Hot loop enable command failed");
        all_successful = false;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Enable cold loop
    retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("2E421F06"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "Cold loop enable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "Cold loop enable command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "Cold loop enable command failed after %d attempts", max_retries);
        writer->puts("ERROR: Cold loop enable command failed");
        all_successful = false;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Reset ECU
    retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("1101"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "ECU reset command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "ECU reset command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "ECU reset command failed after %d attempts", max_retries);
        writer->puts("ERROR: ECU reset command failed");
        all_successful = false;
    }

    if (all_successful) {
        ESP_LOGI(TAG, "All compressor enable commands completed successfully");
        writer->puts("Compressor enabled successfully (hot and cold loops active)");
    } else {
        ESP_LOGW(TAG, "Some compressor enable commands failed - check logs for details");
        writer->puts("WARNING: Some compressor enable commands failed - check logs");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacDisableCompressor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (!StandardMetrics.ms_v_env_awake->AsBool() || StandardMetrics.ms_v_pos_speed->AsFloat() != 0)
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
        return;
    }

    ESP_LOGI(TAG, "Try to send HVAC compressor disable command");
    writer->puts("Try to send HVAC compressor disable command");

    const int max_retries = 3;
    OvmsVehicle::vehicle_command_t result;
    bool all_successful = true;

    // Extended session
    int retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("1003"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "Extended session command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "Extended session command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "Extended session command failed after %d attempts", max_retries);
        writer->puts("ERROR: Extended session command failed");
        all_successful = false;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Disable hot loop
    retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("2E422401"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "Hot loop disable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "Hot loop disable command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "Hot loop disable command failed after %d attempts", max_retries);
        writer->puts("ERROR: Hot loop disable command failed");
        all_successful = false;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Disable cold loop
    retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("2E421F01"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "Cold loop disable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "Cold loop disable command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "Cold loop disable command failed after %d attempts", max_retries);
        writer->puts("ERROR: Cold loop disable command failed");
        all_successful = false;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Reset ECU
    retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("1101"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "ECU reset command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "ECU reset command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "ECU reset command failed after %d attempts", max_retries);
        writer->puts("ERROR: ECU reset command failed");
        all_successful = false;
    }

    if (all_successful) {
        ESP_LOGI(TAG, "All compressor disable commands completed successfully");
        writer->puts("Compressor disabled successfully (hot and cold loops inactive)");
    } else {
        ESP_LOGW(TAG, "Some compressor disable commands failed - check logs for details");
        writer->puts("WARNING: Some compressor disable commands failed - check logs");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtClusterResetService(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (StandardMetrics.ms_v_env_awake->AsBool() && StandardMetrics.ms_v_pos_speed->AsFloat() == 0)
    {
        ESP_LOGI(TAG, "Try to send Cluster service reset command");
        writer->puts("Try to send Cluster service reset command");

        // Extended session
        rz2->CommandDDT(METER_DDT_REQ_ID, METER_DDT_RESP_ID, false, hexdecode("1003"));
        vTaskDelay(500 / portTICK_PERIOD_MS);

        // Reset command
        rz2->CommandDDT(METER_DDT_REQ_ID, METER_DDT_RESP_ID, false, hexdecode("3101500500"));
        vTaskDelay(500 / portTICK_PERIOD_MS);
        rz2->CommandDDT(METER_DDT_REQ_ID, METER_DDT_RESP_ID, false, hexdecode("3101500200"));
    }
    else
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacEnablePTC(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    ESP_LOGI(TAG, "Try to send HVAC PTC enable command");
    writer->puts("Try to send HVAC PTC enable command");

    const int max_retries = 3;
    OvmsVehicle::vehicle_command_t result;
    bool all_successful = true;

    // Enable PTC1
    int retry_count = 0;
    do {
        result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode("2E0533D400"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "PTC1 enable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "PTC1 command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "PTC1 enable command failed after %d attempts", max_retries);
        writer->puts("ERROR: PTC1 enable command failed");
        all_successful = false;
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Enable PTC2
    retry_count = 0;
    do {
        result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode("2E0534BC00"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "PTC2 enable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "PTC2 command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "PTC2 enable command failed after %d attempts", max_retries);
        writer->puts("ERROR: PTC2 enable command failed");
        all_successful = false;
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Enable PTC3
    retry_count = 0;
    do {
        result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode("2E05358C00"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "PTC3 enable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "PTC3 command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "PTC3 enable command failed after %d attempts", max_retries);
        writer->puts("ERROR: PTC3 enable command failed");
        all_successful = false;
    }

    if (all_successful) {
        ESP_LOGI(TAG, "All PTC enable commands completed successfully");
        writer->puts("All PTC heaters enabled successfully");
    } else {
        ESP_LOGW(TAG, "Some PTC enable commands failed - check logs for details");
        writer->puts("WARNING: Some PTC commands failed - check logs");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacDisablePTC(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    ESP_LOGI(TAG, "Try to send HVAC PTC disable command");
    writer->puts("Try to send HVAC PTC disable command");

    const int max_retries = 3;
    OvmsVehicle::vehicle_command_t result;
    bool all_successful = true;

    // Disable PTC1
    int retry_count = 0;
    do {
        result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode("2E05335400"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "PTC1 disable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "PTC1 command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "PTC1 disable command failed after %d attempts", max_retries);
        writer->puts("ERROR: PTC1 disable command failed");
        all_successful = false;
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Disable PTC2
    retry_count = 0;
    do {
        result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode("2E05343C00"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "PTC2 disable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "PTC2 command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "PTC2 disable command failed after %d attempts", max_retries);
        writer->puts("ERROR: PTC2 disable command failed");
        all_successful = false;
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Disable PTC3
    retry_count = 0;
    do {
        result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode("2E05350C00"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "PTC3 disable command sent successfully");
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "PTC3 command failed, retrying (%d/%d)", retry_count, max_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "PTC3 disable command failed after %d attempts", max_retries);
        writer->puts("ERROR: PTC3 disable command failed");
        all_successful = false;
    }

    if (all_successful) {
        ESP_LOGI(TAG, "All PTC disable commands completed successfully");
        writer->puts("All PTC heaters disabled successfully");
    } else {
        ESP_LOGW(TAG, "Some PTC disable commands failed - check logs for details");
        writer->puts("WARNING: Some PTC commands failed - check logs");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDcdcEnable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    if (!rz2->m_vcan_enabled)
    {
        ESP_LOGW(TAG, "DCDC control not possible, because V-CAN is not enabled");
        writer->puts("DCDC control not possible, because V-CAN is not enabled");
        return;
    }

    rz2->EnableDCDC(true);
    ESP_LOGI(TAG, "Manual DCDC converter enabled");
    writer->puts("Manual DCDC converter enabled - sending CAN message every second");
}

void OvmsVehicleRenaultZoePh2::CommandDcdcDisable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    if (!rz2->m_vcan_enabled)
    {
        ESP_LOGW(TAG, "DCDC control not possible, because V-CAN is not enabled");
        writer->puts("DCDC control not possible, because V-CAN is not enabled");
        return;
    }

    rz2->DisableDCDC();
    ESP_LOGI(TAG, "DCDC converter disabled");
    writer->puts("DCDC converter disabled");
}
