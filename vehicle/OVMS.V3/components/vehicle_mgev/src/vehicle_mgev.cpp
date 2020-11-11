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
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsStatusPid, {  5, 5, 5, 5 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryBusVoltagePid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCurrentPid, {  0, 0, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryVoltagePid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  0, 60, 30, 30 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCoolantTempPid, {  0, 30, 30, 30 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPid, {  0, 120, 120, 120 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, chargeRatePid, {  0, 0, 0, 60 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsRangePid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell1StatPid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell2StatPid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell3StatPid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell4StatPid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell5StatPid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell6StatPid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell7StatPid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell8StatPid, {  0, 60, 10, 10 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cell9StatPid, {  0, 60, 10, 10 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcLvCurrentPid, {  0, 60, 60, 60 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcPowerLoadPid, {  0, 60, 60, 60 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcTemperaturePid, {  0, 30, 30, 30 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVinPid, { 0, 999, 999, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuIgnitionStatePid, {  1, 1, 1, 1 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcu12vSupplyPid, {  0, 60, 60, 60 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuCoolantTempPid, {  0, 30, 30, 30 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuChargerConnectedPid, {  0, 5, 30, 30 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVehicleSpeedPid, {  0, 0, 5, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuMotorSpeedPid, {  0, 0, 5, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuOdometerPid, {  0, 0, 20, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuHandbrakePid, {  0, 10, 10, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuGearPid, {  0, 0, 10, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBreakPid, {  0, 0, 10, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBonnetPid, {  0, 30, 30, 30 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcAmbientTempPid, {  0, 60, 60, 60 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcSetTempPid, {  0, 0, 10, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcFaceOutletTempPid, {  0, 0, 10, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcBlowerSpeedPid, {  0, 0, 10, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcPtcTempPid, {  0, 30, 10, 0 }, 0 },
    { pepsId, pepsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, pepsLockPid, {  1, 1, 1, 1 }, 0 },
    // Only poll when running, BCM requests are performed manually to avoid the alarm    
    // FIXME: Disabled until we are happy that the alarm won't go off
    //{ bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid, {  0, 0, 15, 0 }, 0 },
    //{ bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmLightPid, {  0, 0, 15, 0 }, 0 },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, tyrePressurePid, {  0, 60, 60, 0 }, 0 },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, typeTemperaturePid, {  0, 60, 60, 0 }, 0 },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccVoltagePid, {  0, 0, 0, 10 }, 0 },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccAmperagePid, {  0, 0, 0, 10 }, 0 },
    { evccId, evccId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, evccMaxAmperagePid, {  0, 0, 0, 10 }, 0 },
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

    PollSetState(PollStateLocked);

    m_rxPacketTicker = 0u;
    m_rxPackets = 0u;
    // Until we know it's unlocked, say it's locked otherwise we might set off the alarm
    StandardMetrics.ms_v_env_locked->SetValue(true);
    StandardMetrics.ms_v_env_on->SetValue(false);

    // Assume the CAN is off to start with
    m_wakeState = Off;
    m_wakeTicker = 0u;
    m_diagCount = 0u;

    // Create the timer for zombie mode, but no need to start it yet
    m_zombieTimer = xTimerCreate(
        "MG Zombie Timer", 250 / portTICK_PERIOD_MS, pdTRUE,
        this, &OvmsVehicleMgEv::ZombieTimer
    );

    // Allow unlimited polling per second
    PollSetThrottling(0u);

    MyConfig.RegisterParam("xmg", "MG EV", true, true);
    ConfigChanged(nullptr);
    
    // Add command to get the software versions installed
    m_cmdSoftver = MyCommandApp.RegisterCommand(
        "softver", "MG EV Software",
        PickOvmsCommandExecuteCallback(OvmsVehicleMgEv::SoftwareVersions)
    );
}

OvmsVehicleMgEv::~OvmsVehicleMgEv()
{
    ESP_LOGI(TAG, "Shutdown MG EV vehicle module");

    xTimerDelete(m_zombieTimer, 0);

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

    if (m_wakeState != Awake)
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

bool OvmsVehicleMgEv::HasWoken(canbus* currentBus, uint32_t ticker)
{
    const auto rxPackets = currentBus->m_status.packets_rx;
    bool wokenUp = false;
    if (m_rxPackets != rxPackets)
    {
        if (StandardMetrics.ms_v_env_awake->AsBool() == false)
        {
            // If it was asleep before it's just woken up
            wokenUp = true;
            StandardMetrics.ms_v_env_awake->SetValue(true);
        }
        if (m_wakeState == Off)
        {
            m_wakeState = Awake;
            m_wakeTicker = ticker;
        }
        m_rxPackets = rxPackets;
        m_rxPacketTicker = ticker;
    }
    else if (ticker - m_rxPacketTicker > ASLEEP_TIMEOUT)
    {
        StandardMetrics.ms_v_env_awake->SetValue(false);
        if (m_wakeState == Tester)
        {
            ESP_LOGD(TAG, "Car not responding while sending TP, sending diagnostic");
            AttemptDiagnostic();
        }
        else if (m_wakeState != Waking && m_wakeState != Off &&
                (m_wakeState != Diagnostic || monotonictime - m_wakeTicker > ZOMBIE_TIMEOUT))
        {
            m_wakeState = Off;
            m_wakeTicker = monotonictime;
        }
    }

    return wokenUp;
}

void OvmsVehicleMgEv::AttemptDiagnostic()
{
    if (m_diagCount > DIAG_ATTEMPTS)
    {
        ESP_LOGE(TAG, "Woken the car too many times without success, not trying again");
        m_wakeState = Off;
        return;
    }
    ++m_diagCount;
    m_wakeState = Diagnostic;
    m_wakeTicker = monotonictime;
}

void OvmsVehicleMgEv::DeterminePollState(canbus* currentBus, bool wokenUp, uint32_t ticker)
{
    if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    {
        m_diagCount = 0u;
        PollSetState(PollStateCharging);
        if (m_wakeState == Off || m_wakeState == Awake)
        {
            m_wakeState = Tester;
            m_wakeTicker = monotonictime;
        }
        if (m_wakeState == Diagnostic && !StandardMetrics.ms_v_env_locked->AsBool() &&
                monotonictime - m_wakeTicker > UNLOCKED_CHARGING_TIMEOUT)
        {
            m_wakeState = Tester;
            m_wakeTicker = monotonictime;
        }
    }
    else
    {
        if (StandardMetrics.ms_v_env_on->AsBool() &&
                monotonictime - StandardMetrics.ms_v_env_on->LastModified() >= 5)
        {
            m_diagCount = 0u;
            PollSetState(PollStateRunning);
        }
        else if (StandardMetrics.ms_v_env_locked->AsBool())
        {
            PollSetState(PollStateLocked);
            StandardMetrics.ms_v_env_charging12v->SetValue(
                StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= CHARGING_THRESHOLD
            );
        }
        else
        {
            PollSetState(PollStateUnlocked);
        }

        if ((m_wakeState == Diagnostic || m_wakeState == Tester) &&
                monotonictime - m_wakeTicker > TRANSITION_TIMEOUT)
        {
            m_wakeState = Awake;
            m_wakeTicker = monotonictime;
        }
    }
}

void OvmsVehicleMgEv::ZombieTimer(TimerHandle_t timer)
{
    reinterpret_cast<OvmsVehicleMgEv*>(pvTimerGetTimerID(timer))->ZombieTimer();
}

void OvmsVehicleMgEv::ZombieTimer()
{
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }
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
    CAN_frame_t diagnosticControl = {
        currentBus,
        nullptr,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        gwmId,
        { .u8 = {
            (ISOTP_FT_SINGLE<<4) + 2, VEHICLE_POLL_TYPE_OBDIISESSION, 2, 0, 0, 0, 0, 0
        } }
    };
    currentBus->Write(&diagnosticControl);
    // Send to the IPK too which will stop it displaying the motor fault
    diagnosticControl.MsgID = ipkId;
    currentBus->Write(&diagnosticControl);
}

void OvmsVehicleMgEv::SendAlarmSensitive(canbus* currentBus)
{
    SendPollMessage(currentBus, bcmId, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid);
}

bool OvmsVehicleMgEv::SendKeepAliveTo(canbus* currentBus, uint16_t id)
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

void OvmsVehicleMgEv::Ticker1(uint32_t ticker)
{
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }

    if (m_wakeState == Tester)
    {
        SendKeepAliveTo(currentBus, gwmId);
    }

    StandardMetrics.ms_v_env_ctrl_login->SetValue(
        m_wakeState == Tester || m_wakeState == Diagnostic
    );

    const auto previousState = m_poll_state;
    const auto previousWakeState = m_wakeState;
    bool woken = HasWoken(currentBus, ticker);
    DeterminePollState(currentBus, woken, ticker);
    if (previousState != m_poll_state)
    {
        ESP_LOGI(TAG, "Changed poll state from %d to %d", previousState, m_poll_state);
    }
    if (previousWakeState != m_wakeState)
    {
        ESP_LOGI(TAG, "Wake state from %d to %d", previousWakeState, m_wakeState);
    }

    if (xTimerIsTimerActive(m_zombieTimer))
    {
        if (m_wakeState != Diagnostic)
        {
            xTimerStop(m_zombieTimer, 0);
        }
    }
    else if (m_wakeState == Diagnostic)
    {
        // TP seems more reliable at waking the gateway and it doesn't harm to send it
        // We take a second out to do that before starting the session
        xTaskCreatePinnedToCore(
            &OvmsVehicleMgEv::WakeUp, "MG TP Wake Up", 4096, this, tskIDLE_PRIORITY,
            nullptr, CORE(1)
        );
        xTimerStart(m_zombieTimer, 0);
    }
}

void OvmsVehicleMgEv::WakeUp(void* self)
{
    auto* wakeBus = reinterpret_cast<OvmsVehicleMgEv*>(self)->m_poll_bus_default;
    if (wakeBus)
    {
        for (int i = 0; i < 20; ++i)
        {
            reinterpret_cast<OvmsVehicleMgEv*>(self)->SendKeepAliveTo(wakeBus, gwmId);
            vTaskDelay(70 / portTICK_PERIOD_MS);
        }
    }
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
    auto wakeBus = m_poll_bus_default;
    if (m_wakeState == Off && wakeBus)
    {
        // Reset the bus
        ConfigurePollInterface(0);
        ConfigurePollInterface(wakeBus->m_busnumber + 1);
        m_rxPackets = 0u;
        m_wakeState = Waking;
    }
    int counter = 0;
    while (counter < 40 && m_wakeState == Waking)
    {
        SendKeepAliveTo(wakeBus, gwmId);
        vTaskDelay(70 / portTICK_PERIOD_MS);
        ++counter;
    }
    if (m_wakeState != Waking && m_wakeState != Off)
    {
        return OvmsVehicle::Success;
    }
    else
    {
        ESP_LOGD(TAG, "Wake state at completion: %d", m_wakeState);
        m_wakeState = Off;
        return OvmsVehicle::Fail;
    }
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
