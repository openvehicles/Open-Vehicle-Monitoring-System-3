/*
;    Project:       Open Vehicle Monitor System
;    Date:          1st April 2021
;
;    Changes:
;    0.0.1  Initial release
;
;    (C) 2021       Didier Ernotte
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

#include "vehicle_jaguaripace.h"
#include "ipace_obd_pids.h"
#include "metrics_standard.h"

#include <algorithm>
static const char *TAG = "v-jaguaripace";

namespace {

// Responses to the BECM Status PID
enum BecmStatus : unsigned char {
    ConnectedNotCharging1 = 0x0,  // Seen when connected but not locked
    Idle = 0x1,  // When the car does not have the ignition on
    Running = 0x3,  // When the ignition is on aux or running
    Charging = 0x6,  // When charging normally
    CcsCharging = 0x7,  // When charging on a rapid CCS charger
    AboutToSleep = 0x8,  // Seen just before going to sleep
    Connected = 0xa,  // Connected but not charging
    StartingCharge = 0xc  // Seen when the charge was about to start
};

}  // anon namespace


void OvmsVehicleJaguarIpace::IncomingBecmPoll(uint16_t pid, const uint8_t* data, uint8_t length, uint16_t remain) {
    uint8_t value8 = data[0];
    uint16_t value16 = (data[0] << 8 | data[1]);
    uint32_t value24 = (data[0] << 16 | data[1] << 8 | data[2]);
    float power = 0.0f;
    float volt = 0.0f;
    float current = 0.0f;

    ESP_LOGD(TAG, "IncomingBecmPoll, pid=%d, length=%d, remain=%d", pid, length, remain);
    switch (pid)
    {
        case batterySoCPid:
            StandardMetrics.ms_v_bat_soc->SetValue(value16 / 100.0f);
            ESP_LOGD(TAG, "SOC=%f", value16 / 100.0f);
            break;
        case batterySoHCapacityPid:
            StandardMetrics.ms_v_bat_soh->SetValue(value8 / 2.0f);
            ESP_LOGD(TAG, "SOH Capacity Avg=%f", value8 / 2.0f);
            break;
        case batterySoHCapacityMinPid:
            xji_v_bat_capacity_soh_min->SetValue(value8 / 2.0f);
            ESP_LOGD(TAG, "SOH Capacity Min=%f", value8 / 2.0f);
            break;
        case batterySoHCapacityMaxPid:
            xji_v_bat_capacity_soh_max->SetValue(value8 / 2.0f);
            ESP_LOGD(TAG, "SOH Capacity Max=%f", value8 / 2.0f);
            break;
        case batterySoHPowerPid:
            xji_v_bat_power_soh->SetValue(value8 / 2.0f);
            ESP_LOGD(TAG, "SOH Capacity Avg=%f", value8 / 2.0f);
            break;
        case batterySoHPowerMinPid:
            xji_v_bat_power_soh_min->SetValue(value8 / 2.0f);
            ESP_LOGD(TAG, "SOH Capacity Min=%f", value8 / 2.0f);
            break;
        case batterySoHPowerMaxPid:
            xji_v_bat_power_soh_max->SetValue(value8 / 2.0f);
            ESP_LOGD(TAG, "SOH Capacity Max=%f", value8 / 2.0f);
            break;

        case batteryCellMinVoltPid:
            StandardMetrics.ms_v_bat_pack_vmin->SetValue(value16 / 1000.0f);
            ESP_LOGD(TAG, "Cell Volt Min=%f", value16 / 1000.0f);
            break;
        case batteryCellMaxVoltPid:
            StandardMetrics.ms_v_bat_pack_vmax->SetValue(value16 / 1000.0f);
            ESP_LOGD(TAG, "Cell Volt Max=%f", value16 / 1000.0f);
            break;
        case batteryVoltPid:
            volt = value16 / 100.0f;
            StandardMetrics.ms_v_bat_voltage->SetValue(volt);
            ESP_LOGD(TAG, "Battery Volt=%f", volt);
            power = StandardMetrics.ms_v_bat_current->AsFloat() * volt;
            power = power / 1000;
            StandardMetrics.ms_v_bat_power->SetValue( power );
            ESP_LOGD(TAG, "Power=%f", power);
            break;
        case batteryCurrentPid:
            current = (value16 - 0x8000u) / 40.0f;
            StandardMetrics.ms_v_bat_current->SetValue( current );
            ESP_LOGD(TAG, "Battery Current=%f", current);
            power = StandardMetrics.ms_v_bat_voltage->AsFloat() * current;
            power = power / 1000;
            StandardMetrics.ms_v_bat_power->SetValue( power );
            ESP_LOGD(TAG, "Power=%f", power);
            break;
        case batteryTempMinPid:
            StandardMetrics.ms_v_bat_pack_tmin->SetValue(value8 / 2.0f -40.0f);
            ESP_LOGD(TAG, "Battery temp min=%f", value8 / 2.0f -40.0f);
            break;
        case batteryTempMaxPid:
            StandardMetrics.ms_v_bat_pack_tmax->SetValue(value8 / 2.0f -40.0f);
            ESP_LOGD(TAG, "Battery temp max=%f", value8 / 2.0f -40.0f);
            break;
        case batteryTempAvgPid:
            StandardMetrics.ms_v_bat_pack_tavg->SetValue(value8 / 2.0f -40.0f);
            ESP_LOGD(TAG, "Battery temp avg=%f", value8 / 2.0f -40.0f);
            break;
        case batteryMaxRegenPid:
            xji_v_bat_max_regen->SetValue(value16 / 100.0f);
            ESP_LOGD(TAG, "Max Regen=%f", value16 / 100.0f);
            break;
        case odometerPid:
            StandardMetrics.ms_v_pos_odometer->SetValue(value24);
            ESP_LOGD(TAG, "Odometer=%" PRId32, value24);
            break;
        case cabinTempPid:
            StandardMetrics.ms_v_env_cabintemp->SetValue(value8 - 40.0f);
            ESP_LOGD(TAG, "Cabin temp=%f", value8 - 40.0f);
            break;
        case vinPid:
            if (remain == 21) {
                memcpy(m_vin,data,length);
            } else {
                if (remain == 14) {
                    memcpy(m_vin+3,data,length);
                } else {
                    if (remain == 7) {
                        memcpy(m_vin+10,data,length);
                        StandardMetrics.ms_v_vin->SetValue(m_vin);
                        ESP_LOGD(TAG, "Vin=%s", m_vin);
                    } 
                }
            }
            break;
        default:
            ESP_LOGD(TAG, "Unknown PID, pid=%d", pid);
    }
}


