/*
;    Project:       Open Vehicle Monitor System
;    Date:          16th August 2024
;
;    Changes:
;    1.0  Initial stub
;
;    (C) 2024        Jaime Middleton / Tucar
;    (C) 2024        Axel Troncoso   / Tucar
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

#include <bitset>
#include <numeric>

#include "metrics_standard.h"
#include "ovms_log.h"
#include "ovms_metrics.h"

#define VERSION "0.0.1"

static const char *TAG = "v-maple60s";

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

OvmsVehicleMaple60S::OvmsVehicleMaple60S()
{
	ESP_LOGI(TAG, "Maple 60s vehicle module");

	send_can_buffer.id = 0;
	send_can_buffer.status = 0;

	memset(send_can_buffer.byte, 0, sizeof(send_can_buffer.byte));

	m_door_lock_status.fill(false);

	StdMetrics.ms_v_bat_12v_voltage->SetValue(12.5, Volts);
	StdMetrics.ms_v_charge_inprogress->SetValue(false);
	StdMetrics.ms_v_env_on->SetValue(false);
	StdMetrics.ms_v_bat_temp->SetValue(20, Celcius);
	StdMetrics.ms_v_env_locked->SetValue(false);

	// Require GPS.
	MyEvents.SignalEvent("vehicle.require.gps", NULL);
	MyEvents.SignalEvent("vehicle.require.gpstime", NULL);

	RegisterCanBus(1, CAN_MODE_LISTEN, CAN_SPEED_500KBPS);
}

/**
 * Destructor
 */
OvmsVehicleMaple60S::~OvmsVehicleMaple60S()
{
	ESP_LOGI(TAG, "Shutdown Maple 60S vehicle module");
}

void OvmsVehicleMaple60S::Ticker1(uint32_t ticker)
{
}

void OvmsVehicleMaple60S::IncomingFrameCan1(CAN_frame_t *p_frame)
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

	StdMetrics.ms_v_env_aux12v					rev

	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, value, PSI);
	StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, value, PSI);
	*/

	uint8_t *data = p_frame->data.u8;

	switch (p_frame->MsgID)
	{
	case 0x250:
		StdMetrics.ms_v_charge_inprogress->SetValue(CAN_BIT(7, 0));
		break;
	// case 0x46a:
	// 	StdMetrics.ms_v_door_fl->SetValue(CAN_BIT(0, 0));
	// 	StdMetrics.ms_v_door_fr->SetValue(CAN_BIT(0, 3));
	// 	StdMetrics.ms_v_door_rl->SetValue(CAN_BIT(0, 5));
	// 	StdMetrics.ms_v_door_rr->SetValue(CAN_BIT(0, 7));
	// 	StdMetrics.ms_v_door_trunk->SetValue(CAN_BIT(1, 1));
	// 	break;
	case 0x3F1:
		StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(0) / 10.0, Kilometers);
		break;
	case 0x125:
		StdMetrics.ms_v_pos_speed->SetValue((CAN_BYTE(1) * 2) + (2 * (CAN_BYTE(2) - 1) / 250.0));
		break;
	case 0x162: // awake, on, off and power
		{
		StdMetrics.ms_v_env_awake->SetValue(CAN_BIT(5, 0));
		StdMetrics.ms_v_env_on->SetValue(CAN_BIT(3, 7) && CAN_BIT(5, 0));
		StdMetrics.ms_v_bat_power->SetValue(CAN_BYTE(4) - 100);

		auto usingCcsCharger = StdMetrics.ms_v_bat_power->AsFloat(0, kW) < -15;
		StdMetrics.ms_v_charge_type->SetValue(usingCcsCharger ? "ccs" : "type2");
		break;
		}
	case 0x2F4: // Charge state
		StdMetrics.ms_v_bat_soc->SetValue((100 * CAN_BYTE(1)) / 255, Percentage);
		break;
	case 0x235:
		StdMetrics.ms_v_env_aux12v->SetValue((CAN_BYTE(7) + 67)/15);
		break;
	case 0x284:
		{
		StdMetrics.ms_v_door_trunk->SetValue(CAN_BIT(1, 2));
		break;
		}
	case 0x285:
		{
		StdMetrics.ms_v_door_fl->SetValue(CAN_BIT(4, 0));
		StdMetrics.ms_v_door_rl->SetValue(CAN_BIT(4, 1));

		/* It's unclear which bit is associated with which door. 
		 * However this doesn't matter since to consider the 
		 * vehicle locked, all must be locked. */
		m_door_lock_status[0] = CAN_BIT(4, 2);
		m_door_lock_status[1] = CAN_BIT(4, 4);

		auto vehicle_locked = std::accumulate(
			m_door_lock_status.begin(),
			m_door_lock_status.end(),
			true,
			[](bool a, bool b)
				{ return a && b; });
		
		StdMetrics.ms_v_env_locked->SetValue(vehicle_locked);
		break;
		}
	case 0x286:
		{
		StdMetrics.ms_v_door_fr->SetValue(CAN_BIT(4, 0));
		StdMetrics.ms_v_door_rr->SetValue(CAN_BIT(4, 1));

		/* It's unclear which bit is associated with which door. 
		 * However this doesn't matter since to consider the 
		 * vehicle locked, all must be locked. */
		m_door_lock_status[2] = CAN_BIT(4, 2);
		m_door_lock_status[3] = CAN_BIT(4, 4);

		auto vehicle_locked = std::accumulate(
			m_door_lock_status.begin(),
			m_door_lock_status.end(),
			true,
			[](bool a, bool b)
				{ return a && b; });
		
		StdMetrics.ms_v_env_locked->SetValue(vehicle_locked);
		break;
		}
	}
}

class OvmsVehicleMaple60SInit
{
public:
	OvmsVehicleMaple60SInit();
} MyOvmsVehicleMaple60SInit __attribute__((init_priority(9000)));

OvmsVehicleMaple60SInit::OvmsVehicleMaple60SInit()
{
	ESP_LOGI(TAG, "Registering Vehicle: Maple 60S (9000)");
	MyVehicleFactory.RegisterVehicle<OvmsVehicleMaple60S>("MPL60S", "Maple 60S");
}
