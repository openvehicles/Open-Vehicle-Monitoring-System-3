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
    void Ticker3600(uint32_t ticker);

    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);

    void IncomingFrameCan2(CAN_frame_t* p_frame);

protected:
    std::string m_rxbuf;

private:
    static constexpr const char* TAG = "v-toyota-etnga";
    void IncomingHybridControlSystem(uint16_t pid);
    void IncomingPlugInControlSystem(uint16_t pid);
    void IncomingHybridBatterySystem(uint16_t pid);
    void IncomingHPCMHybridPtCtr(uint16_t pid);

    int RequestVIN();

    int frameCount = 0, tickerCount = 0, replyCount = 0;  // Keep track of when the car is talking or silent.

    float GetBatteryCurrent(const std::string& data);
    float CalculateBatteryPower(float voltage, float current);
    std::vector<float> GetBatteryTemperatures(const std::string& data);
    float GetBatteryVoltage(const std::string& data);
    bool GetChargingDoorStatus(const std::string& data);
    float GetOdometer(const std::string& data);
    float GetVehicleSpeed(const std::string& data);
    bool GetReadyStatus(const std::string& data);

    float GetAmbientTemperature(const std::string& data);


    void SetBatteryVoltage(float voltage);
    void SetBatteryCurrent(float current);
    void SetBatteryPower(float power);
    void SetChargingDoorStatus(bool chargingDoorStatus);
    void SetReadyStatus(bool readyStatus);
    void SetBatteryTemperatures(const std::vector<float>& temperatures);
    void SetBatteryTemperatureStatistics(const std::vector<float>& temperatures);
    void SetOdometer(float odometer);
    void SetVehicleSpeed(float speed);
    void SetAmbientTemperature(float speed);

    void handleSleepState();
    void handleActiveState();
    void handleReadyState();
    void handleChargingState();

    void transitionToSleepState();
    void transitionToActiveState();
    void transitionToReadyState();
    void transitionToChargingState();

};

// Poll states
#define POLLSTATE_SLEEP      0
#define POLLSTATE_ACTIVE     1
#define POLLSTATE_READY      2
#define POLLSTATE_CHARGING   3

// ECUs
#define HYBRID_BATTERY_SYSTEM_TX    0x747
#define HYBRID_BATTERY_SYSTEM_RX    0x74F
#define HYBRID_CONTROL_SYSTEM_TX    0x7D2
#define HYBRID_CONTROL_SYSTEM_RX    0x7DA
#define PLUG_IN_CONTROL_SYSTEM_TX   0x745
#define PLUG_IN_CONTROL_SYSTEM_RX   0x74D
#define HPCM_HYBRIDPTCTR_RX         0x7EA
//#define                           0x7E8 <- What is this one? It's where the odometer comes from

// PIDs
#define PID_ODOMETER                        0xA6
#define PID_VEHICLE_SPEED                   0x0D
#define PID_AMBIENT_TEMPERATURE             0x46

#define PID_BATTERY_VOLTAGE_AND_CURRENT     0x1F9A
#define PID_READY_SIGNAL                    0x1076
#define PID_CHARGING_LIDS_SWITCH            0x1625
#define PID_BATTERY_TEMPERATURES            0x1814

// RX buffer access inline functions: b=byte#

inline uint8_t GetRxBByte(const std::string& rxbuf, size_t index)
{
    return rxbuf[index];
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

#endif //#ifndef __VEHICLE_TOYOTA_ETNGA_H__
