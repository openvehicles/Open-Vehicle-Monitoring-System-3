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

// static const char *TAG = "v-netaAya";

/**
 * Handles incoming CAN-frames on bus 1, the C-bus
 */
void OvmsVehicleNetaAya::IncomingFrameCan1(CAN_frame_t *p_frame)
{
	/*
	BASIC METRICS
	StdMetrics.ms_v_pos_speed 					ok
	StdMetrics.ms_v_bat_soc 					ok
	StdMetrics.ms_v_pos_odometer 				ok

	StdMetrics.ms_v_door_fl 					ok
	StdMetrics.ms_v_door_fr 					ok
	StdMetrics.ms_v_door_rl 					ok
	StdMetrics.ms_v_door_rr 					ok
	StdMetrics.ms_v_env_locked 					ok

	StdMetrics.ms_v_env_onepedal 				ok
	StdMetrics.ms_v_env_efficiencymode 			ok
	StdMetrics.ms_v_env_regenlevel Percentage 	rev cuando anda

	StdMetrics.ms_v_bat_current 				rev
	StdMetrics.ms_v_bat_voltage 				ok
	StdMetrics.ms_v_bat_power 					ok

	StdMetrics.ms_v_charge_inprogress 			rev 

	StdMetrics.ms_v_env_on 						ok
	StdMetrics.ms_v_env_awake 					NA

	StdMetrics.ms_v_env_aux12v					yes

	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, value, PSI); NA
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, value, PSI); NA
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, value, PSI); NA
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, value, PSI); NA
	*/

	uint8_t *data = p_frame->data.u8;

	switch (p_frame->MsgID)
	{
	// case 0x21E: // SPEED
	// 	StdMetrics.ms_v_pos_speed->SetValue(CAN_BYTE(0), Kph);
	// 	break;
	// case 0x452: // ODOMETER
	// 	StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT(1), Kilometers);
	// 	break;
	case 0x339:											  // door status
		StdMetrics.ms_v_door_fl->SetValue(CAN_BIT(1, 1)); // true when open
		StdMetrics.ms_v_door_fr->SetValue(CAN_BIT(1, 3));
		StdMetrics.ms_v_door_rl->SetValue(CAN_BIT(1, 5));
		StdMetrics.ms_v_door_rr->SetValue(CAN_BIT(1, 7));
		StdMetrics.ms_v_env_locked->SetValue(CAN_BIT(4, 6));
		break;
	// case 0x590: // SOC
	// 	StdMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(1), Percentage);
	// 	break;
	case 0x403:
		StdMetrics.ms_v_env_onepedal->SetValue(CAN_BIT(2, 4));
		break;
	case 0x404:
		if (CAN_BIT(7, 3))
		{
			StdMetrics.ms_v_env_efficiencymode->SetValue("Normal");
		}
		else if (CAN_BIT(7, 2))
		{
			StdMetrics.ms_v_env_efficiencymode->SetValue("Sport");
		}
	case 0x522: // regen level cambia
		if (CAN_BIT(0, 4) == 1)
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(50, Percentage);
		}
		else if (CAN_BIT(0, 5) == 1)
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(100, Percentage);
		}
		else
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(0, Percentage);
		}
		break;
	// case 0x405:
	// 	StdMetrics.ms_v_bat_current->SetValue((CAN_UINT(4) * 0.05) - 1600, Amps);
	// 	StdMetrics.ms_v_bat_voltage->SetValue(CAN_UINT(6) * 0.05, Volts);
	// 	// maybe send to ticker
	// 	StdMetrics.ms_v_bat_power->SetValue(
	// 		StdMetrics.ms_v_bat_voltage->AsFloat(400, Volts) *
	// 			StdMetrics.ms_v_bat_current->AsFloat(0, Amps) / 1000,
	// 		kW);
	// 	StdMetrics.ms_v_charge_inprogress->SetValue(CAN_BIT(2, 0));
	// 	break;
	default:
		return;
	}
}

void OvmsVehicleNetaAya::IncomingFrameCan2(CAN_frame_t *p_frame)
{
	uint8_t *d = p_frame->data.u8;
	// ESP_LOGE(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x",
	// 		 p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

	// Check if response is from synchronous can message
	if (message_send_can.status == 0xff && p_frame->MsgID == (message_send_can.id + 0x08))
	{
		// Store message bytes so that the async method can continue
		message_send_can.status = 3;

		message_send_can.byte[0] = d[0];
		message_send_can.byte[1] = d[1];
		message_send_can.byte[2] = d[2];
		message_send_can.byte[3] = d[3];
		message_send_can.byte[4] = d[4];
		message_send_can.byte[5] = d[5];
		message_send_can.byte[6] = d[6];
		message_send_can.byte[7] = d[7];
	}
}
