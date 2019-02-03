/*
;    Project:       Open Vehicle Monitor System
;    Date:          21th January 2019
;
;    Changes:
;    0.1.0  Initial stub
;			- Ported from Kia Soul. Totally untested.
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

#include "vehicle_kianiroev.h"

#include "ovms_log.h"

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"
#include <sys/param.h>

#define VERSION "0.0.1"

static const char *TAG = "v-kianiroev";

// Pollstate 0 - car is off
// Pollstate 1 - car is on
// Pollstate 2 - car is charging
static const OvmsVehicle::poll_pid_t vehicle_kianiroev_polls[] =
  {
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIVEHICLE,  0x02, 		{       0,  120,   0 } }, 	// VIN
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDII_2201,  	0x01, 		{       0,   10,  10 } }, 	// BMC Diag page 01
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDII_2201,  	0x02, 		{       0,   10,  10 } }, 	// BMC Diag page 02
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDII_2201,  	0x03, 		{       0,   10,  10 } }, 	// BMC Diag page 03
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDII_2201,  	0x04, 		{       0,   10,  10 } }, 	// BMC Diag page 04
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDII_2201,  	0x05, 		{       0,   10,  10 } },	  // BMC Diag page 05
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, 		{       0,   10,  10 } },	  //
    { 0x7b3, 0x7bb, VEHICLE_POLL_TYPE_OBDII_2201,  	0x00, 		{       0,   10,  10 } },	  // HVAC
    //{ 0x794, 0x79c, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, 		{       0,   60,  10 } }, 	// OBC - On board charger
  	  //{ 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x00, 		{       0,   10,  10 } }, 	// VMCU Shift-stick
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, 		{       0,   10,  30 } }, 	// VMCU Motor temp++
    //{ 0x7df, 0x7de, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x06, 		{       0,   30,  60 } }, 	// TMPS
    //{ 0x7c5, 0x7cd, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x01, 		{       0,   10,  10 } }, 	// LDC - Low voltage DC-DC
    { 0, 0, 0, 0, { 0, 0, 0 } }
  };

/**
 * Constructor for Kia Niro EV
 */
OvmsVehicleKiaNiroEv::OvmsVehicleKiaNiroEv()
  {
  ESP_LOGI(TAG, "Kia Niro EV v3.0 vehicle module");

  StopTesterPresentMessages();

  memset( m_vin, 0, sizeof(m_vin));
  memset( m_street, 0, sizeof(m_street));

  memset( kn_tpms_id, 0, sizeof(kn_tpms_id));

  kn_obc_volt = 230;
  kn_battery_current = 0;

  kn_battery_cum_charge_current = 0;
  kn_battery_cum_discharge_current = 0;
  kn_battery_cum_charge = 0;
  kn_battery_cum_discharge = 0;
  kn_battery_cum_op_time = 0;

  //kn_trip_start_odo = 0;
  //kn_start_cdc = 0;
  //kn_start_cc = 0;

  kn_charge_bits.ChargingCCS = false;
  kn_charge_bits.ChargingType2 = false;
  kn_charge_bits.FanStatus = 0;

  kn_heatsink_temperature = 0;
  kn_battery_fan_feedback = 0;
  kn_aux_bat_ok = true;

  kn_send_can.id = 0;
  kn_send_can.status = 0;
  kn_clock = 0;
  kn_utc_diff = 0;
  kn_openChargePort = false;
  kn_check_door_lock=false;
  kn_lockDoors=false;
  kn_unlockDoors=false;
  kn_emergency_message_sent = false;

  kn_shift_bits.Park = true;

  kn_ready_for_chargepollstate = true;
  kn_secs_with_no_client = 0;

  memset( kn_send_can.byte, 0, sizeof(kn_send_can.byte));

  kn_maxrange = CFG_DEFAULT_MAXRANGE;

  BmsSetCellArrangementVoltage(98, 1);
  BmsSetCellArrangementTemperature(5, 1);
  BmsSetCellLimitsVoltage(2.0,5.0);
  BmsSetCellLimitsTemperature(-35,90);
  BmsSetCellDefaultThresholdsVoltage(0.1, 0.2); //TODO What values do we want here?
  BmsSetCellDefaultThresholdsTemperature(4.0, 8.0); // and here?

  //Disable BMS alerts by default
  MyConfig.SetParamValueBool("vehicle", "bms.alerts.enabled", false);

  //for(int i=0; i<96; i++)
  	//	BmsSetCellVoltage(i, 2.0);

  // init metrics:
  m_version = MyMetrics.InitString("xkn.version", 0, VERSION " " __DATE__ " " __TIME__);
  m_b_cell_volt_max = MyMetrics.InitFloat("xkn.b.cell.volt.max", 10, 0, Volts);
  m_b_cell_volt_min = MyMetrics.InitFloat("xkn.b.cell.volt.min", 10, 0, Volts);
  m_b_cell_volt_max_no = MyMetrics.InitInt("xkn.b.cell.volt.max.no", 10, 0);
  m_b_cell_volt_min_no = MyMetrics.InitInt("xkn.b.cell.volt.min.no", 10, 0);
  m_b_cell_det_max = MyMetrics.InitFloat("xkn.b.cell.det.max", 0, 0, Percentage);
  m_b_cell_det_min = MyMetrics.InitFloat("xkn.b.cell.det.min", 0, 0, Percentage);
  m_b_cell_det_max_no = MyMetrics.InitInt("xkn.b.cell.det.max.no", 10, 0);
  m_b_cell_det_min_no = MyMetrics.InitInt("xkn.b.cell.det.min.no", 10, 0);
  m_c_power = MyMetrics.InitFloat("xkn.c.power", 10, 0, kW);
  m_c_speed = MyMetrics.InitFloat("xkn.c.speed", 10, 0, Kph);
  m_b_min_temperature = MyMetrics.InitInt("xkn.b.min.temp", 10, 0, Celcius);
  m_b_max_temperature = MyMetrics.InitInt("xkn.b.max.temp", 10, 0, Celcius);
  m_b_inlet_temperature = MyMetrics.InitInt("xkn.b.inlet.temp", 10, 0, Celcius);
  m_b_heat_1_temperature = MyMetrics.InitInt("xkn.b.heat1.temp", 10, 0, Celcius);
  m_b_heat_2_temperature = MyMetrics.InitInt("xkn.b.heat2.temp", 10, 0, Celcius);
  m_b_bms_soc = MyMetrics.InitFloat("xkn.b.bms.soc", 10, 0, Percentage);
  m_b_aux_soc = MyMetrics.InitFloat("xkn.b.aux.soc", 0, 0, Percentage);

  m_ldc_out_voltage = MyMetrics.InitFloat("xkn.ldc.out.volt", 10, 12, Volts);
  m_ldc_in_voltage = MyMetrics.InitFloat("xkn.ldc.in.volt", 10, 12, Volts);
  m_ldc_out_current = MyMetrics.InitFloat("xkn.ldc.out.amps", 10, 0, Amps);
  m_ldc_temperature = MyMetrics.InitFloat("xkn.ldc.temp", 10, 0, Celcius);

  m_obc_pilot_duty = MyMetrics.InitFloat("xkn.obc.pilot.duty", 10, 0, Percentage);

  m_obc_timer_enabled = MyMetrics.InitBool("xkn.obc.timer.enabled", 10, 0);

  m_v_env_lowbeam = MyMetrics.InitBool("xkn.e.lowbeam", 10, 0);
  m_v_env_highbeam = MyMetrics.InitBool("xkn.e.highbeam", 10, 0);
  m_v_env_climate_temp = MyMetrics.InitFloat("xkn.e.climate.temp", 10, 0, Celcius);
  m_v_env_climate_driver_only = MyMetrics.InitBool("xkn.e.climate.driver.only", 10, 0);
  m_v_env_climate_resirc = MyMetrics.InitBool("xkn.e.climate.resirc", 10, 0);
  m_v_env_climate_auto = MyMetrics.InitBool("xkn.e.climate.auto", 10, 0);
  m_v_env_climate_ac = MyMetrics.InitBool("xkn.e.climate.ac", 10, 0);
  m_v_env_climate_fan_speed = MyMetrics.InitInt("xkn.e.climate.fan.speed", 10, 0);
  m_v_env_climate_mode = MyMetrics.InitInt("xkn.e.climate.mode", 10, 0);

  m_v_preheat_timer1_enabled = MyMetrics.InitBool("xkn.e.preheat.timer1.enabled", 10, 0);
  m_v_preheat_timer2_enabled = MyMetrics.InitBool("xkn.e.preheat.timer2.enabled", 10, 0);
  m_v_preheating = MyMetrics.InitBool("xkn.e.preheating", 10, 0);

  m_v_pos_dist_to_dest = MyMetrics.InitInt("xkn.e.pos.dist.to.dest", 10, 0, Kilometers);
  m_v_pos_arrival_hour = MyMetrics.InitInt("xkn.e.pos.arrival.hour", 10, 0);
  m_v_pos_arrival_minute = MyMetrics.InitInt("xkn.e.pos.arrival.minute", 10, 0);
  m_v_pos_street = MyMetrics.InitString("xkn.e.pos.street", 10, "");
  ms_v_pos_trip = MyMetrics.InitFloat("xkn.e.trip", 10, 0, Kilometers);
  ms_v_trip_energy_used = MyMetrics.InitFloat("xkn.e.trip.energy.used", 10, 0, kWh);
  ms_v_trip_energy_recd = MyMetrics.InitFloat("xkn.e.trip.energy.recuperated", 10, 0, kWh);

  m_v_seat_belt_driver = MyMetrics.InitBool("xkn.v.seat.belt.driver", 10, 0);
  m_v_seat_belt_passenger = MyMetrics.InitBool("xkn.v.seat.belt.passenger", 10, 0);
  m_v_seat_belt_back_right = MyMetrics.InitBool("xkn.v.seat.belt.back.right", 10, 0);
  m_v_seat_belt_back_middle = MyMetrics.InitBool("xkn.v.seat.belt.back.middle", 10, 0);
  m_v_seat_belt_back_left = MyMetrics.InitBool("xkn.v.seat.belt.back.left", 10, 0);

  m_v_traction_control = MyMetrics.InitBool("xkn.v.traction.control", 10, 0);
  m_v_cruise_control = MyMetrics.InitBool("xkn.v.cruise.control.enabled", 10, 0);

  m_v_emergency_lights = MyMetrics.InitBool("xkn.v.emergency.lights", 10, 0);

  m_v_steering_mode = MyMetrics.InitString("xkn.v.steering.mode", 0, "Unknown");

  m_v_power_usage = MyMetrics.InitFloat("xkn.v.power.usage", 10, 0, kW);

  m_v_trip_consumption1 = MyMetrics.InitFloat("xkn.v.trip.consumption.KWh/100km", 10, 0, Other);
  m_v_trip_consumption2 = MyMetrics.InitFloat("xkn.v.trip.consumption.km/kWh", 10, 0, Other);

  m_b_cell_det_max->SetValue(0);
  m_b_cell_det_min->SetValue(0);

  StdMetrics.ms_v_bat_12v_voltage->SetValue(12.5, Volts);
  StdMetrics.ms_v_charge_inprogress->SetValue(false);
  StdMetrics.ms_v_env_on->SetValue(false);
  StdMetrics.ms_v_bat_temp->SetValue(20, Celcius);

  // init commands:
  cmd_xkn = MyCommandApp.RegisterCommand("xkn","Kia Niro EV",NULL,"",0,0,true);
  cmd_xkn->RegisterCommand("trip","Show trip info since last parked", xkn_trip_since_parked, 0,0, false);
  cmd_xkn->RegisterCommand("tripch","Show trip info since last charge", xkn_trip_since_charge, 0,0, false);
  cmd_xkn->RegisterCommand("tpms","Tire pressure monitor", xkn_tpms, 0,0, false);
  cmd_xkn->RegisterCommand("aux","Aux battery", xkn_aux, 0,0, false);
  cmd_xkn->RegisterCommand("vin","VIN information", xkn_vin, 0,0, false);
/*  cmd_xkn->RegisterCommand("IGN1","IGN1 relay", xkn_ign1, "<on/off><pin>",1,1, false);
  cmd_xkn->RegisterCommand("IGN2","IGN2 relay", xkn_ign2, "<on/off><pin>",1,1, false);
  cmd_xkn->RegisterCommand("ACC","ACC relay", xkn_acc_relay, "<on/off><pin>",1,1, false);
  cmd_xkn->RegisterCommand("START","Start relay", xkn_start_relay, "<on/off><pin>",1,1, false);
  cmd_xkn->RegisterCommand("headlightdelay","Set Head Light Delay", xkn_set_head_light_delay, "<on/off>",1,1, false);
  cmd_xkn->RegisterCommand("onetouchturnsignal","Set one touch turn signal", xkn_set_one_touch_turn_signal, "<0=Off, 1=3 blink, 2=5 blink, 3=7 blink>",1,1, false);
  cmd_xkn->RegisterCommand("autodoorunlock","Set auto door unlock", xkn_set_auto_door_unlock, "<1 = Off, 2 = Vehicle Off, 3 = On shift to P ,4 = Driver door unlock>",1,1, false);
  cmd_xkn->RegisterCommand("autodoorlock","Set auto door lock", xkn_set_auto_door_lock, "<0 =Off, 1=Enable on speed, 2=Enable on Shift>",1,1, false);
*/
  cmd_xkn->RegisterCommand("trunk","Open trunk", CommandOpenTrunk, "<pin>",1,1, false);
  //cmd_xkn->RegisterCommand("ParkBreakService","Enable break pad service", CommandParkBreakService, "<on/off/off2>",1,1, false);

  // For test purposes
  cmd_xkn->RegisterCommand("sjb","Send command to SJB ECU", xkn_sjb, "<b1><b2><b3>", 3,3, false);
  cmd_xkn->RegisterCommand("bcm","Send command to BCM ECU", xkn_bcm, "<b1><b2><b3>", 3,3, false);

  MyConfig.SetParamValueBool("modem","enable.gps", true);
  MyConfig.SetParamValueBool("modem","enable.gpstime", true);
  MyConfig.SetParamValueBool("modem","enable.net", true);
  MyConfig.SetParamValueBool("modem","enable.sms", true);

  // Require GPS.
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "app.connected", std::bind(&OvmsVehicleKiaNiroEv::EventListener, this, _1, _2));

  MyConfig.RegisterParam("xkn", "Kia Niro EV spesific settings.", true, true);
  ConfigChanged(NULL);

  MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);

  WebInit();

  // C-Bus
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  // M-Bus
  RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_100KBPS);

  PollSetPidList(m_can1,vehicle_kianiroev_polls);
  POLLSTATE_OFF;
  }

/**
 * Destructor
 */
OvmsVehicleKiaNiroEv::~OvmsVehicleKiaNiroEv()
  {
  ESP_LOGI(TAG, "Shutdown Kia Niro EV vehicle module");
  MyWebServer.DeregisterPage("/bms/cellmon");
  }

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleKiaNiroEv::ConfigChanged(OvmsConfigParam* param)
	{
  ESP_LOGD(TAG, "Kia Niro EV reload configuration");

  // Instances:
  // xks
  //	  cap_act_kwh			Battery capacity in wH (Default: 270000)
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  //  maxrange         	Maximum ideal range at 20 °C [km] (Default: 160)
  //  remote_charge_port					Use "trunk" button on keyfob to open charge port (Default: 1=enabled)
  //  canwrite					Enable commands
  kn_battery_capacity = (float)MyConfig.GetParamValueInt("xks", "cap_act_kwh", CGF_DEFAULT_BATTERY_CAPACITY);
  kn_key_fob_open_charge_port = (bool)MyConfig.GetParamValueBool("xks", "remote_charge_port", true);

  kn_maxrange = MyConfig.GetParamValueInt("xks", "maxrange", CFG_DEFAULT_MAXRANGE);
  if (kn_maxrange <= 0)
    kn_maxrange = CFG_DEFAULT_MAXRANGE;

  *StdMetrics.ms_v_charge_limit_soc = (float) MyConfig.GetParamValueInt("xks", "suffsoc");
  *StdMetrics.ms_v_charge_limit_range = (float) MyConfig.GetParamValueInt("xks", "suffrange");

  kn_enable_write = MyConfig.GetParamValueBool("xks", "canwrite", false);
	}

/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsVehicleKiaNiroEv::vehicle_kianiroev_car_on(bool isOn)
  {
  if (isOn && !StdMetrics.ms_v_env_on->AsBool())
    {
		// Car is ON
  		ESP_LOGI(TAG,"CAR IS ON");
		StdMetrics.ms_v_env_on->SetValue(isOn);
		StdMetrics.ms_v_env_awake->SetValue(isOn);

		StdMetrics.ms_v_env_charging12v->SetValue( false );
    POLLSTATE_RUNNING;
    kn_ready_for_chargepollstate = true;

    kn_park_trip_counter.Reset(POS_ODO, CUM_DISCHARGE, CUM_CHARGE);

    BmsResetCellStats();
    }
  else if(!isOn && StdMetrics.ms_v_env_on->AsBool())
    {
    // Car is OFF
		ESP_LOGI(TAG,"CAR IS OFF");
    POLLSTATE_OFF;
  		StdMetrics.ms_v_env_on->SetValue( isOn );
  		StdMetrics.ms_v_env_awake->SetValue( isOn );
  		StdMetrics.ms_v_pos_speed->SetValue( 0 );
  	  StdMetrics.ms_v_pos_trip->SetValue( kn_park_trip_counter.GetDistance() );
  		StdMetrics.ms_v_env_charging12v->SetValue( false );
    kn_ready_for_chargepollstate = true;

    kn_park_trip_counter.Update(POS_ODO, CUM_DISCHARGE, CUM_CHARGE);
    }

  //Make sure we update the different start values as soon as we have them available
  if(isOn)
  		{
    POLLSTATE_RUNNING;
		// Trip started. Let's save current state as soon as they are available
  		}
  }

/**
 * Ticker1: Called every second
 */
void OvmsVehicleKiaNiroEv::Ticker1(uint32_t ticker)
	{
	ESP_LOGD(TAG,"Pollstate: %d sec with no client: %d ",m_poll_state, kn_secs_with_no_client);

	// Open charge port. User pressed the third keyfob button.
	if( kn_openChargePort )
		{
		char buffer[6];
		OpenChargePort(itoa(MyConfig.GetParamValueInt("password","pin"), buffer, 10));
		kn_openChargePort = false;
		}

	// Lock or unlock doors. User pressed keyfob while car was on.
	if( kn_lockDoors || kn_unlockDoors)
		{
		char buffer[6];
		SetDoorLock(kn_unlockDoors, itoa(MyConfig.GetParamValueInt("password","pin"), buffer, 10));
		kn_lockDoors=false;
		kn_unlockDoors=false;
		}

	if(m_poll_state>0) // Only do these things if car is on or charging
		{
		UpdateMaxRangeAndSOH();

		if (FULL_RANGE > 0) //  If we have the battery full range, we can calculate the ideal range too
			{
		  	StdMetrics.ms_v_bat_range_ideal->SetValue( FULL_RANGE * BAT_SOC / 100.0, Kilometers);
		  	}

		// Update trip data
		if (StdMetrics.ms_v_env_on->AsBool())
			{
			if(kn_park_trip_counter.Started())
				{
				kn_park_trip_counter.Update(POS_ODO, CUM_DISCHARGE, CUM_CHARGE);
				StdMetrics.ms_v_pos_trip->SetValue( kn_park_trip_counter.GetDistance() , Kilometers);
				if( kn_park_trip_counter.HasEnergyData())
					{
					StdMetrics.ms_v_bat_energy_used->SetValue( kn_park_trip_counter.GetEnergyUsed(), kWh );
					StdMetrics.ms_v_bat_energy_recd->SetValue( kn_park_trip_counter.GetEnergyRecuperated(), kWh );
					}
				}
			if(kn_charge_trip_counter.Started())
				{
				kn_charge_trip_counter.Update(POS_ODO, CUM_DISCHARGE, CUM_CHARGE);
				ms_v_pos_trip->SetValue( kn_charge_trip_counter.GetDistance() , Kilometers);
				if( kn_charge_trip_counter.HasEnergyData())
					{
					ms_v_trip_energy_used->SetValue( kn_charge_trip_counter.GetEnergyUsed(), kWh );
					ms_v_trip_energy_recd->SetValue( kn_charge_trip_counter.GetEnergyRecuperated(), kWh );
					}
				}
		  }

		// Charge timer on/off?
		StdMetrics.ms_v_charge_timermode->SetValue( m_obc_timer_enabled->AsBool() && !kn_charge_timer_off );

		// Cooling?
		StdMetrics.ms_v_env_cooling->SetValue (  m_v_env_climate_ac->AsBool() && StdMetrics.ms_v_env_temp->AsFloat(10,Celcius) > m_v_env_climate_temp->AsFloat(16, Celcius) );

	  if( StdMetrics.ms_v_pos_trip->AsFloat(Kilometers)>0 )
	  		m_v_trip_consumption1->SetValue( StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) * 100 / StdMetrics.ms_v_pos_trip->AsFloat(Kilometers) );
	  if( StdMetrics.ms_v_bat_energy_used->AsFloat(kWh)>0 )
	  		m_v_trip_consumption2->SetValue( StdMetrics.ms_v_pos_trip->AsFloat(Kilometers) / StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) );

		}

  //Keep charging metrics up to date
	if (kn_charge_bits.ChargingType2)  				// **** Type 2  charging ****
		{
		SetChargeMetrics(kn_obc_volt, StdMetrics.ms_v_bat_power->AsFloat(0, kW) * 1000 / kn_obc_volt, 6600 / kn_obc_volt, false);
	  }
	else if (kn_charge_bits.ChargingCCS)  // **** CCS charging ****
		{
		SetChargeMetrics(StdMetrics.ms_v_bat_voltage->AsFloat(400,Volts), (float)kn_battery_current / -10.0, m_c_power->AsFloat(0,kW) * 10 / StdMetrics.ms_v_bat_voltage->AsFloat(400,Volts), true);
	  }


	// Check for charging status changes:
	bool isCharging = (kn_charge_bits.ChargingCCS || kn_charge_bits.ChargingType2) 	&& (CHARGE_CURRENT > 0);

	if (isCharging)
		{
		HandleCharging();
		}
	else if (!isCharging && StdMetrics.ms_v_charge_inprogress->AsBool())
		{
		HandleChargeStop();
		}

	if(m_poll_state==0 && StdMetrics.ms_v_door_chargeport->AsBool() && kn_ready_for_chargepollstate)	{
  		//Set pollstate charging if car is off and chargeport is open.
		ESP_LOGI(TAG, "CHARGEDOOR OPEN. READY FOR CHARGING.");
  		POLLSTATE_CHARGING;
  		kn_ready_for_chargepollstate = false;
  		kn_secs_with_no_client = 0; //Reset no client counter
  }

	//**** AUX Battery drain prevention code ***
	//If poll state is CHARGING and no clients are connected for 60 seconds, we'll turn off polling.
	if((StdMetrics.ms_s_v2_peers->AsInt() + StdMetrics.ms_s_v3_peers->AsInt())==0)
		{
		if(m_poll_state==2 )
			{
			kn_secs_with_no_client++;
			if(kn_secs_with_no_client>60)
				{
				ESP_LOGI(TAG,"NO CLIENTS. Turning off polling.");
				POLLSTATE_OFF;
				}
			}
		}
	//If client connects, we set the appropriate poll state
	else if(kn_secs_with_no_client>0 && m_poll_state==0)
		{
		kn_secs_with_no_client=0;
		ESP_LOGI(TAG,"CLIENT CONNECTED. Turning on polling.");
		if(StdMetrics.ms_v_env_on->AsBool())
			{
			POLLSTATE_RUNNING;
			}
		else
			{
			POLLSTATE_CHARGING;
			}
		}
	//**** End of AUX Battery drain prevention code ***

	// Check door lock status if clients are connected and we think the status might have changed
	if( (StdMetrics.ms_s_v2_peers->AsInt() + StdMetrics.ms_s_v3_peers->AsInt())>0 && kn_check_door_lock)
		{
		GetDoorLockStatus();
		kn_check_door_lock=false;
		}

	// Reset emergency light if it is stale.
	if( m_v_emergency_lights->IsStale() ) m_v_emergency_lights->SetValue(false);

	// Notify if emergency light are turned on or off.
	if( m_v_emergency_lights->AsBool() && !kn_emergency_message_sent)
		{
		kn_emergency_message_sent = true;
		RequestNotify(SEND_EmergencyAlert);
		}
	else if( !m_v_emergency_lights->AsBool() && kn_emergency_message_sent)
		{
		kn_emergency_message_sent=false;
		RequestNotify(SEND_EmergencyAlertOff);
		}

	// Send tester present
	SendTesterPresentMessages();

	DoNotify();
	}

/**
 * Ticker10: Called every second
 */
void OvmsVehicleKiaNiroEv::Ticker10(uint32_t ticker)
	{

	// Calculate difference to UTC
	if( kn_clock>0 && StdMetrics.ms_m_timeutc->AsInt()>0)
		{
		kn_utc_diff = (kn_clock/60)-((StdMetrics.ms_m_timeutc->AsInt()/60) % 1440);
		}

	m_v_pos_street->SetValue(m_street);
	}

/**
 * Ticker300: Called every five minutes
 */
void OvmsVehicleKiaNiroEv::Ticker300(uint32_t ticker)
	{
	}

void OvmsVehicleKiaNiroEv::EventListener(std::string event, void* data)
  {
  if (event == "app.connected")
    {
  		kn_check_door_lock=true;
    }
  }


/**
 * Update metrics when charging
 */
void OvmsVehicleKiaNiroEv::HandleCharging()
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
		kn_cum_charge_start = CUM_CHARGE; // Battery charge base point
		StdMetrics.ms_v_charge_inprogress->SetValue( true );
		StdMetrics.ms_v_env_charging12v->SetValue( true);

		BmsResetCellStats();

		POLLSTATE_CHARGING;
    }
  else
  		{
    // ******* Charging continues: *******
    if (((BAT_SOC > 0) && (LIMIT_SOC > 0) && (BAT_SOC >= LIMIT_SOC) && (kn_last_soc < LIMIT_SOC))
    			|| ((EST_RANGE > 0) && (LIMIT_RANGE > 0)
    					&& (IDEAL_RANGE >= LIMIT_RANGE )
							&& (kn_last_ideal_range < LIMIT_RANGE )))
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
		float chargeTarget_full 	= kn_battery_capacity;
		float chargeTarget_soc 		= kn_battery_capacity;
		float chargeTarget_range 	= kn_battery_capacity;

		if (LIMIT_SOC > 0) //If SOC limit is set, lets calculate target battery capacity
			{
			chargeTarget_soc = kn_battery_capacity * LIMIT_SOC / 100.0;
			}
		else if (LIMIT_RANGE > 0)  //If range limit is set, lets calculate target battery capacity
			{
			chargeTarget_range = LIMIT_RANGE * kn_battery_capacity / FULL_RANGE;
			}

		if (kn_charge_bits.ChargingCCS)
			{ //CCS charging means that we will reach maximum 94%.
			chargeTarget_full = MIN(chargeTarget_full, kn_battery_capacity*0.94); //Limit charge target to 94% when using CCS
			chargeTarget_soc = MIN(chargeTarget_soc, kn_battery_capacity*0.94); //Limit charge target to 94% when using CCS
			chargeTarget_range = MIN(chargeTarget_range, kn_battery_capacity*0.94); //Limit charge target to 94% when using CCS
			//TODO calculate the needed capacity above 94% as 32A
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
  StdMetrics.ms_v_charge_kwh->SetValue((CUM_CHARGE - kn_cum_charge_start)/10.0, kWh); // kWh charged
  kn_last_soc = BAT_SOC;
  kn_last_ideal_range = IDEAL_RANGE;
	}

/**
 * Update metrics when charging stops
 */
void OvmsVehicleKiaNiroEv::HandleChargeStop()
	{
  ESP_LOGI(TAG, "Charging done...");

  // ** Charge completed or interrupted: **
	StdMetrics.ms_v_charge_current->SetValue( 0 );
  StdMetrics.ms_v_charge_climit->SetValue( 0 );
  if (BAT_SOC == 100 || (BAT_SOC > 82 && kn_charge_bits.ChargingCCS))
  		{
  		SET_CHARGE_STATE("done","stopped");
  		}
  else if (BAT_SOC == 80 && kn_charge_bits.ChargingType2)
  		{
  		SET_CHARGE_STATE("stopped","scheduledstop");
  		}
  else
  		{
		SET_CHARGE_STATE("stopped","interrupted");
		}
	StdMetrics.ms_v_charge_substate->SetValue("onrequest");
  StdMetrics.ms_v_charge_kwh->SetValue( (CUM_CHARGE - kn_cum_charge_start)/10.0, kWh );  // kWh charged

  kn_cum_charge_start = 0;
  StdMetrics.ms_v_charge_inprogress->SetValue( false );
	StdMetrics.ms_v_env_charging12v->SetValue( false );
	kn_charge_bits.ChargingCCS = false;
	kn_charge_bits.ChargingType2 = false;
	m_c_speed->SetValue(0);

	// Reset trip counter for this charge
	kn_charge_trip_counter.Reset(POS_ODO, CUM_DISCHARGE, CUM_CHARGE);

	POLLSTATE_OFF;
	}

/**
 *  Sets the charge metrics
 */
void OvmsVehicleKiaNiroEv::SetChargeMetrics(float voltage, float current, float climit, bool ccs)
	{
	StdMetrics.ms_v_charge_voltage->SetValue( voltage, Volts );
	StdMetrics.ms_v_charge_current->SetValue( current, Amps );
	StdMetrics.ms_v_charge_mode->SetValue( ccs ? "performance" : "standard");
	StdMetrics.ms_v_charge_climit->SetValue( climit, Amps);
	StdMetrics.ms_v_charge_type->SetValue( ccs ? "ccs" : "type2");
	StdMetrics.ms_v_charge_substate->SetValue("onrequest");

	ESP_LOGI(TAG, "SetChargeMetrics: volt=%1f current=%1f chargeLimit=%1f", voltage, current, climit);

	//"Typical" consumption based on battery temperature and ambient temperature.
	float temp = ((StdMetrics.ms_v_bat_temp->AsFloat(Celcius) * 3) + StdMetrics.ms_v_env_temp->AsFloat(Celcius)) / 4;
	float consumption = 15+(20-temp)*3.0/8.0; //kWh/100km
	m_c_speed->SetValue( (voltage * current) / (consumption*10), Kph);
	}

/**
 * Calculates minutes remaining before target is reached. Based on current charge speed.
 * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
 */
uint16_t OvmsVehicleKiaNiroEv::calcMinutesRemaining(float target)
  		{
	  return MIN( 1440, (uint16_t) (((target - (kn_battery_capacity * BAT_SOC) / 100.0)*60.0) /
              (CHARGE_VOLTAGE * CHARGE_CURRENT)));
  		}

/**
 * Updates the maximum real world range at current temperature.
 * Also updates the State of Health
 */
void OvmsVehicleKiaNiroEv::UpdateMaxRangeAndSOH(void)
	{
	//Update State of Health using following assumption: 10% buffer
	StdMetrics.ms_v_bat_soh->SetValue( 110 - ( m_b_cell_det_max->AsFloat(0) + m_b_cell_det_min->AsFloat(0)) / 2 );
	StdMetrics.ms_v_bat_cac->SetValue( (kn_battery_capacity * BAT_SOH * BAT_SOC/10000.0) / 400, AmpHours);

	float maxRange = kn_maxrange;// * MIN(BAT_SOH,100) / 100.0;
	float amb_temp = StdMetrics.ms_v_env_temp->AsFloat(20, Celcius);
	float bat_temp = StdMetrics.ms_v_bat_temp->AsFloat(20, Celcius);

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
bool OvmsVehicleKiaNiroEv::SetDoorLock(bool open, const char* password)
	{
	bool result=false;
	if( kn_shift_bits.Park )
  		{
    if( PinCheck((char*)password) )
    		{
    		ACCRelay(true,password	);
    		LeftIndicator(true);
    		result = Send_SJB_Command(0xbc, open?0x11:0x10, 0x03);
    		ACCRelay(false,password	);
    		kn_check_door_lock=true;
    		}
  		}
		return result;
	}

/**
 * Open trunk door
 * 771 04 2F BC 09 03
 */
bool OvmsVehicleKiaNiroEv::OpenTrunk(const char* password)
	{

  if( kn_shift_bits.Park )
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
bool OvmsVehicleKiaNiroEv::OpenChargePort(const char* password)
	{
  //TODO if( kn_shift_bits.Park )
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
bool OvmsVehicleKiaNiroEv::LeftIndicator(bool on)
	{
  if( kn_shift_bits.Park )
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
bool OvmsVehicleKiaNiroEv::RightIndicator(bool on)
	{
  if( kn_shift_bits.Park )
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
bool OvmsVehicleKiaNiroEv::RearDefogger(bool on)
	{
  if( kn_shift_bits.Park )
  		{
  		SET_SJB_TP_TIMEOUT(900);  //Keep SJB in test mode for up to 15 minutes
		return Send_SJB_Command(0xbc, 0x0c, on?0x03:0x00);
  		}
		return false;
	}

/**
 * ACC - relay
 */
bool OvmsVehicleKiaNiroEv::ACCRelay(bool on, const char* password)
	{
	if(PinCheck((char*)password))
		{
		if( kn_shift_bits.Park )
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
bool OvmsVehicleKiaNiroEv::IGN1Relay(bool on, const char* password)
	{
	if(PinCheck((char*)password))
		{
		if( kn_shift_bits.Park )
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
bool OvmsVehicleKiaNiroEv::IGN2Relay(bool on, const char* password)
	{
	if(PinCheck((char*)password))
		{
		if( kn_shift_bits.Park )
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
bool OvmsVehicleKiaNiroEv::StartRelay(bool on, const char* password)
	{
	if(PinCheck((char*)password))
		{
		if( kn_shift_bits.Park )
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
bool OvmsVehicleKiaNiroEv::BlueChargeLed(bool on, uint8_t mode)
	{
	if( kn_shift_bits.Park )
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
void OvmsVehicleKiaNiroEv::RequestNotify(unsigned int which)
	{
  kn_notifications |= which;
	}

void OvmsVehicleKiaNiroEv::DoNotify()
	{
  unsigned int which = kn_notifications;

  if (which & SEND_EmergencyAlert)
  		{
    //TODO MyNotify.NotifyCommand("alert", "Emergency.Alert","Emergency alert signals are turned on");
    kn_notifications &= ~SEND_EmergencyAlert;
  		}

  if (which & SEND_EmergencyAlertOff)
  		{
    //TODO MyNotify.NotifyCommand("alert", "Emergency.Alert","Emergency alert signals are turned off");
    kn_notifications &= ~SEND_EmergencyAlertOff;
  		}

	}


/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleKiaNiroEv::SetFeature(int key, const char *value)
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
const std::string OvmsVehicleKiaNiroEv::GetFeature(int key)
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


class OvmsVehicleKiaNiroEvInit
  {
  public: OvmsVehicleKiaNiroEvInit();
  } MyOvmsVehicleKiaNiroEvInit  __attribute__ ((init_priority (9000)));

OvmsVehicleKiaNiroEvInit::OvmsVehicleKiaNiroEvInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Kia Niro EV (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleKiaNiroEv>("KS","Kia Niro EV");
  }

/*******************************************************************************/
KN_Trip_Counter::KN_Trip_Counter()
	{
	odo_start=0;
	cdc_start=0;
	cc_start=0;
	odo=0;
	cdc=0;
	cc=0;
	}

KN_Trip_Counter::~KN_Trip_Counter()
	{
	}

/*
 * Resets the trip counter.
 *
 * odo - The current ODO
 * cdc - Cumulated Discharge
 * cc - Cumulated Charge
 */
void KN_Trip_Counter::Reset(float odo, float cdc, float cc)
	{
	Update(odo, cdc, cc);
	if(odo_start==0 && odo!=0)
		odo_start = odo;		// ODO at Start of trip
	if(cdc_start==0 && cdc!=0)
	  	cdc_start = cdc; 	// Register Cumulated discharge
	if(cc_start==0 && cc!=0)
	  	cc_start = cc; 		// Register Cumulated charge
	}

/*
 * Update the trip counter with current ODO, accumulated discharge and accumulated charge..
 *
 * odo - The current ODO
 * cdc - Accumulated Discharge
 * cc - Accumulated Charge
 */
void KN_Trip_Counter::Update(float current_odo, float current_cdc, float current_cc)
	{
	odo=current_odo;
	cdc=current_cdc;
	cc=current_cc;
	}

/*
 * Returns true if the trip counter has been initialized properly.
 */
bool KN_Trip_Counter::Started()
	{
	return odo_start!=0;
	}

/*
 * Returns true if the trip counter have energy data.
 */
bool KN_Trip_Counter::HasEnergyData()
	{
	return cdc_start!=0 && cc_start!=0;
	}

float KN_Trip_Counter::GetDistance()
	{
	return odo-odo_start;
	}

float KN_Trip_Counter::GetEnergyUsed()
	{
	return (cdc-cdc_start) - (cc-cc_start);
	}

float KN_Trip_Counter::GetEnergyRecuperated()
	{
	return cc - cc_start;
	}
