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

namespace {

// The bitmasks for the doors being open on the BCM Door PID
enum DoorMasks : unsigned char {
    Driver = 1,
    Passenger = 2,
    RearLeft = 4,
    RearRight = 8,
    Bonnet = 16,
    Boot = 32,
    Locked = 128
};

}  // anon namespace

void OvmsVehicleMgEv::IncomingBcmPoll(uint16_t pid, uint8_t* data, uint8_t length)
{
    switch (pid)
    {
        case bcmDoorPid:
            StandardMetrics.ms_v_door_fl->SetValue(data[0] & Passenger);
            StandardMetrics.ms_v_door_fr->SetValue(data[0] & Driver);
            StandardMetrics.ms_v_door_rl->SetValue(data[0] & RearLeft);
            StandardMetrics.ms_v_door_rr->SetValue(data[0] & RearRight);
            StandardMetrics.ms_v_door_trunk->SetValue(data[0] & Boot);
            break;
        case bcmLightPid:
            StandardMetrics.ms_v_env_headlights->SetValue(data[0] > 1);
            break;
    }    
}
