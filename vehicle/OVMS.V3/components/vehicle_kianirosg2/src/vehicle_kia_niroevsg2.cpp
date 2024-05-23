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

#define VERSION "0.1.6"

static const char *TAG = "v-kianiroevsg2";

// Pollstate 0 - car is off
// Pollstate 1 - car is on
// Pollstate 2 - car is charging
static const OvmsPoller::poll_pid_t vehicle_kianiroevsg2_polls[] =
	{
		// ok2	IncomingIGMP
		{0x770, 0x778, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xbc03, {2, 2, 2}, 1, ISOTP_STD}, // IGMP Door status and lock
		{0x770, 0x778, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xbc04, {2, 2, 2}, 1, ISOTP_STD}, // IGMP Door lock

		// ok2 IncomingBCM
		{0x7a0, 0x7a8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xC00B, {0, 25, 0}, 1, ISOTP_STD}, // TMPS - Pressure
		{0x7a0, 0x7a8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB010, {0, 25, 0}, 1, ISOTP_STD}, // TMPS - Pressure

		// ok2	IncomingVMCU
		{0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02, {600, 25, 25}, 1, ISOTP_STD}, // VMCU - Aux Battery voltage

		// ok2	IncomingBMC
		{0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0101, {5, 2, 2}, 1, ISOTP_STD}, // Voltage and current main bat
		{0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0105, {0, 5, 5}, 1, ISOTP_STD}, // bat soc and soh

		// ok2	IncomingCM
		{0x7c6, 0x7ce, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB002, {0, 25, 60}, 1, ISOTP_STD}, // ODO
		{0x7c6, 0x7ce, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB003, {2, 2, 2}, 1, ISOTP_STD},	// AWAKE

		// ok2	IncomingSteeringwheel
		{0x7d4, 0x7dc, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0101, {2, 2, 2}, 1, ISOTP_STD}, // ON SPEED
		POLL_LIST_END};

static const OvmsPoller::poll_pid_t vehicle_kianiroevsg2_polls_stop[] = {POLL_LIST_END};

/**
 * Constructor for Kia Niro EV OvmsVehicleKiaNiroEvSg2
 */
OvmsVehicleKiaNiroEvSg2::OvmsVehicleKiaNiroEvSg2()
{
	ESP_LOGI(TAG, "Kia Niro Sg2 v1.0 vehicle module");

	message_send_can.id = 0;
	message_send_can.status = 0;
	can_2_sending = true;
	should_poll = true;

	memset(message_send_can.byte, 0, sizeof(message_send_can.byte));
	lock_command = false;
	unlock_command = false;
	// SetParamValue
	// SetParamValueBinary
	// SetParamValueInt
	// SetParamValueFloat
	// SetParamValueBool
	// GetParamValue\(.*, .*, ".*"\);

	// init metrics:
	m_v_door_lock_fl = MyMetrics.InitBool("xkn.v.door.lock.front.left", 10, 0);
	m_v_door_lock_fr = MyMetrics.InitBool("xkn.v.door.lock.front.right", 10, 0);
	m_v_door_lock_rl = MyMetrics.InitBool("xkn.v.door.lock.rear.left", 10, 0);
	m_v_door_lock_rr = MyMetrics.InitBool("xkn.v.door.lock.rear.right", 10, 0);
	m_v_polling = MyMetrics.InitBool("xkn.v.polling", 10, 0);

	StdMetrics.ms_v_bat_12v_voltage->SetValue(12.5, Volts);
	StdMetrics.ms_v_charge_inprogress->SetValue(false);
	StdMetrics.ms_v_env_on->SetValue(false);
	StdMetrics.ms_v_bat_temp->SetValue(20, Celcius);

	// Require GPS.
	MyEvents.SignalEvent("vehicle.require.gps", NULL);
	MyEvents.SignalEvent("vehicle.require.gpstime", NULL);

	using std::placeholders::_1;
	using std::placeholders::_2;

	// #ifdef CONFIG_OVMS_COMP_WEBSERVER
	// 	WebInit();
	// #endif
	// D-Bus
	PollSetThrottling(6);
	RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
	RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
	POLLSTATE_OFF;
	PollSetPidList(m_can1, vehicle_kianiroevsg2_polls);
	m_v_polling->SetValue(true);
}

/**
 * Destructor
 */
OvmsVehicleKiaNiroEvSg2::~OvmsVehicleKiaNiroEvSg2()
{
	ESP_LOGI(TAG, "Shutdown Kia Sg2 EV vehicle module");
	// MyWebServer.DeregisterPage("/bms/cellmon");
}

/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsVehicleKiaNiroEvSg2::HandleCarOn()
{
	POLLSTATE_RUNNING;
	ESP_LOGI(TAG, "CAR IS ON | POLLSTATE RUNNING");
}
void OvmsVehicleKiaNiroEvSg2::HandleCarOff()
{
	POLLSTATE_OFF;
	ESP_LOGI(TAG, "CAR IS OFF | POLLSTATE OFF");
	StdMetrics.ms_v_pos_speed->SetValue(0);
}
void OvmsVehicleKiaNiroEvSg2::HandleCharging()
{
	POLLSTATE_CHARGING;
	ESP_LOGI(TAG, "CAR IS CHARGING | POLLSTATE RUNNING");
}

/**
 * Ticker1: Called every second
 */
void OvmsVehicleKiaNiroEvSg2::Ticker1(uint32_t ticker)
{
	VerifyCanActivity();
	can_2_sending = false;

	// Register car as locked only if all doors are locked
	StdMetrics.ms_v_env_locked->SetValue(
		m_v_door_lock_fr->AsBool() &
		m_v_door_lock_fl->AsBool() &
		m_v_door_lock_rr->AsBool() &
		m_v_door_lock_rl->AsBool());

	StdMetrics.ms_v_bat_power->SetValue(
		StdMetrics.ms_v_bat_voltage->AsFloat(400, Volts) *
			StdMetrics.ms_v_bat_current->AsFloat(1, Amps) / 1000,
		kW);
	StdMetrics.ms_v_charge_inprogress->SetValue(
		(StdMetrics.ms_v_pos_speed->AsFloat(0) < 1) &
		(StdMetrics.ms_v_bat_power->AsFloat(0, kW) < -1));

	if (m_poll_state == 0)
	{
		// ESP_LOGI(TAG, "POLL STATE OFF");
	}
	else if (m_poll_state == 1)
	{
		// ESP_LOGI(TAG, "POLL STATE RUNNING");
	}
	else if (m_poll_state == 2)
	{
		// ESP_LOGI(TAG, "POLL STATE CHARGING");
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
	VerifyDoorLock();
}

/**
 * Ticker10: Called every ten seconds
 */
void OvmsVehicleKiaNiroEvSg2::Ticker10(uint32_t ticker)
{
}

/**
 * Ticker300: Called every five minutes
 */
void OvmsVehicleKiaNiroEvSg2::Ticker300(uint32_t ticker)
{
	// check ecus every 20 min even if car is off
	if (!can_2_sending && poll_counter > 1500)
	{
		can_2_sending = true;
		VerifyCanActivity();
	}
}

/**
 * Update metrics when charging stops
 */
void OvmsVehicleKiaNiroEvSg2::HandleChargeStop()
{
	ESP_LOGI(TAG, "Charging done...");
	StdMetrics.ms_v_charge_type->SetValue("");
}

/**
 *  Sets the charge metrics
 */
void OvmsVehicleKiaNiroEvSg2::SetChargeMetrics()
{
	bool ccs = StdMetrics.ms_v_bat_power->AsFloat(0, kW) < -15;
	StdMetrics.ms_v_charge_type->SetValue(ccs ? "ccs" : "type2");
}

/**
 * Open or lock the doors
 */
bool OvmsVehicleKiaNiroEvSg2::SetDoorLock(bool lock)
{
	if (lock_command || unlock_command)
	{
		return false;
	}
	bool closed_doors = StdMetrics.ms_v_door_fl->AsBool() &&
						StdMetrics.ms_v_door_fr->AsBool() &&
						StdMetrics.ms_v_door_rl->AsBool() &&
						StdMetrics.ms_v_door_rr->AsBool();
	
	if (lock)
	{
		if (closed_doors)
		{
			SendCanMessageSecondary(0x500, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF);
			vTaskDelay(pdMS_TO_TICKS(14));
			SendCanMessageSecondary(0x502, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF);
			vTaskDelay(pdMS_TO_TICKS(120));
			SendCanMessageSecondary(0x401, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
			vTaskDelay(pdMS_TO_TICKS(2));
			SendCanMessageSecondary(0x402, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
			vTaskDelay(pdMS_TO_TICKS(2));
			SendCanMessageSecondary(0x403, 0x06, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00);
			vTaskDelay(pdMS_TO_TICKS(2));
			SendCanMessageSecondary(0x4D6, 0xDD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
			vTaskDelay(pdMS_TO_TICKS(50));
			SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0x7F, 0x00);
			vTaskDelay(pdMS_TO_TICKS(5));
			SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0x7F, 0x00);
			vTaskDelay(pdMS_TO_TICKS(28));
			SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0x7F, 0x00);
			lock_command = true;
			lock_counter = 6;
		}
		else
		{
			return false;
		}
	}
	else
	{
		SendCanMessageSecondary(0x500, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF);
		vTaskDelay(pdMS_TO_TICKS(14));
		SendCanMessageSecondary(0x502, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF);
		vTaskDelay(pdMS_TO_TICKS(120));
		SendCanMessageSecondary(0x401, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x402, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x403, 0x06, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x4D6, 0xDD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(50));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0xDF, 0x00);
		vTaskDelay(pdMS_TO_TICKS(5));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0xDF, 0x00);
		vTaskDelay(pdMS_TO_TICKS(28));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0xDF, 0x00);
		unlock_command = true;
		lock_counter = 6;
	}
	m_can2->Reset();
	return true;
}

void OvmsVehicleKiaNiroEvSg2::VerifyCanActivity()
{
	return;
	// poll_counter++;
	// if (can_2_sending)
	// {
	// 	if (!should_poll)
	// 	{
	// 		should_poll = true;
	// 		ESP_LOGI(TAG, "Re-enable polling...");
	// 		PollSetPidList(m_can1, vehicle_kianiroevsg2_polls);
	// 		m_v_polling->SetValue(true);
	// 	}
	// 	poll_counter = 0;
	// }
	// else
	// {
	// 	if (should_poll)
	// 	{
	// 		if (poll_counter > 300)
	// 		{
	// 			should_poll = false;
	// 			ESP_LOGI(TAG, "Disable polling...");
	// 			PollSetPidList(m_can1, vehicle_kianiroevsg2_polls_stop);
	// 			m_v_polling->SetValue(false);
	// 			poll_counter = 0;
	// 		}
	// 	}
	// }
}

void OvmsVehicleKiaNiroEvSg2::VerifyDoorLock()
{
	if (!(lock_command || unlock_command))
		return;
	if (lock_counter <= 0)
	{
		lock_counter = 0;
		lock_command = false;
		unlock_command = false;
		return;
	}
	lock_counter--;
	bool is_locked = StdMetrics.ms_v_env_locked->AsBool();
	if (lock_command && is_locked)
	{
		lock_counter = 0;
		lock_command = false;
		return;
	}
	if (unlock_command && !is_locked)
	{
		lock_counter = 0;
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
		SendCanMessageSecondary(0x500, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF);
		vTaskDelay(pdMS_TO_TICKS(14));
		SendCanMessageSecondary(0x502, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF);
		vTaskDelay(pdMS_TO_TICKS(120));
		SendCanMessageSecondary(0x401, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x402, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x403, 0x06, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x4D6, 0xDD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(50));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0x7F, 0x00);
		vTaskDelay(pdMS_TO_TICKS(5));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0x7F, 0x00);
		vTaskDelay(pdMS_TO_TICKS(28));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0x7F, 0x00);
	}
	else if (unlock_command)
	{
		SendCanMessageSecondary(0x500, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF);
		vTaskDelay(pdMS_TO_TICKS(14));
		SendCanMessageSecondary(0x502, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF);
		vTaskDelay(pdMS_TO_TICKS(120));
		SendCanMessageSecondary(0x401, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x402, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x403, 0x06, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(2));
		SendCanMessageSecondary(0x4D6, 0xDD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		vTaskDelay(pdMS_TO_TICKS(50));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0xDF, 0x00);
		vTaskDelay(pdMS_TO_TICKS(5));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0xDF, 0x00);
		vTaskDelay(pdMS_TO_TICKS(28));
		SendCanMessageSecondary(0x4A2, 0x00, 0x00, 0xFC, 0xC0, 0xFF, 0xFF, 0xDF, 0x00);
	}
	return;
}

class OvmsVehicleKiaNiroEvSg2Init
{
public:
	OvmsVehicleKiaNiroEvSg2Init();
} MyOvmsVehicleKiaNiroEvSg2Init __attribute__((init_priority(9000)));

OvmsVehicleKiaNiroEvSg2Init::OvmsVehicleKiaNiroEvSg2Init()
{
	ESP_LOGI(TAG, "Registering Vehicle: Kia Niro Sg2 EV (9000)");
	MyVehicleFactory.RegisterVehicle<OvmsVehicleKiaNiroEvSg2>("KN2", "Kia Niro Sg2 EV");
}
