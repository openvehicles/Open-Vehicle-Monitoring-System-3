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
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsStatusPid, {  0, 5, 5, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryBusVoltagePid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCurrentPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryVoltagePid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCoolantTempPid, {  0, 60, 60, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPid, {  0, 120, 120, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsRangePid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell1StatPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell2StatPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell3StatPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell4StatPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell5StatPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell6StatPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell7StatPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell8StatPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell9StatPid, {  0, 30, 30, 0 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcLvCurrentPid, {  0, 30, 30, 0 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcPowerLoadPid, {  0, 30, 30, 0 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcTemperaturePid, {  0, 60, 60, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVinPid, { 0, 999, 999, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuIgnitionStatePid, {  0, 5, 5, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcu12vSupplyPid, {  0, 60, 60, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuCoolantTempPid, {  0, 60, 60, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuChargerConnectedPid, {  0, 5, 5, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVehicleSpeedPid, {  0, 0, 1, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuMotorSpeedPid, {  0, 0, 5, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuOdometerPid, {  0, 0, 20, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuHandbrakePid, {  0, 0, 10, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuGearPid, {  0, 0, 10, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBrakePid, {  0, 0, 10, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBonnetPid, {  0, 0, 60, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, chargeRatePid, {  0, 30, 0, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcAmbientTempPid, {  0, 60, 60, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcSetTempPid, {  0, 60, 60, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcFaceOutletTempPid, {  0, 0, 30, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcBlowerSpeedPid, {  0, 0, 30, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcPtcTempPid, {  0, 30, 10, 0 }, 0 },
    { pepsId, pepsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, pepsLockPid, {  0, 5, 5, 0 }, 0 },
    // Only poll when running, BCM requests are performed manually to avoid the alarm    
    // FIXME: Disabled until we are happy that the alarm won't go off
    //{ bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid, {  0, 0, 15, 0 }, 0 },
    //{ bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmLightPid, {  0, 0, 15, 0 }, 0 },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, tyrePressurePid, {  0, 0, 60, 0 }, 0 },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, typeTemperaturePid, {  0, 0, 60, 0 }, 0 },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccVoltagePid, {  0, 30, 0, 0 }, 0 },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccAmperagePid, {  0, 30, 0, 0 }, 0 },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccMaxAmperagePid, {  0, 30, 0, 0 }, 0 },
    { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0 }
};

/// The number of seconds without receiving a CAN packet before assuming it's asleep
constexpr uint32_t ASLEEP_TIMEOUT = 5u;

/// The number of seconds trying to wake from zombie before giving up
constexpr uint32_t ZOMBIE_TIMEOUT = 30u;

/// The number of seconds trying to keep awake without success before allowing it to
/// go to sleep
constexpr uint32_t TRANSITION_TIMEOUT = 50u;

/// The number of seconds the car has to be unlocked for before transitioning from session
/// keep alive to TP keep alive
constexpr uint32_t UNLOCKED_CHARGING_TIMEOUT = 5u;

/// Maximum number of times to try and wake the car to find it wasn't charging or on
constexpr uint16_t DIAG_ATTEMPTS = 3u;

/// Threshold for 12v where we make the assumption that it is being charged
constexpr float CHARGING_THRESHOLD = 12.8;

}  // anon namespace

OvmsVehicleMgEv::OvmsVehicleMgEv()
{
    ESP_LOGI(TAG, "MG EV vehicle module");

    StandardMetrics.ms_v_type->SetValue("MG");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    // Don't support any other mode
    StandardMetrics.ms_v_charge_mode->SetValue("standard");

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

    // Allow unlimited polling per second
    PollSetThrottling(0u);

    MyConfig.RegisterParam("xmg", "MG EV", true, true);
    ConfigChanged(nullptr);
    
    // Add command to get the software versions installed
    m_cmdSoftver = MyCommandApp.RegisterCommand(
        "softver", "MG EV Software", &OvmsVehicleMgEv::SoftwareVersions
    );
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
    if (m_poll_state != PollStateCharging)
    {
        OvmsVehicle::NotifyVehicleIdling();
    }
}

// This is the only place we evaluate the vehicle state to change the action of OVMS
void OvmsVehicleMgEv::DeterminePollState(canbus* currentBus, uint32_t ticker)
{
    // Store last 12V charging state and re-evaluate if the 12V is charging
    auto charging12vLast = StandardMetrics.ms_v_env_charging12v;
    StandardMetrics.ms_v_env_charging12v->SetValue(
                 StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= CHARGING_THRESHOLD
             );

    if (StandardMetrics.ms_v_env_charging12v->AsBool())
    {
        // 12 V is charging, if this state has changed we should evaluate if the ignition is now on
        if (charging12vLast != StandardMetrics.ms_v_env_charging12v)
        {
            ESP_LOGV(TAG, "12V is now charging, checking to see if ignition is now on.");
            SendPollMessage(currentBus, vcuId, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuIgnitionStatePid);
            vTaskDelay(70 / portTICK_PERIOD_MS);
        } 

        // If ignition is on, we should set the car into running pollstate
        // If car is in a charging state we should set charging pollstate
        // If the state cannot be determined we need to consider zombie override
        if (StandardMetrics.ms_v_env_on->AsBool())
        {
            PollSetState(PollStateRunning);
            m_gwmState = SendTester;
        } 
        else if (StandardMetrics.ms_v_charge_type->AsString() != "not charging")
        {
            PollSetState(PollStateCharging);
        }
        else
        {
            ZombieMode();
        }
    } 
    else
    {
        // 12 V is now not charging, notify
        if (charging12vLast != StandardMetrics.ms_v_env_charging12v)
        {
            ESP_LOGV(TAG, "12V is not charging, remain in current state for 100 count.");
            m_gwmState = SendTester;
        } 
        m_afterRunTicker ++;
        if(m_afterRunTicker >= 100)
        {
            ESP_LOGV(TAG, "12V has not been charging for 100 count, stopping all CAN transmission.");
            PollSetState(PollStateListenOnly);
            m_gwmState = AllowToSleep;
        }
    }
    
    return;
}

void OvmsVehicleMgEv::ZombieMode()
{
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }
    ESP_LOGV(TAG, "OVMS thinks the GWM is in Zombie Mode, for now override is disabled");


    if (!StandardMetrics.ms_v_env_charging12v->AsBool())
    {
        // Don't wake the car if the 12v isn't charging because that can only end in a
        // dead battery
        ESP_LOGV(TAG, "Not sending SO as 12v is not charging.");
        return;
    }
    // Send Diagnostic Session Control (0x10, 0x02).
    // This causes the Gateway to wake up fully even though the car is locked.
    // The major caveat is that the console say there's a motor fault...
    

    // Want to wait in this state for 50s (allowing GWM to reset) then send Diagnostic Session
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
