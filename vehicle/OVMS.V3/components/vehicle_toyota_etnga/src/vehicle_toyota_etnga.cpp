/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#include "ovms_log.h"
#include "vehicle_toyota_etnga.h"

// Poll state descriptions:
//    POLLSTATE_SLEEP (0)     : Vehicle is sleeping; no activity on the CAN bus. We are listening only.
//    POLLSTATE_ACTIVE (1)    : Vehicle is alive; activity on CAN bus
//    POLLSTATE_READY (2)     : Vehicle is "Ready" to drive or being driven
//    POLLSTATE_CHARGING (3)  : Vehicle is charging

// TODO: Do we need a state for remote climate?

static const OvmsVehicle::poll_pid_t obdii_polls[] = {
    // State variables polls
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_READY_SIGNAL, { 0, 5, 1, 0}, 0, ISOTP_STD }, // { 0, 5, 1, 0}
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGING_LID, { 0, 5, 0, 1}, 0, ISOTP_STD }, // { 0, 5, 0, 1}

    // Driving polls
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_VOLTAGE_AND_CURRENT, { 0, 120, 1, 1}, 0, ISOTP_STD }, // { 0, 120, 1, 1}
  { VEHICLE_OBD_BROADCAST_MODULE_TX, HPCM_HYBRIDPTCTR_RX, VEHICLE_POLL_TYPE_OBDIICURRENT, PID_ODOMETER, { 0, 120, 1, 0}, 0, ISOTP_STD }, // {0, 120, 1, 0}
  { VEHICLE_OBD_BROADCAST_MODULE_TX, HPCM_HYBRIDPTCTR_RX, VEHICLE_POLL_TYPE_OBDIICURRENT, PID_VEHICLE_SPEED, { 0, 10, 1, 0}, 0, ISOTP_STD }, // { 0, 10, 1, 0}
  { VEHICLE_OBD_BROADCAST_MODULE_TX, HPCM_HYBRIDPTCTR_RX, VEHICLE_POLL_TYPE_OBDIICURRENT, PID_AMBIENT_TEMPERATURE, { 0, 120, 15, 15}, 0, ISOTP_STD }, // { 0, 120, 15, 15}
  { HYBRID_BATTERY_SYSTEM_TX, HYBRID_BATTERY_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_TEMPERATURES, { 0, 3600, 15, 15}, 0, ISOTP_STD }, // { 0, 3600, 15, 15}
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_SHIFT_POSITION, { 0, 120, 1, 0}, 0, ISOTP_STD }, // { 0, 120, 1, 1}

    // Combined polls
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_SOC, { 0, 0, 1, 1}, 0, ISOTP_STD }, // { 0, 5, 0, 1}

    // Charging polls
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_PISW_STATUS, { 0, 0, 0, 1}, 0, ISOTP_STD }, // { 0, 0, 0, 1}
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGING, { 0, 0, 0, 1}, 0, ISOTP_STD }, // { 0, 0, 0, 1}

    POLL_LIST_END
};

OvmsVehicleToyotaETNGA::OvmsVehicleToyotaETNGA()
{
    ESP_LOGI(TAG, "Toyota eTNGA platform module");

    // Init CAN
    RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

    // Set polling PID list
    PollSetPidList(m_can2, obdii_polls);

    // Set polling state
    PollSetState(static_cast<uint8_t>(PollState::POLLSTATE_SLEEP));

    // Init metrics
    InitializeMetrics();
}

OvmsVehicleToyotaETNGA::~OvmsVehicleToyotaETNGA()
{
    ESP_LOGI(TAG, "Shutdown Toyota eTNGA platform module");
}

void OvmsVehicleToyotaETNGA::Ticker1(uint32_t ticker)
{
    ++tickerCount;

    switch (static_cast<PollState>(m_poll_state)) {
        case PollState::POLLSTATE_SLEEP:
            HandleSleepState();
            break;

        case PollState::POLLSTATE_ACTIVE:
            HandleActiveState();
            break;

        case PollState::POLLSTATE_READY:
            HandleReadyState();
            break;

        case PollState::POLLSTATE_CHARGING:
            HandleChargingState();
            break;

        default:
            ESP_LOGE(TAG, "Invalid poll state: %d", m_poll_state);
            break;
    }

    if (frameCount > 0) {
        // CAN communication. Reset counters.
        frameCount = 0;
        tickerCount = 0;
    }
}

void OvmsVehicleToyotaETNGA::Ticker60(uint32_t ticker)
{
    // Request VIN if not already set
    if (StandardMetrics.ms_v_vin->AsString().empty() && m_poll_state == static_cast<uint8_t>(PollState::POLLSTATE_READY)) {
        RequestVIN();
    }
}
