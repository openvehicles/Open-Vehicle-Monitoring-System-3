/*
;    Project:       Open Vehicle Monitor System
;    Date:          29th December 2017
;
;    (C) 2017       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "ovms_log.h"

#include "vehicle_kiasoulev.h"

//static const char *TAG = "v-kiasoulev";

/**
 * Handle incoming CAN-frames on bus 2, the M-bus
 */
void OvmsVehicleKiaSoulEv::IncomingFrameCan2(const CAN_frame_t* p_frame)
	{
	const uint8_t *d = p_frame->data.u8;

	//ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

  switch (p_frame->MsgID)
		{
		case 0x131:
			{
			// Climate sat temperature
			m_v_env_climate_temp->SetValue(15 + (d[0] - 2) / 2);
			}
			break;

		case 0x132:
			{
			// Climate mode bit 0-2
			m_v_env_climate_mode->SetValue(d[0] & 0x7);
			// Driver only bit 6
			m_v_env_climate_driver_only->SetValue(d[0] & 0x40);

			// Recirculation bit 2
			m_v_env_climate_resirc->SetValue(d[1] & 0x02);
			// Auto bit 0
			m_v_env_climate_auto->SetValue(d[1] & 0x01);

			// A/C bit 0
			m_v_env_climate_ac->SetValue(d[2] & 0x01);

			// Hvac On/Off
			StdMetrics.ms_v_env_hvac->SetValue( (d[3] & 0x30 )== 0x10);

			// Heat bit 2
			StdMetrics.ms_v_env_heating->SetValue(d[4] & 0x02);

			// Fan speed bit 0-3
			m_v_env_climate_fan_speed->SetValue(d[7] & 0x07);

			}
			break;

		case 0x49b:
			{
			// Read current street name
			int pid = d[0];
			if( pid==0x10 || (pid>0x20 && pid<0x30))
				{
				int i = 1 + (pid & 1);
				if (pid == 0x10)
					{
					m_street_pos = 0;
					i = 3;
					}
				for (; i < 8 && m_street_pos<128; i += 2)
					m_street[m_street_pos++] = d[i] == 0xaa ? 0 : d[i];
				}
			}
			break;

		case 0x506:
			{
			// Distanse to destination
			m_v_pos_dist_to_dest->SetValue((d[4] << 8) | d[5]);

			// Arrival hour
			m_v_pos_arrival_hour->SetValue(d[1]);
			// Arrival minute
			m_v_pos_arrival_minute->SetValue(d[2]);
			}
			break;

		default:
			//ESP_LOGD(TAG, "M-CAN2 %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
			break;
		}
	}
