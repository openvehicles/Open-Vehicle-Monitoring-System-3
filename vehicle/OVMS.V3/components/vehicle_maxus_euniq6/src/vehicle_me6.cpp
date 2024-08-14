/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th August 2024
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Jaime Middleton / Tucar
;    (C) 2021       Axel Troncoso   / Tucar
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
static const char *TAG = "v-maxe6";

#include <stdio.h>
#include <stdexcept>

#include "vehicle_me6.h"
#include "me56_pids.h"
#include "ovms_notify.h"
#include <algorithm>
#include "metrics_standard.h"

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     data[b]
#define CAN_INT(b)      ((int16_t)CAN_UINT(b))
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b+1) << 16)  | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (can_databuffer[b] & 0x0f)
#define CAN_NIBH(b)     (can_databuffer[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))
#define CAN_BIT(b,pos) !!(data[b] & (1<<(pos)))

OvmsVehicleMaxe6::OvmsVehicleMaxe6()
  {
  ESP_LOGI(TAG, "Start Maxus Euniq 6 vehicle module");

  // Init CAN:
  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  // Init CAN buffer:
  send_can_buffer.id = 0;
  send_can_buffer.status = 0;
  memset(send_can_buffer.byte, 0, sizeof(send_can_buffer.byte));

  // Init BMS:
  BmsSetCellArrangementVoltage(96, 16);
  BmsSetCellArrangementTemperature(16, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
  
  // Init Energy:
  StdMetrics.ms_v_charge_mode->SetValue("standard");
  StdMetrics.ms_v_env_ctrl_login->SetValue(true);

  // Require GPS:
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);
  }

OvmsVehicleMaxe6::~OvmsVehicleMaxe6()
  {
  ESP_LOGI(TAG, "Stop Euniq 6 vehicle module");
  }

void OvmsVehicleMaxe6::IncomingFrameCan1(CAN_frame_t *p_frame)
{

	uint8_t *d = p_frame->data.u8;

	// Check if response is from synchronous can message
	if (send_can_buffer.status == 0xff && p_frame->MsgID == (send_can_buffer.id + 0x08))
	{
		// Store message bytes so that the async method can continue
		send_can_buffer.status = 3;

		send_can_buffer.byte[0] = d[0];
		send_can_buffer.byte[1] = d[1];
		send_can_buffer.byte[2] = d[2];
		send_can_buffer.byte[3] = d[3];
		send_can_buffer.byte[4] = d[4];
		send_can_buffer.byte[5] = d[5];
		send_can_buffer.byte[6] = d[6];
		send_can_buffer.byte[7] = d[7];
	}

	/*
	BASIC METRICS
	StdMetrics.ms_v_pos_speed 					ok
	StdMetrics.ms_v_bat_soc 					ok
	StdMetrics.ms_v_pos_odometer 				ok

	StdMetrics.ms_v_door_fl 					ok; yes when open, no when closed
	StdMetrics.ms_v_door_fr 					ok
	StdMetrics.ms_v_door_rl 					ok
	StdMetrics.ms_v_door_rr 					ok
	StdMetrics.ms_v_trunk	 					ok
	StdMetrics.ms_v_env_locked 					ok

	StdMetrics.ms_v_env_onepedal 				-
	StdMetrics.ms_v_env_efficiencymode 			-
	StdMetrics.ms_v_env_regenlevel Percentage 	-

	StdMetrics.ms_v_bat_current 				-
	StdMetrics.ms_v_bat_voltage 				-
	StdMetrics.ms_v_bat_power 					wip esta en porcentaje

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

	switch (p_frame->MsgID)
	{
	case 0x0c9:
	{
		StdMetrics.ms_v_charge_inprogress->SetValue(
			CAN_BIT(2,0) && CAN_BIT(2,1) && CAN_BIT(2,2)
			);
		break;
	}
	case 0x281:
	{
		StdMetrics.ms_v_env_locked->SetValue(
			CAN_BIT(1,0) &&	CAN_BIT(1,2) &&	CAN_BIT(1,4) &&	CAN_BIT(1,6) &&
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
		StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(0));
		break;
	}
	case 0x6f0:
	{
		StdMetrics.ms_v_pos_speed->SetValue(CAN_BYTE(4));
		break;
	}
	case 0x6f1:
	{
		StdMetrics.ms_v_env_awake->SetValue(CAN_BIT(4,7));
		StdMetrics.ms_v_env_on->SetValue(CAN_BIT(1,4) && CAN_BIT(4,7));
		break;
	}
	case 0x6f2:
	{
		StdMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(1));
		StdMetrics.ms_v_bat_power->SetValue((CAN_BYTE(2) - 100) * 120, kW); // es porcentaje
		break;
	}
	default:
		break;
	}
}

void OvmsVehicleMaxe6::Ticker1(uint32_t ticker)
{
}

class OvmsVehicleMaxe6Init
  {
  public: OvmsVehicleMaxe6Init();
  } OvmsVehicleMaxe6Init  __attribute__ ((init_priority (9000)));

OvmsVehicleMaxe6Init::OvmsVehicleMaxe6Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Maxus Euniq 6 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxe6>("ME56","Maxus Euniq 6");
  }
