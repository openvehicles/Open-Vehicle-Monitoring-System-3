/*
;    Project:       Open Vehicle Monitor System
;    Module:        Renault Zoe Ph2 - Shell Commands
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
    const int max_attempts = 2;
    const TickType_t wait_ms = 300; // allow one V-CAN cycle to reflect state
    bool lights_on = false;
    bool tx_success = false;

    // Send, then stop as soon as BCM reports the lowbeam bit
    for (int i = 0; i < max_attempts && !lights_on; i++)
    {
        // Clear success flag before sending
        rz2->m_tx_success_flag.store(false);

        // Send the frame
        rz2->m_can1->WriteStandard(HFM_A1_ID, 8, lighting_cmd);

        // Wait for transmission and for BCM to reflect new state
        vTaskDelay(wait_ms / portTICK_PERIOD_MS);

        // Update flags
        tx_success = rz2->m_tx_success_flag.load() && rz2->m_tx_success_id.load() == HFM_A1_ID;
        lights_on = rz2->vcan_light_lowbeam;

        if (tx_success)
        {
            ESP_LOGI(TAG, "Lighting command transmitted successfully on attempt %d", i + 1);
        }
        else
        {
            ESP_LOGW(TAG, "Lighting command transmission failed (attempt %d/%d)", i + 1, max_attempts);
        }
    }

    if (lights_on)
    {
        ESP_LOGI(TAG, "Lighting command completed - headlights on (confirmed by BCM feedback)");
        writer->puts("Lighting command sent - headlights turned on");
    }
    else if (tx_success)
    {
        ESP_LOGW(TAG, "Lighting command sent but BCM feedback not confirmed (headlights may be on for ~30 seconds)");
        writer->puts("Lighting command sent - headlights on for ~30 seconds (BCM feedback pending)");
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

#ifdef RZ2_DEBUG_BUILD
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
    writer->printf("LongPreCond waiting for HVAC: %s\n", rz2->LongPreCondWaitingForHvacOff ? "yes" : "no");

    // DCDC debug information
    writer->printf("\n--- DCDC Debug Info ---\n");
    writer->printf("DCDC active: %s\n", rz2->m_dcdc_active ? "yes" : "no");
    writer->printf("DCDC mode: %s\n",
                   rz2->m_dcdc_manual_enabled ? "manual" :
                   (rz2->m_dcdc_after_ignition_active ? "after-ignition" : "auto"));

    if (rz2->m_dcdc_active)
    {
        uint32_t runtime_seconds = 0;
        if (rz2->m_dcdc_start_time > 0)
        {
            runtime_seconds = (xTaskGetTickCount() * portTICK_PERIOD_MS - rz2->m_dcdc_start_time) / 1000;
        }
        writer->printf("DCDC runtime: %u seconds (%u:%02u)\n",
                       runtime_seconds, runtime_seconds / 60, runtime_seconds % 60);
    }
    else
    {
        writer->printf("DCDC runtime: not active\n");
    }

    writer->printf("Auto 12V recharge enabled: %s\n", rz2->m_auto_12v_recharge_enabled ? "yes" : "no");
    writer->printf("Auto 12V threshold: %.2fV\n", rz2->m_auto_12v_threshold);
    writer->printf("DCDC SOC limit: %d%%\n", rz2->m_dcdc_soc_limit);
    if (rz2->m_dcdc_time_limit == 0) {
        writer->printf("DCDC time limit: infinite\n");
    } else {
        writer->printf("DCDC time limit: %d min\n", rz2->m_dcdc_time_limit);
    }
    writer->printf("After-ignition time: %d min\n", rz2->m_dcdc_after_ignition_time);
    writer->printf("After-ignition active: %s\n", rz2->m_dcdc_after_ignition_active ? "yes" : "no");

    writer->printf("\n--- Current Vehicle State ---\n");
    writer->printf("Ignition: %s\n", StandardMetrics.ms_v_env_on->AsBool() ? "ON" : "OFF");
    writer->printf("Charging: %s\n", StandardMetrics.ms_v_charge_inprogress->AsBool() ? "yes" : "no");
    writer->printf("SOC: %.1f%%\n", StandardMetrics.ms_v_bat_soc->AsFloat(0));
    writer->printf("12V voltage: %.2fV\n", StandardMetrics.ms_v_bat_12v_voltage->AsFloat(0));
    writer->printf("12V current: %.2fA\n", StandardMetrics.ms_v_bat_12v_current->AsFloat(0));
}
#endif // RZ2_DEBUG_BUILD

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

void OvmsVehicleRenaultZoePh2::CommandDcdcEnable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    if (!rz2->m_vcan_enabled)
    {
        ESP_LOGW(TAG, "DCDC control not possible, because V-CAN is not enabled");
        writer->puts("DCDC control not possible, because V-CAN is not enabled");
        return;
    }

    if (rz2->EnableDCDC(true))
    {
        ESP_LOGI(TAG, "Manual DCDC converter enabled");
        writer->puts("Manual DCDC converter enabled - sending CAN message every second");
    }
    else
    {
        float current_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
        writer->printf("ERROR: Cannot enable DCDC - traction battery SOC %.0f%% is at or below limit %d%%\n",
                      current_soc, rz2->m_dcdc_soc_limit);
        writer->printf("DCDC would drain the traction battery too low. Charge the car or lower the limit.\n");
        writer->printf("Use 'xrz2 dcdc soclimit <percent>' to adjust the limit if needed\n");
    }
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
