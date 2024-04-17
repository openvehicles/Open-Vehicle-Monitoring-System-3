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
#include "vehicle_kia_niroevsg2.h"

static const char *TAG = "v-kianiroevsg2";

/**
 * Incoming poll reply messages
 */
void OvmsVehicleKiaNiroEvSg2::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t *data, uint8_t length)
{
	// ESP_LOGD(TAG, "IPR %03x TYPE:%x PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", job.moduleid_rec, type, pid, length, data[0], data[1], data[2], data[3],
	//	data[4], data[5], data[6], data[7]);
	switch (job.moduleid_rec)
	{
	// ****** IGMP *****
	case 0x778:
		IncomingIGMP(job.bus, job.type, job.pid, data, length, job.mlframe, job.mlremain);
		break;

	// ****** BCM ******
	case 0x7a8:
		IncomingBCM(job.bus, job.type, job.pid, data, length, job.mlframe, job.mlremain);
		break;

	// ******* VMCU ******
	case 0x7ea:
		IncomingVMCU(job.bus, job.type, job.pid, data, length, job.mlframe, job.mlremain);
		break;

	// ***** BMC ****
	case 0x7ec:
		IncomingBMC(job.bus, job.type, job.pid, data, length, job.mlframe, job.mlremain);
		break;

	// ***** CM ****
	case 0x7ce:
		IncomingCM(job.bus, job.type, job.pid, data, length, job.mlframe, job.mlremain);
		break;

	// ***** SW ****
	case 0x7dc:
		IncomingSW(job.bus, job.type, job.pid, data, length, job.mlframe, job.mlremain);
		break;

	default:
		ESP_LOGD(TAG, "Unknown module: %03" PRIx32, job.moduleid_rec);
		break;
	}
}

/**
 * Handle incoming messages from cluster.
 */
void OvmsVehicleKiaNiroEvSg2::IncomingCM(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain)
{
//	ESP_LOGI(TAG, "CM PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, mlframe, data[0], data[1], data[2], data[3],
//			data[4], data[5], data[6]);
//	ESP_LOGI(TAG, "---");
	switch (pid)
		{
		case 0xb002:
			if (mlframe == 1)
				{
				StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(3), GetConsoleUnits());
				}
				break;
		case 0xb003:
				if (mlframe == 1)
				{
				StdMetrics.ms_v_env_awake->SetValue(CAN_BYTE(1)!=0);
				if (!StdMetrics.ms_v_env_awake->AsBool())
				{
					StdMetrics.ms_v_env_on->SetValue(false);
				}
				
				}
				break;
		}
}

/**
 * Handle incoming messages from VMCU-poll
 *
 * - Aux battery SOC, Voltage and current
 */
void OvmsVehicleKiaNiroEvSg2::IncomingVMCU(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain)
{
	//ESP_LOGD(TAG, "VMCU TYPE: %02x PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", type, pid, length, mlframe, data[0], data[1], data[2], data[3],
	//		data[4], data[5], data[6]);

	switch (pid)
	{
	case 0x02:
		if (type == VEHICLE_POLL_TYPE_OBDIIGROUP)
			{
			if (mlframe == 3)
				{
				StdMetrics.ms_v_bat_12v_voltage->SetValue(((CAN_BYTE(2)<<8)+CAN_BYTE(1))/1000.0, Volts);
				}
			}
		break;
	}
}

/**
 * Handle incoming messages from BMC-poll
 *
 * - Pilot signal available
 * - CCS / Type2
 * - Battery current
 * - Battery voltage
 * - Battery module temp 1-8
 * - Cell voltage max / min + cell #
 * + more
 */
void OvmsVehicleKiaNiroEvSg2::IncomingBMC(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain)
{
	if (type == VEHICLE_POLL_TYPE_OBDIIEXTENDED)
	{
		switch (pid)
		{
		case 0x0101:
			// diag page 01: skip first frame (no data)
			//ESP_LOGD(TAG, "Frame number %x",mlframe);
			
			if (mlframe == 2)
				{
				StdMetrics.ms_v_bat_current->SetValue((float)CAN_INT(0)/10.0, Amps);
				StdMetrics.ms_v_bat_voltage->SetValue((float)CAN_UINT(2)/10.0, Volts);
				}
			break;

		case 0x0105:
			if (mlframe == 4)
				{
				StdMetrics.ms_v_bat_soh->SetValue( (float)CAN_UINT(1)/10.0 );
				}
			else if (mlframe == 5)
				{
				StdMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(0)/2.0);
				}
			break;
		}
	}
}



/**
 * Handle incoming messages from BCM-poll
 */
void OvmsVehicleKiaNiroEvSg2::IncomingBCM(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain)
{
	uint8_t bVal;
	if (type == VEHICLE_POLL_TYPE_OBDIIEXTENDED)
		{
		switch (pid)
			{
			case 0xC00B:
				if (mlframe == 1)
				{
					bVal = CAN_BYTE(1);
					if (bVal > 0)
						StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, bVal / 5.0, PSI);
					bVal = CAN_BYTE(6);
					if (bVal > 0)
						StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, bVal / 5.0, PSI);
				}
				else if (mlframe == 2)
				{
					bVal = CAN_BYTE(4);
					if (bVal > 0)
						StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, bVal / 5.0, PSI);
				}
				else if (mlframe == 3)
				{
					bVal = CAN_BYTE(2);
					if (bVal > 0)
						StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, bVal / 5.0, PSI);
				}
				break;

			case 0xB010:
				if (mlframe == 1)
				{
					bVal = CAN_BYTE(1);
					if (bVal > 0){
						windows_open = true;
					} else {
						windows_open = false;
					}
				}
				break;
			}
		}
	}

/**
 * Handle incoming messages from IGMP-poll
 *
 *
 */
void OvmsVehicleKiaNiroEvSg2::IncomingIGMP(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain)
{
	if (type == VEHICLE_POLL_TYPE_OBDIIEXTENDED)
	{
		switch (pid)
		{
		case 0xbc03:
			if (mlframe == 1)
				{
				StdMetrics.ms_v_door_fl->SetValue(CAN_BIT(1, 5));
				StdMetrics.ms_v_door_fr->SetValue(CAN_BIT(1, 4));
				StdMetrics.ms_v_door_rl->SetValue(CAN_BIT(1, 0));
				StdMetrics.ms_v_door_rr->SetValue(CAN_BIT(1, 2));
				m_v_door_lock_rl->SetValue(!CAN_BIT(1, 1));
				m_v_door_lock_rr->SetValue(!CAN_BIT(1, 3));
				}
			break;

		case 0xbc04:
			if (mlframe == 1)
				{
				m_v_door_lock_fl->SetValue(!CAN_BIT(1, 3));
				m_v_door_lock_fr->SetValue(!CAN_BIT(1, 2));
				}
			break;
		}
	}
}

void OvmsVehicleKiaNiroEvSg2::IncomingSW(canbus *bus, uint16_t type, uint16_t pid, const uint8_t *data, uint8_t length, uint16_t mlframe, uint16_t mlremain)
{
	if (type == VEHICLE_POLL_TYPE_OBDIIEXTENDED)
	{
		switch (pid)
		{
		case 0x0101:
			if (mlframe == 2)
			{
				StdMetrics.ms_v_env_on->SetValue(CAN_BYTE(6) != 0);
				StdMetrics.ms_v_pos_speed->SetValue(CAN_BYTE(2));
			}
			break;
		}
	}
}

/**
 * Get console ODO units
 *
 * Currently from configuration
 */
metric_unit_t OvmsVehicleKiaNiroEvSg2::GetConsoleUnits()
{
	return MyConfig.GetParamValueBool("xkn", "consoleKilometers", true) ? Kilometers : Miles;
}
