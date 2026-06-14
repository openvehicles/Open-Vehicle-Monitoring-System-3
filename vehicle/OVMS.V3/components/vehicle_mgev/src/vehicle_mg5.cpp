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
 ;    (C) 2023-2025  Peter Harry
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
#include "vehicle_mg5.hpp"

static const char *TAG = "v-mgev";

namespace {

const OvmsPoller::poll_pid_t mg5_obdii_polls[] =
{
    { gwmId, gwmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vehStatusPid, {  0, 1, 1, 0  }, 0, ISOTP_STD },// 0xd001u
    { gwmId, gwmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vehTimePid,  {  0, 60, 60, 0  }, 0, ISOTP_STD },// 0x010bu
    { gwmId, gwmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, odoPid, {  0, 999, 30, 0  }, 0, ISOTP_STD },// 0xb921u
    { gwmId, gwmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vinPid, {  0, 999, 999, 0  }, 0, ISOTP_STD },// 0xf190u

    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIICURRENT, socPid, {  0, 15, 6, 60  }, 0, ISOTP_STD },// 0X5bu

    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsStatusPid, {  0, 15, 6, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryBusVoltagePid, {  0, 5, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCurrentPid, {  0, 5, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryVoltagePid, {  0, 5, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryResistancePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
//    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  0, 30, 30, 60  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCoolantTempPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPid, {  0, 120, 120, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryTempPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryErrorPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsRangePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell1StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell2StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell3StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell4StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell5StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell6StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell7StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell8StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell9StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell10StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell11StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell12StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell13StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell14StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell15StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell16StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell17StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell18StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell19StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell20StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell21StatPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsMaxCellVoltagePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsMinCellVoltagePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsTimePid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    //{ bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayBPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    //{ bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayGPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    //{ bmsMk2Id, bmsMk2Id | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsSystemMainRelayPPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    
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

    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, tyrePressurePid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, tyreTemperaturePid, {  0, 60, 60, 0  }, 0, ISOTP_STD },

    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccVoltagePid, {  0, 5, 0, 0  }, 0, ISOTP_STD },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccAmperagePid, {  0, 5, 0, 0  }, 0, ISOTP_STD },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccMaxAmperagePid, {  0, 30, 0, 0  }, 0, ISOTP_STD },

    { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0, 0 }
};
}

//Called by OVMS when MG5 is chosen as vehicle type (and on startup when MG5 is chosen)
OvmsVehicleMg5::OvmsVehicleMg5()
{
    ESP_LOGI(TAG, "Starting MG5 variant");
    
    // Set manual polling on
    MyConfig.SetParamValueInt("xmg", "polling.manual", 1);
    
    // Set up initial values from the version setting
    int VehicleVersion = MyConfig.GetParamValueInt("xmg", "vehval", 0);
    if(VehicleVersion == 0) {
        ESP_LOGV(TAG,"MG5 Version - SR");
        StandardMetrics.ms_v_bat_range_full->SetValue(290.0);
        m_batt_capacity->SetValue(48.8);
        m_max_dc_charge_rate->SetValue(80);
        m_dod_lower->SetValue(36.0);
        m_dod_upper->SetValue(994.0);

    } else {
        ESP_LOGV(TAG,"MG5 Version - LR");
        StandardMetrics.ms_v_bat_range_full->SetValue(320.0);
        m_batt_capacity->SetValue(57.4);
        m_max_dc_charge_rate->SetValue(87);
        m_dod_lower->SetValue(36.0);
        m_dod_upper->SetValue(950.0);
    }
    ESP_LOGD(TAG, "MG5 Values - Range: %0.1f Battery kWh: %0.1f Charge Max: %0.1f", StandardMetrics.ms_v_bat_range_full->AsFloat(),
             m_batt_capacity->AsFloat(),
             m_max_dc_charge_rate->AsFloat());
        
    //Add variant specific poll data
    ConfigureMG5PollData(mg5_obdii_polls, sizeof(mg5_obdii_polls));
    
    //BMS Configuration
    BmsSetCellArrangementVoltage(42, 2);
    BmsSetCellArrangementTemperature(42, 2);
    BmsSetCellLimitsVoltage(3.0, 5.0);
    BmsSetCellLimitsTemperature(-39, 200);
    BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
    BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
    
    // Register shell commands
    cmd_xmg->RegisterCommand("polls", "Turn polling on", PollsCommandShell, "<command>\non\tTurn on\noff\tTurn off", 1, 1);
    //PollSetState(PollStateRunning);
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    Mg5WebInit();
#endif
}

//Called by OVMS when vehicle type is changed from current
OvmsVehicleMg5::~OvmsVehicleMg5()
{
    ESP_LOGI(TAG, "Shutdown MG5");
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    FeaturesWebDeInit();
    VersionWebDeInit();
#endif
}

//Called by OVMS every 1s
void OvmsVehicleMg5::Ticker1(uint32_t ticker)
{
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }
    
    const auto previousPollState = m_poll_state;
    
    MainStateMachine(currentBus, ticker);
    
    if (previousPollState != m_poll_state)
    {
        ESP_LOGI(TAG, "Changed poll state from %d to %d", previousPollState, m_poll_state);
        m_poll_state_metric->SetValue(m_poll_state);
    }
    
    processEnergy();
}

// This is the only place we evaluate the vehicle state to change the action of OVMS
void OvmsVehicleMg5::MainStateMachine(canbus* currentBus, uint32_t ticker)
{
    
    // Store last 12V charging state and re-evaluate if the 12V is charging
    const auto charging12vLast = StandardMetrics.ms_v_env_charging12v->AsBool();
    
    StandardMetrics.ms_v_env_charging12v->SetValue(
                StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= CHARGING_THRESHOLD);
    
    if(MyConfig.GetParamValueInt("xmg", "polling.manual") == 1) {
        //ESP_LOGD(TAG,"Polling is manual");
        if(!StandardMetrics.ms_v_env_charging12v->AsBool()) {
            // 12V not being charged so vehicle is off
            StandardMetrics.ms_v_env_awake->SetValue(false);
            if(charging12vLast) {
                // Vehicle has just been turnrd off
                ESP_LOGD(TAG,"Vehicle just been turned off");
            }
            PollSetState(PollStateListenOnly);
            m_enable_polling->SetValue(false);
        } else if(m_enable_polling->AsBool()) {
            if (StandardMetrics.ms_v_charge_inprogress->AsBool())
            {
                PollSetState(PollStateCharging);
            }
            else
            {
                PollSetState(PollStateRunning);
            }
        } else {
            PollSetState(PollStateListenOnly);
        }
    } else {
        ESP_LOGD(TAG,"Polling is automatic");
        //Check if 12V is high enough for OVMS to poll otherwise we will drain the battery too much
        if (StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= CHARGING_THRESHOLD)
        {
            //12V level is high enough for OVMS to poll
            m_OVMSActive = true;
            m_afterRunTicker = 0;
            switch (static_cast<GWMStates>(m_gwm_state->AsInt()))
            {
                //Unknown state. Will see if GWM is awake by sending a tester present. If no reply, try waking GWM up by sending another tester present.
                //Once GWM is awake, check if BCM responds to tester present. If it does, GWM is unlocked. If no reply, attempt authentication with GWM.
                //If successful, GWM is unlocked. When GWM is unlocked, set to Unlocked state and start polling.
                //If we cannot wake GWM, set state to WaitToRetryCheckState and wait to retry in GWM_RETRY_CHECK_STATE_TIMEOUT seconds.
                case GWMStates::Unknown:
                {
                    PollSetState(PollStateListenOnly);
                    ESP_LOGI(TAG, "12V is over threshold. Unknown state. Checking if GWM is awake");
                    m_gwm_state->SetValue(static_cast<int>(GWMStates::CheckingState));
                    std::string Response;
                    int PollStatus = PollSingleRequest(currentBus, gwmId, gwmId | rxFlag, hexdecode("3e00"), Response, SYNC_REQUEST_TIMEOUT, ISOTP_STD);
                    ESP_LOGI(TAG, "Response (%d): %s", PollStatus, hexencode(Response).c_str());
                    if (PollStatus == 0)
                    {
                        GWMAwake(currentBus);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "GWM did not respond to tester present, send another tester present to wake up GWM");
                        PollStatus = PollSingleRequest(currentBus, gwmId, gwmId | rxFlag, hexdecode("3e00"), Response, SYNC_REQUEST_TIMEOUT, ISOTP_STD);
                        ESP_LOGI(TAG, "Response (%d): %s", PollStatus, hexencode(Response).c_str());
                        if (PollStatus == 0)
                        {
                            GWMAwake(currentBus);
                        }
                        else
                        {
                            ESP_LOGI(TAG, "Failed to wake GWM");
                            RetryCheckState();
                        }
                    }
                    break;
                }
                case GWMStates::WaitToRetryCheckState:
                {
                    m_RetryCheckStateWaitCount++;
                    ESP_LOGI(TAG, "Waiting 1s before retrying GWM check state (%d)", m_RetryCheckStateWaitCount);
                    if (m_RetryCheckStateWaitCount >= GWM_RETRY_CHECK_STATE_TIMEOUT)
                    {
                        m_gwm_state->SetValue(static_cast<int>(GWMStates::Unknown));
                    }
                    break;
                }
                case GWMStates::Unlocked:
                {
                    //Set poll state
                    if (StandardMetrics.ms_v_charge_inprogress->AsBool())
                    {
                        PollSetState(PollStateCharging);
                    }
                    else
                    {
                        PollSetState(PollStateRunning);
                    }

                    //Count number of ticks that GWM is unresponsive to tester present. If this exceeds threshold, reset state back to Unknown
                    if (m_WaitingGWMTesterPresentResponse)
                    {
                        m_GWMUnresponsiveCount++;
                        ESP_LOGI(TAG, "GWM did not respond to tester present (%d)", m_GWMUnresponsiveCount);
                        if (m_GWMUnresponsiveCount >= CAR_UNRESPONSIVE_THRESHOLD)
                        {
                            GWMUnknown();
                            return;
                        }
                    }
                    else
                    {
                        m_GWMUnresponsiveCount = 0;
                    }

                    //Send tester present to GWM
                    if (m_gwm_task->AsInt() == static_cast<int>(GWMTasks::None))
                    {
                        SendTesterPresentTo(currentBus, gwmId);
                        m_WaitingGWMTesterPresentResponse = true;
                    }
                    else
                    {
                        ESP_LOGI(TAG, "GWM busy, skip sending tester present");
                    }
                    break;
                }
                default: {}
            }
        }
        else
        {
            //12V level too low, going to sleep in TRANSITION_TIMEOUT seconds
            if (m_OVMSActive)
            {
                ESP_LOGI(TAG, "12V level lower than threshold, going to sleep in %us.", TRANSITION_TIMEOUT);
            }
            m_OVMSActive = false;
            StandardMetrics.ms_v_env_on->SetValue(false);
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
            }
            StandardMetrics.ms_v_charge_type->SetValue("undefined");
            StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            if (m_afterRunTicker < TRANSITION_TIMEOUT)
            {
                ESP_LOGV(TAG, "(%" PRIu32 ") Waiting %us before going to sleep", m_afterRunTicker, TRANSITION_TIMEOUT);
                m_afterRunTicker++;
            }
            else if (m_afterRunTicker == TRANSITION_TIMEOUT)
            {
                ESP_LOGI(TAG, "%us has elasped after 12V level is lower than threshold, going to sleep", TRANSITION_TIMEOUT);
                m_afterRunTicker++; //Increment one more so we don't enter this if statement next time round
                GWMUnknown();
            }
        }
    }
}

//Called by OVMS when a wake up command is requested
OvmsVehicle::vehicle_command_t OvmsVehicleMg5::CommandWakeup()
{
    ESP_LOGI(TAG, "Starting Polling");
    m_enable_polling->SetValue(true);
    return OvmsVehicle::Success;
}

//This registers a new vehicle type in the vehicle configuration page drop down menu
class OvmsVehicleMg5Init
{
public:
    OvmsVehicleMg5Init();
} MyOvmsVehicleMg5Init  __attribute__ ((init_priority (9000)));

OvmsVehicleMg5Init::OvmsVehicleMg5Init()
{
    ESP_LOGI(TAG, "Registering Vehicle: MG5 (9000)");
    
    MyVehicleFactory.RegisterVehicle<OvmsVehicleMg5>("MG5", "MG 5 (2020-2023)");
}

