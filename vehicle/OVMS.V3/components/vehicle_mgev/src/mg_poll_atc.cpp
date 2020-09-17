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
#include "mg_obd_pids.h"
#include "metrics_standard.h"

void OvmsVehicleMgEv::IncomingAtcPoll(uint16_t pid, uint8_t* data, uint8_t length)
{
    uint16_t value = (data[0] << 8 | data[1]);

    switch (pid)
    {
        case atcAmbientTempPid:
            StandardMetrics.ms_v_env_temp->SetValue(value / 10.0 - 40.0);
            break;
        case atcSetTempPid:
            StandardMetrics.ms_v_env_cabinsetpoint->SetValue(value / 10.0);
            break;
        case atcFaceOutletTempPid:
            m_env_face_outlet_temp->SetValue(value / 10.0 - 30.0);
            break;
        case atcBlowerSpeedPid:
            // TODO: ms_v_env_cabinfan [%]
            StandardMetrics.ms_v_env_hvac->SetValue(data[0] != 0 && data[1] != 0);
            break;
        case atcPtcTempPid:
            StandardMetrics.ms_v_env_cabintemp->SetValue(value / 10.0 - 40.0);
            break;
    }
}
