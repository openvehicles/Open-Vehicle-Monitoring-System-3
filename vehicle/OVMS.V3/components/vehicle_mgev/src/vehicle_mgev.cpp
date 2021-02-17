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

#include "ovms_log.h"
static const char *TAG = "v-mgev";

#include "vehicle_mgev.h"
#include "mg_obd_pids.h"
#include "mg_poll_states.h"
#include "metrics_standard.h"

namespace {

const OvmsVehicle::poll_pid_t obdii_polls[] =
{
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsStatusPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryBusVoltagePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCurrentPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryVoltagePid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  0, 30, 30, 60  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCoolantTempPid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPid, {  0, 120, 120, 0  }, 0, ISOTP_STD },
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
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcLvCurrentPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcPowerLoadPid, {  0, 30, 30, 0  }, 0, ISOTP_STD },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcTemperaturePid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVinPid, { 0, 999, 999, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuIgnitionStatePid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcu12vSupplyPid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuCoolantTempPid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuChargerConnectedPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVehicleSpeedPid, {  0, 0, 1, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuMotorSpeedPid, {  0, 0, 5, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuOdometerPid, {  0, 0, 20, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuHandbrakePid, {  0, 0, 10, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuGearPid, {  0, 0, 10, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBrakePid, {  0, 0, 10, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBonnetPid, {  0, 0, 60, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, chargeRatePid, {  0, 30, 0, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcAmbientTempPid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcSetTempPid, {  0, 60, 60, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcFaceOutletTempPid, {  0, 0, 30, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcBlowerSpeedPid, {  0, 0, 30, 0  }, 0, ISOTP_STD },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcPtcTempPid, {  0, 30, 10, 0  }, 0, ISOTP_STD },
    { pepsId, pepsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, pepsLockPid, {  0, 5, 5, 0  }, 0, ISOTP_STD },
    // Only poll when running, BCM requests are performed manually to avoid the alarm    
    // FIXME: Disabled until we are happy that the alarm won't go off
    //{ bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid, {  0, 0, 15, 0  }, 0, ISOTP_STD },
    //{ bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmLightPid, {  0, 0, 15, 0  }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, tyrePressurePid, {  0, 0, 60, 0  }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, typeTemperaturePid, {  0, 0, 60, 0  }, 0, ISOTP_STD },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccVoltagePid, {  0, 30, 0, 0  }, 0, ISOTP_STD },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccAmperagePid, {  0, 30, 0, 0  }, 0, ISOTP_STD },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccMaxAmperagePid, {  0, 30, 0, 0  }, 0, ISOTP_STD },
    { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0, 0 }
};

/// The number of seconds without receiving a CAN packet before we assume the GWM may be in Zombie Mode
constexpr uint32_t ZOMBIE_DETECT_TIMEOUT = 30u;

/// The number of seconds to wait to allow the car to sleep until Zombie Mode Override is used
constexpr uint32_t ZOMBIE_TIMEOUT = 50u;

/// The number of seconds trying to keep awake without success before allowing it to
/// go to sleep
constexpr uint32_t TRANSITION_TIMEOUT = 50u;

/// The number of seconds the car has to be unlocked for before transitioning from session
/// keep alive to TP keep alive
constexpr uint32_t UNLOCKED_CHARGING_TIMEOUT = 5u;

/// Maximum number of times to try and wake the car from Zombie Mode before giving up
constexpr uint16_t DIAG_ATTEMPTS = 3u;

/// Threshold for 12v where we make the assumption that it is being charged
constexpr float CHARGING_THRESHOLD = 12.9;

}  // anon namespace

OvmsVehicleMgEv::OvmsVehicleMgEv()
{
    ESP_LOGI(TAG, "MG EV vehicle module");

    StandardMetrics.ms_v_type->SetValue("MG");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    // Don't support any other mode
    StandardMetrics.ms_v_charge_mode->SetValue("standard");

    mg_cum_energy_charge_wh = 0;
    
    memset(m_vin, 0, sizeof(m_vin));

    m_bat_pack_voltage = MyMetrics.InitFloat(
        "xmg.v.bat.voltage", 0, SM_STALE_HIGH, Volts
    );
    m_env_face_outlet_temp = MyMetrics.InitFloat(
        "xmg.v.env.faceoutlet.temp", 0, SM_STALE_HIGH, Celcius
    );
    m_dcdc_load = MyMetrics.InitFloat("xmg.v.dcdc.load", 0, SM_STALE_HIGH, Percentage);

    PollSetState(PollStateListenOnly);

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

    // Allow unlimited polling per second
    PollSetThrottling(0u);

    MyConfig.RegisterParam("xmg", "MG EV", true, true);
    ConfigChanged(nullptr);
    
    // Add command to get the software versions installed
    m_cmdSoftver = MyCommandApp.RegisterCommand(
        "softver", "MG EV Software",
        PickOvmsCommandExecuteCallback(OvmsVehicleMgEv::SoftwareVersions)
    );
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
#endif
}

OvmsVehicleMgEv::~OvmsVehicleMgEv()
{
    ESP_LOGI(TAG, "Shutdown MG EV vehicle module");

    //xTimerDelete(m_zombieTimer, 0);

    if (m_cmdSoftver)
    {
        MyCommandApp.UnregisterCommand(m_cmdSoftver->GetName());
    }
}

void OvmsVehicleMgEv::SoftwareVersions(
        int, OvmsWriter* writer, OvmsCommand*, int, const char* const*)
{
    auto vehicle = MyVehicleFactory.ActiveVehicle();
    if (vehicle == nullptr)
    {
        writer->puts("No active vehicle");
        return;
    }
    if (strcmp(MyVehicleFactory.ActiveVehicleType(), "MG") != 0)
    {
        writer->puts("Vehicle is not an MG");
        return;
    }
    reinterpret_cast<OvmsVehicleMgEv*>(vehicle)->SoftwareVersions(writer);
}

void OvmsVehicleMgEv::SoftwareVersions(OvmsWriter* writer)
{
    constexpr uint32_t ecus[] = {
        bmsId, dcdcId, vcuId, atcId, bcmId, gwmId, tpmsId, pepsId, 0x761u, 0x760u, 0x784u,
        0x750u, 0x776u, 0x771u, 0x782u, 0x734u, 0x733u, 0x732u, 0x730u, 0x723u, 0x721u,
        0x720u, 0x711u
    };
    const char *names[] = {
        "BMS", "DCDC", "VCU", "ATC", "BCM", "GWM", "TPMS", "PEPS", "ICE", "IPK", "EVCC",
        "ATC", "PLC", "SCU", "TC", "FDR", "FVCM", "RDRA", "SRM", "EPB", "EPS",
        "ABS", "TBOX"
    };
    constexpr uint32_t ecuCount = sizeof(ecus) / sizeof(ecus[0]);

    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        writer->puts("No CAN configured for the car");
        return;
    }

    if (m_gwmState != SendTester)
    {
        writer->puts("Please turn on the ignition first");
        return;
    }

    // Pause the poller so we're not being interrupted
    {
        OvmsRecMutexLock lock(&m_poll_mutex);
        m_poll_plist = nullptr;
    }

    // Send the software version requests
    m_versions.clear();
    for (size_t i = 0u; i < ecuCount; ++i)
    {
        if (ecus[i] == bcmId && StandardMetrics.ms_v_env_locked->AsBool())
        {
            // This will set off the alarm...
            continue;
        }
        ESP_LOGV(TAG, "Sending query to %03x", ecus[i]);
        SendPollMessage(
            currentBus, ecus[i], VEHICLE_POLL_TYPE_OBDIIEXTENDED, softwarePid
        );
    }

    // Wait for the responses
    int counter = 0;
    while (counter < 20 && m_versions.size() < ecuCount)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ++counter;
    }

    // Output responses
    for (const auto& version : m_versions)
    {
        const char* versionString = &version.second.front();
        while (!*versionString)
        {
            ++versionString;
        }
        const char* name = nullptr;
        for (size_t i = 0u; i < ecuCount; ++i)
        {
            if (ecus[i] == version.first)
            {
                name = names[i];
            }
        }
        writer->printf("%s %s\n", name, versionString);
    }
    if (m_versions.empty())
    {
        writer->puts("No ECUs responded");
    }

    // Free memory
    m_versions.clear();

    // Re-start everything
    m_poll_plist = obdii_polls;
}

const char* OvmsVehicleMgEv::VehicleShortName()
{
    return "MG";
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

void OvmsVehicleMgEv::NotifyVehicleIdling()
{
    if (m_poll_state == PollStateRunning)
    {
        OvmsVehicle::NotifyVehicleIdling();
    }
}

// This is the only place we evaluate the vehicle state to change the action of OVMS
void OvmsVehicleMgEv::DeterminePollState(canbus* currentBus, uint32_t ticker)
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
        // 12 V is charging, if this state has just changed we go straignt to running pollstate
        StandardMetrics.ms_v_env_awake->SetValue(true);
        m_afterRunTicker = 0u;
        if (charging12vLast != StandardMetrics.ms_v_env_charging12v->AsBool())
        {
            PollSetState(PollStateRunning);
            ESP_LOGI(TAG, "12V has just started charging, setting to running poll mode. Reading");
        } 

        if(m_gwmState == Undefined)
        {
            PollSetState(PollStateRunning);
            ESP_LOGI(TAG, "GWM in Unknown state, possibly just changed vehicle type. Setting to running poll mode.");
        } 

        
        if ( carIgnitionOn && m_gwmState !=SendDiagnostic) // If ignition is on, we should set the car into running pollstate, Diagnostic override confuses things
        {
            if( m_gwmState != SendTester )
            {
                m_gwmState = SendTester;
                ESP_LOGI(TAG, "Car is Detected as Running, setting Tester Present GWM Mode");
            }

            if( m_poll_state != PollStateRunning)
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
            if(m_poll_state != PollStateCharging)
            {
                PollSetState(PollStateCharging);
                // Do not want to set GWM control here as it should remain in previous state
                ESP_LOGI(TAG, "Car is Detected as Charging, setting Charging Pollstate" );
            }
            if( m_gwmState != SendDiagnostic && m_gwmState != SendTester)
            {
                m_gwmState = SendTester;
                ESP_LOGI(TAG, "Car is Detected as Charging without Session Override, setting Tester Present GWM Mode");
            }
            // ESP_LOGV(TAG, "Car is responding to Queries, rx %i and m_rx %i", rxPackets, m_rxPackets);
            carIsResponsiveToQueries = true;
            m_noRxCount = 0u;
            m_diagCount = 0;
            m_preZombieOverrideTicker = 0; 
            
        } else if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 97.0) // Car has completed charge, topping off 12V
        {
            ESP_LOGV(TAG, "Vehicle is topping of the 12V and is fully charged");
        }
        else // State is not known
        {
            
            if( m_poll_state != PollStateListenOnly)
            {
                ESP_LOGV(TAG, "Vehicle State is Unknown, checking if responsive");
            }

            if (m_rxPackets != rxPackets)
            {
                ESP_LOGV(TAG, "RX Frames Recieved, rx %i and m_rx %i and count %i ", rxPackets, m_rxPackets, m_noRxCount);
                m_rxPackets = rxPackets;
                if( m_noRxCount != 0)
                {
                    m_noRxCount --;
                }
                
            }
            else
            {
                ESP_LOGV(TAG, "No RX Frames Recieved, rx %i and m_rx %i and count %i ", rxPackets, m_rxPackets, m_noRxCount);
                m_noRxCount ++;
                if( m_noRxCount >= ZOMBIE_DETECT_TIMEOUT)
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
        
        if( m_gwmState == SendDiagnostic)
        {
            //Immediately sleep if we were in diagnostics mode
            ESP_LOGI(TAG, "12V has stopped charging, as we were sending diagnostics override, immediately stop this and all polling to unlock cable.");
            PollSetState(PollStateListenOnly);
            m_gwmState = AllowToSleep;
            m_afterRunTicker = (TRANSITION_TIMEOUT +1);
            StandardMetrics.ms_v_env_on->SetValue(false);
            StandardMetrics.ms_v_charge_type->SetValue("not charging");
            if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 97.0)
            {
                StandardMetrics.ms_v_charge_state->SetValue("done");
                StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            }
            else
            {
                StandardMetrics.ms_v_charge_state->SetValue("stopped");
                StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            }
        }else if (charging12vLast != StandardMetrics.ms_v_env_charging12v->AsBool())
        {
            ESP_LOGI(TAG, "12V has stopped charging, remain in current state for %i seconds.", TRANSITION_TIMEOUT);
            //StandardMetrics.ms_v_env_on->SetValue(false);
        }
        m_afterRunTicker ++;
        if(m_afterRunTicker == TRANSITION_TIMEOUT)
        {
            ESP_LOGI(TAG, "12V has not been charging for timeout, stopping all CAN transmission.");
            PollSetState(PollStateListenOnly);
            m_gwmState = AllowToSleep;
            StandardMetrics.ms_v_env_awake->SetValue(false);
        } 
        else if(m_afterRunTicker >= TRANSITION_TIMEOUT)
        {
            // Do not let afterrun ticker go crazy, peg it at TRANSITION_TIMEOUT + 1
            m_afterRunTicker = (TRANSITION_TIMEOUT +1) ;
        }

        

    }

    if( m_afterRunTicker != (TRANSITION_TIMEOUT +1))
    {
        ESP_LOGV(TAG, "Pollstate: %i , GWM State: %i , Rx Packet Count: %i , 12V level: %.2f.", m_poll_state, m_gwmState, rxPackets, voltage12V);
    }
    return;
}

void OvmsVehicleMgEv::ZombieMode()
{
    // This state is reached as the car is now charging the 12V but the state is not known
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }
    if( m_diagCount > DIAG_ATTEMPTS)
    {
        if( m_poll_state != PollStateBackup)
        {
            ESP_LOGI(TAG, "Maximum number of Session Override Attempts Limit Reached going to backup mode, SoC reports only");
            PollSetState(PollStateBackup);
        }
        return;
    }
    
    if(m_preZombieOverrideTicker == 0)
    {
        ESP_LOGI(TAG, "OVMS thinks the GWM is in Zombie Mode, we are stopping all queries for 50 s until to try session override");
        PollSetState(PollStateListenOnly);
        m_gwmState = AllowToSleep;
    }
    m_preZombieOverrideTicker ++;
    if( m_preZombieOverrideTicker <= ZOMBIE_TIMEOUT)
    {
        ESP_LOGV(TAG, "Zombie Wait for %i Seconds.", m_preZombieOverrideTicker);
        return;
    }
    m_diagCount ++;
    
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
    return;
}

void OvmsVehicleMgEv::SendAlarmSensitive(canbus* currentBus)
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

void OvmsVehicleMgEv::Ticker1(uint32_t ticker)
{
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }

    if (m_gwmState == SendTester)
    {
        SendTesterPresentTo(currentBus, gwmId);
    }

    if(m_gwmState == SendDiagnostic)
    {
        SendDiagSessionTo(currentBus, gwmId, 2u);
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
    }
    if (previousGwmState != m_gwmState)
    {
        ESP_LOGI(TAG, "GWM Control state from %d to %d", previousGwmState, m_gwmState);
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
    
    // Calculate cumulative charge energy each second
    if(StandardMetrics.ms_v_charge_inprogress->AsBool())
    {
        mg_cum_energy_charge_wh += StandardMetrics.ms_v_charge_power->AsFloat()*1000/3600;
        StandardMetrics.ms_v_charge_kwh->SetValue(mg_cum_energy_charge_wh/1000);
        ESP_LOGV(TAG,"Cumulative Energy = %.2f",mg_cum_energy_charge_wh);
    } else { // Not charging set back to zero. TODO - Only reset when charge first stops
        mg_cum_energy_charge_wh=0;
        StandardMetrics.ms_v_charge_duration_full->SetValue(0);
    }
}

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
    PollSetPidList(newBus, obdii_polls);

    if (oldBus != nullptr)
    {
        ESP_LOGI(TAG, "Stopping previous CAN bus");
        oldBus->Stop();
        oldBus->SetPowerMode(PowerMode::Off);
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleMgEv::CommandWakeup()
{
    // Sets up the Poller Interface, cannot do anything else yet as OVMS does not wake the car
    auto wakeBus = m_poll_bus_default;
    ConfigurePollInterface(0);
    ConfigurePollInterface(wakeBus->m_busnumber + 1);
    m_rxPackets = 0u;
    m_gwmState = Undefined;
    return OvmsVehicle::Success;
}

class OvmsVehicleMgEvInit
{
  public:
    OvmsVehicleMgEvInit();
} MyOvmsVehicleMgEvInit  __attribute__ ((init_priority (9000)));

OvmsVehicleMgEvInit::OvmsVehicleMgEvInit()
{
    ESP_LOGI(TAG, "Registering Vehicle: MG EV (9000)");

    MyVehicleFactory.RegisterVehicle<OvmsVehicleMgEv>("MG", "MG EV");
}
