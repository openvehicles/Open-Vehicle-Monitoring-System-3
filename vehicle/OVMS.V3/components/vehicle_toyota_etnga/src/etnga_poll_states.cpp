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
        frameCount = 0;
        tickerCount = 0;
    }
}

void OvmsVehicleToyotaETNGA::handleActiveState()
{
    if (frameCount == 0 && tickerCount > 3) {
        // No CAN communication for three ticks - stop polling
        transitionToSleepState();
    }

    if (frameCount > 0) {
        // CAN communication. Reset counters.
        frameCount = 0;
        tickerCount = 0;
    }

}

void OvmsVehicleToyotaETNGA::handleReadyState()
{
  // Handle actions specific to the READY state
  // ...

}

void OvmsVehicleToyotaETNGA::handleChargingState()
{
  // Handle actions specific to the CHARGING state
  // ...

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

