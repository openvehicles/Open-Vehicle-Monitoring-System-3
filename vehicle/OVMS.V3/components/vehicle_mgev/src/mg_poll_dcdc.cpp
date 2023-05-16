/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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

void OvmsVehicleMgEv::IncomingDcdcPoll(uint16_t pid, const uint8_t* data, uint8_t length)
{
    uint16_t value = (data[0] << 8 | data[1]);

    switch (pid)
    {
        // case dcdcLvCurrentPid:
        //     // A massive guess that is probably very wrong
        //     //16-04-21 Ohm: Confirmed incorrect. 5-6A corresponds to 00 00 to 00 01. Negative 5-6A corresponds to 00 0d to 00 0f. Seems to suggest that if value is negative, 12V battery is charging. But can also use vcuHvContactorPid to determine if 12V battery is charging.
        //     StandardMetrics.ms_v_env_charging12v->SetValue(data[1] & 0x08);
        //     StandardMetrics.ms_v_bat_12v_current->SetValue(value + 1);
        //     break;
        case dcdcPowerLoadPid:
            m_dcdc_load->SetValue(value * 0.25);
            break;
        case dcdcTemperaturePid:
            StandardMetrics.ms_v_inv_temp->SetValue(value - 40);
            break;
    }
}
