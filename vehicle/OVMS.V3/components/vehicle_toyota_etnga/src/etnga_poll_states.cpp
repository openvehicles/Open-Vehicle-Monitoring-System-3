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
//    SLEEP (0)     : Vehicle is sleeping; no activity on the can bus. We are listening only.
//    ACTIVE (1)    : Vehicle is alive; activity on CAN bus
//    READY (2)     : Vehicle is "Ready" to drive or being driven
//    CHARGING (3)  : Vehicle is charging

void OvmsVehicleToyotaETNGA::HandleSleepState()
{
    if (frameCount > 0) {
        // There is life.
        TransitionToActiveState();
    }

    // TODO: Need to add a battery voltage check as well
}

void OvmsVehicleToyotaETNGA::HandleActiveState()
{
    if (frameCount == 0 && tickerCount > 3) {
        // No CAN communication for three ticks - stop polling
        TransitionToSleepState();
    } else if (StdMetrics.ms_v_env_on->AsBool()) {
        TransitionToReadyState();
    } else if (StdMetrics.ms_v_door_chargeport->AsBool()) {
        TransitionToChargingState();
    }
}

void OvmsVehicleToyotaETNGA::HandleReadyState()
{
    // Check to make sure the 'on' signal has been updated recently
    if (StdMetrics.ms_v_env_on->IsStale()) {
        ESP_LOGD(TAG, "Ready is stale. Manually setting to off");
        SetReadyStatus(false);
    }

    if (!StdMetrics.ms_v_env_on->AsBool()) {
        TransitionToActiveState();
    }
}

void OvmsVehicleToyotaETNGA::HandleChargingState()
{
    // Check to make sure the 'charging door' signal has been updated recently
    if (StdMetrics.ms_v_door_chargeport->IsStale()) {
        ESP_LOGD(TAG, "Charging Door is stale. Manually setting to off");
        SetChargingDoorStatus(false);
    }

    if (!StdMetrics.ms_v_door_chargeport->AsBool()) {
        TransitionToActiveState();
    }
}

void OvmsVehicleToyotaETNGA::TransitionToSleepState()
{
    // Perform actions needed for transitioning to the SLEEP state
    SetPollState(PollState::SLEEP);
    StdMetrics.ms_v_env_awake->SetValue(false);
}

void OvmsVehicleToyotaETNGA::TransitionToActiveState()
{
    // Perform actions needed for transitioning to the ACTIVE state
    SetPollState(PollState::ACTIVE);
    StdMetrics.ms_v_env_awake->SetValue(true);
}

void OvmsVehicleToyotaETNGA::TransitionToReadyState()
{
    // Perform actions needed for transitioning to the READY state
    SetPollState(PollState::READY); // Update the state
    m_v_pos_trip_start->SetStale(true);  // Set the start trip metric as stale so it resets next odometer reading
}

void OvmsVehicleToyotaETNGA::TransitionToChargingState()
{
    // Perform actions needed for transitioning to the CHARGING state
    SetPollState(PollState::CHARGING);
}
