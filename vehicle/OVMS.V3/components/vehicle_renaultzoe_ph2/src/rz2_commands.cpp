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

// Useful commands which sends CAN messages are ClimateControl, Wakeup and DDT commands. The other is actual a playground

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

            for (int i = 0; i < 11; i++)
            {
                m_can1->WriteStandard(0x46F, 8, data);
                vTaskDelay(250 / portTICK_PERIOD_MS);
                ESP_LOGI(TAG, "Send wake-up and pre-heat command to V-CAN (%d/10) ...", i);
            }

            // We know the car will wake up and switch on DC-DC converter, so we can start polling
            if (!rz2->mt_bus_awake->AsBool())
            {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                if (rz2->m_vcan_enabled == false)
                {
                    rz2->ZoeWakeUp();
                }
            }

            // Wait one second for hvac ecu to switch on compressor or ptc and set hvac to on for user experience directly
            vTaskDelay(1000 / portTICK_PERIOD_MS);
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
        // Sending idle HFM commands, HFM, BCM and EVC are waking up for abount 20 secs wihout DC-DC, enough to get all metrics
        uint8_t wake_cmd1[8] = {0x64, 0x78, 0x60, 0x00, 0x48, 0x84, 0x07, 0x10};
        uint8_t wake_cmd2[8] = {0x74, 0x47, 0x95, 0x00, 0x48, 0x84, 0x07, 0x10};
        uint8_t wake_cmd3[8] = {0x84, 0x47, 0x95, 0x00, 0x48, 0x84, 0x07, 0x10};
        uint8_t wake_cmd4[8] = {0x94, 0x90, 0x9A, 0x00, 0x44, 0x84, 0x07, 0x10};

        ESP_LOGI(TAG, "Wake up V-CAN...");

        for (int i = 0; i < 3; i++)
        {
            m_can1->WriteStandard(0x46f, 8, wake_cmd1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 3; i++)
        {
            m_can1->WriteStandard(0x46f, 8, wake_cmd2);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 3; i++)
        {
            m_can1->WriteStandard(0x46f, 8, wake_cmd3);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 3; i++)
        {
            m_can1->WriteStandard(0x46f, 8, wake_cmd4);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        return Success;
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandLock(const char *pin)
{
    if (!m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandLock not possible, because V-CAN is not enabled");
        MyNotify.NotifyString("error", "lock.control", "V-CAN is not enabled");
        return Fail;
    }
    else
    {
        uint8_t lock[8] = {0x44, 0xE3, 0x4F, 0x10, 0x04, 0x84, 0x07, 0x10};

        if (!mt_bus_awake->AsBool())
        {
            for (int i = 0; i < 5; i++)
            {
                m_can1->WriteStandard(0x46f, 8, lock);
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
        }
        else
        {
            m_can1->WriteStandard(0x46f, 8, lock);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            m_can1->WriteStandard(0x46f, 8, lock);
        }

        return Success;
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandUnlock(const char *pin)
{
    if (!m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandUnlock not possible, because V-CAN is not enabled");
        MyNotify.NotifyString("error", "lock.control", "V-CAN is not enabled");
        return Fail;
    }
    else
    {
        uint8_t unlock[8] = {0x44, 0xE3, 0x4F, 0x18, 0x04, 0x84, 0x07, 0x10};

        if (!mt_bus_awake->AsBool())
        {
            for (int i = 0; i < 5; i++)
            {
                m_can1->WriteStandard(0x46f, 8, unlock);
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
        }
        else
        {
            m_can1->WriteStandard(0x46f, 8, unlock);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            m_can1->WriteStandard(0x46f, 8, unlock);
        }

        return Success;
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

void OvmsVehicleRenaultZoePh2::CommandUnlockTrunk(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (!rz2->m_vcan_enabled)
    {
        ESP_LOGW(TAG, "CommandUnlock not possible, because V-CAN is not enabled");
        MyNotify.NotifyString("error", "lock.control", "V-CAN is not enabled");
    }
    else
    {
        uint8_t unlock[8] = {0x44, 0xE3, 0x4F, 0x20, 0x04, 0x84, 0x07, 0x10};

        if (!rz2->mt_bus_awake->AsBool())
        {
            for (int i = 0; i < 5; i++)
            {
                rz2->m_can1->WriteStandard(0x46f, 8, unlock);
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
        }
        else
        {
            rz2->m_can1->WriteStandard(0x46f, 8, unlock);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            rz2->m_can1->WriteStandard(0x46f, 8, unlock);
        }
    }

    ESP_LOGI(TAG, "Send Trunk unlock command");
    writer->puts("Send Trunk unlock command");
}

void OvmsVehicleRenaultZoePh2::CommandUnlockChargeport(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    // This commands are special, not working everytime even if prerequisites are met, look further into it and how to start a charge if stopped
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    uint8_t ext_session[8] = {0x02, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t open_chgport_cmd1[8] = {0x05, 0x2F, 0x2B, 0x47, 0x03, 0x01, 0x00, 0x00};
    uint8_t open_chgport_cmd2[8] = {0x05, 0x2F, 0x2B, 0x47, 0x03, 0x00, 0x00, 0x00};
    uint8_t open_chgport_cmd3[8] = {0x05, 0x2F, 0x2B, 0x47, 0x00, 0x00, 0x00, 0x00};
    uint8_t abort_charge_cmd1[8] = {0x05, 0x2F, 0x2B, 0x48, 0x03, 0x01, 0x00, 0x00};
    uint8_t abort_charge_cmd2[8] = {0x05, 0x2F, 0x2B, 0x48, 0x03, 0x00, 0x00, 0x00};
    uint8_t abort_charge_cmd3[8] = {0x05, 0x2F, 0x2B, 0x48, 0x00, 0x00, 0x00, 0x00};

    // Init sequence
    if (!rz2->mt_bus_awake->AsBool())
    {
        rz2->CommandWakeup();
    }

    rz2->m_can1->WriteExtended(0x18DADAF1, 8, ext_session);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    if (StandardMetrics.ms_v_charge_pilot->AsBool())
    {
        rz2->m_can1->WriteExtended(0x18DADAF1, 8, abort_charge_cmd1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        rz2->m_can1->WriteExtended(0x18DADAF1, 8, abort_charge_cmd2);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        rz2->m_can1->WriteExtended(0x18DADAF1, 8, abort_charge_cmd3);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    rz2->m_can1->WriteExtended(0x18DADAF1, 8, open_chgport_cmd1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    rz2->m_can1->WriteExtended(0x18DADAF1, 8, open_chgport_cmd2);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    rz2->m_can1->WriteExtended(0x18DADAF1, 8, open_chgport_cmd3);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Send Chargeport open command");
    writer->puts("Send Chargeport open command");
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
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    uint8_t protocol = ISOTP_STD;
    int timeout_ms = 250;
    std::string response;
    canbus *can1 = rz2->m_can1;

    if (ext_frame)
    {
        protocol = ISOTP_EXTADR;
    }

    int err = rz2->PollSingleRequest(can1, txid, rxid, request, response, timeout_ms, protocol);

    if (err == POLLSINGLE_TXFAILURE)
    {
        ESP_LOGE(TAG, "ERROR: transmission failure (CAN bus error)");
        return Fail;
    }
    else if (err < 0)
    {
        ESP_LOGE(TAG, "ERROR: timeout waiting for poller/response");
        return Fail;
    }
    else if (err)
    {
        ESP_LOGE(TAG, "ERROR: request failed with response error code %02X\n", err);
        return Fail;
    }
    else
    {
        return Success;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacEnableCompressor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (StandardMetrics.ms_v_env_awake->AsBool() && StandardMetrics.ms_v_pos_speed->AsFloat() == 0)
    {
        ESP_LOGI(TAG, "Try to send HVAC compressor enable command");
        writer->puts("Try to send HVAC compressor enable command");

        // Extended session
        rz2->CommandDDT(0x744, 0x764, false, hexdecode("1003"));

        // Enable hot loop
        rz2->CommandDDT(0x744, 0x764, false, hexdecode("2E422402"));

        // Enable cold loop
        rz2->CommandDDT(0x744, 0x764, false, hexdecode("2E421F06"));

        // Reset ECU
        rz2->CommandDDT(0x744, 0x764, false, hexdecode("1101"));
    }
    else
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacDisableCompressor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (StandardMetrics.ms_v_env_awake->AsBool() && StandardMetrics.ms_v_pos_speed->AsFloat() == 0)
    {
        ESP_LOGI(TAG, "Try to send HVAC compressor disable command");
        writer->puts("Try to send HVAC compressor disable command");

        // Extended session
        rz2->CommandDDT(0x744, 0x764, false, hexdecode("1003"));

        // Disable hot loop
        rz2->CommandDDT(0x744, 0x764, false, hexdecode("2E422401"));

        // Disable cold loop
        rz2->CommandDDT(0x744, 0x764, false, hexdecode("2E421F01"));

        // Reset ECU
        rz2->CommandDDT(0x744, 0x764, false, hexdecode("1101"));
    }
    else
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
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
        rz2->CommandDDT(0x743, 0x763, false, hexdecode("1003"));

        // Reset command
        rz2->CommandDDT(0x743, 0x763, false, hexdecode("3101500500"));
        rz2->CommandDDT(0x743, 0x763, false, hexdecode("3101500200"));
    }
    else
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHEVCResetWaterPumpCounter(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (StandardMetrics.ms_v_env_awake->AsBool() && StandardMetrics.ms_v_pos_speed->AsFloat() == 0)
    {
        ESP_LOGI(TAG, "Try to send HEVC Water pump counter reset command");
        writer->puts("Try to send HEVC Water pump counter reset command");

        // Extended session
        rz2->CommandDDT(0x18DADAF1, 0x18DAF1DA, true, hexdecode("1003"));

        // Reset command
        rz2->CommandDDT(0x18DADAF1, 0x18DAF1DA, true, hexdecode("2E2B6100000000"));
    }
    else
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
    }
}