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

void OvmsVehicleToyotaETNGA::HandleSleepState()
{
    if (StandardMetrics.ms_v_env_awake->AsBool()) {
        // There is life.
        TransitionToAwakeState();
    }

    // TODO:    Need to add a battery voltage check as well
    //          If the vehicle is awake but the CAN bus is 'stuck'
}

void OvmsVehicleToyotaETNGA::HandleAwakeState()
{
    if (!StandardMetrics.ms_v_env_awake->AsBool()) {
        // No CAN communication for 120s - stop polling
        TransitionToSleepState();
    } else if (StandardMetrics.ms_v_env_on->AsBool()) {
        TransitionToReadyState();
    } else if (StandardMetrics.ms_v_door_chargeport->AsBool()) {
        TransitionToChargingState();
    }
}

void OvmsVehicleToyotaETNGA::HandleReadyState()
{
    if (!StandardMetrics.ms_v_env_on->AsBool()) {
        TransitionToAwakeState();
    }
}

void OvmsVehicleToyotaETNGA::HandleChargingState()
{
    if (!StandardMetrics.ms_v_door_chargeport->AsBool()) {
        TransitionToAwakeState();
    }
}

void OvmsVehicleToyotaETNGA::TransitionToSleepState()
{
    // Perform actions needed for transitioning to the SLEEP state
    SetPollState(PollState::SLEEP); // Update the state
}

void OvmsVehicleToyotaETNGA::TransitionToAwakeState()
{
    // Perform actions needed for transitioning to the ACTIVE state
    SetPollState(PollState::AWAKE);
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
    SetPollState(PollState::CHARGING); // Update the state
}
