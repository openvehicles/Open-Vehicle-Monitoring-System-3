/*
;    Project:       Open Vehicle Monitor System
;    Date:          21th January 2019
;
;    (C) 2019       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_neta_aya.h"

static const char *TAG = "v-netaAya";

/**
 * Incoming poll reply messages
 */
void OvmsVehicleNetaAya::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t *data, uint8_t length)
{
	// ESP_LOGD(TAG, "IPR %03x TYPE:%x PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", job.moduleid_rec, type, pid, length, data[0], data[1], data[2], data[3],
	//	data[4], data[5], data[6], data[7]);
	switch (job.moduleid_rec)
	{
	case 0x7ea:
		switch (job.pid)
		{
		// speed
		case 0xb100:
			StdMetrics.ms_v_pos_speed->SetValue(CAN_UINT(0) / 100, Kph);
			;
			break;
		// soc
		case 0xf015:
			StdMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(0), Percentage);
			break;
		// odometer
		case 0xe101:
			StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(0), Kilometers);
			break;
		// current
		case 0xf013:
			float curr;
			uid_t value;
			value = CAN_UINT(0);
			if (value < 32000)
			{
				curr = (32000 - value) * -1;
			}
			else
			{
				curr = value - 32000;
			}
			StdMetrics.ms_v_bat_current->SetValue(curr / 20, Amps);
			break;
		// voltage
		case 0xf012:
			StdMetrics.ms_v_bat_voltage->SetValue(CAN_UINT(0) / 20, Volts);
			break;
		// on
		case 0xd001:
			bool on = CAN_BYTE(0) != 0x01;
			StdMetrics.ms_v_env_awake->SetValue(on);
			StdMetrics.ms_v_env_on->SetValue(on);
			break;
		}
	case 0x718:
		switch (job.pid)
		{
		// evse code
		case 0xf012:
			if (job.mlframe == 1)
			{
				// 04 es cs2 06 tmb
				// 27 es conectada
				// 01 es dc
				bool charging;
				charging = data[1] != 0x00 && data[1] != 0x27;
				charging = charging && StdMetrics.ms_v_bat_current->AsFloat(0, Amps) > 1;
				if (charging)
				{
					charger_disconected = false;
				}
				StdMetrics.ms_v_charge_inprogress->SetValue(charging);
			}
			if (job.mlframe == 3) {
				ESP_LOGE(TAG, "\n");
			}
			break;
		}
	default:
		ESP_LOGD(TAG, "Unknown module: %03" PRIx32, job.moduleid_rec);
		break;
	}
}
