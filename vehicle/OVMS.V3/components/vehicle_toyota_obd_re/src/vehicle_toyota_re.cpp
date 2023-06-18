/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota Reverse Engineering tool
   Date:          18th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota Reverse Engineering tool
   Date:          18th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#include <stdio.h>
#include "vehicle_toyota_re.h"
#include "ovms_log.h"

const char *TAG = "v-toyota-re";

OvmsVehicleToyotaRE::OvmsVehicleToyotaRE()
{
    ESP_LOGI(TAG, "Toyota RE vehicle module");

    // Init CAN
    RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

}

OvmsVehicleToyotaRE::~OvmsVehicleToyotaRE()
{
    ESP_LOGI(TAG, "Shutdown Toyota RE vehicle module");
}

void OvmsVehicleToyotaRE::IncomingFrameCan2(CAN_frame_t* p_frame)
{
    uint8_t* data = p_frame->data.u8;

    // Check if the message should be ignored based on MsgID
    if (p_frame->MsgID == 0x0000045a || p_frame->MsgID == 0x000004e0)
    {
        return; // Exit the function without processing further
    }

    ESP_LOGV(TAG, "CAN2 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
             p_frame->MsgID, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    // Handle the specified message to capture 0x1F0D PID
    if (!watchMessage_ && data[0] == 0x10 && data[2] == 0x2c && data[3] == 0x01)
    {
        ESP_LOGD(TAG, "Specified message received: %08x", p_frame->MsgID);
        // Store the PID for later use
        pid_ = (data[6] << 8) | data[7];  // Capture the PID

        ESP_LOGD(TAG, "Captured PID: %04x", pid_);

        // Wait for a later message to get the rest of the information
        watchMessage_ = true; // Set the flag to start watching for the follow-up message
        return; // Exit the function after capturing the PID
    }

    if (watchMessage_ && data[0] == 0x21)
    {
        uint8_t start = data[1];
        uint8_t length = data[2];

    ESP_LOGI(TAG, "MsgID: 0x%03x, Captured PID: 0x%04x, Start: %d, Length: %d", p_frame->MsgID, pid_, start, length);

        watchMessage_ = false; // Set the flag to stop watching for the follow-up message
        return; // Exit the function
    }

}

class OvmsVehicleToyotaREInitializer
{
public:
    OvmsVehicleToyotaREInitializer();
};

OvmsVehicleToyotaREInitializer::OvmsVehicleToyotaREInitializer()
{
    ESP_LOGI(TAG, "Registering Vehicle: Toyota RE (9000)");

    MyVehicleFactory.RegisterVehicle<OvmsVehicleToyotaRE>("TOYRE", "Toyota RE");
}

OvmsVehicleToyotaREInitializer MyOvmsVehicleToyotaREInitializer;
