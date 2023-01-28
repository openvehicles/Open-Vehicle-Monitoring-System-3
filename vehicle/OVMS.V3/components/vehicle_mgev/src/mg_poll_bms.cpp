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
    // The stats are per block rather than per cell, but we'll record them in cells
    // Rather than cache all of the data as it's split over two frames, just cache the one
    // byte that we need
    int i = index * 2;
    if (remain)
    {
        uint16_t vmin = (data[0] << 8 | data[1]);
        m_bmsCache = data[2];
        BmsSetCellVoltage(i, (vmin / 2000.0f) + 1.0f);
        //ESP_LOGI("BMS_CELL_VOLTAGE", "Setting Min Cell Voltage at %i to %f", index, (vmin / 2000.0f) + 1.0f);
    } else {
        uint16_t vmax = (m_bmsCache << 8 | data[0]);
        uint8_t tmin = data[1];
        uint8_t tmax = data[2];
        BmsSetCellVoltage(i + 1, (vmax / 2000.0f) + 1.0f);
        //ESP_LOGI("BMS_CELL_VOLTAGE", "Setting Max Cell Voltage at %i to %f", index * 2 + 1, (vmax / 2000.0f) + 1.0f);
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
        case batteryBusVoltagePid:
        {
            float voltage;
            // Check that the bus is not turned off
            if (value != 0xfffeu)
            {
                voltage = value * 0.25;
            }
            else
            {
                voltage = m_bat_pack_voltage->AsFloat();
            }
            StandardMetrics.ms_v_bat_voltage->SetValue(voltage);
            StandardMetrics.ms_v_bat_power->SetValue( (voltage * StandardMetrics.ms_v_bat_current->AsFloat()) / 1000 );               
            break;
        }
        case batteryCurrentPid:
        {
            auto current = ((static_cast<int32_t>(value) - 40000) * 0.25) / 10.0;
            StandardMetrics.ms_v_bat_current->SetValue( current );
            StandardMetrics.ms_v_bat_power->SetValue( (StandardMetrics.ms_v_bat_voltage->AsFloat() * current) / 1000 );
            break;
        }
        case batteryVoltagePid:
            m_bat_pack_voltage->SetValue(value * 0.25);
            break;
        case batteryResistancePid:
            m_bat_resistance->SetValue(value / 2.0);
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
                // Ideal range set to SoC percentage of WLTP Range
                StandardMetrics.ms_v_bat_range_ideal->SetValue(WLTP_RANGE * (scaledSoc / 100));
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
            m_bat_coolant_temp->SetValue(data[0] * 0.5 - 40.0);
            break;
        case batteryTempPid:
            // Temperature is half degrees from -40C
            StandardMetrics.ms_v_bat_temp->SetValue(data[0] * 0.5 - 40.0);
            break;
        case batterySoHPid:
            StandardMetrics.ms_v_bat_soh->SetValue(value / 100.0);
            break;
        case bmsRangePid:
            StandardMetrics.ms_v_bat_range_est->SetValue(value / 10.0);
            break;
        case bmsMaxCellVoltagePid:
            m_bms_max_cell_voltage->SetValue(value / 1000.0);
            break;
        case bmsMinCellVoltagePid:
            m_bms_min_cell_voltage->SetValue(value / 1000.0);
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
            StandardMetrics.ms_v_charge_climit->SetValue(82);
            StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
            break;
        default:
            if (StandardMetrics.ms_v_charge_inprogress->AsBool())
            {
                StandardMetrics.ms_v_charge_type->SetValue("undefined");
                StandardMetrics.ms_v_charge_inprogress->SetValue(false);
                if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 99.9) //Set to 99.9 instead of 100 incase of mathematical errors
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
    int BMSVersion = MyConfig.GetParamValueInt("xmg", "bms.version", DEFAULT_BMS_VERSION);
    float lowerlimit = BMSDoDLimits[BMSVersion].Lower*10;
    float upperlimit = BMSDoDLimits[BMSVersion].Upper*10;
    
    // Calculate SOC from upper and lower limits
    return (value - lowerlimit) * 100.0f / (upperlimit - lowerlimit);
}
