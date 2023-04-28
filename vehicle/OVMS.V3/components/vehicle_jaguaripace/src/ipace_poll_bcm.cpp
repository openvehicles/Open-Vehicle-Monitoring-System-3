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


void OvmsVehicleJaguarIpace::IncomingBcmPoll(uint16_t pid, const uint8_t* data, uint8_t length, uint16_t remain) {
    uint8_t value8 = data[0];
    //uint16_t value16 = (data[0] << 8 | data[1]);
    //uint32_t value24 = (data[0] << 16 | data[1] << 8 | data[2]);


    ESP_LOGD(TAG, "IncomingBcmPoll, pid=%d, length=%d, remain=%d", pid, length, remain);
    switch (pid)
    {
        case vehicleSpeedPid:
            StandardMetrics.ms_v_pos_speed->SetValue(value8);
            ESP_LOGD(TAG, "Speed=%d", value8);
            break;
        // case batterySoHPid:
        //     StandardMetrics.ms_v_bat_soh->SetValue(value8 / 2.0f);
        //     ESP_LOGD(TAG, "SOH=%f", value8 / 2.0f);
        //     break;
        default:
            ESP_LOGD(TAG, "Unknown PID, pid=%d", pid);
    }
}


