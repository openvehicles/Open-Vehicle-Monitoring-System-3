/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#ifndef __VEHICLE_TOYOTA_ETNGA_H__
#define __VEHICLE_TOYOTA_ETNGA_H__

#include "vehicle.h"

class OvmsVehicleToyotaETNGA : public OvmsVehicle
{
public:
    OvmsVehicleToyotaETNGA();
    ~OvmsVehicleToyotaETNGA();

    void Ticker1(uint32_t ticker);
    void Ticker60(uint32_t ticker);

    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);

    void IncomingFrameCan2(CAN_frame_t* p_frame);

protected:
    std::string m_rxbuf;
    OvmsMetricFloat* m_v_pos_trip_start;

private:
    static constexpr const char* TAG = "v-toyota-etnga";

    void InitializeMetrics();  // Initializes the metrics specific to this vehicle module

    // Incoming message handling functions
    void IncomingHPCMHybridPtCtr(uint16_t pid);
    void IncomingHybridBatterySystem(uint16_t pid);
    void IncomingHybridControlSystem(uint16_t pid);
    void IncomingPlugInControlSystem(uint16_t pid);

    // Data calculation functions
    float CalculateAmbientTemperature(const std::string& data);
    float CalculateBatteryCurrent(const std::string& data);
    float CalculateBatteryPower(float voltage, float current);
    float CalculateBatterySOC(const std::string& data);
    std::vector<float> CalculateBatteryTemperatures(const std::string& data);
    float CalculateBatteryVoltage(const std::string& data);
    bool CalculateChargingDoorStatus(const std::string& data);
    bool CalculateChargingStatus(const std::string& data);
    float CalculateOdometer(const std::string& data);
    bool CalculatePISWStatus(const std::string& data);
    bool CalculateReadyStatus(const std::string& data);
    int CalculateShiftPosition(const std::string& data);
    float CalculateVehicleSpeed(const std::string& data);

    // Metric setter functions
    void SetAmbientTemperature(float temperature);
    void SetBatteryCurrent(float current);
    void SetBatteryPower(float power);
    void SetBatterySOC(float soc);
    void SetBatteryTemperatures(const std::vector<float>& temperatures);
    void SetBatteryTemperatureStatistics(const std::vector<float>& temperatures);
    void SetBatteryVoltage(float voltage);
    void SetChargingDoorStatus(bool status);
    void SetChargingStatus(bool status);
    void SetOdometer(float odometer);
    void SetPISWStatus(bool status);
    void SetReadyStatus(bool status);
    void SetShiftPosition(int position);
    void SetVehicleSpeed(float speed);
    void SetVehicleVIN(std::string vin);

    // State transition functions
    void HandleSleepState();
    void HandleActiveState();
    void HandleReadyState();
    void HandleChargingState();
    void TransitionToSleepState();
    void TransitionToActiveState();
    void TransitionToReadyState();
    void TransitionToChargingState();

    void RequestVIN();

    int frameCount = 0, tickerCount = 0, replyCount = 0;  // Keep track of when the car is talking or silent.

};

// Poll states
enum PollState
{
    POLLSTATE_SLEEP,
    POLLSTATE_ACTIVE,
    POLLSTATE_READY,
    POLLSTATE_CHARGING
};

// CAN bus addresses
enum CANAddress
{
    HYBRID_BATTERY_SYSTEM_TX = 0x747,
    HYBRID_BATTERY_SYSTEM_RX = 0x74F,
    HYBRID_CONTROL_SYSTEM_TX = 0x7D2,
    HYBRID_CONTROL_SYSTEM_RX = 0x7DA,
    PLUG_IN_CONTROL_SYSTEM_TX = 0x745,
    PLUG_IN_CONTROL_SYSTEM_RX = 0x74D,
    HPCM_HYBRIDPTCTR_RX = 0x7EA
};

// CAN PIDs
enum CANPID
{
    PID_AMBIENT_TEMPERATURE = 0x46,
    PID_BATTERY_TEMPERATURES = 0x1814,
    PID_BATTERY_SOC = 0x1738,
    PID_BATTERY_VOLTAGE_AND_CURRENT = 0x1F9A,
    PID_READY_SIGNAL = 0x1076,
    PID_CHARGING_LID = 0x1625,
    PID_ODOMETER = 0xA6,
    PID_VEHICLE_SPEED = 0x0D,
    PID_SHIFT_POSITION = 0x1061,
    PID_PISW_STATUS = 0x1669,
    PID_CHARGING = 0x10D1,
    PID_VIN = 0xF190
};

// RX buffer access functions

inline uint8_t GetRxBByte(const std::string& rxbuf, size_t index)
{
    return static_cast<uint8_t>(rxbuf[index]);
}

inline uint16_t GetRxBUint16(const std::string& rxbuf, size_t index)
{
    return (static_cast<uint16_t>(GetRxBByte(rxbuf, index)) << 8) | GetRxBByte(rxbuf, index + 1);
}

inline uint32_t GetRxBUint24(const std::string& rxbuf, size_t index)
{
    return (static_cast<uint32_t>(GetRxBByte(rxbuf, index)) << 16) |
        (static_cast<uint32_t>(GetRxBByte(rxbuf, index + 1)) << 8) |
        GetRxBByte(rxbuf, index + 2);
}

inline uint32_t GetRxBUint32(const std::string& rxbuf, size_t index)
{
    return (static_cast<uint32_t>(GetRxBByte(rxbuf, index)) << 24) |
        (static_cast<uint32_t>(GetRxBByte(rxbuf, index + 1)) << 16) |
        (static_cast<uint32_t>(GetRxBByte(rxbuf, index + 2)) << 8) |
        GetRxBByte(rxbuf, index + 3);
}

inline int8_t GetRxBInt8(const std::string& rxbuf, size_t index)
{
    return static_cast<int8_t>(GetRxBByte(rxbuf, index));
}

inline int16_t GetRxBInt16(const std::string& rxbuf, size_t index)
{
    return static_cast<int16_t>(GetRxBUint16(rxbuf, index));
}

inline int32_t GetRxBInt32(const std::string& rxbuf, size_t index)
{
    return static_cast<int32_t>(GetRxBUint32(rxbuf, index));
}

inline bool GetRxBBit(const std::string& rxbuf, size_t byteIndex, size_t bitIndex)
{
    uint8_t byte = GetRxBByte(rxbuf, byteIndex);
    return (byte & (1 << bitIndex)) != 0;
}

#endif // __VEHICLE_TOYOTA_ETNGA_H__
