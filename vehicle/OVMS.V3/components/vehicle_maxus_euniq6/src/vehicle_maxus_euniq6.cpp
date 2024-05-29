/*
;    Project:       Open Vehicle Monitor System
;    Date:          21th January 2019
;
;    Changes:
;    0.0.1  Initial stub
;			- Ported from Kia Soul. Totally untested.
;
;    0.1.0  First version on par with Soul
;			- First "complete" version.
;
;		0.1.1 06-apr-2019 - Geir Øyvind Vælidalo
;			- Minor changes after proper real life testing
;			- VIN is working
;			- Removed more of the polling when car is off in order to prevent AUX battery drain
;
;		0.1.2 10-apr-2019 - Geir Øyvind Vælidalo
;			- Fixed TPMS reading
;			- Fixed xks aux
;			- Estimated range show WLTP in lack of the actual displayed range
;			- Door lock works even after ECU goes to sleep.
;
;		0.1.3 12-apr-2019 - Geir Øyvind Vælidalo
;			- Fixed OBC temp reading
;			- Removed a couple of pollings when car is off, in order to save AUX battery
;			- Added range calculator for estimated range instead of WLTP. It now uses 20 last trips as a basis.
;
;		0.1.5 18-apr-2019 - Geir Øyvind Vælidalo
;			- Changed poll frequencies to minimize the strain on the CAN-write function.
;
;		0.1.6 20-apr-2019 - Geir Øyvind Vælidalo
;			- AUX Battery monitor..
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2019       Geir Øyvind Vælidalo <geir@validalo.net>
;;
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

#include "ovms_log.h"
#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"
#include <sys/param.h>
#include "common.h"
#include "ovms_boot.h"
#include "ovms_events.h"
#include "ovms_peripherals.h"

#define VERSION "0.1.6"

static const char *TAG = "v-maxeu6";

// Pollstate 0 - car is off
// Pollstate 1 - car is on
// Pollstate 2 - car is charging

static const OvmsPoller::poll_pid_t vehicle_maxusEU6_polls[] =
	{
		// ok2	IPK
		// {0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE102, {0, 1, 0}, 1, ISOTP_STD}, // Speed
		// {0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE107, {0, 0, 1}, 1, ISOTP_STD}, // SOC
		// {0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB101, {0, 1, 0}, 1, ISOTP_STD}, // ODO

		{0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB110, {0, 2, 0}, 1, ISOTP_STD}, // TPMS
		{0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB111, {0, 2, 0}, 1, ISOTP_STD}, // TPMS
		{0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB112, {0, 2, 0}, 1, ISOTP_STD}, // TPMS
		{0x700, 0x780, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB113, {0, 2, 0}, 1, ISOTP_STD}, // TPMS

		// ok2	IncomingCM
		{0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE010, {0, 1, 1}, 1, ISOTP_STD}, // Current
		{0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE004, {0, 1, 1}, 1, ISOTP_STD}, // Voltage
		{0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE014, {0, 1, 1}, 1, ISOTP_STD}, // SOC
		POLL_LIST_END};

OvmsVehicleMaxEu6::OvmsVehicleMaxEu6()
{
	ESP_LOGI(TAG, "Maxus Euniq 6 vehicle module");

	message_send_can.id = 0;
	message_send_can.status = 0;

	memset(message_send_can.byte, 0, sizeof(message_send_can.byte));
	lock_command = false;
	unlock_command = false;
	fully_configured = false;
	reset_by_config = false;

	StdMetrics.ms_v_bat_12v_voltage->SetValue(12.5, Volts);
	StdMetrics.ms_v_charge_inprogress->SetValue(false);
	StdMetrics.ms_v_env_on->SetValue(false);
	StdMetrics.ms_v_bat_temp->SetValue(20, Celcius);

	// Require GPS.
	MyEvents.SignalEvent("vehicle.require.gps", NULL);
	MyEvents.SignalEvent("vehicle.require.gpstime", NULL);

	using std::placeholders::_1;
	using std::placeholders::_2;
	MyEvents.RegisterEvent(TAG, "app.connected", std::bind(&OvmsVehicleMaxEu6::EventListener, this, _1, _2));

	// #ifdef CONFIG_OVMS_COMP_WEBSERVER
	// 	WebInit();
	// #endif
	PollSetThrottling(10);
	RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
	RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
	POLLSTATE_OFF;
	PollSetPidList(m_can1, vehicle_maxusEU6_polls);
}

/**
 * Destructor
 */
OvmsVehicleMaxEu6::~OvmsVehicleMaxEu6()
{
	ESP_LOGI(TAG, "Shutdown Maxus Euniq 6 vehicle module");
	// MyWebServer.DeregisterPage("/bms/cellmon");
}
//  housekeeping: Auto init inhibited: too many early crashes

/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsVehicleMaxEu6::HandleCarOn()
{
	POLLSTATE_RUNNING;
	ESP_LOGI(TAG, "CAR IS ON | POLLSTATE RUNNING");
}
void OvmsVehicleMaxEu6::HandleCarOff()
{
	POLLSTATE_OFF;
	ESP_LOGI(TAG, "CAR IS OFF | POLLSTATE OFF");
	StdMetrics.ms_v_pos_speed->SetValue(0);
}
void OvmsVehicleMaxEu6::HandleCharging()
{
	POLLSTATE_CHARGING;
	ESP_LOGI(TAG, "CAR IS CHARGING | POLLSTATE RUNNING");
}

/**
 * Ticker1: Called every second
 */
void OvmsVehicleMaxEu6::Ticker1(uint32_t ticker)
{
	VerifyConfigs(true);
	StdMetrics.ms_v_bat_power->SetValue(
		StdMetrics.ms_v_bat_voltage->AsFloat(400, Volts) *
			StdMetrics.ms_v_bat_current->AsFloat(1, Amps) / 1000,
		kW);

	if (m_poll_state == 0)
	{
		// ESP_LOGE(TAG, "POLL STATE OFF");
	}
	else if (m_poll_state == 1)
	{
		// ESP_LOGE(TAG, "POLL STATE RUNNING");
	}
	else if (m_poll_state == 2)
	{
		// ESP_LOGE(TAG, "POLL STATE CHARGING");
		if (!StdMetrics.ms_v_charge_inprogress->AsBool())
		{
			HandleChargeStop();
		}
		else
		{
			SetChargeMetrics();
		}
	}

	if (m_poll_state != 0 && !StdMetrics.ms_v_env_on->AsBool() && !StdMetrics.ms_v_charge_inprogress->AsBool())
	{
		HandleCarOff();
	}
	else if (m_poll_state != 1 && StdMetrics.ms_v_env_on->AsBool() && !StdMetrics.ms_v_charge_inprogress->AsBool())
	{
		HandleCarOn();
	}
	else if (m_poll_state != 2 && StdMetrics.ms_v_charge_inprogress->AsBool())
	{
		HandleCharging();
	}
	CheckLock();
}

/**
 * Ticker10: Called every ten seconds
 */
void OvmsVehicleMaxEu6::Ticker10(uint32_t ticker)
{
}

/**
 * Ticker300: Called every five minutes
 */
void OvmsVehicleMaxEu6::Ticker300(uint32_t ticker)
{
	VerifyConfigs(false);
}

void OvmsVehicleMaxEu6::EventListener(std::string event, void *data)
{
}

/**
 * Update metrics when charging stops
 */
void OvmsVehicleMaxEu6::HandleChargeStop()
{
	ESP_LOGI(TAG, "Charging done...");
	StdMetrics.ms_v_charge_type->SetValue("");
}

/**
 *  Sets the charge metrics
 */
void OvmsVehicleMaxEu6::SetChargeMetrics()
{
	bool ccs = StdMetrics.ms_v_bat_power->AsFloat(0, kW) < -15;
	StdMetrics.ms_v_charge_type->SetValue(ccs ? "ccs" : "type2");
}

/**
 * Open or lock the doors
 */
bool OvmsVehicleMaxEu6::SetDoorLock(bool lock)
{
	if (lock_command || unlock_command)
	{
		ESP_LOGE(TAG, "Lock active, lock:%x unlock:%x", lock_command, unlock_command);
		return false;
	}
	if (lock)
	{
		bool closed_doors = !StdMetrics.ms_v_door_fl->AsBool() &&
							!StdMetrics.ms_v_door_fr->AsBool() &&
							!StdMetrics.ms_v_door_rl->AsBool() &&
							!StdMetrics.ms_v_door_rr->AsBool();
		closed_doors = closed_doors &&
					   !StdMetrics.ms_v_env_awake->AsBool() &&
					   !StdMetrics.ms_v_env_on->AsBool();
		if (closed_doors)
		{
			lock_command = true;
			lock_counter = 11;
			CanMultimpleSend(0x310, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 5, 20);
			CanMultimpleSend(0x310, 0x00, 0x02, 0x10, 0x80, 0x00, 0x00, 0x01, 0x00, 10, 20);
			CanMultimpleSend(0x310, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0x01, 0x00, 40, 20);
			return true;
		}
		return false;
	}
	else
	{
		unlock_command = true;
		lock_counter = 11;
		CanMultimpleSend(0x310, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 5, 20);
		CanMultimpleSend(0x310, 0x00, 0x01, 0x10, 0x80, 0x00, 0x00, 0x01, 0x00, 10, 20);
		CanMultimpleSend(0x310, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0x01, 0x00, 40, 20);
		return true;
	}
	return false;
}

void OvmsVehicleMaxEu6::CheckLock()
{
	if (!(lock_command || unlock_command))
	{
		return;
	}
	if (lock_counter <= 0)
	{
		lock_counter = 0;
		lock_command = false;
		unlock_command = false;
		return;
	}
	lock_counter--;
	bool is_locked = StdMetrics.ms_v_env_locked->AsBool();
	if ((lock_command && is_locked) || (unlock_command && !is_locked))
	{
		lock_counter = 0;
		lock_command = false;
		unlock_command = false;
		return;
	}
	if (lock_counter % 2 == 1)
	{
		m_can2->Reset();
		return;
	}

	if (lock_command)
	{
		CanMultimpleSend(0x310, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 5, 20);
		CanMultimpleSend(0x310, 0x00, 0x02, 0x10, 0x80, 0x00, 0x00, 0x01, 0x00, 10, 20);
		CanMultimpleSend(0x310, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0x01, 0x00, 40, 20);
	}
	else if (unlock_command)
	{
		CanMultimpleSend(0x310, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 5, 20);
		CanMultimpleSend(0x310, 0x00, 0x01, 0x10, 0x80, 0x00, 0x00, 0x01, 0x00, 10, 20);
		CanMultimpleSend(0x310, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0x01, 0x00, 40, 20);
	}
}

class OvmsVehicleMaxEu6Init
{
public:
	OvmsVehicleMaxEu6Init();
} MyOvmsVehicleMaxEu6Init __attribute__((init_priority(9000)));

OvmsVehicleMaxEu6Init::OvmsVehicleMaxEu6Init()
{
	ESP_LOGI(TAG, "Registering Vehicle: Maxus Euniq 6");
	MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxEu6>("ME6", "Maxus Euniq 6");
}
