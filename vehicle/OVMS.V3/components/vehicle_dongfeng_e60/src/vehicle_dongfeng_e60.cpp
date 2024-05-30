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

#include "vehicle_dongfeng_e60.h"

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

static const char *TAG = "v-dfe60";

// Pollstate 0 - car is off
// Pollstate 1 - car is on
// Pollstate 2 - car is charging

OvmsVehicleDFE60::OvmsVehicleDFE60()
{
	ESP_LOGI(TAG, "Dongfeng E60 vehicle module");

	message_send_can.id = 0;
	message_send_can.status = 0;

	memset(message_send_can.byte, 0, sizeof(message_send_can.byte));
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
	MyEvents.RegisterEvent(TAG, "app.connected", std::bind(&OvmsVehicleDFE60::EventListener, this, _1, _2));

	// #ifdef CONFIG_OVMS_COMP_WEBSERVER
	// 	WebInit();
	// #endif
	RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
	POLLSTATE_OFF;
}

/**
 * Destructor
 */
OvmsVehicleDFE60::~OvmsVehicleDFE60()
{
	ESP_LOGI(TAG, "Shutdown Dongfeng E60 vehicle module");
	// MyWebServer.DeregisterPage("/bms/cellmon");
}
//  housekeeping: Auto init inhibited: too many early crashes

/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsVehicleDFE60::HandleCarOn()
{
	POLLSTATE_RUNNING;
	ESP_LOGI(TAG, "CAR IS ON | POLLSTATE RUNNING");
}
void OvmsVehicleDFE60::HandleCarOff()
{
	POLLSTATE_OFF;
	ESP_LOGI(TAG, "CAR IS OFF | POLLSTATE OFF");
	StdMetrics.ms_v_pos_speed->SetValue(0);
}
void OvmsVehicleDFE60::HandleCharging()
{
	POLLSTATE_CHARGING;
	ESP_LOGI(TAG, "CAR IS CHARGING | POLLSTATE RUNNING");
}

/**
 * Ticker1: Called every second
 */
void OvmsVehicleDFE60::Ticker1(uint32_t ticker)
{
	VerifyConfigs(true);
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
}

/**
 * Ticker10: Called every ten seconds
 */
void OvmsVehicleDFE60::Ticker10(uint32_t ticker)
{
}

/**
 * Ticker300: Called every five minutes
 */
void OvmsVehicleDFE60::Ticker300(uint32_t ticker)
{
	VerifyConfigs(false);
}

void OvmsVehicleDFE60::EventListener(std::string event, void *data)
{
}

/**
 * Update metrics when charging stops
 */
void OvmsVehicleDFE60::HandleChargeStop()
{
	ESP_LOGI(TAG, "Charging done...");
	StdMetrics.ms_v_charge_type->SetValue("");
}

/**
 *  Sets the charge metrics
 */
void OvmsVehicleDFE60::SetChargeMetrics()
{
	bool ccs = StdMetrics.ms_v_bat_power->AsFloat(0, kW) < -15;
	StdMetrics.ms_v_charge_type->SetValue(ccs ? "ccs" : "type2");
}

/**
 * Open or lock the doors
 */
bool OvmsVehicleDFE60::SetDoorLock(bool lock)
{
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
			// MyPeripherals->m_max7317->Output(9, 0);
			// vTaskDelay(pdMS_TO_TICKS(1000));
			// MyPeripherals->m_max7317->Output(9, 255);
			return true;
		}
		return false;
	}
	else
	{
		// MyPeripherals->m_max7317->Output(8, 0);
		// vTaskDelay(pdMS_TO_TICKS(1000));
		// MyPeripherals->m_max7317->Output(8, 255);
	return true;
	}
	return false;
}

class OvmsVehicleDFE60Init
{
public:
	OvmsVehicleDFE60Init();
} MyOvmsVehicleDFE60Init __attribute__((init_priority(9000)));

OvmsVehicleDFE60Init::OvmsVehicleDFE60Init()
{
	ESP_LOGI(TAG, "Registering Vehicle: DongFeng E60");
	MyVehicleFactory.RegisterVehicle<OvmsVehicleDFE60>("DFE60", "DongFeng E60");
}
