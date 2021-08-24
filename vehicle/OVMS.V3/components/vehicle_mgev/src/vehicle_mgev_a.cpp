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

#include "vehicle_mgev_a.h"

namespace {

/// The number of seconds without receiving a CAN packet before we assume the GWM may be in Zombie Mode
constexpr uint32_t ZOMBIE_DETECT_TIMEOUT = 30u;

/// The number of seconds to wait to allow the car to sleep until Zombie Mode Override is used
constexpr uint32_t ZOMBIE_TIMEOUT = 50u;

/// The number of seconds the car has to be unlocked for before transitioning from session
/// keep alive to TP keep alive
constexpr uint32_t UNLOCKED_CHARGING_TIMEOUT = 5u;

/// Maximum number of times to try and wake the car from Zombie Mode before giving up
constexpr uint16_t DIAG_ATTEMPTS = 3u;

//Variant a specific polls
const OvmsVehicle::poll_pid_t obdii_polls_a[] =
{
    { pepsId, pepsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, pepsLockPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBonnetPid, {  0, 0, 5, 0  }, 0, ISOTP_STD }
};

}  // anon namespace

//Called by OVMS when MG EV variant A is chosen as vehicle type (and on startup when MG EV variant A is chosen)
OvmsVehicleMgEvA::OvmsVehicleMgEvA()
{
    ESP_LOGI(TAG, "Starting MG EV variant A");
    m_rxPacketTicker = 0u;
    m_rxPackets = 0u;
    
    // Until we know it's unlocked, say it's locked otherwise we might set off the alarm
    StandardMetrics.ms_v_env_locked->SetValue(true);
    StandardMetrics.ms_v_env_on->SetValue(false);

    // Assume the CAN is off to start with
    m_gwmState = Undefined;
    m_afterRunTicker = 0u;
    m_diagCount = 0u;
    m_noRxCount = 0u;
    m_preZombieOverrideTicker = 0u;
    carIsResponsiveToQueries = true;

    //Add variant specific poll data
    ConfigurePollData(obdii_polls_a, sizeof(obdii_polls_a));
}

//Called by OVMS when vehicle type is changed from current
OvmsVehicleMgEvA::~OvmsVehicleMgEvA()
{
    ESP_LOGI(TAG, "Shutdown MG EV variant A");
}

// This is the only place we evaluate the vehicle state to change the action of OVMS
void OvmsVehicleMgEvA::DeterminePollState(canbus* currentBus, uint32_t ticker)
{
    // Store last 12V charging state and re-evaluate if the 12V is charging
    
    const auto charging12vLast = StandardMetrics.ms_v_env_charging12v->AsBool();
    /// Current Number of packets recieved on CAN
    const auto rxPackets = currentBus->m_status.packets_rx;
    
    // Evaluate if Ignition has just switched on
    const auto carIgnitionOn = StandardMetrics.ms_v_env_on->AsBool();
    const auto carIsCharging = StandardMetrics.ms_v_charge_inprogress->AsBool();
    const auto voltage12V = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
    StandardMetrics.ms_v_env_charging12v->SetValue(
                 StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= CHARGING_THRESHOLD
             );

    if (StandardMetrics.ms_v_env_charging12v->AsBool())
    {
        // 12 V is charging, if this state has just changed we go straight to running pollstate
        StandardMetrics.ms_v_env_awake->SetValue(true);
        m_afterRunTicker = 0u;
        if (!charging12vLast)
        {
            PollSetState(PollStateRunning);
            ESP_LOGI(TAG, "12V has just started charging, setting to running poll mode. Reading");
        } 

        if (m_gwmState == Undefined)
        {
            PollSetState(PollStateRunning);
            ESP_LOGI(TAG, "GWM in Unknown state, possibly just changed vehicle type. Setting to running poll mode.");
        } 

        if (carIgnitionOn && m_gwmState != SendDiagnostic) // If ignition is on, we should set the car into running pollstate, Diagnostic override confuses things
        {
            if (m_gwmState != SendTester)
            {
                m_gwmState = SendTester;
                ESP_LOGI(TAG, "Car is Detected as Running, setting Tester Present GWM Mode");
            }

            if (m_poll_state != PollStateRunning)
            {
                // Car ignition Is on
                PollSetState(PollStateRunning);
                ESP_LOGI(TAG, "Car is Detected as Running, setting Running Pollstate");
            }
            // ESP_LOGV(TAG, "Car is responding to Queries, rx %i and m_rx %i", rxPackets, m_rxPackets);
            carIsResponsiveToQueries = true;
            m_noRxCount = 0u;
            m_diagCount = 0;
            m_preZombieOverrideTicker = 0;
        } 
        else if (carIsCharging) // If car is in a charging state we should set charging pollstate
        {
            if (m_poll_state != PollStateCharging)
            {
                PollSetState(PollStateCharging);
                // Do not want to set GWM control here as it should remain in previous state
                ESP_LOGI(TAG, "Car is Detected as Charging, setting Charging Pollstate" );
            }
            if (m_gwmState != SendDiagnostic && m_gwmState != SendTester)
            {
                m_gwmState = SendTester;
                ESP_LOGI(TAG, "Car is Detected as Charging without Session Override, setting Tester Present GWM Mode");
            }
            // ESP_LOGV(TAG, "Car is responding to Queries, rx %i and m_rx %i", rxPackets, m_rxPackets);
            carIsResponsiveToQueries = true;
            m_noRxCount = 0u;
            m_diagCount = 0;
            m_preZombieOverrideTicker = 0; 
        }
        else if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 99.9) // Car has completed charge, topping off 12V
        {
            ESP_LOGV(TAG, "Vehicle is topping off the 12V and is fully charged");
        }
        else  // State is not known
        {
            if (m_poll_state != PollStateListenOnly)
            {
                ESP_LOGV(TAG, "Vehicle State is Unknown, checking if responsive");
            }

            if (m_rxPackets != rxPackets)
            {
                ESP_LOGV(TAG, "RX Frames Recieved, rx %i and m_rx %i and count %i ", rxPackets, m_rxPackets, m_noRxCount);
                m_rxPackets = rxPackets;
                if (m_noRxCount != 0)
                {
                    --m_noRxCount;
                }
            }
            else
            {
                ESP_LOGV(TAG, "No RX Frames Recieved, rx %i and m_rx %i and count %i ", rxPackets, m_rxPackets, m_noRxCount);
                ++m_noRxCount;
                if (m_noRxCount >= ZOMBIE_DETECT_TIMEOUT)
                {
                    carIsResponsiveToQueries = false;
                    ZombieMode();
                }
            }
        }
    } 
    else
    {
        // 12 V is now not charging, notify
        if (m_gwmState == SendDiagnostic)
        {
            //Immediately sleep if we were in diagnostics mode
            ESP_LOGI(TAG, "12V has stopped charging, as we were sending diagnostics override, immediately stop this and all polling to unlock cable.");
            PollSetState(PollStateListenOnly);
            m_gwmState = AllowToSleep;
            m_afterRunTicker = (TRANSITION_TIMEOUT +1);
            StandardMetrics.ms_v_env_on->SetValue(false);
            StandardMetrics.ms_v_charge_type->SetValue("undefined");
            StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 99.9) //Set to 99.9 instead of 100 incase of mathematical errors
            {
                StandardMetrics.ms_v_charge_state->SetValue("done");
            }
            else
            {
                StandardMetrics.ms_v_charge_state->SetValue("stopped");
            }
        }
        else if (charging12vLast != StandardMetrics.ms_v_env_charging12v->AsBool())
        {
            ESP_LOGI(TAG, "12V has stopped charging, remain in current state for %i seconds.", TRANSITION_TIMEOUT);
            //StandardMetrics.ms_v_env_on->SetValue(false);
        }

        ++m_afterRunTicker;
        if (m_afterRunTicker == TRANSITION_TIMEOUT)
        {
            ESP_LOGI(TAG, "12V has not been charging for timeout, stopping all CAN transmission.");
            PollSetState(PollStateListenOnly);
            m_gwmState = AllowToSleep;
            StandardMetrics.ms_v_env_awake->SetValue(false);
            m_bcm_auth->SetValue(false);
        } 
        else if (m_afterRunTicker >= TRANSITION_TIMEOUT)
        {
            // Do not let afterrun ticker go crazy, peg it at TRANSITION_TIMEOUT + 1
            m_afterRunTicker = (TRANSITION_TIMEOUT +1) ;
        }
    }

    if (m_afterRunTicker != (TRANSITION_TIMEOUT +1))
    {
        ESP_LOGV(TAG, "Pollstate: %i , GWM State: %i , Rx Packet Count: %i , 12V level: %.2f.", m_poll_state, m_gwmState, rxPackets, voltage12V);
    }
    return;
}

void OvmsVehicleMgEvA::ZombieMode()
{
    // This state is reached as the car is now charging the 12V but the state is not known
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }
    if (m_diagCount > DIAG_ATTEMPTS)
    {
        if (m_poll_state != PollStateBackup)
        {
            ESP_LOGI(TAG, "Maximum number of Session Override Attempts Limit Reached going to backup mode, SoC reports only");
            PollSetState(PollStateBackup);
        }
        return;
    }

    if (m_preZombieOverrideTicker == 0)
    {
        ESP_LOGI(TAG, "OVMS thinks the GWM is in Zombie Mode, we are stopping all queries for 50 s until to try session override");
        PollSetState(PollStateListenOnly);
        m_gwmState = AllowToSleep;
    }
    ++m_preZombieOverrideTicker;
    if (m_preZombieOverrideTicker <= ZOMBIE_TIMEOUT)
    {
        ESP_LOGV(TAG, "Zombie Wait for %i Seconds.", m_preZombieOverrideTicker);
        return;
    }
    ++m_diagCount;

    m_preZombieOverrideTicker = 0u;
    m_noRxCount = 0u;
    ESP_LOGI(TAG, "Sending Session Override");
    for (int i = 0; i < 5; ++i)
    {
        SendDiagSessionTo(currentBus, gwmId, 2u);
        vTaskDelay(70 / portTICK_PERIOD_MS);
    }
    PollSetState(PollStateCharging);
    m_gwmState = SendDiagnostic;
}

void OvmsVehicleMgEvA::SendAlarmSensitive(canbus* currentBus)
{
    // Disabled BMS queries when 12V is not charging as this can (slowly) drain the 12V 
    // over 3 days or so as it continually wakes the GWM, wake is now going to be detected by 12V change
    // TODO delete this?
    if (StandardMetrics.ms_v_env_charging12v->AsBool())
    {
        ESP_LOGV(TAG, "12V is charging send BMS Queries.");
        SendPollMessage(currentBus, bcmId, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid);
        return;
    }
}

//Called by OVMS every 1s
void OvmsVehicleMgEvA::Ticker1(uint32_t ticker)
{
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }

    if (m_gwmState == SendTester)
    {
        //If GWM is not busy, send tester present
        if (m_gwm_task->AsInt() == static_cast<int>(GWMTasks::None))
        {
            SendTesterPresentTo(currentBus, gwmId);
        }
        else
        {
            ESP_LOGI(TAG, "GWM busy, skip sending tester present");
        }           
        
        // If BCM is authenticated and not busy, send tester present to keep authentication. 
        // If we have other PID requests to the BCM, we must make sure that tester present doesn't get sent while we are still waiting for a PID response. We can either use SendManualPolls() or add tester present to poll table (like variant B).
        if (m_bcm_auth->AsBool())
        {
            if (m_bcm_task->AsInt() == static_cast<int>(BCMTasks::None))
            {
                SendTesterPresentTo(currentBus, bcmId);
            }
            else
            {
                ESP_LOGI(TAG, "BCM busy, skip sending tester present");
            }     
        }          
    }

    if (m_gwmState == SendDiagnostic)
    {
        //If GWM is not busy, send diagnostic session
        if (m_gwm_task->AsInt() == static_cast<int>(GWMTasks::None))
        {
            SendDiagSessionTo(currentBus, gwmId, 2u);
        }
        else
        {
            ESP_LOGI(TAG, "GWM busy, skip sending diagnostic session");
        }            
        SendDiagSessionTo(currentBus, ipkId, 2u);
    }

    StandardMetrics.ms_v_env_ctrl_login->SetValue(
        m_gwmState == SendTester || m_gwmState == SendDiagnostic
    );

    const auto previousState = m_poll_state;
    const auto previousGwmState = m_gwmState;
    DeterminePollState(currentBus, ticker);
    if (previousState != m_poll_state)
    {
        ESP_LOGI(TAG, "Changed poll state from %d to %d", previousState, m_poll_state);
        m_poll_state_metric->SetValue(m_poll_state);
    }
    if (previousGwmState != m_gwmState)
    {
        ESP_LOGI(TAG, "GWM Control state from %d to %d", previousGwmState, m_gwmState);
        m_gwm_state->SetValue(static_cast<int>(m_gwmState));
    }

    // if (xTimerIsTimerActive(m_zombieTimer))
    // {
    //     if (m_gwmState != Diagnostic)
    //     {
    //         xTimerStop(m_zombieTimer, 0);
    //     }
    // }
    // else if (m_gwmState == Diagnostic)
    // {
    //     // TP seems more reliable at waking the gateway and it doesn't harm to send it
    //     // We take a second out to do that before starting the session
    //     xTaskCreatePinnedToCore(
    //         &OvmsVehicleMgEv::WakeUp, "MG TP Wake Up", 4096, this, tskIDLE_PRIORITY,
    //         nullptr, CORE(1)
    //     );
    //     xTimerStart(m_zombieTimer, 0);
    // }
    //
    processEnergy();
}

OvmsVehicle::vehicle_command_t OvmsVehicleMgEvA::CommandWakeup()
{
    // Sets up the Poller Interface, cannot do anything else yet as OVMS does not wake the car
    auto wakeBus = m_poll_bus_default;
    ConfigurePollInterface(0);
    ConfigurePollInterface(wakeBus->m_busnumber + 1);
    m_rxPackets = 0u;
    m_gwmState = Undefined;
    return OvmsVehicle::Success;
}

//This registers a new vehicle type in the vehicle configuration page drop down menu
class OvmsVehicleMgEvAInit
{
    public:
        OvmsVehicleMgEvAInit();
} MyOvmsVehicleMgEvAInit  __attribute__ ((init_priority (9000)));

OvmsVehicleMgEvAInit::OvmsVehicleMgEvAInit()
{
    ESP_LOGI(TAG, "Registering Vehicle: MG EV (UK/EU) (9000)");

    MyVehicleFactory.RegisterVehicle<OvmsVehicleMgEvA>("MGA", "MG EV (UK/EU)");
}
