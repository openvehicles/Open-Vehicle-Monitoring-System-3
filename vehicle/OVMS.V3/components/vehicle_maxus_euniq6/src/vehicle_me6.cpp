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
#include "ovms_notify.h"
#include <algorithm>
#include "metrics_standard.h"

static const OvmsPoller::poll_pid_t vehicle_maxusEU6_polls[] =
{
  {0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB110, {0, 2, 0}, 1, ISOTP_STD}, // TPMS
  {0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB111, {0, 2, 0}, 1, ISOTP_STD}, // TPMS
  {0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB112, {0, 2, 0}, 1, ISOTP_STD}, // TPMS
  {0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB113, {0, 2, 0}, 1, ISOTP_STD}, // TPMS

  {0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE010, {0, 1, 1}, 1, ISOTP_STD}, // Current
  {0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE004, {0, 1, 1}, 1, ISOTP_STD}, // Voltage
  {0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE014, {0, 0, 1}, 1, ISOTP_STD}, // SOC
  POLL_LIST_END
};

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
  
  // Init Energy:
  StdMetrics.ms_v_charge_mode->SetValue("standard");

  // Require GPS:
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);

  PollSetThrottling(10);
  PollSetState(0); // OFF
}

OvmsVehicleMaxe6::~OvmsVehicleMaxe6()
{
  ESP_LOGI(TAG, "Stop Euniq 6 vehicle module");
}

void OvmsVehicleMaxe6::IncomingFrameCan1(CAN_frame_t *p_frame)
{
  /*
  BASIC METRICS
  StdMetrics.ms_v_pos_speed         ok
  StdMetrics.ms_v_bat_soc           ok
  StdMetrics.ms_v_pos_odometer      ok

  StdMetrics.ms_v_door_fl           ok
  StdMetrics.ms_v_door_fr           ok
  StdMetrics.ms_v_door_rl           ok
  StdMetrics.ms_v_door_rr           ok
  StdMetrics.ms_v_trunk             ok
  StdMetrics.ms_v_env_locked        ok

  StdMetrics.ms_v_bat_current       -
  StdMetrics.ms_v_bat_voltage       -
  StdMetrics.ms_v_bat_power         wip (percentage units)

  StdMetrics.ms_v_charge_inprogress rev

  StdMetrics.ms_v_env_on            ok
  StdMetrics.ms_v_env_awake         ok

  StdMetrics.ms_v_env_aux12v        rev
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
        CAN_BIT(1,0) &&  CAN_BIT(1,2) &&  CAN_BIT(1,4) &&  CAN_BIT(1,6) &&
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
      // Units percentage.
      StdMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(1));
      StdMetrics.ms_v_bat_power->SetValue((CAN_BYTE(2) - 100) * 120, kW);
      break;
    }
    default:
      break;
  }
}
void OvmsVehicleMaxe6::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t *data, uint8_t length)
{
	switch (job.moduleid_rec)
	{
	// ****** IGMP *****
	case 0x780:
		switch (job.pid)
		{
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

void OvmsVehicleMaxe6::Ticker1(uint32_t ticker)
{
  /* Set batery power */
  StdMetrics.ms_v_bat_power->SetValue(
    StdMetrics.ms_v_bat_voltage->AsFloat(400, Volts) *
      StdMetrics.ms_v_bat_current->AsFloat(1, Amps) / 1000,
    kW);
  
  /* Set charge status */
  StdMetrics.ms_v_charge_inprogress->SetValue(
    (StdMetrics.ms_v_pos_speed->AsFloat(0) < 1) &
    (StdMetrics.ms_v_bat_power->AsFloat(0, kW) < -1));
  
  auto vehicle_on = (bool) StdMetrics.ms_v_env_on->AsBool();
  auto vehicle_charging = (bool) StdMetrics.ms_v_charge_inprogress->AsBool();
  
  /* Define next state. */
  auto to_run = vehicle_on && !vehicle_charging;
  auto to_charge = vehicle_charging;
  auto to_off = !vehicle_on && !vehicle_charging;

  /* There may be only one next state. */
  assert(to_run + to_charge + to_off == 1);
  
  /* Run actions depending on state. */
  auto poll_state = GetPollState();
  switch (poll_state)
  {
    case PollState::OFF:
      if (to_run)
        HandleCarOn();
      else if (to_charge)
        HandleCharging();
    case PollState::RUNNING:
      if (to_off)
        HandleCarOff();
      else if (to_charge)
        HandleCharging();
    case PollState::CHARGING:
      if (to_off)
      {
        HandleChargeStop();
        HandleCarOff();
      }
      else if (to_run)
      {
        HandleChargeStop();
        HandleCarOn();
      }
  }
}

OvmsVehicleMaxe6::PollState OvmsVehicleMaxe6::GetPollState()
{
  switch (m_poll_state)
  {
    case 0:
      return PollState::OFF;
    case 1:
      return PollState::RUNNING;
    case 2:
      return PollState::CHARGING;
    default:
      assert (false);
      return PollState::OFF;
  }
}

void OvmsVehicleMaxe6::HandleCharging()
{
  PollSetState(2); // CHARGING
  ESP_LOGI(TAG, "CAR IS CHARGING | POLLSTATE CHARGING");

  SetChargeType();
}

void OvmsVehicleMaxe6::HandleChargeStop()
{  
  ESP_LOGI(TAG, "CAR CHARGING STOPPED");
  ResetChargeType();
}

void OvmsVehicleMaxe6::HandleCarOn()
{
  PollSetState(1); // RUNNING
  ESP_LOGI(TAG, "CAR IS ON | POLLSTATE RUNNING");
}

void OvmsVehicleMaxe6::HandleCarOff()
{
  PollSetState(0); // OFF
  ESP_LOGI(TAG, "CAR IS OFF | POLLSTATE OFF");
}

void OvmsVehicleMaxe6::SetChargeType()
{
  auto using_css = StdMetrics.ms_v_bat_power->AsFloat(0, kW) < -15;
  StdMetrics.ms_v_charge_type->SetValue(using_css ? "CCS" : "Type2");
}

void OvmsVehicleMaxe6::ResetChargeType()
{
  StdMetrics.ms_v_charge_type->SetValue("");
}

class OvmsVehicleMaxe6Init
{
public: OvmsVehicleMaxe6Init();
} OvmsVehicleMaxe6Init  __attribute__ ((init_priority (9000)));

OvmsVehicleMaxe6Init::OvmsVehicleMaxe6Init()
{
  ESP_LOGI(TAG, "Registering Vehicle: Maxus Euniq 6 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxe6>("ME6","Maxus Euniq 6");
}
