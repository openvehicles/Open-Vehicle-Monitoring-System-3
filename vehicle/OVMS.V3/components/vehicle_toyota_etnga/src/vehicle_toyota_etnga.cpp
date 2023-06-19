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
//    SLEEP (0)             : Vehicle is sleeping; no activity on the CAN bus. We are listening only.
//    AWAKE (1)             : Vehicle is alive; vehicle has been switched on by driver
//    READY (2)             : Vehicle is "Ready" to drive or being driven
//    CHARGING (3)          : Vehicle is charging

static const OvmsVehicle::poll_pid_t obdii_polls[] = {
    // State variables polls
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_READY_SIGNAL, { 0, 1, 1, 10}, 0, ISOTP_STD }, // { 0, 5, 1, 60}
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGING_LID, { 0, 1, 10, 1}, 0, ISOTP_STD }, // { 0, 5, 60, 1}

    // Driving polls
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_VOLTAGE_AND_CURRENT, { 0, 1, 1, 1}, 0, ISOTP_STD }, // { 0, 0, 1, 1}
  { VEHICLE_OBD_BROADCAST_MODULE_TX, HPCM_HYBRIDPTCTR_RX, VEHICLE_POLL_TYPE_OBDIICURRENT, PID_ODOMETER, { 0, 60, 1, 60}, 0, ISOTP_STD }, // {0, 120, 1, 0}
  { VEHICLE_OBD_BROADCAST_MODULE_TX, HPCM_HYBRIDPTCTR_RX, VEHICLE_POLL_TYPE_OBDIICURRENT, PID_VEHICLE_SPEED, { 0, 60, 1, 60}, 0, ISOTP_STD }, // { 0, 10, 1, 0}
  { VEHICLE_OBD_BROADCAST_MODULE_TX, HPCM_HYBRIDPTCTR_RX, VEHICLE_POLL_TYPE_OBDIICURRENT, PID_AMBIENT_TEMPERATURE, { 0, 60, 15, 15}, 0, ISOTP_STD }, // { 0, 120, 15, 15}
  { HYBRID_BATTERY_SYSTEM_TX, HYBRID_BATTERY_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_TEMPERATURES, { 0, 60, 15, 15}, 0, ISOTP_STD }, // { 0, 3600, 15, 15}
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_SHIFT_POSITION, { 0, 0, 1, 60}, 0, ISOTP_STD }, // { 0, 120, 1, 1}

    // Combined polls
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_SOC, { 0, 60, 1, 1}, 0, ISOTP_STD }, // { 0, 5, 0, 1}

    // Charging polls
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_PISW_STATUS, { 0, 0, 0, 10}, 0, ISOTP_STD }, // { 0, 0, 0, 1}
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGING, { 0, 0, 0, 10}, 0, ISOTP_STD }, // { 0, 0, 0, 1}

    POLL_LIST_END
};

OvmsVehicleToyotaETNGA::OvmsVehicleToyotaETNGA()
{
    ESP_LOGI(TAG, "Toyota eTNGA platform module");

    // Init metrics
    InitializeMetrics();

    // Init CAN
    RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

    // Set polling state
    PollSetState(PollState::SLEEP);

    // Set polling PID list
    PollSetPidList(m_can2, obdii_polls);
    PollSetThrottling(0);
}

OvmsVehicleToyotaETNGA::~OvmsVehicleToyotaETNGA()
{
    ESP_LOGI(TAG, "Shutdown Toyota eTNGA platform module");
}

void OvmsVehicleToyotaETNGA::Ticker1(uint32_t ticker)
{
    ResetStaleMetrics();

    switch (static_cast<PollState>(m_poll_state)) {
        case PollState::SLEEP:
            HandleSleepState();
            break;

        case PollState::AWAKE:
            HandleAwakeState();
            break;

        case PollState::READY:
            HandleReadyState();
            break;

        case PollState::CHARGING:
            HandleChargingState();
            break;

        default:
            ESP_LOGE(TAG, "Invalid poll state: %d", m_poll_state);
            break;
    }
}

void OvmsVehicleToyotaETNGA::Ticker60(uint32_t ticker)
{
    // Request VIN if not already set
if (StandardMetrics.ms_v_vin->AsString().empty() && (m_poll_state == PollState::AWAKE || m_poll_state == PollState::READY)) {
        RequestVIN();
    }
}
