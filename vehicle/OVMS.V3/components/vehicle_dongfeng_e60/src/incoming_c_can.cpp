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
#include "vehicle_dongfeng_e60.h"

// static const char *TAG = "v-dfe60";

/**
 * Handles incoming CAN-frames on bus 1, the C-bus
 */
void OvmsVehicleDFE60::IncomingFrameCan1(CAN_frame_t *p_frame)
{

	// static const uint8_t byteMask[8] = {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F};
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

	/*
	BASIC METRICS
	StdMetrics.ms_v_pos_speed 					ok (it doesnt show the same speed as in the screen but it is close, a little less when going slow and a little more when going fast)
	StdMetrics.ms_v_bat_soc 					ok
	StdMetrics.ms_v_pos_odometer 				ok

	StdMetrics.ms_v_door_fl 					ok; yes when open, no when closed
	StdMetrics.ms_v_door_fr 					ok
	StdMetrics.ms_v_door_rl 					ok
	StdMetrics.ms_v_door_rr 					ok
	StdMetrics.ms_v_trunk	 					NA
	StdMetrics.ms_v_env_locked 					ok

	StdMetrics.ms_v_env_onepedal 				NA
	StdMetrics.ms_v_env_efficiencymode 			-
	StdMetrics.ms_v_env_regenlevel Percentage 	ok

	StdMetrics.ms_v_bat_current 				-
	StdMetrics.ms_v_bat_voltage 				-
	StdMetrics.ms_v_bat_power 					-

	StdMetrics.ms_v_charge_inprogress 			ok

	StdMetrics.ms_v_env_on 						ok
	StdMetrics.ms_v_env_awake 					ok

	StdMetrics.ms_v_env_aux12v					ok

	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, value, PSI);
	*/

	uint8_t *data = p_frame->data.u8;

	// set batt temp
	switch (p_frame->MsgID)
	{
	case 0x355:
	{
		StdMetrics.ms_v_env_on->SetValue(CAN_BIT(1, 5));
		bool bit1 = CAN_BIT(5, 2);
		bool bit2 = CAN_BIT(5, 3);

		if (bit1 && !bit2)
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(0, Percentage);
		}
		else if (!bit1 && bit2)
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(50, Percentage);
		}
		else if (bit1 && bit2)
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(100, Percentage);
		}

		break;
	}
	case 0x488:
	{
		StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(0) / 10, Kilometers);
		break;
	}
	case 0x347:
	{
		StdMetrics.ms_v_env_awake->SetValue(CAN_BIT(0, 6));
		break;
	}
	case 0x23a:
	{
		StdMetrics.ms_v_door_fl->SetValue(CAN_BIT(1, 3));
		StdMetrics.ms_v_door_fr->SetValue(CAN_BIT(1, 4));
		StdMetrics.ms_v_door_rr->SetValue(CAN_BIT(1, 5));
		StdMetrics.ms_v_door_rl->SetValue(CAN_BIT(1, 6));
		break;
	}
	case 0x380:
	{
		StdMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(0), Percentage);
		if (CAN_BIT(1, 2) && !CAN_BIT(1, 3))
		{
			StdMetrics.ms_v_charge_inprogress->SetValue(true);
		}
		else
		{
			StdMetrics.ms_v_charge_inprogress->SetValue(false);
		}
		break;
	}
	case 0x490:
	{
		StdMetrics.ms_v_env_locked->SetValue(CAN_BIT(0, 2));
		break;
	}
	case 0x0a0:
	{
		StdMetrics.ms_v_pos_speed->SetValue(CAN_UINT(6) / 93, Kph);
		break;
	}
	default:
		break;
	}
}
