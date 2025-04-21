/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          19th September 2023
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2011       Michael Stegen / Stegen Electronics
 ;    (C) 2011-2017  Mark Webb-Johnson
 ;    (C) 2011       Sonny Chen @ EPRO/DX
 ;    (C) 2020       Chris Staite
 ;    (C) 2023       Peter Harry
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

#include "vehicle_mg4.hpp"

static const char *TAG = "v-mgev4";

namespace {

const OvmsPoller::poll_pid_t mg4_obdii_polls[] =
{
	{ gwmId, gwmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vehStatusPid, 					{  0, 1, 1, 0  }, 0, ISOTP_STD },// 0xd001u
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsStatusPid, 			{  0, 15, 6, 0  }, 0, ISOTP_STD },// 0xb048u
	{ broadcastId, ccuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccChargingPid, 			{  0, 15, 6, 0  }, 0, ISOTP_STD },// 0xb00fu
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIICURRENT, socPid,                   {  0, 15, 6, 60  }, 0, ISOTP_STD },// 0X5bu
	{ gwmId, gwmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vehTimePid, 						{  0, 60, 60, 0  }, 0, ISOTP_STD },// 0x010bu
	{ gwmId, gwmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, odoPid, 							{  0, 999, 30, 0  }, 0, ISOTP_STD },// 0xb921u
	{ gwmId, gwmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vinPid, 							{  0, 999, 999, 0  }, 0, ISOTP_STD },// 0xf190u

	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIICURRENT, vehicleSpeedPid, 			{  0, 999, 1, 0  }, 0, ISOTP_STD },// 0xdu
	//{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIICURRENT, odometerPid, 			{  0, 999, 30, 0  }, 0, ISOTP_STD },
	//{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIICURRENT, engineSpeedPid, 		{  0, 999, 60, 0  }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIICURRENT, ambTempPid, 				{  0, 999, 60, 0  }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryBusVoltagePid, 	{  0, 15, 30, 0 }, 0, ISOTP_STD },// 0xb041u
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryVoltagePid, 		{  0, 15, 30, 0 }, 0, ISOTP_STD },// 0xb042u
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCurrentPid, 		{  0, 15, 30, 0 }, 0, ISOTP_STD },// 0xb043u
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryResistancePid, 	{  0, 60, 60, 0 }, 0, ISOTP_STD },// 0xb045u
	//{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  0, 90, 90, 120  }, 0, ISOTP_STD },// 0xb046u
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCoolantTempPid,   {  0, 60, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPid,           {  0, 120, 120, 0  }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryTempPid,          {  0, 60, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryErrorPid,         {  0, 60, 60, 0 }, 0, ISOTP_STD },// 0xb047u
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsRangePid,             {  0, 60, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell1StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell2StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell3StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell4StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell5StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell6StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell7StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell8StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell9StatPid,            {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell10StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell11StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell12StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell13StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell14StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell15StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell16StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell17StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell18StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell19StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell20StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell21StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell22StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell23StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell24StatPid,           {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsMaxCellVoltagePid,    {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsMinCellVoltagePid,    {  0, 180, 60, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsTimePid,              {  0, 90, 90, 0  }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayBPid,  {  0, 260, 260, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayGPid,  {  0, 260, 260, 0 }, 0, ISOTP_STD },
	{ broadcastId, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayPPid,  {  0, 260, 260, 0 }, 0, ISOTP_STD },
	
	{ broadcastId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid,                 {  0, 15, 15, 0  }, 0, ISOTP_STD },// 0xd112u
	{ broadcastId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDrlPid,                  {  0, 60, 60, 0  }, 0, ISOTP_STD },
	{ broadcastId, ccuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccVoltagePid,             {  0, 15, 0, 0  }, 0, ISOTP_STD },
	{ broadcastId, ccuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccAmperagePid,            {  0, 15, 0, 0  }, 0, ISOTP_STD },
	{ broadcastId, ccuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccMaxAmperagePid,         {  0, 180, 0, 0  }, 0, ISOTP_STD },
	{ 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0, 0 }
};
}

//Called by OVMS when MG4 is chosen as vehicle type (and on startup when MG5 is chosen)
OvmsVehicleMg4::OvmsVehicleMg4()
{
	ESP_LOGI(TAG, "Starting MG4 variant");
	
	// Set manual polling on
	MyConfig.SetParamValueInt("xmg", "polling.manual", 1);
	
	// Set up initial values from the version setting
	int vehicleVersion = MyConfig.GetParamValueInt("xmg", "vehval", 0);
	switch (vehicleVersion) {
		case 0:
			ESP_LOGI(TAG,"MG4 51kWh");
			StandardMetrics.ms_v_bat_range_full->SetValue(400.0);
			m_batt_capacity->SetValue(50.8);
			m_max_dc_charge_rate->SetValue(90);
			MyConfig.SetParamValueFloat("xmg","bms.dod.lower", 25.0);
			MyConfig.SetParamValueFloat("xmg","bms.dod.upper", 930.0);
			m_trip_consumption->SetValue(145.0, WattHoursPK);
			m_avg_consumption->SetValue(145.0, WattHoursPK);
			break;
		case 1:
			ESP_LOGI(TAG,"MG4 64kWh");
			StandardMetrics.ms_v_bat_range_full->SetValue(450.0);
			m_batt_capacity->SetValue(61.7); //NCM 104cells 400V
			m_max_dc_charge_rate->SetValue(140.0);
			MyConfig.SetParamValueFloat("xmg","bms.dod.lower", 25.0);
			MyConfig.SetParamValueFloat("xmg","bms.dod.upper", 930.0);
			m_trip_consumption->SetValue(140.0, WattHoursPK);
			m_avg_consumption->SetValue(140.0, WattHoursPK);
			break;
		case 2:
			ESP_LOGI(TAG,"MG4 77kWh");
			StandardMetrics.ms_v_bat_range_full->SetValue(625.0);
			m_batt_capacity->SetValue(74.4); //NCM 104cells 400V
			m_max_dc_charge_rate->SetValue(144.0);
			MyConfig.SetParamValueFloat("xmg","bms.dod.lower", 25.0);
			MyConfig.SetParamValueFloat("xmg","bms.dod.upper", 930.0);
			m_trip_consumption->SetValue(143.0, WattHoursPK);
			m_avg_consumption->SetValue(143.0, WattHoursPK);
			break;
		default:
			ESP_LOGI(TAG,"MG4 51kWh");
			StandardMetrics.ms_v_bat_range_full->SetValue(400.0);
			m_batt_capacity->SetValue(50.8);
			m_max_dc_charge_rate->SetValue(90);
			MyConfig.SetParamValueFloat("xmg","bms.dod.lower", 25.0);
			MyConfig.SetParamValueFloat("xmg","bms.dod.upper", 930.0);
			m_trip_consumption->SetValue(145.0, WattHoursPK);
			m_avg_consumption->SetValue(145.0, WattHoursPK);
			break;
	}
	
	ESP_LOGD(TAG, "MG4 Values - Range: %0.1f Battery kWh: %0.1f Charge Max: %0.1f DoD lower: %0.1f DoD Upper: %0.1f", StandardMetrics.ms_v_bat_range_full->AsFloat(), m_batt_capacity->AsFloat(), m_max_dc_charge_rate->AsFloat(),
			 MyConfig.GetParamValueFloat("xmg","bms.dod.lower"), MyConfig.GetParamValueFloat("xmg","bms.dod.upper"));
	
	previousPollEnable = m_enable_polling->AsBool();
	batteryVoltage = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
	speed = StandardMetrics.ms_v_pos_gpsspeed->AsFloat();
	
	//Add variant specific poll data
	//ConfigureMG5PollData(mg4_obdii_polls, sizeof(mg4_obdii_polls));
    RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS, nullptr, false);
    PollSetState(PollStateListenOnly);
    PollSetPidList(m_can1,mg4_obdii_polls);
	//BMS Configuration
	BmsSetCellArrangementVoltage(42, 2);
	BmsSetCellArrangementTemperature(42, 2);
	BmsSetCellLimitsVoltage(3.0, 5.0);
	BmsSetCellLimitsTemperature(-39, 200);
	BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
	BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
	
	// Register shell commands
	cmd_xmg->RegisterCommand("polls", "Turn polling on", PollsCommandShell, "<command>\non\tTurn on\noff\tTurn off", 1, 1);
	
#ifdef CONFIG_OVMS_COMP_WEBSERVER
	Mg4WebInit();
#endif
}

//Called by OVMS when vehicle type is changed from current
OvmsVehicleMg4::~OvmsVehicleMg4()
{
	ESP_LOGI(TAG, "Shutdown MG4");
#ifdef CONFIG_OVMS_COMP_WEBSERVER
	VersionWebDeInit();
#endif
}

//Called by OVMS every 1s
void OvmsVehicleMg4::Ticker1(uint32_t ticker)
{
	canbus* currentBus = m_poll_bus_default;
	if (currentBus == nullptr)
	{
		// Polling is disabled, so there's nothing to do here
		return;
	}
	
	// Sometimes gpsspeed seems to go above 0 when parked
	const auto parkTime = StandardMetrics.ms_v_env_parktime->AsInt();
	const auto previousPollState = m_poll_state;
	
	//MainStateMachine(currentBus, ticker);
	
	if (previousPollState != m_poll_state)
	{
		ESP_LOGI(TAG, "Changed poll state from %d to %d", previousPollState, m_poll_state);
		m_poll_state_metric->SetValue(m_poll_state);
	}
	
	if (previousPollEnable != m_enable_polling->AsBool()) {
		if (m_enable_polling->AsBool()) {
			ESP_LOGD(TAG,"Polling Enabled");
			MyNotify.NotifyStringf("info", "xmg.polling", "Polling has been enabled");
		} else {
			MyNotify.NotifyStringf("info", "xmg.polling", "Polling has been disabled");
			ESP_LOGD(TAG,"Polling Disabled");
		}
		previousPollEnable = m_enable_polling->AsBool();
	}
	
	if(!StandardMetrics.ms_v_env_charging12v->AsBool() && StandardMetrics.ms_v_pos_gpslock->AsBool() && StandardMetrics.ms_v_pos_gpsspeed->AsFloat() > 5.0) {
		// If the 12V battery is not charging and we have GPS lock and we are travelling at over 5KPH we have an error condition
		if(speed != StandardMetrics.ms_v_pos_gpsspeed->AsFloat() || batteryVoltage != StandardMetrics.ms_v_bat_12v_voltage->AsFloat()) {
			// If the state has changed sen a notification
			ESP_LOGE(TAG, "Speed is %0.1f and 12V Battery is %0.2f Parktime = %d", speed, batteryVoltage, parkTime);
			//MyNotify.NotifyStringf("info", "xmg.polling", "Speed is %0.1f and 12V Battery is %0.2f Parktime = %d", speed, batteryVoltage, parkTime);
		}
		batteryVoltage = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
		speed = StandardMetrics.ms_v_pos_gpsspeed->AsFloat();
	}
	
	// New code
	batteryVoltage = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
	if (batteryVoltage >= 13.9) { // Charging with vehicle OFF
		StandardMetrics.ms_v_env_charging12v->SetValue(true);
		if (m_enable_polling->AsBool()) {
			PollSetState(PollStateCharging);
		}
		if (vehState != VehicleStateCharging) {
			ESP_LOGD(TAG, "Vehicle is charging. 12V Battery=%0.1f", batteryVoltage);
			vehState = VehicleStateCharging;
		}
	} else if (batteryVoltage <= 13) { // Parked
		StandardMetrics.ms_v_env_charging12v->SetValue(false);
		StandardMetrics.ms_v_env_on->SetValue(false);
		if (vehState != VehicleStateParked) {
			ESP_LOGD(TAG, "Vehicle is parked. 12V Battery=%0.1f", batteryVoltage);
			vehState = VehicleStateParked;
			PollSetState(PollStateListenOnly);
			m_enable_polling->SetValue(false);
			StandardMetrics.ms_v_env_awake->SetValue(false);
			//StandardMetrics.ms_v_env_locked->SetValue(true);
			if (StandardMetrics.ms_v_charge_inprogress->AsBool())
			{
				if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 99.9) //Set to 99.9 instead of 100 incase of mathematical errors
				{
					StandardMetrics.ms_v_charge_state->SetValue("done");
				}
				else
				{
					StandardMetrics.ms_v_charge_state->SetValue("stopped");
				}
				StandardMetrics.ms_v_charge_type->SetValue("undefined");
				StandardMetrics.ms_v_charge_inprogress->SetValue(false);
			}
		} else {
			StandardMetrics.ms_v_env_awake->SetValue(false);
		}
	} else { // Driving or Charging
		StandardMetrics.ms_v_env_charging12v->SetValue(true);
		if (vehState != VehicleStateDriving) {
			ESP_LOGD(TAG, "Vehicle is ON. 12V Battery=%0.1f", batteryVoltage);
			vehState = VehicleStateDriving;
			StandardMetrics.ms_v_env_awake->SetValue(true);
		} else if(!m_enable_polling->AsBool()) {
			if(StandardMetrics.ms_v_pos_gpslock->AsBool() && StandardMetrics.ms_v_pos_gpsspeed->AsFloat() > 5.0) {
				if(speed > 5.0) {
					PollSetState(PollStateRunning);
					m_enable_polling->SetValue(true);
					ESP_LOGD(TAG, "Driving - Polling has been enabled at %0.1fkPh and %0.2fVolts ", StandardMetrics.ms_v_pos_gpsspeed->AsFloat(), batteryVoltage);
					MyNotify.NotifyStringf("info", "xmg.polling", "Driving - polling has been enabled");
					StandardMetrics.ms_v_env_on->SetValue(true);
					StandardMetrics.ms_v_charge_inprogress->SetValue(false);
				}
				speed = StandardMetrics.ms_v_pos_gpsspeed->AsFloat();
				StandardMetrics.ms_v_pos_speed->SetValue(speed);
			}
		} else { // Polling is enabled
			if (StandardMetrics.ms_v_charge_inprogress->AsBool())
			{
				PollSetState(PollStateCharging);
				ESP_LOGD(TAG,"Poll State Charging");
			}
			else
			{
				PollSetState(PollStateRunning);
				ESP_LOGD(TAG,"Poll State Running");
			}
		}
	}
	processEnergy();
}

//Called by OVMS when a wake up command is requested
//Toggles polling each time it is called
OvmsVehicle::vehicle_command_t OvmsVehicleMg4::CommandWakeup()
{
	if (m_enable_polling->AsBool()) {
		MyNotify.NotifyStringf("info", "xmg.polling", "Polling Stopped");
		m_enable_polling->SetValue(false);
		return OvmsVehicle::Success;
	} else {
		MyNotify.NotifyStringf("info", "xmg.polling", "Polling Started");
		m_enable_polling->SetValue(true);
		return OvmsVehicle::Success;
	}
}

//This registers a new vehicle type in the vehicle configuration page drop down menu
class OvmsVehicleMg4Init
{
public:
	OvmsVehicleMg4Init();
} MyOvmsVehicleMg4Init  __attribute__ ((init_priority (9000)));

OvmsVehicleMg4Init::OvmsVehicleMg4Init()
{
	ESP_LOGI(TAG, "Registering Vehicle: MG4 (9000)");
	
	MyVehicleFactory.RegisterVehicle<OvmsVehicleMg4>("MG4", "MG 4");
}

/*
 // This is the only place we evaluate the vehicle state to change the action of OVMS
 void OvmsVehicleMg4::MainStateMachine(canbus* currentBus, uint32_t ticker)
 {
 
 // Store last 12V charging state and re-evaluate if the 12V is charging
 const auto charging12vLast = StandardMetrics.ms_v_env_charging12v->AsBool();
 
 StandardMetrics.ms_v_env_charging12v->SetValue(
 StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= CHARGING_THRESHOLD);
 //ESP_LOGD(TAG,"Polling is manual");
 if(!StandardMetrics.ms_v_env_charging12v->AsBool()) {
 // 12V not being charged so vehicle is off
 PollSetState(PollStateListenOnly);
 m_enable_polling->SetValue(false);
 StandardMetrics.ms_v_env_awake->SetValue(false);
 StandardMetrics.ms_v_env_on->SetValue(false);
 if(charging12vLast) {
 // Vehicle has just been turned off
 ESP_LOGD(TAG,"Vehicle just been turned off");
 MyNotify.NotifyStringf("info", "xmg.polling", "Vehicle just been turned off");
 //It is possible that when the car stops charging, we go straight from charging to 12V level too low and so OVMS won't know that we have stopped charging.
 //So we manually set charging metrics here
 if (StandardMetrics.ms_v_charge_inprogress->AsBool())
 {
 if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 99.9) //Set to 99.9 instead of 100 incase of mathematical errors
 {
 StandardMetrics.ms_v_charge_state->SetValue("done");
 }
 else
 {
 StandardMetrics.ms_v_charge_state->SetValue("stopped");
 }
 StandardMetrics.ms_v_charge_type->SetValue("undefined");
 StandardMetrics.ms_v_charge_inprogress->SetValue(false);
 }
 }
 } else {
 if (m_enable_polling->AsBool()) {
 if (StandardMetrics.ms_v_charge_inprogress->AsBool())
 {
 PollSetState(PollStateCharging);
 ESP_LOGV(TAG,"Poll State Charging");
 }
 else
 {
 PollSetState(PollStateRunning);
 ESP_LOGV(TAG,"Poll State Running");
 }
 } else {
 PollSetState(PollStateListenOnly);
 ESP_LOGV(TAG,"Poll State Listen Only");
 if (!StandardMetrics.ms_v_charge_inprogress->AsBool()) {
 if (StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= 14.0) {
 ESP_LOGI(TAG,"12V Battery is over 14V so vehicle is charging");
 StandardMetrics.ms_v_charge_inprogress->SetValue(true);
 }
 }
 }
 }
 }
 */

