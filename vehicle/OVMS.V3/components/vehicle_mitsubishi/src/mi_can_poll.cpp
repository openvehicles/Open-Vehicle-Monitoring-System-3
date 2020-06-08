/*
;    Project:       Open Vehicle Monitor System
;    Date:          02th May 2020
;
;    (C) 2020       Tamás Kovács (KommyKT)
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
#include "vehicle_mitsubishi.h"

static const char *TAG = "v-mitsubishi";

/**
 * Incoming poll reply messages
 */
void OvmsVehicleMitsubishi::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{

  	OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();
    switch (m_poll_moduleid_low)
		{

		// ****** BMU *****
		case 0x762:
    {

      ESP_LOGW(TAG, "%03x TYPE:%x PID:%02x %02x %02x %02x %02x %02x %02x %02x %02x LENG:%02x REM:%02x", m_poll_moduleid_low, type, pid, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], length, mlremain);

      break;
    }

    // ****** HVAC *****
    case 0x772:
    {
      
      if(m_poll_ml_frame == 0){
        StandardMetrics.ms_v_env_cabintemp->SetValue((data[2] + data[3] - 68) / 10.0);
      }

      if(m_poll_ml_frame == 1){
        StandardMetrics.ms_v_env_temp->SetValue((data[0] + data[1] - 68) / 10.0);
      }

      break;
    }

    // ****** METER  *****
    case 0x783:
    {
      //ESP_LOGW(TAG, "%03x TYPE:%x PID:%02x %02x %02x %02x %02x %02x %02x %02x %02x LENG:%02x REM:%02x", m_poll_moduleid_low, type, pid, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], length, mlremain);
      if(pid == 0xCE)
      {
        if(m_poll_ml_frame == 0){
          trio->ms_v_trip_A->SetValue((((int)data[2] << 16 ) + ((int)data[1] << 8) + data[0])/10.0, Kilometers);
          tripb += (int)data[3];
        }
        if(m_poll_ml_frame == 1){
          tripb += ((int)data[1] << 16 ) + ((int)data[0] << 8);
          trio->ms_v_trip_B->SetValue(tripb/10.0, Kilometers);
          tripb = 0;
        }

      }
      break;
    }


    default:
			ESP_LOGW(TAG, "Unknown module: %03x", m_poll_moduleid_low);
			break;
	  }

}
