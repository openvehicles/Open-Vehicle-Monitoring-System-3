/*
;    Project:       Open Vehicle Monitor System
;    Date:          22th October 2017
;
;    Changes:
;    0.1.0  Initial stub
;			- Most basic functionality is ported from V2.
;
;		 0.1.1  03-Dec-2017 - Geir Øyvind Vælidalo
;			- Added xks cells-command which prints out battery voltages
;
;		 0.1.2  03-Dec-2017 - Geir Øyvind Vælidalo
;			- Moved more ks-variables to metrics.
;
;		 0.1.3  04-Dec-2017 - Geir Øyvind Vælidalo
;			- Added Low voltage DC-DC converter metrics
;
;		 0.1.4  04-Dec-2017 - Geir Øyvind Vælidalo
;			- Added pilot duty cycle and proper charger temp from OBC.
;
;		 0.1.5  04-Dec-2017 - Geir Øyvind Vælidalo
;			- Fetch proper RPM from VMCU.
;
;		 0.1.6  06-Dec-2017 - Geir Øyvind Vælidalo
;			- Added some verbosity-handling in CELLS and TRIP. Plus other minor changes.
;
;		 0.1.7  07-Dec-2017 - Geir Øyvind Vælidalo
;			- All doors added (except trunk).
;			- Daylight led driving lights, low beam and highbeam added.
;
;		 0.1.8  08-Dec-2017 - Geir Øyvind Vælidalo
;			- Added charge speed in km/h, adjusted for temperature.
;
;		 0.1.9  09-Dec-2017 - Geir Øyvind Vælidalo
;			- Added reading of door lock.
;
;		 0.2.0  12-Dec-2017 - Geir Øyvind Vælidalo
;			- x.ks.BatteryCapacity-parameter renamed to x.ks.acp_cap_kwh to mimic Renault Twizy naming.
;
;		 0.2.1  12-Dec-2017 - Geir Øyvind Vælidalo
;			- Kia Soul EV requires OVMS GPS, since we don't know how to access the cars own GPS at the moment.
;
;		 0.2.2  15-Dec-2017 - Geir Øyvind Vælidalo
;			- Minor bugfixes after quick in-car test.
;
;		 0.2.3  17-Dec-2017 - Geir Øyvind Vælidalo
;			- Open trunk, lock/unlock doors and open chargeport + minor changes.
;
;		 0.2.4  18-Dec-2017 - Geir Øyvind Vælidalo
;			- Added xks sjb - command + RearDefogger, Leftindicator, right indicator.
;
;		 0.2.5  22-Dec-2017 - Geir Øyvind Vælidalo
;			- "Trunk"-button of keyfob opens up the chargeport
;			- Added temporary, developer-commands: sjb and bcm
;			- Added methods for controlling ACC-relay, IGN1-relay, IGN2-relay and start-relay.
;
;		 0.2.6  22-Dec-2017 - Geir Øyvind Vælidalo
;			- Electric park break service-command
;
;		 0.2.7  22-Dec-2017 - Geir Øyvind Vælidalo
;			- New config: xks.remote_charge_port - enables the possibility to open charge port from keyfob
;			- Renamed config-root from x.ks to xks
;
;		 0.2.8  22-Dec-2017 - Geir Øyvind Vælidalo
;			- Renamed xks to ks - Config, metric list and command.
;
;		 0.2.9  24-Dec-2017 - Geir Øyvind Vælidalo
;			- Renamed ks back to xks - Config, metric list and command.
;
;		 0.3.0  25-Dec-2017 - Geir Øyvind Vælidalo
;			- VIN is tested and is working.
;			- Added xks vin command which displays decoded VIN data.
;			- V2 Feature-handling
;			- Unlock doors and trunk now works even when car is off.
;
;		 0.3.1  29-Dec-2017 - Geir Øyvind Vælidalo
;			- Added inside temperature. C-can: 654h byte 7.
;			- First data from M-bus.
;			- Splitted source into five new files.
;
;		 0.3.2  01-Jan-2018 - Geir Øyvind Vælidalo
;			- Minor clean ups.
;			- Fixed some incorrect unit types in commands.
;
;		 0.3.3  17-Jan-2018 - Geir Øyvind Vælidalo
;		  - Added
;					- seat belt status
;		  			- traction control status
;					- steering mode
;					- Preheat timer status and charge timer status
;					- Cruise control
;
;		 0.3.4  27-Jan-2018 - Geir Øyvind Vælidalo
;			- Added commands for settings (as on cluster)
;					- headlightdelay;
;					- onetouchturnsignal
;					- autodoorunlock
;					- autodoorlock
;			- Fixed seat belt status and traction control which was inverted.
;			- Charge time start
;			- Power usage
;			- Hvac status (M-bus)
;			- Front Door lock status
;			- Trip consumption: kWh/100km & km/kWh.
;
;		0.3.5	3-May-2018 - Geir Øyvind Vælidalo
;			- Added proper pincode to some commands.
;
;		0.3.6 10-May-2018 - Geir Øyvind Vælidalo
;			- Moved pincode to password-store for more secure storage, and renamed it pin.
;						Use: OVMS# config set password pin 1234
;
;		0.3.7 15-May-2018 - Geir Øyvind Vælidalo
;			- Added Kia Soul web configuration: Features, Battery
;			- Trip returns distance in units based on users settings.
;			- Corrected LDC-metrics.
;			- Changed Ticker1 in order to do less processing when car is off and hopefully save the 12V battery.
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Geir Øyvind Vælidalo <geir@validalo.net>
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

//TODO
//		- parkbreakservice is not working
//		- Rear defogger works, but only as long as TesterPresent is sent.
//			- Is it enough to send the testetpresent message every second?


#include "ovms_log.h"

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_kiasoulev.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"
#include <sys/param.h>

#define VERSION "0.3.7"

static const char *TAG = "v-kiasoulev";

// Pollstate 0 - car is off
// Pollstate 1 - car is on
// Pollstate 2 - car is charging
static const OvmsVehicle::poll_pid_t vehicle_kiasoulev_polls[] =
  {
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIVEHICLE,  0x02, 		{  120, 120,   0 } }, 	// VIN
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x01, 		{  0,    10,  10 } }, 	// BMC Diag page 01
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, 		{  0,    30,  10 } }, 	// BMC Diag page 02
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x03, 		{  0,    30,  10 } }, 	// BMC Diag page 03
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x04, 		{  0,    30,  10 } }, 	// BMC Diag page 04
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x05, 		{  0,    30,  10 } },		// BMC Diag page 05
    { 0x794, 0x79c, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, 		{  120,  30,  10 } }, 	// OBC - On board charger
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x00, 		{  120,  10,  10 } }, 	// VMCU Shift-stick
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, 		{  120,  10,  30 } }, 	// VMCU Motor temp++
    { 0x7df, 0x7de, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x06, 		{  0,    30,  60 } }, 	// TMPS
    { 0x7c5, 0x7cd, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x01, 		{  120,  10,  10 } }, 	// LDC - Low voltage DC-DC
    { 0, 0, 0, 0, { 0, 0, 0 } }
  };

/**
 * Constructor for Kia Soul EV
 */
OvmsVehicleKiaSoulEv::OvmsVehicleKiaSoulEv()
  {
  ESP_LOGI(TAG, "Kia Soul EV v3.0 vehicle module");

  StopTesterPresentMessages();

  memset( m_vin, 0, sizeof(m_vin));
  memset( m_street, 0, sizeof(m_street));

  memset( ks_battery_cell_voltage ,0, sizeof(ks_battery_cell_voltage));
  memset( ks_battery_module_temp, 0, sizeof(ks_battery_module_temp));
  memset( ks_tpms_id, 0, sizeof(ks_tpms_id));

  ks_obc_volt = 230;
  ks_battery_current = 0;

  ks_battery_cum_charge_current = 0;
  ks_battery_cum_discharge_current = 0;
  ks_battery_cum_charge = 0;
  ks_battery_cum_discharge = 0;
  ks_battery_cum_op_time = 0;

  ks_trip_start_odo = 0;
  ks_start_cdc = 0;
  ks_start_cc = 0;

  ks_charge_bits.ChargingChademo = false;
  ks_charge_bits.ChargingJ1772 = false;
  ks_charge_bits.FanStatus = 0;

  ks_heatsink_temperature = 0;
  ks_battery_fan_feedback = 0;
  ks_aux_bat_ok = true;

  ks_send_can.id = 0;
  ks_send_can.status = 0;
  ks_clock = 0;
  ks_utc_diff = 0;
  ks_openChargePort = false;
  ks_emergency_message_sent = false;

  memset( ks_send_can.byte, 0, sizeof(ks_send_can.byte));

  ks_maxrange = CFG_DEFAULT_MAXRANGE;

  // init metrics:
  m_version = MyMetrics.InitString("xks.version", 0, VERSION " " __DATE__ " " __TIME__);
  m_b_cell_volt_max = MyMetrics.InitFloat("xks.b.cell.volt.max", 10, 0, Volts);
  m_b_cell_volt_min = MyMetrics.InitFloat("xks.b.cell.volt.min", 10, 0, Volts);
  m_b_cell_volt_max_no = MyMetrics.InitInt("xks.b.cell.volt.max.no", 10, 0);
  m_b_cell_volt_min_no = MyMetrics.InitInt("xks.b.cell.volt.min.no", 10, 0);
  m_b_cell_det_max = MyMetrics.InitFloat("xks.b.cell.det.max", 0, 0, Percentage);
  m_b_cell_det_min = MyMetrics.InitFloat("xks.b.cell.det.min", 0, 0, Percentage);
  m_b_cell_det_max_no = MyMetrics.InitInt("xks.b.cell.det.max.no", 10, 0);
  m_b_cell_det_min_no = MyMetrics.InitInt("xks.b.cell.det.min.no", 10, 0);
  m_c_power = MyMetrics.InitFloat("xks.c.power", 10, 0, kW);
  m_c_speed = MyMetrics.InitFloat("xks.c.speed", 10, 0, Kph);
  m_b_min_temperature = MyMetrics.InitInt("xks.b.min.temp", 10, 0, Celcius);
  m_b_max_temperature = MyMetrics.InitInt("xks.b.max.temp", 10, 0, Celcius);
  m_b_inlet_temperature = MyMetrics.InitInt("xks.b.inlet.temp", 10, 0, Celcius);
  m_b_heat_1_temperature = MyMetrics.InitInt("xks.b.heat1.temp", 10, 0, Celcius);
  m_b_heat_2_temperature = MyMetrics.InitInt("xks.b.heat2.temp", 10, 0, Celcius);
  m_b_bms_soc = MyMetrics.InitFloat("xks.b.bms.soc", 10, 0, Percentage);

  m_ldc_out_voltage = MyMetrics.InitFloat("xks.ldc.out.volt", 10, 12, Volts);
  m_ldc_in_voltage = MyMetrics.InitFloat("xks.ldc.in.volt", 10, 12, Volts);
  m_ldc_out_current = MyMetrics.InitFloat("xks.ldc.out.amps", 10, 0, Amps);
  m_ldc_temperature = MyMetrics.InitFloat("xks.ldc.temp", 10, 0, Celcius);

  m_obc_pilot_duty = MyMetrics.InitFloat("xks.obc.pilot.duty", 10, 0, Percentage);

  m_obc_timer_enabled = MyMetrics.InitBool("xks.obc.timer.enabled", 10, 0);

  m_v_env_lowbeam = MyMetrics.InitBool("xks.e.lowbeam", 10, 0);
  m_v_env_highbeam = MyMetrics.InitBool("xks.e.highbeam", 10, 0);
  m_v_env_inside_temp = MyMetrics.InitFloat("xks.e.inside.temp", 10, 0, Celcius);
  m_v_env_climate_temp = MyMetrics.InitFloat("xks.e.climate.temp", 10, 0, Celcius);
  m_v_env_climate_driver_only = MyMetrics.InitBool("xks.e.climate.driver.only", 10, 0);
  m_v_env_climate_resirc = MyMetrics.InitBool("xks.e.climate.resirc", 10, 0);
  m_v_env_climate_auto = MyMetrics.InitBool("xks.e.climate.auto", 10, 0);
  m_v_env_climate_ac = MyMetrics.InitBool("xks.e.climate.ac", 10, 0);
  m_v_env_climate_fan_speed = MyMetrics.InitInt("xks.e.climate.fan.speed", 10, 0);
  m_v_env_climate_mode = MyMetrics.InitInt("xks.e.climate.mode", 10, 0);

  m_v_preheat_timer1_enabled = MyMetrics.InitBool("xks.e.preheat.timer1.enabled", 10, 0);
  m_v_preheat_timer2_enabled = MyMetrics.InitBool("xks.e.preheat.timer2.enabled", 10, 0);
  m_v_preheating = MyMetrics.InitBool("xks.e.preheating", 10, 0);

  m_v_pos_dist_to_dest = MyMetrics.InitInt("xks.e.pos.dist.to.dest", 10, 0, Kilometers);
  m_v_pos_arrival_hour = MyMetrics.InitInt("xks.e.pos.arrival.hour", 10, 0);
  m_v_pos_arrival_minute = MyMetrics.InitInt("xks.e.pos.arrival.minute", 10, 0);
  m_v_pos_street = MyMetrics.InitString("xks.e.pos.street", 10, "");

  m_v_seat_belt_driver = MyMetrics.InitBool("xks.v.seat.belt.driver", 10, 0);
  m_v_seat_belt_passenger = MyMetrics.InitBool("xks.v.seat.belt.passenger", 10, 0);
  m_v_seat_belt_back_right = MyMetrics.InitBool("xks.v.seat.belt.back.right", 10, 0);
  m_v_seat_belt_back_middle = MyMetrics.InitBool("xks.v.seat.belt.back.middle", 10, 0);
  m_v_seat_belt_back_left = MyMetrics.InitBool("xks.v.seat.belt.back.left", 10, 0);

  m_v_traction_control = MyMetrics.InitBool("xks.v.traction.control", 10, 0);
  m_v_cruise_control = MyMetrics.InitBool("xks.v.cruise.control.enabled", 10, 0);

  m_v_emergency_lights = MyMetrics.InitBool("xks.v.emergency.lights", 10, 0);

  m_v_steering_mode = MyMetrics.InitString("xks.v.steering.mode", 0, "Unknown");

  m_v_power_usage = MyMetrics.InitFloat("xks.v.power.usage", 10, 0, kW);

  m_v_trip_consumption1 = MyMetrics.InitFloat("xks.v.trip.consumption.KWh/100km", 10, 0, Other);
  m_v_trip_consumption2 = MyMetrics.InitFloat("xks.v.trip.consumption.km/kWh", 10, 0, Other);

  m_b_cell_det_max->SetValue(0);
  m_b_cell_det_min->SetValue(0);

  StdMetrics.ms_v_bat_12v_voltage->SetValue(12.5, Volts);
  StdMetrics.ms_v_charge_inprogress->SetValue(false);
  StdMetrics.ms_v_env_on->SetValue(false);
  StdMetrics.ms_v_bat_temp->SetValue(20, Celcius);

  // init commands:
  cmd_xks = MyCommandApp.RegisterCommand("xks","Kia Soul EV",NULL,"",0,0,true);
  cmd_xks->RegisterCommand("trip","Show trip info", xks_trip, 0,0, false);
  cmd_xks->RegisterCommand("tpms","Tire pressure monitor", xks_tpms, 0,0, false);
  cmd_xks->RegisterCommand("cells","Cell voltages", xks_cells, 0,0, false);
  cmd_xks->RegisterCommand("aux","Aux battery", xks_aux, 0,0, false);
  cmd_xks->RegisterCommand("vin","VIN information", xks_vin, 0,0, false);
  cmd_xks->RegisterCommand("IGN1","IGN1 relay", xks_ign1, "<on/off><pin>",1,1, false);
  cmd_xks->RegisterCommand("IGN2","IGN2 relay", xks_ign2, "<on/off><pin>",1,1, false);
  cmd_xks->RegisterCommand("ACC","ACC relay", xks_acc_relay, "<on/off><pin>",1,1, false);
  cmd_xks->RegisterCommand("START","Start relay", xks_start_relay, "<on/off><pin>",1,1, false);
  cmd_xks->RegisterCommand("headlightdelay","Set Head Light Delay", xks_set_head_light_delay, "<on/off>",1,1, false);
  cmd_xks->RegisterCommand("onetouchturnsignal","Set one touch turn signal", xks_set_one_touch_turn_signal, "<0=Off, 1=3 blink, 2=5 blink, 3=7 blink>",1,1, false);
  cmd_xks->RegisterCommand("autodoorunlock","Set auto door unlock", xks_set_auto_door_unlock, "<1 = Off, 2 = Vehicle Off, 3 = On shift to P ,4 = Driver door unlock>",1,1, false);
  cmd_xks->RegisterCommand("autodoorlock","Set auto door lock", xks_set_auto_door_lock, "<0 =Off, 1=Enable on speed, 2=Enable on Shift>",1,1, false);

  cmd_xks->RegisterCommand("trunk","Open trunk", CommandOpenTrunk, "<pin>",1,1, false);
  cmd_xks->RegisterCommand("chargeport","Open chargeport", CommandOpenChargePort, "<pin>",1,1, false);
  cmd_xks->RegisterCommand("ParkBreakService","Enable break pad service", CommandParkBreakService, "<on/off/off2>",1,1, false);

  // For test purposes
  cmd_xks->RegisterCommand("sjb","Send command to SJB ECU", xks_sjb, "<b1><b2><b3>", 3,3, false);
  cmd_xks->RegisterCommand("bcm","Send command to BCM ECU", xks_bcm, "<b1><b2><b3>", 3,3, false);

  MyConfig.RegisterParam("xks", "Kia Soul EV spesific settings.", true, true);
  ConfigChanged(NULL);

  WebInit();

  // C-Bus
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  // M-Bus
  RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_100KBPS);

  POLLSTATE_OFF;
  PollSetPidList(m_can1,vehicle_kiasoulev_polls);
  }

/**
 * Destructor
 */
OvmsVehicleKiaSoulEv::~OvmsVehicleKiaSoulEv()
  {
  ESP_LOGI(TAG, "Shutdown Kia Soul EV vehicle module");
  }

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleKiaSoulEv::ConfigChanged(OvmsConfigParam* param)
	{
  ESP_LOGD(TAG, "Kia Soul EV reload configuration");

  // Instances:
  // xks
  //	  cap_act_kwh			Battery capacity in wH (Default: 270000)
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  //  maxrange         	Maximum ideal range at 20 °C [km] (Default: 160)
  //  remote_charge_port					Use "trunk" button on keyfob to open charge port (Default: 1=enabled)
  ks_battery_capacity = (float)MyConfig.GetParamValueInt("xks", "cap_act_kwh", CGF_DEFAULT_BATTERY_CAPACITY);
  ks_key_fob_open_charge_port = (bool)MyConfig.GetParamValueBool("xks", "remote_charge_port", true);

  ks_maxrange = MyConfig.GetParamValueInt("xks", "maxrange", CFG_DEFAULT_MAXRANGE);
  if (ks_maxrange <= 0)
    ks_maxrange = CFG_DEFAULT_MAXRANGE;

  *StdMetrics.ms_v_charge_limit_soc = (float) MyConfig.GetParamValueInt("xks", "suffsoc");
  *StdMetrics.ms_v_charge_limit_range = (float) MyConfig.GetParamValueInt("xks", "suffrange");

  //TODO ks_enable_write = MyConfig.GetParamValueBool("xks", "canwrite", false);
	}

/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsVehicleKiaSoulEv::vehicle_kiasoulev_car_on(bool isOn)
  {

  if (isOn && !StdMetrics.ms_v_env_on->AsBool())
    {
		// Car is ON
		StdMetrics.ms_v_env_on->SetValue(isOn);
		StdMetrics.ms_v_env_awake->SetValue(isOn);

		//Reset trip variables so that they are updated as soon as they are available
		ks_trip_start_odo = 0;
		ks_start_cdc = 0;
		ks_start_cc = 0;

		StdMetrics.ms_v_env_charging12v->SetValue( false );
    POLLSTATE_RUNNING;
    }
  else if(!isOn && StdMetrics.ms_v_env_on->AsBool())
    {
    // Car is OFF
    POLLSTATE_OFF;
  		StdMetrics.ms_v_env_on->SetValue( isOn );
  		StdMetrics.ms_v_env_awake->SetValue( isOn );
  		StdMetrics.ms_v_pos_speed->SetValue( 0 );
  	  StdMetrics.ms_v_pos_trip->SetValue( POS_ODO- ks_trip_start_odo );
  		StdMetrics.ms_v_env_charging12v->SetValue( false );
    }

  //Make sure we update the different start values as soon as we have them available
  if(isOn)
  		{
		// Trip started. Let's save current state as soon as they are available
  		if(ks_trip_start_odo==0 && POS_ODO!=0)
  			ks_trip_start_odo = POS_ODO;	// ODO at Start of trip
  		if(ks_start_cdc==0 && CUM_DISCHARGE!=0)
  			ks_start_cdc = CUM_DISCHARGE; // Register Cumulated discharge
  		if(ks_start_cc==0 && CUM_CHARGE!=0)
  			ks_start_cc = CUM_CHARGE; 		// Register Cumulated charge
  		}
  }

/**
 * Ticker1: Called every second
 */
void OvmsVehicleKiaSoulEv::Ticker1(uint32_t ticker)
	{
	// Open charge port if user demands it
	if( ks_openChargePort )
		{
		char buffer[6];
		OpenChargePort(itoa(MyConfig.GetParamValueInt("password","pin"), buffer, 10));
		ks_openChargePort = false;
		}

	if(m_poll_state>0) // Only do these things if car is on or charging
		{
		UpdateMaxRangeAndSOH();

		if (FULL_RANGE > 0) //  If we have the battery full range, we can calculate the ideal range too
			{
		  	StdMetrics.ms_v_bat_range_ideal->SetValue( FULL_RANGE * BAT_SOC / 100.0, Kilometers);
		  	}

		// Register battery temperature
		StdMetrics.ms_v_bat_temp->SetValue(((float)ks_battery_module_temp[0] + ks_battery_module_temp[1] +
		          ks_battery_module_temp[2] + ks_battery_module_temp[3] +
		          ks_battery_module_temp[4] + ks_battery_module_temp[5] +
		          ks_battery_module_temp[6] + ks_battery_module_temp[7]) / 8, Celcius);

		// Update trip data
		if (StdMetrics.ms_v_env_on->AsBool() && ks_trip_start_odo!=0)
			{
			StdMetrics.ms_v_pos_trip->SetValue( POS_ODO - ks_trip_start_odo , Kilometers);
			if( ks_start_cdc!=0 && ks_start_cc!=0)
				{
				StdMetrics.ms_v_bat_energy_used->SetValue( (CUM_DISCHARGE - ks_start_cdc) - (CUM_CHARGE - ks_start_cc), kWh );
				StdMetrics.ms_v_bat_energy_recd->SetValue( CUM_CHARGE - ks_start_cc, kWh );
				}
		  }

		// Charge timer on/off?
		StdMetrics.ms_v_charge_timermode->SetValue( m_obc_timer_enabled->AsBool() && !ks_charge_timer_off );

		// Cooling?
		StdMetrics.ms_v_env_cooling->SetValue (  m_v_env_climate_ac->AsBool() && StdMetrics.ms_v_env_temp->AsFloat(10,Celcius) > m_v_env_climate_temp->AsFloat(16, Celcius) );

	  if( StdMetrics.ms_v_pos_trip->AsFloat(Kilometers)>0 )
	  		m_v_trip_consumption1->SetValue( StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) * 100 / StdMetrics.ms_v_pos_trip->AsFloat(Kilometers) );
	  if( StdMetrics.ms_v_bat_energy_used->AsFloat(kWh)>0 )
	  		m_v_trip_consumption2->SetValue( StdMetrics.ms_v_pos_trip->AsFloat(Kilometers) / StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) );

		}

	//Keep charging metrics up to date
	if (ks_charge_bits.ChargingJ1772)  				// **** J1772 - Type 1  charging ****
		{
		SetChargeMetrics(ks_obc_volt, StdMetrics.ms_v_bat_power->AsFloat(0, kW) * 1000 / ks_obc_volt, 6600 / ks_obc_volt, false);
	  }
	else if (ks_charge_bits.ChargingChademo)  // **** ChaDeMo charging ****
		{
		SetChargeMetrics(StdMetrics.ms_v_bat_voltage->AsFloat(400,Volts), -ks_battery_current / 10.0, m_c_power->AsFloat(0,kW) * 10 / StdMetrics.ms_v_bat_voltage->AsFloat(400,Volts), true);
	  }


	// Check for charging status changes:
	bool isCharging = StdMetrics.ms_v_charge_pilot->AsBool()
			  						&& (ks_charge_bits.ChargingChademo || ks_charge_bits.ChargingJ1772)
									&& (CHARGE_CURRENT > 0);

	if (isCharging)
		{
		HandleCharging();
		}
	else if (!isCharging && StdMetrics.ms_v_charge_inprogress->AsBool())
		{
		HandleChargeStop();
		}

	if(!isCharging && !StdMetrics.ms_v_env_on->AsBool())
		{
		POLLSTATE_OFF;
		}

	// Reset emergency light if it is stale.
	if( m_v_emergency_lights->IsStale() ) m_v_emergency_lights->SetValue(false);

	// Notify if emergency light are turned on or off.
	if( m_v_emergency_lights->AsBool() && !ks_emergency_message_sent)
		{
		ks_emergency_message_sent = true;
		RequestNotify(SEND_EmergencyAlert);
		}
	else if( !m_v_emergency_lights->AsBool() && ks_emergency_message_sent)
		{
		ks_emergency_message_sent=false;
		RequestNotify(SEND_EmergencyAlertOff);
		}

	// Send tester present
	SendTesterPresentMessages();

	DoNotify();
	}

/**
 * Ticker10: Called every second
 */
void OvmsVehicleKiaSoulEv::Ticker10(uint32_t ticker)
	{
	// If we have clients connected, check every 10 seconds
	if( (StdMetrics.ms_s_v2_peers->AsInt() + StdMetrics.ms_s_v3_peers->AsInt())>0)
		{
		//Read door lock status
		//TODO GetDoorLockStatus();
		}

	// Calculate difference to UTC
	if( ks_clock>0 && StdMetrics.ms_m_timeutc->AsInt()>0)
		{
		ks_utc_diff = (ks_clock/60)-((StdMetrics.ms_m_timeutc->AsInt()/60) % 1440);
		}

	m_v_pos_street->SetValue(m_street);

	}

/**
 * Ticker300: Called every five minutes
 */
void OvmsVehicleKiaSoulEv::Ticker300(uint32_t ticker)
	{
	//Always read door lock status every five minutes
	GetDoorLockStatus();
	}

/**
 * Update metrics when charging
 */
void OvmsVehicleKiaSoulEv::HandleCharging()
	{
	if (!StdMetrics.ms_v_charge_inprogress->AsBool() )
  		{
	  ESP_LOGI(TAG, "Charging starting");
    // ******* Charging started: **********
    StdMetrics.ms_v_charge_duration_full->SetValue( 1440, Minutes ); // Lets assume 24H to full.
    if( StdMetrics.ms_v_charge_timermode->AsBool())
    		{
    		SET_CHARGE_STATE("charging","scheduledstart");
    		}
    	else
    		{
    		SET_CHARGE_STATE("charging","onrequest");
    		}
    	StdMetrics.ms_v_charge_kwh->SetValue( 0, kWh );  // kWh charged
		ks_cum_charge_start = CUM_CHARGE; // Battery charge base point
		StdMetrics.ms_v_charge_inprogress->SetValue( true );
		StdMetrics.ms_v_env_charging12v->SetValue( true);

		POLLSTATE_CHARGING;

    }
  else
  		{
    // ******* Charging continues: *******
    if (((BAT_SOC > 0) && (LIMIT_SOC > 0) && (BAT_SOC >= LIMIT_SOC) && (ks_last_soc < LIMIT_SOC))
    			|| ((EST_RANGE > 0) && (LIMIT_RANGE > 0)
    					&& (IDEAL_RANGE >= LIMIT_RANGE )
							&& (ks_last_ideal_range < LIMIT_RANGE )))
    		{
      // ...enter state 2=topping off when we've reach the needed range / SOC:
  			SET_CHARGE_STATE("topoff", NULL);
      }
    else if (BAT_SOC >= 95) // ...else set "topping off" from 94% SOC:
    		{
			SET_CHARGE_STATE("topoff", NULL);
    		}
  		}

  // Check if we have what is needed to calculate remaining minutes
  if (CHARGE_VOLTAGE > 0 && CHARGE_CURRENT > 0)
  		{
    	//Calculate remaining charge time
		float chargeTarget_full 	= ks_battery_capacity;
		float chargeTarget_soc 		= ks_battery_capacity;
		float chargeTarget_range 	= ks_battery_capacity;

		if (LIMIT_SOC > 0) //If SOC limit is set, lets calculate target battery capacity
			{
			chargeTarget_soc = ks_battery_capacity * LIMIT_SOC / 100.0;
			}
		else if (LIMIT_RANGE > 0)  //If range limit is set, lets calculate target battery capacity
			{
			chargeTarget_range = LIMIT_RANGE * ks_battery_capacity / FULL_RANGE;
			}

		if (ks_charge_bits.ChargingChademo)
			{ //ChaDeMo charging means that we will reach maximum 83%.
			chargeTarget_full = MIN(chargeTarget_full, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
			chargeTarget_soc = MIN(chargeTarget_soc, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
			chargeTarget_range = MIN(chargeTarget_range, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
			//TODO calculate the needed capacity above 83% as 32A
			}

		// Calculate time to full, SOC-limit and range-limit.
		StdMetrics.ms_v_charge_duration_full->SetValue( calcMinutesRemaining(chargeTarget_full), Minutes);
		StdMetrics.ms_v_charge_duration_soc->SetValue( calcMinutesRemaining(chargeTarget_soc), Minutes);
		StdMetrics.ms_v_charge_duration_range->SetValue( calcMinutesRemaining(chargeTarget_range), Minutes);
    }
  else
  		{
  		if( m_v_preheating->AsBool())
  			{
  			SET_CHARGE_STATE("heating","scheduledstart");
  			}
  		else
  			{
  			SET_CHARGE_STATE("charging",NULL);
  			}
  		}
  StdMetrics.ms_v_charge_kwh->SetValue(CUM_CHARGE - ks_cum_charge_start, kWh); // kWh charged
  ks_last_soc = BAT_SOC;
  ks_last_ideal_range = IDEAL_RANGE;
	}

/**
 * Update metrics when charging stops
 */
void OvmsVehicleKiaSoulEv::HandleChargeStop()
	{
  ESP_LOGI(TAG, "Charging done...");

  // ** Charge completed or interrupted: **
	StdMetrics.ms_v_charge_current->SetValue( 0 );
  StdMetrics.ms_v_charge_climit->SetValue( 0 );
  if (BAT_SOC == 100 || (BAT_SOC > 82 && ks_charge_bits.ChargingChademo))
  		{
  		SET_CHARGE_STATE("done","stopped");
  		}
  else if (BAT_SOC == 80 && ks_charge_bits.ChargingJ1772)
  		{
  		SET_CHARGE_STATE("stopped","scheduledstop");
  		}
  else
  		{
		SET_CHARGE_STATE("stopped","interrupted");
		}
	StdMetrics.ms_v_charge_substate->SetValue("onrequest");
  StdMetrics.ms_v_charge_kwh->SetValue( CUM_CHARGE - ks_cum_charge_start, kWh );  // kWh charged

  ks_cum_charge_start = 0;
  StdMetrics.ms_v_charge_inprogress->SetValue( false );
	StdMetrics.ms_v_env_charging12v->SetValue( false );
	m_c_speed->SetValue(0);
	}

/**
 *  Sets the charge metrics
 */
void OvmsVehicleKiaSoulEv::SetChargeMetrics(float voltage, float current, float climit, bool chademo)
	{
	StdMetrics.ms_v_charge_voltage->SetValue( voltage, Volts );
	StdMetrics.ms_v_charge_current->SetValue( current, Amps );
	StdMetrics.ms_v_charge_mode->SetValue( chademo ? "performance" : "standard");
	StdMetrics.ms_v_charge_climit->SetValue( climit, Amps);
	StdMetrics.ms_v_charge_type->SetValue( chademo ? "chademo" : "type1");
	StdMetrics.ms_v_charge_substate->SetValue("onrequest");

	//"Typical" consumption based on battery temperature and ambient temperature.
	float temp = ((StdMetrics.ms_v_bat_temp->AsFloat(Celcius) * 3) + StdMetrics.ms_v_env_temp->AsFloat(Celcius)) / 4;
	float consumption = 15+(20-temp)*3.0/8.0; //kWh/100km
	m_c_speed->SetValue( (voltage * current) / (consumption*10), Kph);
	}

/**
 * Calculates minutes remaining before target is reached. Based on current charge speed.
 * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
 */
uint16_t OvmsVehicleKiaSoulEv::calcMinutesRemaining(float target)
  		{
	  return MIN( 1440, (uint16_t) (((target - (ks_battery_capacity * BAT_SOC) / 100.0)*60.0) /
              (CHARGE_VOLTAGE * CHARGE_CURRENT)));
  		}

/**
 * Updates the maximum real world range at current temperature.
 * Also updates the State of Health
 */
void OvmsVehicleKiaSoulEv::UpdateMaxRangeAndSOH(void)
	{
	//Update State of Health using following assumption: 10% buffer
	StdMetrics.ms_v_bat_soh->SetValue( 110 - ( m_b_cell_det_max->AsFloat(0) + m_b_cell_det_min->AsFloat(0)) / 2 );
	StdMetrics.ms_v_bat_cac->SetValue( (ks_battery_capacity * BAT_SOH * BAT_SOC/10000.0) / 400, AmpHours);

	float maxRange = ks_maxrange * BAT_SOH / 100.0;
	float amb_temp = StdMetrics.ms_v_env_temp->AsFloat(20, Celcius);
	float bat_temp = StdMetrics.ms_v_bat_temp->AsFloat(20, Celcius);

	//ESP_LOGI(TAG, "MaxRange %.1f %.1f %.1f", maxRange, amb_temp, bat_temp);

	// Temperature compensation:
	//   - Assumes standard maxRange specified at 20 degrees C
	//   - Range halved at -20C.
	if (maxRange != 0)
		{
		maxRange = (maxRange * (100.0 - (int) (ABS(20.0 - (amb_temp+bat_temp * 3)/4)* 1.25))) / 100.0;
		}
	StdMetrics.ms_v_bat_range_full->SetValue(maxRange, Kilometers);
	}


/**
 * Open or lock the doors
 * 771 04 2F BC 1[0:1] 03
 */
bool OvmsVehicleKiaSoulEv::SetDoorLock(bool open, const char* password)
	{
	bool result=false;
	if( ks_shift_bits.Park )
  		{
    if( PinCheck((char*)password) )
    		{
    		ACCRelay(true,password	);
    		LeftIndicator(true);
    		result = Send_SJB_Command(0xbc, open?0x11:0x10, 0x03);
    		ACCRelay(false,password	);
    		GetDoorLockStatus();
    		}
  		}
		return result;
	}

/**
 * Open trunk door
 * 771 04 2F BC 09 03
 */
bool OvmsVehicleKiaSoulEv::OpenTrunk(const char* password)
	{
  if( ks_shift_bits.Park )
  		{
		if( PinCheck((char*)password) )
			{
    		StartRelay(true,password	);
    		LeftIndicator(true);
  			return Send_SJB_Command(0xbc, 0x09, 0x03);
    		StartRelay(false,password	);
  			}
  		}
		return false;
	}

/**
 * Open charge port
 * 7a0 04 2F B0 61 03
 */
bool OvmsVehicleKiaSoulEv::OpenChargePort(const char* password)
	{
  if( ks_shift_bits.Park )
  		{
		if( PinCheck((char*)password) )
			{
			return Send_BCM_Command(0xb0, 0x61, 0x03);
  			}
  		}
		return false;
	}

/**
 * Turn on and off left indicator light
 * 771 04 2f bc 15 0[3:0]
 */
bool OvmsVehicleKiaSoulEv::LeftIndicator(bool on)
	{
  if( ks_shift_bits.Park )
  		{
		SET_SJB_TP_TIMEOUT(10);		//Keep SJB in test mode for up to 10 seconds
		return Send_SJB_Command(0xbc, 0x15, on?0x03:0x00);
  		}
		return false;
	}

/**
 * Turn on and off right indicator light
 * 771 04 2f bc 16 0[3:0]
 */
bool OvmsVehicleKiaSoulEv::RightIndicator(bool on)
	{
  if( ks_shift_bits.Park )
  		{
		SET_SJB_TP_TIMEOUT(10);		//Keep SJB in test mode for up to 10 seconds
		return Send_SJB_Command(0xbc, 0x16, on?0x03:0x00);
  		}
		return false;
	}

/**
 * Turn on and off rear defogger
 * Needs to send tester present or something... Turns of immediately.
 * 771 04 2f bc 0c 0[3:0]
 */
bool OvmsVehicleKiaSoulEv::RearDefogger(bool on)
	{
  if( ks_shift_bits.Park )
  		{
  		SET_SJB_TP_TIMEOUT(900);  //Keep SJB in test mode for up to 15 minutes
		return Send_SJB_Command(0xbc, 0x0c, on?0x03:0x00);
  		}
		return false;
	}

/**
 * ACC - relay
 */
bool OvmsVehicleKiaSoulEv::ACCRelay(bool on, const char* password)
	{
	if(PinCheck((char*)password))
		{
		if( ks_shift_bits.Park )
				{
				if( on ) return Send_SMK_Command(7, 0xb1, 0x08, 0x03, 0x0a, 0x0a, 0x05);
				else return Send_SMK_Command(4, 0xb1, 0x08, 0, 0, 0, 0);
				}
		}
	return false;
	}

/**
 * IGN1 - relay
 */
bool OvmsVehicleKiaSoulEv::IGN1Relay(bool on, const char* password)
	{
	if(PinCheck((char*)password))
		{
		if( ks_shift_bits.Park )
				{
				if( on ) return Send_SMK_Command(7, 0xb1, 0x09, 0x03, 0x0a, 0x0a, 0x05);
				else return Send_SMK_Command(4, 0xb1, 0x09, 0, 0, 0, 0);
				}
		}
	return false;
	}

/**
 * IGN2 - relay
 */
bool OvmsVehicleKiaSoulEv::IGN2Relay(bool on, const char* password)
	{
	if(PinCheck((char*)password))
		{
		if( ks_shift_bits.Park )
				{
				if( on ) return Send_SMK_Command(7, 0xb1, 0x0a, 0x03, 0x0a, 0x0a, 0x05);
				else return Send_SMK_Command(4, 0xb1, 0x0a, 0, 0, 0, 0);
				}
		}
	return false;
	}

/**
 * Start - relay
 */
bool OvmsVehicleKiaSoulEv::StartRelay(bool on, const char* password)
	{
	if(PinCheck((char*)password))
		{
		if( ks_shift_bits.Park )
				{
				if( on ) return Send_SMK_Command(7, 0xb1, 0x0b, 0x03, 0x02, 0x02, 0x01);
				else return Send_SMK_Command(4, 0xb1, 0x0b, 0, 0, 0, 0);
				}
		}
	return false;
	}

/**
 * Control blue charger leds
 * mode:
 * 	4 - All leds
 * 	3 - Right led
 * 	2 - Left led
 * 	1 - Center led
 */
bool OvmsVehicleKiaSoulEv::BlueChargeLed(bool on, uint8_t mode)
	{
	if( ks_shift_bits.Park )
			{
  			SendTesterPresent(ON_BOARD_CHARGER_UNIT, 2);
			SetSessionMode(ON_BOARD_CHARGER_UNIT, 0x81); //Nesessary?
			vTaskDelay( xDelay );
			SetSessionMode(ON_BOARD_CHARGER_UNIT, 0x90);
			vTaskDelay( xDelay );
			SendCanMessage(ON_BOARD_CHARGER_UNIT, 	8, 0x03, VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_LOC_ID, mode, on? 0 : 0x11,0,0,0);
			vTaskDelay( xDelay );
			SendTesterPresent(ON_BOARD_CHARGER_UNIT, 2);
			}
	return false;
	}

/**
 * RequestNotify: send notifications / alerts / data updates
 */
void OvmsVehicleKiaSoulEv::RequestNotify(unsigned int which)
	{
  ks_notifications |= which;
	}

void OvmsVehicleKiaSoulEv::DoNotify()
	{
  unsigned int which = ks_notifications;

  if (which & SEND_EmergencyAlert)
  		{
    MyNotify.NotifyCommand("alert", "Emergency.Alert","Emergency alert signals are turned on");
    ks_notifications &= ~SEND_EmergencyAlert;
  		}

  if (which & SEND_EmergencyAlertOff)
  		{
    MyNotify.NotifyCommand("alert", "Emergency.Alert","Emergency alert signals are turned off");
    ks_notifications &= ~SEND_EmergencyAlertOff;
  		}

	}


/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleKiaSoulEv::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 10:
      MyConfig.SetParamValue("xks", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("xks", "suffrange", value);
      return true;
    case 12:
      MyConfig.SetParamValue("xks", "maxrange", value);
      return true;
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xks", "canwrite",  (bits& 1)!=0);
      return true;
    }
    default:
      return OvmsVehicle::SetFeature(key, value);
  }
}


/**
 * GetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
const std::string OvmsVehicleKiaSoulEv::GetFeature(int key)
{
  switch (key)
  {
    case 0:
    case 10:
      return MyConfig.GetParamValue("xks", "suffsoc", STR(0));
    case 11:
      return MyConfig.GetParamValue("xks", "suffrange", STR(0));
    case 12:
      return MyConfig.GetParamValue("xks", "maxrange", STR(CFG_DEFAULT_MAXRANGE));
    case 15:
    {
      int bits =
        ( MyConfig.GetParamValueBool("xks", "canwrite",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
  }
}


class OvmsVehicleKiaSoulEvInit
  {
  public: OvmsVehicleKiaSoulEvInit();
  } MyOvmsVehicleKiaSoulEvInit  __attribute__ ((init_priority (9000)));

OvmsVehicleKiaSoulEvInit::OvmsVehicleKiaSoulEvInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Kia Soul EV (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleKiaSoulEv>("KS","Kia Soul EV");
  }
