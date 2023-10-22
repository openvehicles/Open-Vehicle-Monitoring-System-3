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

#include "vehicle_mgev_b.h"

namespace 
{

//Variant b specific polls
const OvmsVehicle::poll_pid_t obdii_polls_b[] =
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
    { bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid, {  0, 1, 1, 0  }, 0, ISOTP_STD },
    { bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDrlPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_TESTERPRESENT, 0, {  0, 2, 2, 0  }, 0, ISOTP_STD} //This is required for keeping BCM authentication. Technically it is only needed if BCM is authenticated but it doesn't seem to cause any side effects when BCM is not authenticated so it is put here for simplicity sake so that we don't have to add/remove this line depending on whether BCM is authenticated or not.
};

}

//Called by OVMS when MG EV variant B is chosen as vehicle type (and on startup when MG EV variant B is chosen)
OvmsVehicleMgEvB::OvmsVehicleMgEvB()
{
    ESP_LOGI(TAG, "Starting MG EV variant B");

    //Initialise GWM state to Unknown
    m_gwm_state->SetValue(static_cast<int>(GWMStates::Unknown));
    
    // Set Max Range to WLTP Range
    StandardMetrics.ms_v_bat_range_full->SetValue(WLTP_RANGE);
    m_batt_capacity->SetValue(BATT_CAPACITY);
    m_max_dc_charge_rate->SetValue(MAX_CHARGE_RATE);
    m_dod_lower->SetValue(BMSDoDLowerLimit);
    m_dod_upper->SetValue(BMSDoDUpperLimit);

    //Add variant specific poll data
    ConfigurePollData(obdii_polls_b, sizeof(obdii_polls_b));
    
    //BMS Configuration
    BmsSetCellArrangementVoltage(18, 2);
    BmsSetCellArrangementTemperature(18, 2);
    BmsSetCellLimitsVoltage(3.0, 5.0);
    BmsSetCellLimitsTemperature(-39, 200);
    BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
    BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
    
    // Register shell commands
    cmd_xmg->RegisterCommand("auth", "Authenticate with ECUs", AuthenticateECUShell, "<ECU>\nall\tAll ECUs\ngwm\tGWM only\nbcm\tBCM only", 1, 1);
    cmd_xmg->RegisterCommand("drl", "Daytime running light control", DRLCommandWithAuthShell, "<command>\non\tTurn on\noff\tTurn off", 1, 1);
    cmd_xmg->RegisterCommand("drln", "Daytime running light control (no BCM authentication)", DRLCommandShell, "<command>\non\tTurn on\noff\tTurn off", 1, 1);

}

//Called by OVMS when vehicle type is changed from current
OvmsVehicleMgEvB::~OvmsVehicleMgEvB()
{
    ESP_LOGI(TAG, "Shutdown MG EV variant B");
}

//Called by OVMS every 1s
void OvmsVehicleMgEvB::Ticker1(uint32_t ticker)
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
void OvmsVehicleMgEvB::MainStateMachine(canbus* currentBus, uint32_t ticker)
{
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

OvmsVehicle::vehicle_command_t OvmsVehicleMgEvB::CommandWakeup()
{
    // Set up the Poller Interface
    auto wakeBus = m_poll_bus_default;
    ConfigurePollInterface(0);
    ConfigurePollInterface(wakeBus->m_busnumber + 1);

    ESP_LOGI(TAG, "Waking up car by sending 2 tester present frames");
    std::string Response;
    ESP_LOGI(TAG, "Sending first tester present");
	int PollStatus = PollSingleRequest(wakeBus, gwmId, gwmId | rxFlag, hexdecode("3e00"), Response, SYNC_REQUEST_TIMEOUT, ISOTP_STD);
	ESP_LOGI(TAG, "Response (%d): %s", PollStatus, hexencode(Response).c_str());
	if (PollStatus == 0)
	{    
        ESP_LOGI(TAG, "Wake success");
        return OvmsVehicle::Success;
    }
    else
    {
        ESP_LOGI(TAG, "Sending second tester present");
        PollStatus = PollSingleRequest(wakeBus, gwmId, gwmId | rxFlag, hexdecode("3e00"), Response, SYNC_REQUEST_TIMEOUT, ISOTP_STD);
	    ESP_LOGI(TAG, "Response (%d): %s", PollStatus, hexencode(Response).c_str());
        if (PollStatus == 0)
        {
            ESP_LOGI(TAG, "Wake success");
            return OvmsVehicle::Success;            
        }
        else
        {
            ESP_LOGI(TAG, "Wake failed");
            return OvmsVehicle::Fail;
        }
    }
}

//This registers a new vehicle type in the vehicle configuration page drop down menu
class OvmsVehicleMgEvBInit
{
    public:
        OvmsVehicleMgEvBInit();
} MyOvmsVehicleMgEvBInit  __attribute__ ((init_priority (9000)));

OvmsVehicleMgEvBInit::OvmsVehicleMgEvBInit()
{
    ESP_LOGI(TAG, "Registering Vehicle: MG ZS EV (AU/TH) (9000)");

    MyVehicleFactory.RegisterVehicle<OvmsVehicleMgEvB>("MGB", "MG ZS EV (AU/TH)");
}
