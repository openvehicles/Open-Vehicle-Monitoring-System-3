/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.

   Code adapted from Michael Geddes & Geir Øyvind Vælidalo vehicle modules
*/

#include "ovms_log.h"
#include "vehicle_toyota_etnga.h"

void OvmsVehicleToyotaETNGA::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
    // Check if this is the first frame of the multi-frame response
    if (m_poll_ml_frame == 0) {
        m_rxbuf.clear();
        m_rxbuf.reserve(length + mlremain);
    }

    // Append the data to the receive buffer
    m_rxbuf.append(reinterpret_cast<char*>(data), length);

    // Check if response is complete
    if (mlremain != 0)
        return;

    // Log the received response
    ESP_LOGV(TAG, "IncomingPollReply: PID %02X: len=%d %s", pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());

    // Process based on m_poll_moduleid_low
    switch (m_poll_moduleid_low) {
        case HYBRID_BATTERY_SYSTEM_RX:
            IncomingHybridBatterySystem(pid);
            break;

        case HYBRID_CONTROL_SYSTEM_RX:
            IncomingHybridControlSystem(pid);
            break;

        case PLUG_IN_CONTROL_SYSTEM_RX:
            IncomingPlugInControlSystem(pid);
            break;

        case HPCM_HYBRIDPTCTR_RX:
            IncomingHPCMHybridPtCtr(pid);
            break;

        default:
            ESP_LOGD(TAG, "Unknown module: %03" PRIx32, m_poll_moduleid_low);
            return;
    }
}

void OvmsVehicleToyotaETNGA::IncomingHybridControlSystem(uint16_t pid)
{
    switch (pid) {
        case PID_BATTERY_VOLTAGE_AND_CURRENT: {
            float batVoltage = CalculateBatteryVoltage(m_rxbuf);
            float batCurrent = CalculateBatteryCurrent(m_rxbuf);
            float batPower = CalculateBatteryPower(batVoltage, batCurrent);

            SetBatteryVoltage(batVoltage);
            SetBatteryCurrent(batCurrent);
            SetBatteryPower(batPower);

            break;
        }

        case PID_READY_SIGNAL: {
            bool readyStatus = CalculateReadyStatus(m_rxbuf);
            SetReadyStatus(readyStatus);
            break;
        }

        case PID_SHIFT_POSITION: {
            int shiftPosition = CalculateShiftPosition(m_rxbuf);
            SetShiftPosition(shiftPosition);
            break;
        }

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGD(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::IncomingPlugInControlSystem(uint16_t pid)
{
    switch (pid) {
        case PID_CHARGING_LID: {
            bool chargingDoorStatus = CalculateChargingDoorStatus(m_rxbuf);
            SetChargingDoorStatus(chargingDoorStatus);
            break;
        }

        case PID_BATTERY_SOC: {
            float SOC = CalculateBatterySOC(m_rxbuf);
            SetBatterySOC(SOC);
            break;
        }

        case PID_PISW_STATUS: {
            bool PISWStatus = CalculatePISWStatus(m_rxbuf);
            SetPISWStatus(PISWStatus);
            break;
        }

        case PID_CHARGING: {
            bool chargingStatus = CalculateChargingStatus(m_rxbuf);
            SetChargingStatus(chargingStatus);
            break;
        }

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGD(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::IncomingHybridBatterySystem(uint16_t pid)
{
    switch (pid) {
        case PID_BATTERY_TEMPERATURES: {
            std::vector<float> temperatures = CalculateBatteryTemperatures(m_rxbuf);
            SetBatteryTemperatures(temperatures);
            SetBatteryTemperatureStatistics(temperatures);
            break;
        }

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGD(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::IncomingHPCMHybridPtCtr(uint16_t pid)
{
    switch (pid) {
        case PID_ODOMETER: {
            float odometer = CalculateOdometer(m_rxbuf);
            SetOdometer(odometer);
            break;
        }

        case PID_VEHICLE_SPEED: {
            float speed = CalculateVehicleSpeed(m_rxbuf);
            SetVehicleSpeed(speed);
            break;
        }

        case PID_AMBIENT_TEMPERATURE: {
            float temperature = CalculateAmbientTemperature(m_rxbuf);
            SetAmbientTemperature(temperature);
            break;
        }

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGD(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::RequestVIN()
{
    std::string vin;
    int res = PollSingleRequest(
        m_can2,
        HYBRID_CONTROL_SYSTEM_TX,
        HYBRID_CONTROL_SYSTEM_RX,
        VEHICLE_POLL_TYPE_READDATA,
        PID_VIN,
        vin,
        1000,
        ISOTP_STD
    );

    if (res == POLLSINGLE_OK)
    {
        SetVehicleVIN(vin);
    }
    else
    {
        ESP_LOGE(TAG, "RequestVIN: Failed with error code %d", res);
    }
}
