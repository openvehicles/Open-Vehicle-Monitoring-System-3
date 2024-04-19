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
#include "vehicle_maple_60s.h"

// static const char *TAG = "v-maple60s";

/**
 * Handles incoming CAN-frames on bus 1, the C-bus
 */
void OvmsVehicleMaple60S::IncomingFrameCan1(CAN_frame_t *p_frame)
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
	StdMetrics.ms_v_pos_speed 					ok
	StdMetrics.ms_v_bat_soc 					ok
	StdMetrics.ms_v_pos_odometer 				ok

	StdMetrics.ms_v_door_fl 					rev
	StdMetrics.ms_v_door_fr 					rev
	StdMetrics.ms_v_door_rl 					rev
	StdMetrics.ms_v_door_rr 					rev
	StdMetrics.ms_v_env_locked 					no

	StdMetrics.ms_v_env_onepedal 				NA
	StdMetrics.ms_v_env_efficiencymode 			ok
	StdMetrics.ms_v_env_regenlevel Percentage 	ok

	StdMetrics.ms_v_bat_current 				NA
	StdMetrics.ms_v_bat_voltage 				NA
	StdMetrics.ms_v_bat_power 					ok

	StdMetrics.ms_v_charge_inprogress 			rev

	StdMetrics.ms_v_env_on 						ok
	StdMetrics.ms_v_env_awake 					ok

	StdMetrics.ms_v_env_aux12v					ok

	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, value, PSI);
	*/
	uint8_t *data = p_frame->data.u8;

	switch (p_frame->MsgID)
	{
	case 0x250:
	{
		// StdMetrics.ms_v_charge_inprogress->SetValue(((d[7] | byteMask[0]) == 0xff));
		StdMetrics.ms_v_charge_inprogress->SetValue(CAN_BIT(7, 0));
		break;
	}
	case 0x2fD:
	{
		switch (CAN_BYTE(4))
		{
		case 0x01:
		{
			StdMetrics.ms_v_env_efficiencymode->SetValue("Normal");
			break;
		}
		case 0x05:
		{
			StdMetrics.ms_v_env_efficiencymode->SetValue("ECO");
			break;
		}
		case 0x09:
		{
			StdMetrics.ms_v_env_efficiencymode->SetValue("Sport");
			break;
		}
		}
		break;
	}
	case 0x46a:
	{
		// StdMetrics.ms_v_door_fl->SetValue(((d[0] | byteMask[0]) != 0xff)); // me tira 0 con la puerta cerrada, revisar que efectivamente deba ser asi
		StdMetrics.ms_v_door_fl->SetValue(CAN_BIT(0, 0));
		// StdMetrics.ms_v_door_fr->SetValue(((d[0] | byteMask[3]) != 0xff));
		StdMetrics.ms_v_door_fr->SetValue(CAN_BIT(0, 3));
		// StdMetrics.ms_v_door_rl->SetValue(((d[0] | byteMask[5]) != 0xff));
		StdMetrics.ms_v_door_rl->SetValue(CAN_BIT(0, 5));
		// StdMetrics.ms_v_door_rr->SetValue(((d[0] | byteMask[7]) != 0xff));
		StdMetrics.ms_v_door_rr->SetValue(CAN_BIT(0, 7));
		// StdMetrics.ms_v_door_trunk->SetValue(((d[1] | byteMask[1]) != 0xff));
		StdMetrics.ms_v_door_trunk->SetValue(CAN_BIT(1, 1));
		break;
	}
	case 0x3F1:
	{
		// StdMetrics.ms_v_pos_odometer->SetValue((d[0] << 16 | d[1] << 8 | d[2]) / 10.0);
		StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(0) / 10.0, Kilometers);
		break;
	}
	case 0x125:
	{
		// ESP_LOGE(TAG, "IFCP %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x",
		// 		 p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
		// ESP_LOGE(TAG, "IFCP %lf", (CAN_BYTE(2) - 1) / 250.0);
		StdMetrics.ms_v_pos_speed->SetValue((CAN_BYTE(1) * 2) + (2 * (CAN_BYTE(2) - 1) / 250.0));
		// StdMetrics.ms_v_pos_speed->SetValue((CAN_BYTE(1) * 2) + 2 * (CAN_BYTE(2) - 2 / 253.0));
		break;
	}
	// 1.952756 
	case 0x162: // awake on/off // potencia
	{
		// StdMetrics.ms_v_env_awake->SetValue(((d[5] | byteMask[0]) == 0xff));
		StdMetrics.ms_v_env_awake->SetValue(CAN_BIT(5, 0));
		// StdMetrics.ms_v_env_on->SetValue(((d[3] | byteMask[7]) == 0xff) && ((d[5] | byteMask[0]) == 0xff));
		StdMetrics.ms_v_env_on->SetValue(CAN_BIT(3, 7) && CAN_BIT(5, 0));
		// StdMetrics.ms_v_bat_power->SetValue(d[4] - 100);
		StdMetrics.ms_v_bat_power->SetValue(CAN_BYTE(4) - 100);
		switch (CAN_BYTE(5) | 0xF3)
		{
		case 0xF7:
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(0, Percentage);
			break;
		}
		case 0xFB:
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(50, Percentage);
			break;
		}
		case 0xFF:
		{
			StdMetrics.ms_v_env_regenlevel->SetValue(100, Percentage);
			break;
		}
		}
		break;
	}
	case 0x2F4: // carga
	{
		StdMetrics.ms_v_bat_soc->SetValue((100 * CAN_BYTE(1)) / 255, Percentage);
		break;
	}
	}
}
