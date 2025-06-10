/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd July 2025
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

#include "vehicle_niu_gtevo.h"

OvmsVehicle::vehicle_command_t OvmsVehicleNiuGTEVO::CommandWakeup()
{

    uint8_t wake_cmd1[8] = {0x04, 0xf2, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t wake_cmd2[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    ESP_LOGI(TAG, "Wake up GT EVO...");

    for (int i = 0; i < 10; i++)
    {
        m_can1->WriteExtended(0x08ea110b, 8, wake_cmd2);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        m_can1->WriteExtended(0x08ea110b, 8, wake_cmd1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        wake_cmd1[0] = 0x08;
        m_can1->WriteExtended(0x08ea110b, 8, wake_cmd1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        wake_cmd1[0] = 0x18;
        m_can1->WriteExtended(0x08ea110b, 8, wake_cmd1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        wake_cmd1[0] = 0x17;
        m_can1->WriteExtended(0x08ea110b, 8, wake_cmd1);
        wake_cmd1[0] = 0x04;
    }

    return Success;
}

void OvmsVehicleNiuGTEVO::CommandDebug(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleNiuGTEVO *xnevo = (OvmsVehicleNiuGTEVO *)MyVehicleFactory.ActiveVehicle();
    float can_message_counter = xnevo->can_message_counter;
    // Debug 100km rolling consumption calculation
    float batteryConsumption = StandardMetrics.ms_v_bat_consumption->AsFloat(0, kWhP100K);
    float estimatedRange = StandardMetrics.ms_v_bat_range_est->AsFloat(0, Kilometers);
    float totalDistance = xnevo->totalDistance;
    size_t historySize = xnevo->consumptionHistory.size();

    writer->printf("CAN message count: %.0f msgs/s\n", can_message_counter);
    writer->printf("Battery Consumption: %.2f kWh/100km\n", batteryConsumption);
    writer->printf("Estimated Range: %.2f km\n", estimatedRange);
    writer->printf("Total Distance: %.2f km\n", totalDistance);
    writer->printf("Consumption history buffer size: %zu entries\n", historySize);
}

void OvmsVehicleNiuGTEVO::CommandTripReset(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleNiuGTEVO *xnevo = (OvmsVehicleNiuGTEVO *)MyVehicleFactory.ActiveVehicle();
    StandardMetrics.ms_v_pos_trip->SetValue(0);
    StandardMetrics.ms_v_bat_energy_used->SetValue(0);
    StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
    xnevo->mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers), Kilometers);

    writer->printf("Trip reset done.");
}

void OvmsVehicleNiuGTEVO::CommandChargerEnable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleNiuGTEVO *xnevo = (OvmsVehicleNiuGTEVO *)MyVehicleFactory.ActiveVehicle();

    if (StandardMetrics.ms_v_pos_speed->AsFloat(0) == 0)
    {
        if (!StandardMetrics.ms_v_env_aux12v->AsBool())
        {
            xnevo->CommandWakeup();
        }

        xnevo->chargerEmulation = true;
        writer->printf("Charger emulation enabled");
    }
    else
    {
        writer->printf("Charger emulation prohibited");
    }
}

void OvmsVehicleNiuGTEVO::CommandChargerDisable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleNiuGTEVO *xnevo = (OvmsVehicleNiuGTEVO *)MyVehicleFactory.ActiveVehicle();
    xnevo->chargerEmulation = false;
    writer->printf("Charger emulation disabled");
}

OvmsVehicle::vehicle_command_t OvmsVehicleNiuGTEVO::CommandHomelink(int button, int durationms)
{
    OvmsVehicleNiuGTEVO *xnevo = (OvmsVehicleNiuGTEVO *)MyVehicleFactory.ActiveVehicle();
    ESP_LOGI(TAG, "CommandHomelink button=%d durationms=%d", button, durationms);

    OvmsVehicle::vehicle_command_t res;
    if (button == 0)
    {
        if (StandardMetrics.ms_v_pos_speed->AsFloat(0) == 0)
        {
            xnevo->chargerEmulation = true;
            res = Success;
        }
        else
        {
            res = Fail;
        }
    }
    else if (button == 1)
    {
        xnevo->chargerEmulation = false;
        res = Success;
    }
    else
    {
        res = NotImplemented;
    }

    return res;
}