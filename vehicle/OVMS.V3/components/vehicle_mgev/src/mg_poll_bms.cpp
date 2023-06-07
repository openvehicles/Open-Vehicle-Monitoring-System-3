/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd September 2020
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
static const char *TAG = "v-mgev-bms";

#include "vehicle_mgev.h"

#include <algorithm>

namespace {

// Responses to the BMS Status PID
enum BmsStatus : unsigned char {
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

void OvmsVehicleMgEv::ProcessBatteryStats(int index, uint8_t* data, uint16_t remain)
{
    // The stats are per block rather than per cell, but we'll record them in cells.
    // Each cell consisting of 2 values. One for minimum value and the other for the maximum value.
    // This should produce meaningful results for Cell Avg, Cell Min, Cell Max, Std Dev.
    
    // Rather than cache all of the data as it's split over two frames, just cache the one byte we need.
    int i = index * 2;
    if (remain)
    {
        uint16_t vmin = (data[0] << 8 | data[1]);
        m_bmsCache = data[2];
        BmsSetCellVoltage(i, (vmin / 2000.0f) + 1.0f);
        //ESP_LOGV("BMS_CELL_VOLTAGE", "Setting Min Cell Voltage at %i to %f", index, (vmin / 2000.0f) + 1.0f);
    } else {
        uint16_t vmax = (m_bmsCache << 8 | data[0]);
        uint8_t tmin = data[1];
        uint8_t tmax = data[2];
        BmsSetCellVoltage(i + 1, (vmax / 2000.0f) + 1.0f);
        //ESP_LOGV("BMS_CELL_VOLTAGE", "Setting Max Cell Voltage at %i to %f", index * 2 + 1, (vmax / 2000.0f) + 1.0f);
        BmsSetCellTemperature(i, tmin * 0.5f - 40.0f);
        BmsSetCellTemperature(i + 1, tmax * 0.5f - 40.0f);
    }
}

void OvmsVehicleMgEv::IncomingBmsPoll(
        uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain)
{
    uint16_t value = (data[0] << 8 | data[1]);

    switch (pid)
    {
        case cell1StatPid:
            ProcessBatteryStats(0, data, remain);
            break;
        case cell2StatPid:
            ProcessBatteryStats(1, data, remain);
            break;
        case cell3StatPid:
            ProcessBatteryStats(2, data, remain);
            break;
        case cell4StatPid:
            ProcessBatteryStats(3, data, remain);
            break;
        case cell5StatPid:
            ProcessBatteryStats(4, data, remain);
            break;
        case cell6StatPid:
            ProcessBatteryStats(5, data, remain);
            break;
        case cell7StatPid:
            ProcessBatteryStats(6, data, remain);
            break;
        case cell8StatPid:
            ProcessBatteryStats(7, data, remain);
            break;
        case cell9StatPid:
            ProcessBatteryStats(8, data, remain);
            break;
        case cell10StatPid:
            ProcessBatteryStats(9, data, remain);
            break;
        case cell11StatPid:
            ProcessBatteryStats(10, data, remain);
            break;
        case cell12StatPid:
            ProcessBatteryStats(11, data, remain);
            break;
        case cell13StatPid:
            ProcessBatteryStats(12, data, remain);
            break;
        case cell14StatPid:
            ProcessBatteryStats(13, data, remain);
            break;
        case cell15StatPid:
            ProcessBatteryStats(14, data, remain);
            break;
        case cell16StatPid:
            ProcessBatteryStats(15, data, remain);
            break;
        case cell17StatPid:
            ProcessBatteryStats(16, data, remain);
            break;
        case cell18StatPid:
            ProcessBatteryStats(17, data, remain);
            break;
        case cell19StatPid:
            ProcessBatteryStats(18, data, remain);
            break;
        case cell20StatPid:
            ProcessBatteryStats(19, data, remain);
            break;
        case cell21StatPid:
            ProcessBatteryStats(20, data, remain);
            break;
        case cell22StatPid:
            ProcessBatteryStats(21, data, remain);
            break;
        case cell23StatPid:
            ProcessBatteryStats(22, data, remain);
            break;
        case cell24StatPid:
            ProcessBatteryStats(23, data, remain);
            break;
        case batteryBusVoltagePid:
        {
            float voltage = value / 4.0f;
            // Check that the bus is not turned off
            if (voltage > 1000)
            {
                ESP_LOGI("v-mgev", "Voltage greater than 1000V = %0.0f", voltage);
                voltage = m_bat_pack_voltage->AsFloat();
                ESP_LOGI("v-mgev", "Voltage set to: %0.0f", voltage);
            }
            StandardMetrics.ms_v_bat_voltage->SetValue(voltage);
            StandardMetrics.ms_v_bat_power->SetValue( (voltage * StandardMetrics.ms_v_bat_current->AsFloat()) / 1000.0f );
            break;
        }
        case batteryCurrentPid:
        {
            auto current = ((static_cast<int32_t>(value) - 40000) / 4.0f) / 10.0f;
            StandardMetrics.ms_v_bat_current->SetValue( current );
            StandardMetrics.ms_v_bat_power->SetValue( (StandardMetrics.ms_v_bat_voltage->AsFloat() * current) / 1000.0f );
            break;
        }
        case batteryVoltagePid:
            m_bat_pack_voltage->SetValue(value / 4.0f);
            break;
        case batteryResistancePid:
            m_bat_resistance->SetValue(value / 2.0f);
            break;            
        case batterySoCPid:
            {
                // Get raw value to display on Charging Metrics Page
                m_soc_raw->SetValue(value / 10.0f);
                auto scaledSoc = calculateSoc(value);
                if (StandardMetrics.ms_v_charge_inprogress->AsBool())
                {
                    if (scaledSoc < 99.5)
                    {
                        StandardMetrics.ms_v_charge_state->SetValue("charging");
                    }
                    else
                    {
                        StandardMetrics.ms_v_charge_state->SetValue("topoff");
                    }
                }
                // Save SOC for display
                StandardMetrics.ms_v_bat_soc->SetValue(scaledSoc);
                // Calculate Estimated Range
                float batTemp = StandardMetrics.ms_v_bat_temp->AsFloat();
                float effSoh = StandardMetrics.ms_v_bat_soh->AsFloat();
                // Get average trip consumption weighted by current trip consumption (25%)
                float kmPerKwh = (m_avg_consumption->AsFloat(0, KPkWh) * 3.0f + m_trip_consumption->AsFloat(0,KPkWh)) / 4.0f;
                
                if(kmPerKwh < 4.648)  kmPerKwh = 4.648; //21.5 kWh/100km
                if(kmPerKwh > 7.728) kmPerKwh = 7.728; //13 kWh/100km
                if(batTemp > 20) batTemp = 20;
                // Set full range accounting for battery State of Health (SoH)
                StdMetrics.ms_v_bat_range_full->SetValue(StandardMetrics.ms_v_bat_range_full->AsFloat() * effSoh * 0.01f, Kilometers);
                // Set battery capacity reduced by SOC and SOH
                float batteryCapacity = 42.5f * (scaledSoc * 0.01f) * (effSoh * 0.01f);
                StandardMetrics.ms_v_bat_range_est->SetValue(batteryCapacity * (kmPerKwh * (1-((20 - batTemp) * 1.3f) * 0.01f)));
                // Ideal range set to SoC percentage of WLTP Range
                StandardMetrics.ms_v_bat_range_ideal->SetValue(StdMetrics.ms_v_bat_range_full->AsFloat(0, Kilometers) * (scaledSoc * 0.01f));
            }
            break;
        case batteryErrorPid:
            m_bat_error->SetValue(data[0]);
            break;            
        case bmsStatusPid:
            SetBmsStatus(data[0]);
            break;
        case batteryCoolantTempPid:
            // Temperature is half degrees from -40C
            m_bat_coolant_temp->SetValue(data[0] * 0.5f - 40.0f);
            break;
        case batteryTempPid:
            // Temperature is half degrees from -40C
            StandardMetrics.ms_v_bat_temp->SetValue(data[0] * 0.5f - 40.0f);
            break;
        case batterySoHPid:
            StandardMetrics.ms_v_bat_soh->SetValue(value / 100.0f);
            break;
        case bmsRangePid:
            //StandardMetrics.ms_v_bat_range_est->SetValue(value / 10.0);
            break;
        case bmsMaxCellVoltagePid:
            m_bms_max_cell_voltage->SetValue(value / 1000.0f);
            break;
        case bmsMinCellVoltagePid:
            m_bms_min_cell_voltage->SetValue(value / 1000.0f);
            break;     
        case bmsTimePid:     
            // Will get answer in 2 frames. 1st frame will have year month date in data[0], data[1] and data[2], length = 3 and remain = 3.
            // 2nd frame will have hour minute second in data[0], data[1] and data[2], length 3 and remain 0
            if (remain > 0)
            {
                m_bmsTimeTemp = OvmsVehicleMgEv::IntToString(data[2], 2, "0") + "/" + OvmsVehicleMgEv::IntToString(data[1], 2, "0") + "/" + OvmsVehicleMgEv::IntToString(data[0], 2, "0") + " ";
            }
            else
            {
                m_bmsTimeTemp += OvmsVehicleMgEv::IntToString(data[0], 2, "0") + ":" + OvmsVehicleMgEv::IntToString(data[1], 2, "0") + ":" + OvmsVehicleMgEv::IntToString(data[2], 2, "0");
                m_bms_time->SetValue(m_bmsTimeTemp);
            }
            break;    
        case bmsSystemMainRelayBPid:
            m_bms_main_relay_b->SetValue(data[0]);
            break;
        case bmsSystemMainRelayGPid:
            m_bms_main_relay_g->SetValue(data[0]);
            break;
        case bmsSystemMainRelayPPid:     
            m_bms_main_relay_p->SetValue(data[0]);                          
            break;            
    }
}

void OvmsVehicleMgEv::SetBmsStatus(uint8_t status)
{
    switch (status) {
        case StartingCharge:
        case Charging:
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            StandardMetrics.ms_v_charge_type->SetValue("type2");
            break;
        case CcsCharging:
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            StandardMetrics.ms_v_charge_type->SetValue("ccs");
            //These are normally set in mg_poll_evcc.cpp but while CCS charging, EVCC won't show up so we set these here
            StandardMetrics.ms_v_charge_current->SetValue(-StandardMetrics.ms_v_bat_current->AsFloat());
            StandardMetrics.ms_v_charge_power->SetValue(-StandardMetrics.ms_v_bat_power->AsFloat());
            // Need to fix this as value varies for model
            StandardMetrics.ms_v_charge_climit->SetValue(82);
            StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
            break;
        default:
            if (StandardMetrics.ms_v_charge_inprogress->AsBool())
            {
                StandardMetrics.ms_v_charge_type->SetValue("undefined");
                StandardMetrics.ms_v_charge_inprogress->SetValue(false);
                //Set to 99.9 instead of 100 incase of mathematical errors
                if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 99.9)
                {
                    StandardMetrics.ms_v_charge_state->SetValue("done");
                }
                else
                {
                    StandardMetrics.ms_v_charge_state->SetValue("stopped");
                }
            } 
            break;
    }
}

float OvmsVehicleMgEv::calculateSoc(uint16_t value)
{
    float lowerlimit = MyConfig.GetParamValueInt("xmg","bms.dod.lower");
    float upperlimit = MyConfig.GetParamValueInt("xmg","bms.dod.upper");
    ESP_LOGD(TAG, "BMS Limits: Lower = %f Upper = %f",lowerlimit,upperlimit);
    // Calculate SOC from upper and lower limits
    return (value - lowerlimit) * 100.0f / (upperlimit - lowerlimit);
}
