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

void OvmsVehicleMgEv::IncomingBmsPoll(uint16_t pid, uint8_t* data, uint8_t length)
{
    uint16_t value = (data[0] << 8 | data[1]);

    switch (pid)
    {
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
                // SoC is 3% - 97%, so we need to scale it
                StandardMetrics.ms_v_bat_soc->SetValue(((soc * 103.0) / 97.0) - 3.0);
            }
            break;
        case bmsStatusPid:
            SetBmsStatus(data[0]);
            break;
        case batteryCellMaxVPid:
            // In data[0] -> data[2] where 0x0ff6ff = 4.09V, no other values known yet
            //StandardMetrics.ms_v_bat_pack_vmax->SetValue();
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
