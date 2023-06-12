/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#include "vehicle_toyota_etnga.h"
#include <algorithm>
#include <cmath>

float OvmsVehicleToyotaETNGA::GetBatteryCurrent(const std::string& data)
{
  return GetRxBUint16(data, 4) / 10.0f;
}

float OvmsVehicleToyotaETNGA::GetBatteryVoltage(const std::string& data)
{
  return GetRxBInt16(data, 2) / 64.0f;
}

std::vector<float> OvmsVehicleToyotaETNGA::GetBatteryTemperatures(const std::string& data)
{
    std::vector<float> temperatures;

    for (size_t i = 0; i < 48; i += 2) {
        int16_t temperatureRaw = GetRxBInt16(data, i);
        float temperature = static_cast<float>(temperatureRaw) / 256.0f - 50.0f;
        temperatures.push_back(temperature);
    }

    return temperatures;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryPower(float voltage, float current)
{
  return voltage * current / 1000.0f;
}

bool OvmsVehicleToyotaETNGA::GetChargingDoorStatus(const std::string& data)
{
  return GetRxBBit(data, 1, 1);
}

bool OvmsVehicleToyotaETNGA::GetReadyStatus(const std::string& data)
{
  return GetRxBBit(data, 1, 0);
}

float OvmsVehicleToyotaETNGA::GetOdometer(const std::string& data)
{
    return static_cast<float>(GetRxBUint32(data, 0)) / 10.0f;
}

void OvmsVehicleToyotaETNGA::SetBatteryVoltage(float voltage)
{
  ESP_LOGV(TAG, "Voltage: %f", voltage);
  StdMetrics.ms_v_bat_voltage->SetValue(voltage);
}

void OvmsVehicleToyotaETNGA::SetBatteryCurrent(float current)
{
  ESP_LOGV(TAG, "Current: %f", current);
  StdMetrics.ms_v_bat_current->SetValue(current);
}

void OvmsVehicleToyotaETNGA::SetBatteryPower(float power)
{
  ESP_LOGV(TAG, "Power: %f", power);
  StdMetrics.ms_v_bat_power->SetValue(power);
}

void OvmsVehicleToyotaETNGA::SetChargingDoorStatus(bool chargingDoorStatus)
{
  ESP_LOGV(TAG, "Charging Door Status: %s", chargingDoorStatus ? "Open" : "Closed");
  StdMetrics.ms_v_door_chargeport->SetValue(chargingDoorStatus);
}

void OvmsVehicleToyotaETNGA::SetReadyStatus(bool readyStatus)
{
  ESP_LOGV(TAG, "Ready Status: %s", readyStatus ? "Ready" : "Not Ready");
  StdMetrics.ms_v_env_on->SetValue(readyStatus);
}

void OvmsVehicleToyotaETNGA::SetBatteryTemperatures(const std::vector<float>& temperatures)
{
  StdMetrics.ms_v_bat_cell_temp->SetValue(temperatures);
}

void OvmsVehicleToyotaETNGA::SetBatteryTemperatureStatistics(const std::vector<float>& temperatures)
{
    // Calculate the minimum temperature
    float minTemperature = *std::min_element(temperatures.begin(), temperatures.end());

    // Calculate the maximum temperature
    float maxTemperature = *std::max_element(temperatures.begin(), temperatures.end());

    // Calculate the average temperature
    float sum = std::accumulate(temperatures.begin(), temperatures.end(), 0.0f);
    float averageTemperature = sum / temperatures.size();

    // Calculate the standard deviation
    float variance = 0.0f;
    for (float temperature : temperatures) {
        variance += pow(temperature - averageTemperature, 2);
    }
    float standardDeviation = sqrt(variance / temperatures.size());

    // Store the results
    StdMetrics.ms_v_bat_pack_tmin->SetValue(minTemperature);
    StdMetrics.ms_v_bat_pack_tmax->SetValue(maxTemperature);
    StdMetrics.ms_v_bat_pack_tavg->SetValue(averageTemperature);
    StdMetrics.ms_v_bat_pack_tstddev->SetValue(standardDeviation);
}
void OvmsVehicleToyotaETNGA::SetOdometer(float odometer)
{
  StdMetrics.ms_v_pos_odometer->SetValue(odometer);
}
