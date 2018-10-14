/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2018       Marcos Mezo
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
static const char *TAG = "v-zoe";

#include <stdio.h>
#include "vehicle_renaultzoe.h"
#include "metrics_standard.h"

// Pollstate 0 - car is off
// Pollstate 1 - car is on
// Pollstate 2 - car is charging
static const OvmsVehicle::poll_pid_t vehicle_renaultzoe_polls[] =
  {
		// Battery Rack Temperature
	  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2001, { 300, 10, 10 } },
		// SOC
	  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2002, { 300, 10, 10 } },
  	// Odometer
  	{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2006, {   0, 60,  0 } },
		// Key On/Off
  	{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x200e, {  10, 10,  0 } },
		// -END-
		{ 0, 0, 0x00, 0x00, { 0, 0, 0 } }
	};

OvmsVehicleRenaultZoe::OvmsVehicleRenaultZoe()
  {
  ESP_LOGI(TAG, "Renault Zoe Vehicle module");

  StandardMetrics.ms_v_type->SetValue("RZ");
  StandardMetrics.ms_v_bat_12v_voltage->SetValue(12.5, Volts);
  StandardMetrics.ms_v_charge_inprogress->SetValue(false);

	// Zoe CAN bus runs at 500 kbps
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

	// Poll Zoe Specific PIDs
  POLLSTATE_OFF;
  PollSetPidList(m_can1,vehicle_renaultzoe_polls);
  }

OvmsVehicleRenaultZoe::~OvmsVehicleRenaultZoe()
  {
  ESP_LOGI(TAG, "Shutdown Renault Zoe vehicle module");
  }

/**
 * Handles incoming CAN-frames on bus 1
 */
void OvmsVehicleRenaultZoe::IncomingFrameCan1(CAN_frame_t* p_frame)
	{
	//uint8_t *data = p_frame->data.u8;
	//ESP_LOGI(TAG, "PID:%x DATA: %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, data[0], data[1], data[2], data[3],
	//		data[4], data[5], data[6], data[7]);
	} 
/**
 * Handles incoming poll results
 */
void OvmsVehicleRenaultZoe::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain)
  {
	ESP_LOGI(TAG, "%03x TYPE:%x PID:%02x Lenght:%x Data:%02x %02x %02x %02x %02x %02x %02x %02x", m_poll_moduleid_low, type, pid, length, data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]);
	switch (m_poll_moduleid_low)
		{
		// ****** EVC *****
		case 0x7ec:
			IncomingEVC(bus, type, pid, data, length, remain);
			break;
		}
	}

/**
 * Handle incoming polls from the EVC Computer
 */
void OvmsVehicleRenaultZoe::IncomingEVC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain)
	{
	switch (m_poll_pid)
		{
	case 0x2001:			// Bat. rack Temperature
		StandardMetrics.ms_v_bat_temp->SetValue((float) CAN_BYTE(0)-40, Celcius);
		break;
	case 0x2002: 			// SoC
		StandardMetrics.ms_v_bat_soc->SetValue((float) CAN_UINT(0)*2/100, Percentage);
		break;
	case 0x2006:			// Odometer (Total Vehicle Distance)
		StandardMetrics.ms_v_pos_odometer->SetValue((float) CAN_UINT24(0), Kilometers);
		break;
	case 0x200e:			// Key On/Off (0 off / 1 on)
		if (CAN_BYTE(0) && 0x01 == 0x01)
		{
		car_on(true);
		}
		else
		{
		car_on(false);
		}

		break;
		}
	}


/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsVehicleRenaultZoe::car_on(bool isOn)
  {

  if (isOn && !StandardMetrics.ms_v_env_on->AsBool())
    {
		// Car is beeing turned ON
		StandardMetrics.ms_v_env_on->SetValue(isOn);
		StandardMetrics.ms_v_env_awake->SetValue(isOn);
    POLLSTATE_RUNNING;
    }
  else if(!isOn && StandardMetrics.ms_v_env_on->AsBool())
    {
    // Car is being turned OFF
    POLLSTATE_OFF;
		StandardMetrics.ms_v_env_on->SetValue( isOn );
		StandardMetrics.ms_v_env_awake->SetValue( isOn );
		StandardMetrics.ms_v_pos_speed->SetValue( 0 );
    }
  }


//-----------------------------------------------------------------------------
//
// RenaultZoeInit
//

class OvmsVehicleRenaultZoeInit
  {
  public: OvmsVehicleRenaultZoeInit();
} MyOvmsVehicleRenaultZoeInit  __attribute__ ((init_priority (9000)));

OvmsVehicleRenaultZoeInit::OvmsVehicleRenaultZoeInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Renault Zoe (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleRenaultZoe>("RZ","Renault Zoe");
  }
