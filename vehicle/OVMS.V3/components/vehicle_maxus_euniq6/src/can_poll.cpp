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
#include "vehicle_maxus_euniq6.h"

static const char *TAG = "v-maxeu6";

/**
 * Incoming poll reply messages
 */
void OvmsVehicleMaxEu6::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t *data, uint8_t length)
{
	// ESP_LOGE(TAG, "IPR %03x TYPE:%x PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", job.moduleid_rec, job.type, job.pid, length, data[0], data[1], data[2], data[3],
	// 		 data[4], data[5], data[6], data[7]);
	switch (job.moduleid_rec)
	{
	// ****** IGMP *****
	case 0x780:
		switch (job.pid)
		{
		// case 0xE102:
		// 	if (job.mlframe == 0)
		// 	{

		// 		ESP_LOGE(TAG, "Speed %02x %02x %02x %02x %02x %02x %02x %02x" ,data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		// 		StdMetrics.ms_v_pos_speed->SetValue(CAN_UINT(0), Kph);
		// 	}
		// 	break;
		// case 0xB101:
		// 	if (job.mlframe == 0)
		// 	{
		// 		ESP_LOGE(TAG, "ODO %02x %02x %02x %02x %02x %02x %02x %02x", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		// 		StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(0), Kilometers);
		// 	}
		// 	break;
		case 0xB110:
		{
			if (job.mlframe == 0)
			{
				int value = CAN_UINT(0);
				int preassure = (3.122 * value) - 29.26;
				StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, preassure, kPa);
			}
			break;
		}
		case 0xB111:
		{
			if (job.mlframe == 0)
			{
				int value = CAN_UINT(0);
				int preassure = (3.122 * value) - 29.26;
				StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, preassure, kPa);
			}
			break;
		}
		case 0xB112:
		{
			if (job.mlframe == 0)
			{
				int value = CAN_UINT(0);
				int preassure = (3.122 * value) - 29.26;
				StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, preassure, kPa);
			}
			break;
		}
		case 0xB113:
		{
			if (job.mlframe == 0)
			{
				int value = CAN_UINT(0);
				int preassure = (3.122 * value) - 29.26;
				StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, preassure, kPa);
			}
			break;
		}
		}
		break;

	// ****** BCM ******
	case 0x7c8:
		switch (job.pid)
		{
		case 0xE010:
		{
			if (job.mlframe == 0)
			{
				float value = (float)CAN_UINT(0);
				float amps = (value - 5000) / 10;
				StdMetrics.ms_v_bat_current->SetValue(amps, Amps);
			}
			break;
		}
		case 0xE004:
		{
			if (job.mlframe == 0)
			{
				StdMetrics.ms_v_bat_voltage->SetValue((float)CAN_UINT(0) / 10, Volts);
			}
			break;
		}
		case 0xE014:
		{
			if (job.mlframe == 0)
			{
				StdMetrics.ms_v_bat_soc->SetValue((float)CAN_UINT(0) / 100, Percentage);
			}
			break;
		}
		}
		break;

	default:
		ESP_LOGD(TAG, "Unknown module: %03" PRIx32, job.moduleid_rec);
		break;
	}
}
