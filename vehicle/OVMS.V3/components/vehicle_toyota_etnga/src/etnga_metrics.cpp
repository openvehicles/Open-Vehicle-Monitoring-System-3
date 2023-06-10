/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.
*/

#include "vehicle_toyota_etnga.h"

float OvmsVehicleToyotaETNGA::GetBatteryVoltage(const std::string& data)
{
  return GetRxBInt16(data, 2) / 64.0f;
}

float OvmsVehicleToyotaETNGA::GetBatteryCurrent(const std::string& data)
{
  return GetRxBUint16(data, 4) / 10.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryPower(float voltage, float current)
{
  return voltage * current / 1000;
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
