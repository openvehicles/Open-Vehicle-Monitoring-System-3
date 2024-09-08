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

static const char *TAG = "v-mgev-gwm-poll";

namespace {

// Responses to the GWM Status PID
enum GwmStatus : unsigned char {
  Idle = 0x0,  // Seen when connected but not locked
  Acc = 0x31,  // When the car does not have the ignition on
  Ready = 0x75,  // When the ignition is on aux or running
};
}

void OvmsVehicleMgEv::IncomingGwmPoll(uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain)
{
  uint16_t odo = (data[0] << 16 | data[1] << 8 | data[2]);
  //ESP_LOGI(TAG,"GWM Poll received");
  
  switch (pid)
  {
    case vehTimePid:
      if(remain > 0) {
        ESP_LOGD(TAG,"Date: %d/%d/%d",data[0], data[1], data[2]);
      } else {
        ESP_LOGD(TAG,"Time: %d:%d:%d",data[0], data[1], data[2]);
      }
      break;
    case odoPid:
      ESP_LOGD(TAG,"Vehicle ODO = %d", odo);
      StandardMetrics.ms_v_pos_odometer->SetValue(
                                                  data[0] << 16 | data[1] << 8 | data[2]
                                                  );
      break;
    case vehStatusPid:
      ESP_LOGD(TAG,"Vehicle Status = %02" PRIx8,data[0]);
      if(m_gwm_state->AsInt() != data[0]) {
        switch (data[0]) {
          case Acc:
            ESP_LOGI(TAG,"Vehicle Status = ACC");
            if(m_gwm_state->AsInt() == Ready) {
              ESP_LOGI(TAG,"Vehicle Status changed from Ready to Acc. Turning polling off");
              StandardMetrics.ms_v_env_on->SetValue(false);
              StandardMetrics.ms_v_env_locked->SetValue(true);
              PollSetState(PollStateListenOnly);
              m_enable_polling->SetValue(false);
            }
            break;
          case Ready:
            ESP_LOGI(TAG,"Vehicle Status = Ready");
            StandardMetrics.ms_v_env_locked->SetValue(false);
            break;
          case Idle:
            ESP_LOGI(TAG,"Vehicle Status = Idle");
            break;
          default:
            ESP_LOGI(TAG,"Vehicle Status = Unknown");
            break;
        }
      }
      m_gwm_state->SetValue(data[0]);
      break;
    case vinPid:
      HandleVinMessage(data, length, remain);
      if(remain==0) {
        ESP_LOGD(TAG,"Vehicle VIN received");
      }
      break;
  }
}
