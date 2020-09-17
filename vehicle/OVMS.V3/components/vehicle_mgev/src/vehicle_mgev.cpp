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
    // When in asleep poll state, manually poll for BMS and ignition status and the SoC
    // this is because when the gateway is in "Zombie" mode it only responds to one query
    // every 40 seconds.
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsStatusPid, {  5, 5, 5, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryBusVoltagePid, {  30, 10, 10, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCurrentPid, {  30, 10, 10, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryVoltagePid, {  30, 10, 10, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  60, 30, 30, 0 }, 0 },
    // Can't translate this currently, so don't bother polling
    //{ bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCellMaxVPid, {  0, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCoolantTempPid, {  30, 30, 30, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPid, {  0, 120, 120, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, chargeRatePid, {  0, 0, 60, 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsRangePid, {  0, 60, 60, 0 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcLvCurrentPid, {  0, 60, 60, 0 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcPowerLoadPid, {  0, 60, 60, 0 }, 0 },
    { dcdcId, dcdcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, dcdcTemperaturePid, {  30, 30, 30, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVinPid, { 999, 999, 999, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuIgnitionStatePid, {  5, 1, 5, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcu12vSupplyPid, {  60, 60, 60, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuCoolantTempPid, {  30, 30, 0, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuChargerConnectedPid, {  30, 0, 30, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVehicleSpeedPid, {  0, 5, 0, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuMotorSpeedPid, {  0, 5, 0, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuOdometerPid, {  0, 20, 0, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuHandbrakePid, {  20, 10, 0, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuGearPid, {  0, 10, 0, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBreakPid, {  0, 10, 0, 0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuBonnetPid, {  30, 30, 30, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcAmbientTempPid, {  30, 60, 60, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcSetTempPid, {  0, 10, 0, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcFaceOutletTempPid, {  0, 10, 0, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcBlowerSpeedPid, {  0, 10, 0, 0 }, 0 },
    { atcId, atcId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, atcPtcTempPid, {  30, 10, 0, 0 }, 0 },
    { pepsId, pepsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, pepsLockPid, {  2, 30, 3, 0 }, 0 },
    // Only poll when running, BCM requests are performed manually to avoid the alarm
    { bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid, {  0, 30, 0, 0 }, 0 },
    { bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmLightPid, {  0, 30, 0, 0 }, 0 },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, tyrePressurePid, {  30, 30, 0, 0 }, 0 },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, typeTemperaturePid, {  30, 30, 0, 0 }, 0 },
    { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0 }
};

/// Poll items that will be performed while sleeping to work through the zombie mode
/// Each time it wakes to zombie mode the first item is polled first
const OvmsVehicle::poll_pid_t obdii_sleep_polls[] =
{
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bmsStatusPid, { 0 }, 0 },
    { bmsId, bmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  0 }, 0 },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuIgnitionStatePid, { 0 }, 0 },
    { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0 }
};

// The number of seconds to keep checking the BCM for
constexpr uint32_t IDLE_KEEP_ALIVE = 45u;

// The number of seconds without receiving a CAN packet before assuming it's asleep
constexpr uint32_t ASLEEP_TIMEOUT = 50u;

/// The number of tester present packets required before coming out of sleep
constexpr uint16_t PRESENT_COUNT = 2u;

/// The number of seconds between sending alarm sensitive messages
constexpr uint32_t ALARM_SENSITIVE_TICKS = 3u;

/// The number of CAN messages per second from the poller when running
constexpr uint8_t MAX_CAN_RUNNING_PS = 0u;

/// The number of CAN messages per second from the poller when not running
constexpr uint8_t MAX_CAN_AWAKE_PS = 4u;

}  // anon namespace

OvmsVehicleMgEv::OvmsVehicleMgEv()
{
    ESP_LOGI(TAG, "MG EV vehicle module");

    StandardMetrics.ms_v_type->SetValue("MG");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    // Don't support any other mode
    StandardMetrics.ms_v_charge_mode->SetValue("standard");

    memset(m_vin, 0, sizeof(m_vin));

    m_keepAliveCallback = std::bind(
        &OvmsVehicleMgEv::KeepAliveCallback, this, placeholders::_1, placeholders::_2
    );

    m_bat_pack_voltage = MyMetrics.InitFloat(
        "xmg.v.bat.voltage", 0, SM_STALE_HIGH, Volts
    );
    m_env_face_outlet_temp = MyMetrics.InitFloat(
        "xmg.v.env.faceoutlet.temp", 0, SM_STALE_HIGH, Celcius
    );
    m_dcdc_load = MyMetrics.InitFloat("xmg.v.dcdc.load", 0, SM_STALE_HIGH, Percentage);

    PollSetState(PollStateAsleep);
    StandardMetrics.ms_v_env_ctrl_login->SetValue(false);

    m_packetTicker = 0u;
    m_rxPackets = 0u;
    m_presentSent = 0u;
    m_unlockedBeforeKeepAlive = false;
    // Until we know it's unlocked, say it's locked otherwise we might set off the alarm
    StandardMetrics.ms_v_env_locked->SetValue(true);

    m_sleepPolls = obdii_sleep_polls;
    m_sleepPollsItem = obdii_sleep_polls;
    // Assume the CAN is off to start with
    m_wakeState = Off;
    m_sleepPacketTime = 0u;

    // Allow unlimited polling per second
    PollSetThrottling(MAX_CAN_AWAKE_PS);

    MyConfig.RegisterParam("xmg", "MG EV", true, true);
    ConfigChanged(nullptr);
}

OvmsVehicleMgEv::~OvmsVehicleMgEv()
{
    ESP_LOGI(TAG, "Shutdown MG EV vehicle module");
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
        m_rxPackets = rxPackets;
        m_packetTicker = ticker;
    }
    else if (ticker - m_packetTicker > ASLEEP_TIMEOUT)
    {
        StandardMetrics.ms_v_env_awake->SetValue(false);
        if (m_wakeState != Waking)
        {
            m_wakeState = Off;
        }
    }
    return wokenUp;
}

void OvmsVehicleMgEv::DeterminePollState(canbus* currentBus, bool wokenUp)
{
    if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    {
        // If it's charging, but we're in zombie state then don't change
        PollSetState(m_wakeState != Awake ? PollStateAsleep : PollStateCharging);
        StandardMetrics.ms_v_env_ctrl_login->SetValue(true);
    }
    else if (StandardMetrics.ms_v_env_on->AsBool())
    {
        StandardMetrics.ms_v_env_ctrl_login->SetValue(false);
        PollSetState(PollStateRunning);
    }
    else if (StandardMetrics.ms_v_env_aux12v->AsBool())
    {
        StandardMetrics.ms_v_env_ctrl_login->SetValue(false);
        PollSetState(PollStateAwake);
    }
    else if (StandardMetrics.ms_v_env_ctrl_login->AsBool() == false)
    {
        StandardMetrics.ms_v_env_charging12v->SetValue(false);

        // If we've managed to fall asleep and have now woken back up then start keep
        // alive once more
        if (wokenUp)
        {
            ESP_LOGI(TAG, "OBD has woken up, re-starting keep alive for a bit");
            // Assume we're in a zombie state until proven otherwise
            if (m_wakeState != Awake)
            {
                // Always get the first item each time it's woken up
                m_sleepPollsItem = m_sleepPolls;
                m_wakeState = Zombie;
            }
            StandardMetrics.ms_v_env_ctrl_login->SetValue(true);
        }

        // If we're transitioning from running, then poll for a bit
        if (m_poll_state == PollStateRunning)
        {
            ESP_LOGI(TAG, "Transition from running, keep alive for a bit");
            StandardMetrics.ms_v_env_ctrl_login->SetValue(true);
        }

        PollSetState(PollStateAsleep);
    }
    else
    {
        StandardMetrics.ms_v_env_charging12v->SetValue(false);

        // After 40 seconds of keeping the car awake, let it go to sleep
        if (monotonictime - StandardMetrics.ms_v_env_ctrl_login->LastModified() >
                IDLE_KEEP_ALIVE)
        {
            ESP_LOGI(TAG, "Tester present signal expired timeout, going to sleep");
            // Don't disable until there's no multi-line left
            if (m_poll_ml_remain == 0 && m_poll_state == PollStateAsleep)
            {
                StandardMetrics.ms_v_env_ctrl_login->SetValue(false);
            }
            PollSetState(PollStateAsleep);
        }
        else
        {
            PollSetState(m_wakeState != Awake ? PollStateAsleep : PollStateAwake);
        }
    }
}

void OvmsVehicleMgEv::SendAlarmSensitive(canbus* currentBus)
{
    SendPollMessage(currentBus, bcmId, VEHICLE_POLL_TYPE_OBDIIEXTENDED, bcmDoorPid);
}

bool OvmsVehicleMgEv::SendKeepAliveTo(canbus* currentBus, uint16_t id)
{
    CAN_frame_t testerFrame = {
        currentBus,
        &m_keepAliveCallback,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        id,
        { .u8 = {
            (ISOTP_FT_SINGLE<<4) + 2, VEHICLE_POLL_TYPE_TESTERPRESENT, 0, 0, 0, 0, 0, 0
        } }
    };
    return currentBus->Write(&testerFrame) != ESP_FAIL;
}

void OvmsVehicleMgEv::SendKeepAlive(canbus* currentBus, uint32_t ticker)
{
    // Send tester present PID to keep the gateway alive and stop BCM queries setting
    // off the alarm
    if (m_presentSent == 0u)
    {
        // Only send keep alive to the BCM if it was unlocked when we started
        m_unlockedBeforeKeepAlive = !StandardMetrics.ms_v_env_locked->AsBool();
    }
    else if (!StandardMetrics.ms_v_env_locked->AsBool())
    {
        // When unlocked, start sending the keep-alive to the BCM
        m_unlockedBeforeKeepAlive = true;
    }

    if (!SendKeepAliveTo(currentBus, gwmId) ||
            (m_unlockedBeforeKeepAlive && !SendKeepAliveTo(currentBus, bcmId)))
    {
        // If we can't send the packet then need to stop sending BCM requests
        ESP_LOGI(TAG, "Sending keep-alive failed, sending to sleep");
        StandardMetrics.ms_v_env_ctrl_login->SetValue(false);
        PollSetState(PollStateAsleep);
        m_presentSent = 0u;
        m_unlockedBeforeKeepAlive = false;
    }
}

void OvmsVehicleMgEv::KeepAliveCallback(const CAN_frame_t* frame, bool success)
{
    if (success == false)
    {
        ESP_LOGI(TAG, "Keep-alive failed to transmit, sending to sleep");
        // Don't poll the BCM if the keep alive failed
        StandardMetrics.ms_v_env_ctrl_login->SetValue(true);
        PollSetState(PollStateAsleep);
        m_presentSent = 0u;
    }
    else if (m_presentSent < PRESENT_COUNT)
    {
        ++m_presentSent;
    }
}

void OvmsVehicleMgEv::Ticker1(uint32_t ticker)
{
    canbus* currentBus = m_poll_bus_default;
    if (currentBus == nullptr)
    {
        // Polling is disabled, so there's nothing to do here
        return;
    }

    const auto previousState = m_poll_state;

    if (previousState == PollStateAsleep)
    {
        SendSleepPoll(currentBus);
    }

    bool woken = HasWoken(currentBus, ticker);
    DeterminePollState(currentBus, woken);
    if (previousState != m_poll_state)
    {
        ESP_LOGI(TAG, "Changed poll state from %d to %d", previousState, m_poll_state);
    }

    if (StandardMetrics.ms_v_env_ctrl_login->AsBool())
    {
        PollSetThrottling(MAX_CAN_AWAKE_PS);
        if (m_wakeState == Awake)
        {
            SendKeepAlive(currentBus, ticker);
        }
    }
    else
    {
        PollSetThrottling(MAX_CAN_RUNNING_PS);
        m_presentSent = 0u;
    }
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
        SendKeepAliveTo(m_poll_bus_default, gwmId);
        vTaskDelay(70 / portTICK_PERIOD_MS);
        ++counter;
    }
    if (m_wakeState == Awake || m_wakeState == Zombie)
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
