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
#include "mg_obd_pids.h"
#include "metrics_standard.h"

#include <algorithm>

namespace {

// Responses to the BMS Status PID
enum BmsStatus : unsigned char {
    ConnectedNotCharging1 = 0x0,  // Seen when connected but not locked
    Idle = 0x1,  // When the car does not have the ignition on
    Running = 0x3,  // When the ignition is on aux or running
    Charging = 0x6,  // When charging normally
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
    if (remain)
    {
        uint16_t vmin = (data[0] << 8 | data[1]);
        m_bmsCache = data[2];

        StandardMetrics.ms_v_bat_cell_vmin->SetElemValue(index, vmin / 1475.0f);
        
        {
            auto pvmin = StandardMetrics.ms_v_bat_cell_vmin->AsVector();
            StandardMetrics.ms_v_bat_pack_vmin->SetValue(
                *std::min_element(pvmin.begin(), pvmin.end())
            );
        }
    }
    else
    {
        uint16_t vmax = (m_bmsCache << 8 | data[0]);
        uint8_t tmin = data[1];
        uint8_t tmax = data[2];
        //uint8_t tpcb = data[3]; tpcb / 2.0 - 40.0;

        StandardMetrics.ms_v_bat_cell_vmax->SetElemValue(index, vmax / 1475.0f);
        StandardMetrics.ms_v_bat_cell_tmin->SetElemValue(index, tmin * 0.5f - 40.0f);
        StandardMetrics.ms_v_bat_cell_tmax->SetElemValue(index, tmax * 0.5f - 40.0f);

        {
            auto pvmax = StandardMetrics.ms_v_bat_cell_vmax->AsVector();
            StandardMetrics.ms_v_bat_pack_vmax->SetValue(
                *std::max_element(pvmax.begin(), pvmax.end())
            );
        }
        {
            auto ptmin = StandardMetrics.ms_v_bat_cell_tmin->AsVector();
            StandardMetrics.ms_v_bat_pack_tmin->SetValue(
                *std::min_element(ptmin.begin(), ptmin.end())
            );
        }
        {
            auto ptmax = StandardMetrics.ms_v_bat_cell_tmax->AsVector();
            StandardMetrics.ms_v_bat_pack_tmax->SetValue(
                *std::max_element(ptmax.begin(), ptmax.end())
            );
        }
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
            // Check that the bus is not turned off
            if (value != 0xfffeu)
            {
                StandardMetrics.ms_v_bat_voltage->SetValue(value * 0.25);
            }
            else
            {
                StandardMetrics.ms_v_bat_voltage->SetValue(m_bat_pack_voltage->AsFloat());
            }
            break;
        case batteryCurrentPid:
            StandardMetrics.ms_v_bat_current->SetValue(
                ((static_cast<int32_t>(value) - 40000) * 0.25) / 10.0
            );
            break;
        case batteryVoltagePid:
            m_bat_pack_voltage->SetValue(value * 0.25);
            break;
        case batterySoCPid:
            {
                auto soc = value / 10.0;
                if (StandardMetrics.ms_v_charge_inprogress->AsBool())
                {
                    if (soc < 96.5)
                    {
                        StandardMetrics.ms_v_charge_state->SetValue("charging");
                    }
                    else
                    {
                        StandardMetrics.ms_v_charge_state->SetValue("topoff");
                    }
                }
                // SoC is 7% - 97%, so we need to scale it
                StandardMetrics.ms_v_bat_soc->SetValue(((soc * 107.0) / 97.0) - 7.0);
            }
            break;
        case bmsStatusPid:
            SetBmsStatus(data[0]);
            break;
        case batteryCoolantTempPid:
            // Temperature is half degrees from -40C
            StandardMetrics.ms_v_bat_temp->SetValue(data[0] * 0.5 - 40.0);
            break;
        case batterySoHPid:
            StandardMetrics.ms_v_bat_soh->SetValue(value / 100.0);
            break;
        case chargeRatePid:
            // The kW of the charger, crude way to determine the charge type
            {
                auto rate = value / 10.0;
                if (rate < 0.1)
                {
                    StandardMetrics.ms_v_charge_type->SetValue("undefined");
                }
                else if (rate > 7.0)
                {
                    StandardMetrics.ms_v_charge_type->SetValue("ccs");
                }
                else
                {
                    StandardMetrics.ms_v_charge_type->SetValue("type2");
                }
            }
            break;
        case bmsRangePid:
            StandardMetrics.ms_v_bat_range_est->SetValue(value / 10.0);
            break;
    }
}

void OvmsVehicleMgEv::SetBmsStatus(uint8_t status)
{
    switch (status) {
        case StartingCharge:
        case Charging:
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            break;
        default:
            if (StandardMetrics.ms_v_charge_inprogress->AsBool())
            {
                if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 97.0)
                {
                    StandardMetrics.ms_v_charge_state->SetValue("done");
                }
                else
                {
                    StandardMetrics.ms_v_charge_state->SetValue("stopped");
                }
            }
            StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            break;
    }
}
