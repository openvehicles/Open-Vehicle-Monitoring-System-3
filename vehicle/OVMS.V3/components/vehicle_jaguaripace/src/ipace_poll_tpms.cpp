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


void OvmsVehicleJaguarIpace::IncomingTpmsPoll(uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
    uint8_t value8 = data[0];
    uint16_t value16 = (data[0] << 8 | data[1]);

    ESP_LOGD(TAG, "IncomingTpmsPoll, pid=%d, length=%d, remain=%d", pid, length, remain);
    switch (pid)
    {
        case frontLeftTirePressurePid:
            StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, value16); 
            ESP_LOGD(TAG, "Tire pressure Front left=%d", value16);
            break;
        case frontRightTirePressurePid:
            StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, value16);
            ESP_LOGD(TAG, "Tire pressure Front Right=%d", value16);
            break;
        case rearLeftTirePressurePid:
            StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, value16);
            ESP_LOGD(TAG, "Tire pressure Rear left=%d", value16);
            break;
        case rearRightTirePressurePid:
            StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, value16);
            ESP_LOGD(TAG, "Tire pressure Rear Right=%d", value16);
            break;
        case frontLeftTireTempPid:
            StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, value8 - 50);
            ESP_LOGD(TAG, "Tire temp Front left=%d", value8 - 50);
            break;
        case frontRightTireTempPid:
            StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, value8 - 50);
            ESP_LOGD(TAG, "Tire temp Front Right=%d", value8 - 50);
            break;
        case rearLeftTireTempPid:
            StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, value8 - 50);
            ESP_LOGD(TAG, "Tire temp Rear left=%d", value8 - 50);
            break;
        case rearRightTireTempPid:
            StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, value8 - 50);
            ESP_LOGD(TAG, "Tire temp Rear Right=%d", value8 - 50);
            break;
        default:
            ESP_LOGD(TAG, "Unknown PID, pid=%d", pid);
    }
}




