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
//    POLLSTATE_SLEEP (0)     : Vehicle is sleeping; no activity on the can bus. We are listening only.
//    POLLSTATE_ACTIVE (1)    : Vehicle is alive; activity on CAN bus
//    POLLSTATE_READY (2)     : Vehicle is "Ready" to drive or being driven
//    POLLSTATE_CHARGING (3)  : Vehicle is charging

void OvmsVehicleToyotaETNGA::handleSleepState()
{
    if (frameCount > 0) {
        // There is life.
        transitionToActiveState();
    }
}

void OvmsVehicleToyotaETNGA::handleActiveState()
{
    if (frameCount == 0 && tickerCount > 3) {
        // No CAN communication for three ticks - stop polling
        transitionToSleepState();
    }
    else if (StdMetrics.ms_v_env_on->AsBool()) {
        transitionToReadyState();
    }
    else if (StdMetrics.ms_v_door_chargeport->AsBool()) {
        transitionToChargingState();
    }
}

void OvmsVehicleToyotaETNGA::handleReadyState()
{
    // Check to make sure the 'on' signal has been updated recently
    if (StdMetrics.ms_v_env_on->IsStale()) {
        ESP_LOGD(TAG, "Ready is stale. Manually setting to off");
        SetReadyStatus(false);
    }

    if (!StdMetrics.ms_v_env_on->AsBool()) {
        transitionToActiveState();
    }

}

void OvmsVehicleToyotaETNGA::handleChargingState()
{
    // Check to make sure the 'charging door' signal has been updated recently
    if (StdMetrics.ms_v_door_chargeport->IsStale()) {
        ESP_LOGD(TAG, "Charging Door is stale. Manually setting to off");
        SetChargingDoorStatus(false);
    }

    if (!StdMetrics.ms_v_door_chargeport->AsBool()) {
        transitionToActiveState();
    }

}

void OvmsVehicleToyotaETNGA::transitionToSleepState()
{
    // Perform actions needed for transitioning to the SLEEP state
    ESP_LOGI(TAG, "Transitioning to the SLEEP state");
    PollSetState(POLLSTATE_SLEEP);
    StdMetrics.ms_v_env_awake->SetValue(false);

}

void OvmsVehicleToyotaETNGA::transitionToActiveState()
{
    // Perform actions needed for transitioning to the ACTIVE state
    ESP_LOGI(TAG, "Transitioning to the ACTIVE state");
    PollSetState(POLLSTATE_ACTIVE);
    StdMetrics.ms_v_env_awake->SetValue(true);
}

void OvmsVehicleToyotaETNGA::transitionToReadyState()
{
    // Perform actions needed for transitioning to the READY state
    ESP_LOGI(TAG, "Transitioning to the READY state");
    PollSetState(POLLSTATE_READY);
}

void OvmsVehicleToyotaETNGA::transitionToChargingState()
{
    // Perform actions needed for transitioning to the CHARGING state
    ESP_LOGI(TAG, "Transitioning to the CHARGING state");
    PollSetState(POLLSTATE_CHARGING);

}
