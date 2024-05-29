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

// static const char *TAG = "v-maxeu6";

/**
 * Handles incoming CAN-frames on bus 1, the C-bus
 */
void OvmsVehicleMaxEu6::IncomingFrameCan1(CAN_frame_t *p_frame)
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
	StdMetrics.ms_v_pos_speed 					ok 0x700 - 03 22 E1 02 00 00 00 00

	StdMetrics.ms_v_bat_soc 					ok 0x700 - 03 22 E1 07 00 00 00 00
	StdMetrics.ms_v_pos_odometer 				ok 0x700 - 03 22 B1 01 00 00 00 00

	StdMetrics.ms_v_door_fl 					ok; yes when open, no when closed
	StdMetrics.ms_v_door_fr 					ok
	StdMetrics.ms_v_door_rl 					ok
	StdMetrics.ms_v_door_rr 					ok
	StdMetrics.ms_v_trunk	 					ok
	StdMetrics.ms_v_env_locked 					ok

	StdMetrics.ms_v_env_onepedal 				-
	StdMetrics.ms_v_env_efficiencymode 			-
	StdMetrics.ms_v_env_regenlevel Percentage 	-

	StdMetrics.ms_v_bat_current 				- req 22 748 e010 resp en 7c8 (val-5000)/10
	StdMetrics.ms_v_bat_voltage 				- req 22 748 e004 resp en 7c8 val/10
	StdMetrics.ms_v_bat_power 					wip esta en porcentaje

	StdMetrics.ms_v_charge_inprogress 			ok

	StdMetrics.ms_v_env_on 						ok
	StdMetrics.ms_v_env_awake 					ok

	StdMetrics.ms_v_env_aux12v					ok

	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, value, PSI);  0x700
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, value, PSI);  0x700
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, value, PSI);  0x700
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, value, PSI);  0x700

	03 22 B1 10 00 00 00 00
	05 62 B1 10 00 4D 00 00
	03 22 B1 13 00 00 00 00
	05 62 B1 13 00 4F 00 00
	03 22 B1 12 00 00 00 00
	05 62 B1 12 00 4E 00 00
	03 22 B1 11 00 00 00 00
	05 62 B1 11 00 50 00 00
	de 120 (80) a 192.15 (71) kPa (FR)
	05 62 B1 11 00 47 00 00
	47 71	192.15
	4D 77 	211.75
	4E 78	214.5
	4F 79	217.25
	50 80	220
	f(x) = 3.122*X - 29.26
	*/

	uint8_t *data = p_frame->data.u8;

	// set batt temp
	switch (p_frame->MsgID)
	{
	case 0x0c9:
	{
		StdMetrics.ms_v_charge_inprogress->SetValue(
			CAN_BIT(2, 0) && CAN_BIT(2, 1) && CAN_BIT(2, 2));
		break;
	}
	case 0x281:
	{
		StdMetrics.ms_v_env_locked->SetValue(
			CAN_BIT(1, 0) && CAN_BIT(1, 2) && CAN_BIT(1, 4) && CAN_BIT(1, 6) &&
			!StdMetrics.ms_v_door_fl->AsBool() &&
			!StdMetrics.ms_v_door_fr->AsBool() &&
			!StdMetrics.ms_v_door_rl->AsBool() &&
			!StdMetrics.ms_v_door_rr->AsBool() &&
			!StdMetrics.ms_v_door_trunk->AsBool());
		break;
	}
	case 0x46a:
	{
		StdMetrics.ms_v_door_fl->SetValue(CAN_BIT(0, 0));
		StdMetrics.ms_v_door_fr->SetValue(CAN_BIT(0, 3));
		StdMetrics.ms_v_door_rr->SetValue(CAN_BIT(0, 5));
		StdMetrics.ms_v_door_rl->SetValue(CAN_BIT(0, 7));
		StdMetrics.ms_v_door_trunk->SetValue(CAN_BIT(1, 1));
		break;
	}
	case 0x540:
	{
		StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(0), Kilometers);
		break;
	}
	case 0x6f0:
	{
		StdMetrics.ms_v_pos_speed->SetValue(CAN_BYTE(4), Kph);
		break;
	}
	case 0x6f1:
	{
		StdMetrics.ms_v_env_awake->SetValue(CAN_BIT(4, 7));
		StdMetrics.ms_v_env_on->SetValue(CAN_BIT(1, 4) && CAN_BIT(4, 7));
		break;
	}
	case 0x6f2:
	{
		if (m_poll_state == 1)
		{
			StdMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(1), Percentage);
		}
		// StdMetrics.ms_v_bat_power->SetValue((CAN_BYTE(2) - 100) * 120, kW); // es porcentaje
		break;
	}
	default:
		break;
	}
}
