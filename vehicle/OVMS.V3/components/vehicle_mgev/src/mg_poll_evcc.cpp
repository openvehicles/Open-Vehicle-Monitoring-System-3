/*
;    Project:       Open Vehicle Monitor System
;    Date:          26th September 2020
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

void OvmsVehicleMgEv::IncomingEvccPoll(uint16_t pid, uint8_t* data, uint8_t length)
{
    uint16_t value = (data[0] << 8 | data[1]);

    switch (pid)
    {
        case evccAmperagePid:
            StandardMetrics.ms_v_charge_current->SetValue(value / 10.0f);
            StandardMetrics.ms_v_charge_power->SetValue(
                StandardMetrics.ms_v_charge_voltage->AsFloat() * (value / 10.0f) / 1000.0f
            );
            break;
        case evccMaxAmperagePid:
            StandardMetrics.ms_v_charge_climit->SetValue(data[0] / 4.0f);
            break;
        case evccVoltagePid:
            StandardMetrics.ms_v_charge_voltage->SetValue(value / 100.0f);
            break;
    }
}
