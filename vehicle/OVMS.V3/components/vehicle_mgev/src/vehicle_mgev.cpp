/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris Staite
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

static const char *TAG = "v-mgev";

#include "vehicle_mgev.h"
#include "ovms_notify.h"

namespace {

const OvmsVehicle::poll_pid_t common_obdii_polls[] =
{
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsStatusPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryBusVoltagePid, {  0, 5, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCurrentPid, {  0, 5, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryVoltagePid, {  0, 5, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryResistancePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  0, 30, 30, 60  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCoolantTempPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPid, {  0, 120, 120, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryTempPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryErrorPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsRangePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell1StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell2StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell3StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell4StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell5StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell6StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell7StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell8StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell9StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsMaxCellVoltagePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsMinCellVoltagePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsTimePid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayBPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayGPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayPPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    // { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcLvCurrentPid, {  0, 30, 30, 0  }, 0, ISOTP_STD }, //Unknown mapping for now
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcPowerLoadPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcTemperaturePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVinPid, { 0, 999, 999, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuHvContactorPid, {  0, 1, 1, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuIgnitionStatePid, {  0, 1, 1, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcu12vSupplyPid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuCoolantTempPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuMotorTempPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuChargerConnectedPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVehicleSpeedPid, {  0, 0, 1, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuMotorSpeedPid, {  0, 0, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuMotorTorquePid, {  0, 0, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuOdometerPid, {  0, 0, 20, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuHandbrakePid, {  0, 0, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuGearPid, {  0, 0, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBrakePid, {  0, 0, 5, 0  }, 0, ISOTP_STD },
    // { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBonnetPid, {  0, 0, 5, 0  }, 0, ISOTP_STD }, //Can use bcmDoorPid.
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, chargeRatePid, {  0, 30, 0, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuAcPowerPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBatteryVoltagePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuRadiatorFanPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuDcDcModePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuDcDcInputCurrentPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuDcDcInputVoltagePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuDcDcOutputCurrentPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuDcDcOutputVoltagetPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuDcDcTempPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcAmbientTempPid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcSetTempPid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcFaceOutletTempPid, {  0, 0, 30, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcBlowerSpeedPid, {  0, 0, 30, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcPtcTempPid, {  0, 30, 10, 0  }, 0, ISOTP_STD },
    // { pepsId, pepsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, pepsLockPid, {  0, 5, 5, 0  }, 0, ISOTP_STD }, //Can use bcmDoorPid.
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, tyrePressurePid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, tyreTemperaturePid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccVoltagePid, {  0, 5, 0, 0  }, 0, ISOTP_STD },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccAmperagePid, {  0, 5, 0, 0  }, 0, ISOTP_STD },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccMaxAmperagePid, {  0, 30, 0, 0  }, 0, ISOTP_STD },
    { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0, 0 }
};

charging_profile granny_steps[]  = {
    {0,98,6375}, // Max charge rate (7200) less charge loss
    {98,100,2700}, // Observed charge rate
    {0,0,0},
    };

charging_profile ccs_steps[] = {
    {0,20,75000},
    {20,30,67000},
    {30,40,59000},
    {40,50,50000},
    {50,60,44000},
    {60,80,37000},
    {80,83,26600},
    {83,95,16200},
    {95,100,5700},
    {0,0,0},
};
// From table found here https://ev-database.org/car/1201/MG-ZS-EV

}

//Called by OVMS when an MG EV variant is chosen as vehicle type (and on startup when an MG EV variant is chosen)
OvmsVehicleMgEv::OvmsVehicleMgEv()
{
    ESP_LOGI(TAG, "Starting MG EV vehicle module");

    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    // Don't support any other mode
    StandardMetrics.ms_v_charge_mode->SetValue("standard");

    mg_cum_energy_charge_wh = 0;

    // Set Max Range to WLTP Range
    StandardMetrics.ms_v_bat_range_full->SetValue(WLTP_RANGE);

    memset(m_vin, 0, sizeof(m_vin));

    PollSetState(PollStateListenOnly);
    // Allow unlimited polling per second
    PollSetThrottling(0u);

    m_bat_pack_voltage = MyMetrics.InitFloat("xmg.v.bat.voltage.bms", 0, SM_STALE_HIGH, Volts);
    m_bat_voltage_vcu = MyMetrics.InitFloat("xmg.v.bat.voltage.vcu", 0, SM_STALE_HIGH, Volts);
    m_bat_coolant_temp = MyMetrics.InitFloat("xmg.v.bat.coolant.temp", 0, SM_STALE_HIGH, Celcius);
    m_bat_resistance = MyMetrics.InitFloat("xmg.v.bat.resistance", SM_STALE_MAX, SM_STALE_MAX);
    m_bms_max_cell_voltage = MyMetrics.InitFloat("xmg.v.bms.cell.voltage.max", 0, SM_STALE_HIGH, Volts);
    m_bms_min_cell_voltage = MyMetrics.InitFloat("xmg.v.bms.cell.voltage.min", 0, SM_STALE_HIGH, Volts);
    m_bms_main_relay_b = MyMetrics.InitBool("xmg.v.bms.mainrelay.b", SM_STALE_MAX, SM_STALE_MAX);
    m_bms_main_relay_g = MyMetrics.InitBool("xmg.v.bms.mainrelay.g", SM_STALE_MAX, SM_STALE_MAX);
    m_bms_main_relay_p = MyMetrics.InitBool("xmg.v.bms.mainrelay.p", SM_STALE_MAX, SM_STALE_MAX);
    m_bms_time = MyMetrics.InitString("xmg.v.bms.time", 0, "");
    m_bat_error = MyMetrics.InitInt("xmg.v.bat.error", SM_STALE_MAX, SM_STALE_MAX);
    m_env_face_outlet_temp = MyMetrics.InitFloat("xmg.v.env.faceoutlet.temp", 0, SM_STALE_HIGH, Celcius);
    m_radiator_fan = MyMetrics.InitBool("xmg.v.radiator.fan", SM_STALE_MAX, SM_STALE_MAX);
    m_dcdc_load = MyMetrics.InitFloat("xmg.v.dcdc.load", 0, SM_STALE_HIGH, Percentage);
    m_vcu_dcdc_mode = MyMetrics.InitInt("xmg.v.vcu.dcdc.mode", SM_STALE_MAX, SM_STALE_MAX);
    m_vcu_dcdc_input_current = MyMetrics.InitFloat("xmg.v.vcu.dcdc.input.current", 0, SM_STALE_HIGH, Amps);
    m_vcu_dcdc_input_voltage = MyMetrics.InitFloat("xmg.v.vcu.dcdc.input.voltage", 0, SM_STALE_HIGH, Volts);
    m_vcu_dcdc_output_current = MyMetrics.InitFloat("xmg.v.vcu.dcdc.output.current", 0, SM_STALE_HIGH, Amps);
    m_vcu_dcdc_output_voltage = MyMetrics.InitFloat("xmg.v.vcu.dcdc.output.voltage", 0, SM_STALE_HIGH, Volts);
    m_vcu_dcdc_temp = MyMetrics.InitFloat("xmg.v.vcu.dcdc.temp", 0, SM_STALE_HIGH, Celcius);
    m_soc_raw = MyMetrics.InitFloat("xmg.v.soc.raw", 0, SM_STALE_HIGH, Percentage);
    m_motor_coolant_temp = MyMetrics.InitFloat("xmg.v.m.coolant.temp", 0, SM_STALE_HIGH, Celcius);
    m_motor_torque = MyMetrics.InitFloat("xmg.v.m.torque", 0, SM_STALE_HIGH, Nm);
    m_ignition_state = MyMetrics.InitInt("xmg.v.ignition.state", SM_STALE_MAX, SM_STALE_MAX);
    m_poll_state_metric = MyMetrics.InitInt("xmg.state.poll", SM_STALE_MAX, m_poll_state);
    m_gwm_state = MyMetrics.InitInt("xmg.state.gwm", SM_STALE_MAX, SM_STALE_MAX);
    m_bcm_auth = MyMetrics.InitBool("xmg.auth.bcm", SM_STALE_MAX, false);
    m_gwm_task = MyMetrics.InitInt("xmg.task.gwm", SM_STALE_MAX, static_cast<int>(GWMTasks::None));
    m_bcm_task = MyMetrics.InitInt("xmg.task.bcm", SM_STALE_MAX, static_cast<int>(BCMTasks::None));
    
    //m_trip_start = MyMetrics.InitFloat("xmg.p.trip.start", SM_STALE_MAX, SM_STALE_MAX);
    m_trip_consumption = MyMetrics.InitFloat("xmg.p.trip.consumption", SM_STALE_MID, 165.0, WattHoursPK);
    m_avg_consumption = MyMetrics.InitFloat("xmg.p.avg.consumption", SM_STALE_MID, 165.0, WattHoursPK);

    DRLFirstFrameSentCallback = std::bind(&OvmsVehicleMgEv::DRLFirstFrameSent, this, std::placeholders::_1, std::placeholders::_2);

    // Register config params
    MyConfig.RegisterParam("xmg", "MG EV configuration", true, true);

    // ConfigChanged(nullptr); //Now called in ConfigurePollData(). Call ConfigurePollData() in variant specific code.

    // Register shell commands
    m_cmdSoftver = MyCommandApp.RegisterCommand("softver", "MG EV Software", PickOvmsCommandExecuteCallback(OvmsVehicleMgEv::SoftwareVersions));
	  m_cmdAuth = MyCommandApp.RegisterCommand("auth", "Authenticate with ECUs", AuthenticateECUShell, "<ECU>\nall\tAll ECUs\ngwm\tGWM only\nbcm\tBCM only", 1, 1);
    m_cmdDRL = MyCommandApp.RegisterCommand("drl", "Daytime running light control", DRLCommandWithAuthShell, "<command>\non\tTurn on\noff\tTurn off", 1, 1);
    m_cmdDRLNoAuth = MyCommandApp.RegisterCommand("drln", "Daytime running light control (no BCM authentication)", DRLCommandShell, "<command>\non\tTurn on\noff\tTurn off", 1, 1);
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
#endif
}

//Called by OVMS when vehicle type is changed from current
OvmsVehicleMgEv::~OvmsVehicleMgEv()
{
    ESP_LOGI(TAG, "Shutdown MG EV vehicle module");

    //xTimerDelete(m_zombieTimer, 0);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebDeInit();
#endif

    if (m_pollData)
    {
        free(m_pollData);
    }

    if (m_cmdSoftver)
    {
        MyCommandApp.UnregisterCommand(m_cmdSoftver->GetName());
    }
	if (m_cmdAuth)
	{
		MyCommandApp.UnregisterCommand(m_cmdAuth->GetName());
	}
	if (m_cmdDRL)
	{
		MyCommandApp.UnregisterCommand(m_cmdDRL->GetName());
	}
    if (m_cmdDRLNoAuth)
	{
		MyCommandApp.UnregisterCommand(m_cmdDRLNoAuth->GetName());
	}
    // MyConfig.DeregisterParam("xmg"); //Seem to sometimes cause OVMS to factory reset
}

const char* OvmsVehicleMgEv::VehicleShortName()
{
    return "MG";
}

//Default implementation in vehicle.h is 3s and seem to be not enough (sometimes voltage and current is still ramping up)
//Another possible solution is to poll evccAmperagePid and evccVoltagePid faster than 5s.
int OvmsVehicleMgEv::GetNotifyChargeStateDelay(const char* state)
{
    constexpr uint8_t Delay = 5;
    ESP_LOGI(TAG, "Notifying charge state in %ds", Delay);
    return Delay;
}

//Call this function in variant specific code to setup poll data
void OvmsVehicleMgEv::ConfigurePollData(const OvmsVehicle::poll_pid_t *SpecificPollData, size_t DataSize)
{
    if (m_pollData)
    {
        PollSetPidList(nullptr, nullptr);
        free(m_pollData);
        m_pollData = nullptr;
    }

    //Allocate memory for m_pollData which should be enough for both the common_obdii_polls and SpecificPollData
    size_t size = sizeof(common_obdii_polls) + DataSize;
    ESP_LOGI(TAG, "Number of common obdii polls: %u. Number of variant specific polls: %u, total size: %u", sizeof(common_obdii_polls)/sizeof(common_obdii_polls[0]), DataSize/sizeof(SpecificPollData[0]), size/sizeof(common_obdii_polls[0]));
    m_pollData = reinterpret_cast<OvmsVehicle::poll_pid_t*>(ExternalRamMalloc(size));
    if (m_pollData == nullptr)
    {
        ESP_LOGE(TAG, "Unable to allocate memory for polling");
        return;
    }

    //Copy data to m_pollData. Must copy specific poll data first because we must end the poll list with a zero entry so OVMS knows where the array ends and the zero entry is at the end of common_obdii_polls.
    auto ptr = m_pollData;
    if (SpecificPollData)
    {
        ptr = std::copy(SpecificPollData, &SpecificPollData[DataSize / sizeof(SpecificPollData[0])], ptr);
    }
    ptr = std::copy(common_obdii_polls, &common_obdii_polls[sizeof(common_obdii_polls) / sizeof(common_obdii_polls[0])], ptr);

    // Re-start CAN bus, setting up the PID list
    ConfigurePollInterface(CanInterface());

}

void OvmsVehicleMgEv::ConfigurePollData()
{
    ConfigurePollData(nullptr, 0);
}

void OvmsVehicleMgEv::processEnergy()
{
    // When called each to battery power for a second is calculated.
    // This is added to ms_v_bat_energy_recd if regenerating or
    // ms_v_bat_energy_used if power is being drawn from the battery

    // Only calculate if the car is turned on and not charging.
    if (StandardMetrics.ms_v_env_awake->AsBool() &&
        !StandardMetrics.ms_v_charge_inprogress->AsBool()) {
        // Are we in READY state? Ready to drive off.
        if (StandardMetrics.ms_v_env_on->AsBool()) {
            auto bat_power = StandardMetrics.ms_v_bat_power->AsFloat();
            // Calculate battery power (kW) for one second
            auto energy = bat_power / 3600.0;
            // Calculate current (A) used for one second.
            auto coulombs = (StandardMetrics.ms_v_bat_current->AsFloat() / 3600.0);
            // Car is still parked so trip has not started.
            // Set all values to zero
            if (StandardMetrics.ms_v_env_drivetime->AsInt() == 0) {
                ESP_LOGI(TAG, "Trip has started");
                // Reset trip
                ResetTripCounters();
            // Otherwise we are already moving so do calculations
            }
            else
            {
                // Calculate regeneration power
                if (bat_power < 0) {
                    StandardMetrics.ms_v_bat_energy_recd->SetValue
                    (StandardMetrics.ms_v_bat_energy_recd->AsFloat() + -(energy));

                    StandardMetrics.ms_v_bat_coulomb_recd->SetValue
                    (StandardMetrics.ms_v_bat_coulomb_recd->AsFloat() + -(coulombs));

                // Calculate power usage. Add power used each second
                }
                else
                {
                    StandardMetrics.ms_v_bat_energy_used->SetValue
                    (StandardMetrics.ms_v_bat_energy_used->AsFloat() + energy);

                    StandardMetrics.ms_v_bat_coulomb_used->SetValue
                    (StandardMetrics.ms_v_bat_coulomb_used->AsFloat() + coulombs);
                }
                
                // Update trip counters
                UpdateTripCounters();
            }
        }
        // Not in READY so must have been turned off
        else
        {
            // We have only just stopped so add trip values to the totals
            
            if (StandardMetrics.ms_v_env_parktime->AsInt() == 0) {
                ESP_LOGI(TAG, "Trip has ended");
                
                StandardMetrics.ms_v_bat_energy_used_total->SetValue
                (StandardMetrics.ms_v_bat_energy_used_total->AsFloat()
                    + StandardMetrics.ms_v_bat_energy_used->AsFloat());

                StandardMetrics.ms_v_bat_energy_recd_total->SetValue
                (StandardMetrics.ms_v_bat_energy_recd_total->AsFloat()
                 + StandardMetrics.ms_v_bat_energy_recd->AsFloat());

                StandardMetrics.ms_v_bat_coulomb_used_total->SetValue
                (StandardMetrics.ms_v_bat_coulomb_used_total->AsFloat()
                 + StandardMetrics.ms_v_bat_coulomb_used->AsFloat());

                StandardMetrics.ms_v_bat_coulomb_recd_total->SetValue
                (StandardMetrics.ms_v_bat_coulomb_recd_total->AsFloat()
                 + StandardMetrics.ms_v_bat_coulomb_recd->AsFloat());
                
                // Set average consumption over 5 trips
                m_avg_consumption->SetValue((m_avg_consumption->AsFloat() * 4.0 + m_trip_consumption->AsFloat()) / 5.0);
            }
        }
    }

    // Add cumulative charge energy each second to ms_v_charge_power
    if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    {
        mg_cum_energy_charge_wh += StandardMetrics.ms_v_charge_power->AsFloat()*1000/3600;
        StandardMetrics.ms_v_charge_kwh->SetValue(mg_cum_energy_charge_wh/1000);

        int limit_soc      = StandardMetrics.ms_v_charge_limit_soc->AsInt(0);
        float limit_range    = StandardMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers);
        
        // Calculate  a maximum range taking the battery state of health into account
        float adjusted_max_range = StandardMetrics.ms_v_bat_range_full->AsFloat(0, Kilometers) * StandardMetrics.ms_v_bat_soh->AsFloat() / 100;
        
        // Calculate time to reach 100% charge
        int minsremaining_full = StandardMetrics.ms_v_charge_type->AsString() == "ccs" ? calcMinutesRemaining(100, ccs_steps) : calcMinutesRemaining(100, granny_steps);
        StandardMetrics.ms_v_charge_duration_full->SetValue(minsremaining_full);
        ESP_LOGV(TAG, "Time remaining: %d mins to full", minsremaining_full);

        // Calculate time to charge to SoC Limit
        if (limit_soc > 0)
        {
            // if limit_soc is set, then calculate remaining time to limit_soc
            int minsremaining_soc = StandardMetrics.ms_v_charge_type->AsString() == "ccs" ? calcMinutesRemaining(limit_soc, ccs_steps) : calcMinutesRemaining(limit_soc, granny_steps);
            StandardMetrics.ms_v_charge_duration_soc->SetValue(minsremaining_soc, Minutes);
            if ( minsremaining_soc == 0 && !soc_limit_reached && limit_soc >= StandardMetrics.ms_v_bat_soc->AsFloat(100))
            {
                  MyNotify.NotifyStringf("info", "charge.limit.soc", "Charge limit of %d%% reached", limit_soc);
                  soc_limit_reached = true;
            }
            ESP_LOGV(TAG, "Time remaining: %d mins to %d%% soc", minsremaining_soc, limit_soc);
        }

        // Calculate time to charge to Range Limit
        if (limit_range > 0.0 && adjusted_max_range > 0.0)
        {
            // if range limit is set, then compute required soc and then calculate remaining time to that soc
            int range_soc = limit_range / adjusted_max_range * 100.0;
            int minsremaining_range = StandardMetrics.ms_v_charge_type->AsString() == "ccs" ? calcMinutesRemaining(range_soc, ccs_steps) : calcMinutesRemaining(range_soc, granny_steps);
            StandardMetrics.ms_v_charge_duration_range->SetValue(minsremaining_range, Minutes);
            if ( minsremaining_range == 0 && !range_limit_reached && range_soc >= StandardMetrics.ms_v_bat_soc->AsFloat(100))
            {
                MyNotify.NotifyStringf("info", "charge.limit.range", "Charge limit of %dkm reached", (int) limit_range);
                range_limit_reached = true;
            }
            ESP_LOGV(TAG, "Time remaining: %d mins for %0.0f km (%d%% soc)", minsremaining_range, limit_range, range_soc);
        }
        // When we are not charging set back to zero ready for next charge.
    }
    else
    {
        mg_cum_energy_charge_wh = 0;
        StandardMetrics.ms_v_charge_duration_full->SetValue(0);
        StandardMetrics.ms_v_charge_duration_soc->SetValue(0);
        StandardMetrics.ms_v_charge_duration_range->SetValue(0);
        soc_limit_reached = false;
        range_limit_reached = false;
    }
}

// Calculate the time to reach the Target and return in minutes
int OvmsVehicleMgEv::calcMinutesRemaining(int toSoc, charging_profile charge_steps[])
{

    int minutes = 0;
    int percents_In_Step;
    int fromSoc = StandardMetrics.ms_v_bat_soc->AsInt(100);
    int batterySize = 42500;
    float chargespeed = -StandardMetrics.ms_v_bat_power->AsFloat() * 1000;

    for (int i = 0; charge_steps[i].toPercent != 0; i++) {
          if (charge_steps[i].toPercent > fromSoc && charge_steps[i].fromPercent<toSoc) {
                 percents_In_Step = (charge_steps[i].toPercent>toSoc ? toSoc : charge_steps[i].toPercent) - (charge_steps[i].fromPercent<fromSoc ? fromSoc : charge_steps[i].fromPercent);
                 minutes += batterySize * percents_In_Step * 0.6 / (chargespeed<charge_steps[i].maxChargeSpeed ? chargespeed : charge_steps[i].maxChargeSpeed);
          }
    }
    return minutes;
}

/**
 * ResetTripCounters: called at trip start to set reference points
 */
void OvmsVehicleMgEv::ResetTripCounters()
{
  StdMetrics.ms_v_pos_trip->SetValue(0);
  m_odo_trip            = 0;
  m_tripfrac_reftime    = esp_log_timestamp();
  m_tripfrac_refspeed   = StdMetrics.ms_v_pos_speed->AsFloat();

  StdMetrics.ms_v_bat_energy_recd->SetValue(0);
  StdMetrics.ms_v_bat_energy_used->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_recd->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_used->SetValue(0);
}

/**
 * UpdateTripCounters: odometer resolution is only 1 km, so trip distances lack
 *  precision. To compensate, this method derives trip distance from speed.
 */
void OvmsVehicleMgEv::UpdateTripCounters()
{
  // Process speed update:

  uint32_t now = esp_log_timestamp();
  float speed = StdMetrics.ms_v_pos_speed->AsFloat();

  if (m_tripfrac_reftime && now > m_tripfrac_reftime) {
    float speed_avg = ABS(speed + m_tripfrac_refspeed) / 2;
    uint32_t time_ms = now - m_tripfrac_reftime;
    double meters = speed_avg / 3.6 * time_ms / 1000;
    m_odo_trip += meters / 1000;
    StdMetrics.ms_v_pos_trip->SetValue(TRUNCPREC(m_odo_trip,3));
  }

  m_tripfrac_reftime = now;
  m_tripfrac_refspeed = speed;
    
    // Calculate consumption (Wh/km) and average it over 5 minutes
    if ( m_odo_trip > 0.1 && speed > 5.0) {
        float tripconsumption = (StdMetrics.ms_v_bat_energy_used->AsFloat() - StdMetrics.ms_v_bat_energy_recd->AsFloat()) * 1000 / m_odo_trip;
        if (m_trip_consumption->AsFloat() > 0) {
            m_trip_consumption->SetValue((m_trip_consumption->AsFloat() * 299.0 + tripconsumption) / 300.0);
        } else {
            m_trip_consumption->SetValue(tripconsumption);
        }
        ESP_LOGW(TAG, "%0.2fKWh/100k over %0.2fKm @ %0.2fkph", m_trip_consumption->AsFloat(0, kWhP100K), m_odo_trip, speed);
    }
}

//Called by OVMS when a wake up command is requested
void OvmsVehicleMgEv::WakeUp(void* self)
{
    // WakeUp Is not used, as OVMS cannot currently wake the car
    ESP_LOGI(TAG, "WakeUp was called but this not yet supported on OVMS for the MG.");

    // auto* wakeBus = reinterpret_cast<OvmsVehicleMgEv*>(self)->m_poll_bus_default;
    // if (wakeBus)
    // {
    //     for (int i = 0; i < 20; ++i)
    //     {
    //         reinterpret_cast<OvmsVehicleMgEv*>(self)->SendTesterPresentTo(wakeBus, gwmId);
    //         vTaskDelay(70 / portTICK_PERIOD_MS);
    //     }
    // }
    vTaskDelete(xTaskGetCurrentTaskHandle());
    // Should never be reached, but just in case
    for (;;) { }
}

void OvmsVehicleMgEv::ConfigurePollInterface(int bus)
{
    canbus* newBus = IdToBus(bus);
    canbus* oldBus = m_poll_bus_default;

    if (newBus != nullptr && oldBus == newBus)
    {
        // Already configured for that interface
        ESP_LOGI(TAG, "Already configured for interface, not re-configuring");
        if (m_pollData && !m_poll_plist)
        {
            PollSetPidList(newBus, m_pollData);
        }
        return;
    }

    if (bus > 0)
    {
        ESP_LOGI(TAG, "Starting to poll for data on bus %d", bus);
        RegisterCanBus(bus, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
        // The bus is only populated when RegisterCanBus is called for the first time
        if (newBus == nullptr)
        {
            newBus = IdToBus(bus);
        }
    }

    // When called sets poll default bus which is what is used when set to 0 (as is
    // our case above).  If this is extended to use more busses, then will either need
    // to be called last, or the obdii_polls will need changing to set the bus to 1.
    if (m_pollData)
    {
        PollSetPidList(newBus, m_pollData);
    }

    if (oldBus != nullptr)
    {
        ESP_LOGI(TAG, "Stopping previous CAN bus");
        oldBus->Stop();
        oldBus->SetPowerMode(PowerMode::Off);
    }
}

canbus* OvmsVehicleMgEv::IdToBus(int id)
{
    canbus* bus;
    switch (id)
    {
        case 1:
            bus = m_can1;
            break;
        case 2:
            bus = m_can2;
            break;
        case 3:
            bus = m_can3;
            break;
        case 4:
            bus = m_can4;
            break;
        default:
            bus = nullptr;
            break;
    }
    return bus;
}

string OvmsVehicleMgEv::IntToString(int x)
{
    ostringstream ss;
    ss << x;
    return ss.str();
}

string OvmsVehicleMgEv::IntToString(int x, uint8_t length, string padding)
{
    char result[length + 1];
    string format = "%" + padding + IntToString(length) + "d";
    snprintf(result, length + 1, format.c_str(), x);
    return string(result);
}

//Called by OVMS when it detects vehicle is idling
void OvmsVehicleMgEv::NotifyVehicleIdling()
{
    if (m_poll_state == PollStateRunning)
    {
        OvmsVehicle::NotifyVehicleIdling();
    }
}

bool OvmsVehicleMgEv::SendTesterPresentTo(canbus* currentBus, uint16_t id)
{
    CAN_frame_t testerFrame = {
        currentBus,
        nullptr,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        id,
        { .u8 = {
            (ISOTP_FT_SINGLE<<4) + 2, VEHICLE_POLL_TYPE_TESTERPRESENT, 0, 0, 0, 0, 0, 0
        } }
    };
    return currentBus->Write(&testerFrame) != ESP_FAIL;
}

bool OvmsVehicleMgEv::SendDiagSessionTo(canbus* currentBus, uint16_t id, uint8_t mode)
{
    CAN_frame_t diagnosticControl = {
        currentBus,
        nullptr,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        id,
        { .u8 = {
            (ISOTP_FT_SINGLE<<4) + 2, VEHICLE_POLL_TYPE_OBDIISESSION, mode, 0, 0, 0, 0, 0
        } }
    };
    return currentBus->Write(&diagnosticControl) != ESP_FAIL;
}

bool OvmsVehicleMgEv::SendPollMessage(
        canbus* bus, uint16_t id, uint8_t type, uint16_t pid)
{
    CAN_frame_t sendFrame = {
        bus,
        nullptr,  // send callback (ignore)
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        id,
        // Only support for 16-bit PIDs because that's all we use
        { .u8 = {
            (ISOTP_FT_SINGLE << 4) + 3, static_cast<uint8_t>(type),
            static_cast<uint8_t>(pid >> 8),
            static_cast<uint8_t>(pid & 0xff), 0, 0, 0, 0
        } }
    };
    return bus->Write(&sendFrame) != ESP_FAIL;
}

// bool OvmsVehicleMgEv::SendPidQueryTo(canbus* currentBus, uint16_t id, uint8_t pid)
// {
//     //TODO
//     // CAN_frame_t diagnosticControl = {
//     //     currentBus,
//     //     nullptr,
//     //     { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
//     //     id,
//     //     { .u8 = {
//     //         (ISOTP_FT_SINGLE<<4) + 2, VEHICLE_POLL_TYPE_OBDIISESSION, mode, 0, 0, 0, 0, 0
//     //     } }
//     // };
//     return currentBus->Write(&diagnosticControl) != ESP_FAIL;
// }

// void OvmsVehicleMgEv::SetupManualPolls(const OvmsVehicle::poll_pid_t *ManualPolls, size_t ManualPollSize)
// {
//     m_ManualPollList.clear();
//     for (uint8_t a = 0; a < ManualPollSize/sizeof(ManualPolls[0]); a++)
//     {
//         array<uint16_t, VEHICLE_POLL_NSTATES> PollIntervals;
//         for (uint8_t b = 0; b < VEHICLE_POLL_NSTATES; b++)
//         {
//             PollIntervals[b] = ManualPolls[a].polltime[b];
//         }
//         // ManualPollList_t Entry = {ManualPolls[a].txmoduleid, ManualPolls[a].pid, PollIntervals, 0, false};
//         ManualPollList_t Entry = {ManualPolls[a].txmoduleid, ManualPolls[a].pid, PollIntervals, 0};
//         m_ManualPollList.push_back(Entry);
//     }
// }

// void OvmsVehicleMgEv::SendManualPolls(canbus* currentBus, uint32_t ticker)
// {
//     for (auto& PollItem : m_ManualPollList)
//     {
//         ESP_LOGV(TAG, "[%u] Checking manual poll list. ECU: %03x, PID: %04x, poll interval: %us, last sent time: %u", ticker, PollItem.TXID, PollItem.PID, PollItem.PollInterval[m_poll_state], PollItem.SentTime);
//         //Only send message if poll interval is not 0 and interval time has elapsed
//         if ((PollItem.PollInterval[m_poll_state] > 0) && (ticker - PollItem.SentTime >= PollItem.PollInterval[m_poll_state]))
//         {
//             ESP_LOGV(TAG, "Sending manual poll to %03x @ %04x @ time %u", PollItem.TXID, PollItem.PID, ticker);
//             PollItem.SentTime = ticker;
//             std::string Response;
//             int PollStatus = PollSingleRequest(currentBus, PollItem.TXID, PollItem.TXID | rxFlag, VEHICLE_POLL_TYPE_READDATA, PollItem.PID, Response, SYNC_REQUEST_TIMEOUT, ISOTP_STD);
//             ESP_LOGV(TAG, "Response (%d): %s", PollStatus, hexencode(Response).c_str());
//         }
//     }
// }

OvmsVehicleMgEv* OvmsVehicleMgEv::GetActiveVehicle(OvmsWriter* writer)
{
	OvmsVehicleMgEv* vehicle = reinterpret_cast<OvmsVehicleMgEv*>(MyVehicleFactory.ActiveVehicle());
    if (vehicle == nullptr)
    {
        writer->puts("No active vehicle");
    }

    //Trim active vehicle name to same length as ExpectedVehicleName and checks whether they are equal
    constexpr const char ExpectedVehicleName[] = "MG EV";
    size_t ExpectedVehicleNameLength = strlen(ExpectedVehicleName);
    char ActiveVehicleName[ExpectedVehicleNameLength + 1];
    strncpy(ActiveVehicleName, MyVehicleFactory.ActiveVehicleName(), ExpectedVehicleNameLength);
    ActiveVehicleName[ExpectedVehicleNameLength] = '\0';
    if (strcmp(ActiveVehicleName, ExpectedVehicleName) != 0)
    {
        writer->printf("Unexpected vehicle type %s", ActiveVehicleName);
    }
    return vehicle;
}

void OvmsVehicleMgEv::SoftwareVersions(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
    OvmsVehicleMgEv* vehicle = OvmsVehicleMgEv::GetActiveVehicle(writer);
    vehicle->SoftwareVersions(writer);
}

void OvmsVehicleMgEv::AuthenticateECUShell(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
	OvmsVehicleMgEv* vehicle = OvmsVehicleMgEv::GetActiveVehicle(writer);
    bool AuthSucceeded = false;
    if (strcmp(argv[0], "all") == 0)
    {
        AuthSucceeded = vehicle->AuthenticateECU({ECUAuth::GWM, ECUAuth::BCM});
    }
    else if (strcmp(argv[0], "gwm") == 0)
    {
        AuthSucceeded = vehicle->AuthenticateECU({ECUAuth::GWM});
    }
    else if (strcmp(argv[0], "bcm") == 0)
    {
        AuthSucceeded = vehicle->AuthenticateECU({ECUAuth::BCM});
    }
    else
    {
        writer->puts("Unknown authentication command");
    }
    if (AuthSucceeded)
    {
        writer->puts("Authentication successful");
    }
    else
    {
        writer->puts("Authentication failed");
    }
}

void OvmsVehicleMgEv::DRLCommandShell(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
    OvmsVehicleMgEv* vehicle = OvmsVehicleMgEv::GetActiveVehicle(writer);
    if (strcmp(argv[0], "on") == 0)
    {
        ESP_LOGI(TAG, "DRL on requested");
        vehicle->DRLCommand(writer, vehicle->m_poll_bus_default, true);
    }
    else if (strcmp(argv[0], "off") == 0)
    {
        ESP_LOGI(TAG, "DRL off requested");
        vehicle->DRLCommand(writer, vehicle->m_poll_bus_default, false);
    }
    else
    {
        writer->puts("Unknown authentication command");
    }
}

void OvmsVehicleMgEv::DRLCommandWithAuthShell(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
    OvmsVehicleMgEv* vehicle = reinterpret_cast<OvmsVehicleMgEv*>(MyVehicleFactory.ActiveVehicle());
    //If BCM has not been authenticated, do that first. Otherwise we can skip and go straight to the actual DRL command.
    if (!vehicle->m_bcm_auth->AsBool())
    {
        ESP_LOGI(TAG, "BCM has not been authenticated, will do that first");
        if (vehicle->AuthenticateECU({ECUAuth::BCM}))
        {
            vehicle->m_bcm_auth->SetValue(true);
        }
        else
        {
            writer->puts("Failed to authenticate BCM");
        }
    }
    if (vehicle->m_bcm_auth->AsBool())
    {
        OvmsVehicleMgEv::DRLCommandShell(verbosity, writer, cmd, argc, argv);
    }
}

bool OvmsVehicleMgEv::AuthenticateECU(vector<ECUAuth> ECUsToAuth)
{
    // Pause the poller so we're not being interrupted
    {
        OvmsRecMutexLock lock(&m_poll_mutex);
        m_poll_plist = nullptr;
    }
    bool AuthSucceeded = true;
    uint8_t a = 0;
    while (a < ECUsToAuth.size() && AuthSucceeded)
    {
        switch (ECUsToAuth[a])
        {
            case ECUAuth::GWM:
            {
                ESP_LOGI(TAG, "GWM auth requested");
                AuthSucceeded = AuthenticateGWM(m_poll_bus_default);
                break;
            }
            case ECUAuth::BCM:
            {
                ESP_LOGI(TAG, "BCM auth requested");
                AuthSucceeded = AuthenticateBCM(m_poll_bus_default);
                break;
            }
        }
        a++;
    }
    // Re-start polling
    m_poll_plist = m_pollData;
    return AuthSucceeded;
}
