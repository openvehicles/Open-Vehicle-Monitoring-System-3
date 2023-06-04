/*
;    Project:       Open Vehicle Monitor System
;    Date:          02th May 2020
;
;    (C) 2023       Tamás Kovács (KommyKT)
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

static const char *TAGPOLL = "v-trio-poll";

/**
 * Incoming poll reply messages
 */
void OvmsVehicleMitsubishi::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
  //ESP_LOGW(TAGPOLL, "%03" PRIx32 " TYPE:%x PID:%02x Data:%02x %02x %02x %02x %02x %02x %02x %02x LENG:%02x REM:%02x", m_poll_moduleid_low, type, pid, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], length, mlremain);

  	//OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();
    switch (m_poll_moduleid_low)
		{
  		// ****** BMU *****
  		case 0x762:
      {
        switch (m_poll_ml_frame) {
          case 0:
          {
            OvmsMetricFloat* xmi_bat_soc_real = MyMetrics.InitFloat("xmi.b.soc.real", 10, 0, Percentage);
            xmi_bat_soc_real->SetValue((data[0] * 0.5) - 5);

            // displayed SOC
            OvmsMetricFloat* xmi_bat_soc_display = MyMetrics.InitFloat("xmi.b.soc.display", 10, 0, Percentage);
            xmi_bat_soc_display->SetValue((data[1] * 0.5) - 5);
            break;
          }

          case 4:
          {
            // battery "max" capacity
            StandardMetrics.ms_v_bat_cac->SetValue(((data[2] * 256.0 + data[3]) * 0.1));

            // battery remain capacity
            ms_v_bat_cac_rem->SetValue(((data[4] * 256.0 + data[5]) * 0.1));

            //max charging kW
            ms_v_bat_max_input->SetValue(data[6] * 0.25);
            break;
          }

          case 5:
          {
            //max output kW
            ms_v_bat_max_output->SetValue(data[0] * 0.25);
            break;
          }
          default:
          break;
        }
        break;
      }

      // ****** OBC *****
      case 0x766:
      {
        ESP_LOGW(TAGPOLL, "%03" PRIx32 " TYPE:%x PID:%02x Data:%02x %02x %02x %02x %02x %02x %02x %02x LENG:%02x REM:%02x", m_poll_moduleid_low, type, pid, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], length, mlremain);
        break;
      }

      // ****** HVAC *****
      case 0x772:
      {
        switch (m_poll_ml_frame) {
          case 0:
          {
            StandardMetrics.ms_v_env_cabintemp->SetValue((data[0] * 0.25) - 16.0);
            break;
          }

          case 1:
          {
            //StandardMetrics.ms_v_env_temp->SetValue((data[0] * 0.3) - 29.0);
             break;
          }
          default:
          break;
        }
        break;
      }

      // ****** METER  *****
      case 0x783:
      {
        if(pid == 0xCE)
        {
          switch (m_poll_ml_frame) {
            case 0:
            {
              ms_v_trip_A->SetValue(((data[2] << 16 ) + (data[1] << 8) + data[0]) * 0.1, Kilometers);
              tripb += data[3];
              break;
            }

            case 1:
            {
             tripb += (data[1] << 16 ) + (data[0] << 8);
              if((tripb * 0.1) > 0.1){
                ms_v_trip_B->SetValue(tripb * 0.1, Kilometers);
                has_trip = true;
              }
              else
              {
                has_trip = false;
              }
              tripb = 0.0;
              break;
            }
            default:
            break;
          }
        }
        break;
      }

     default:
		   ESP_LOGW(TAGPOLL, "Unknown module: %03" PRIx32, m_poll_moduleid_low);
	  }

}
