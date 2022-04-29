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


void OvmsVehicleJaguarIpace::IncomingTcuPoll(uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
    //uint8_t value8 = data[0];
    //uint16_t value16 = (data[0] << 8 | data[1]);
    //uint32_t value24 = (data[0] << 16 | data[1] << 8 | data[2]);
    float longitude;
    float latitude;

    ESP_LOGI(TAG, "IncomingTcuPoll, pid=%d, length=%d, remain=%d", pid, length, remain);
    switch (pid)
    {
        case localizationPid:
            if (remain != 0) {
                memcpy(m_localization, data, length);
            } else {
                memcpy(m_localization+3, data, length);
                longitude = ((m_localization[0] << 24) + (m_localization[1] << 16) + (m_localization[2] << 8) + m_localization[3]);// / 131072.0f; //0x20000u
                latitude = ((m_localization[4] << 24) + (m_localization[5] << 16) + (m_localization[6] << 8) + m_localization[7]); // / 131072.0f; //0x20000u
                ESP_LOGD(TAG, "Longitude=%f", longitude);
                ESP_LOGD(TAG, "Latitude=%f", latitude);
                if (longitude > 0x80000000u) {
                    longitude -= 0x100000000u;
                }
                if (latitude > 0x80000000u) {
                    latitude -= 0x100000000u;
                }
                ESP_LOGD(TAG, "Longitude=%f", longitude);
                ESP_LOGD(TAG, "Latitude=%f", latitude);
                longitude = longitude / 131072.0f;
                latitude = latitude / 131072.0f;
                StandardMetrics.ms_v_pos_longitude->SetValue(longitude);
                StandardMetrics.ms_v_pos_latitude->SetValue(latitude);
                StandardMetrics.ms_v_pos_satcount->SetValue(m_localization[9]);
                ESP_LOGD(TAG, "Longitude=%f", longitude);
                ESP_LOGD(TAG, "Latitude=%f", latitude);
                ESP_LOGD(TAG, "Sat Count=%d", m_localization[9]);
                
                // Derive GPS signal quality from satellite count, as we don't have HDOP:
                // 6 satellites = 50%
                StandardMetrics.ms_v_pos_gpssq->SetValue(LIMIT_MAX(LIMIT_MIN(m_localization[9]-1,0) * 10, 100));
                StandardMetrics.ms_v_pos_gpstime->SetValue(time(NULL));
            }
            
            break;
        // case batterySoHPid:
        //     StandardMetrics.ms_v_bat_soh->SetValue(value8 / 2.0f);
        //     ESP_LOGD(TAG, "SOH=%f", value8 / 2.0f);
        //     break;
        default:
            ESP_LOGD(TAG, "Unknown PID, pid=%d", pid);
    }
}


