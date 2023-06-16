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

void OvmsVehicleToyotaETNGA::InitalizeMetrics()
{
  m_v_pos_trip_start = MyMetrics.InitFloat("xte.v.p.trip.start", SM_STALE_NONE, 0.0f, Kilometers, false); // This variable will store the odometer reading at the beginning of a trip.

  // Set initial values for metrics
  SetReadyStatus(false);

  // Set poll state transition variables to shorter autostale than default
  StdMetrics.ms_v_env_on->SetAutoStale(SM_STALE_MIN);
  StdMetrics.ms_v_door_chargeport->SetAutoStale(SM_STALE_MIN);

}

float OvmsVehicleToyotaETNGA::CalculateBatteryCurrent(const std::string& data)
{
  return static_cast<float>(GetRxBInt16(data, 4)) / 10.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryVoltage(const std::string& data)
{
  return static_cast<float>(GetRxBUint16(data, 2)) / 64.0f;
}

float OvmsVehicleToyotaETNGA::CalculateVehicleSpeed(const std::string& data)
{
  return static_cast<float>(GetRxBInt8(data, 0));
}

float OvmsVehicleToyotaETNGA::CalculateAmbientTemperature(const std::string& data)
{
  return static_cast<float>(GetRxBInt8(data, 0)) - 40.0f;
}

std::vector<float> OvmsVehicleToyotaETNGA::CalculateBatteryTemperatures(const std::string& data)
{
    std::vector<float> temperatures;

    for (size_t i = 0; i < 48; i += 2) {
        int16_t temperatureRaw = GetRxBInt16(data, i);
        float temperature = static_cast<float>(temperatureRaw) / 256.0f - 50.0f;
        temperatures.push_back(temperature);
    }

    return temperatures;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryPower(const float voltage, const float current)
{
  return voltage * current / 1000.0f;
}

bool OvmsVehicleToyotaETNGA::CalculateChargingDoorStatus(const std::string& data)
{
  return GetRxBBit(data, 1, 1);
}

bool OvmsVehicleToyotaETNGA::CalculateReadyStatus(const std::string& data)
{
  return GetRxBBit(data, 1, 0);
}

float OvmsVehicleToyotaETNGA::CalculateOdometer(const std::string& data)
{
    return static_cast<float>(GetRxBUint32(data, 0)) / 10.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatterySOC(const std::string& data)
{
    return static_cast<float>(GetRxBInt8(data, 0));
}

int OvmsVehicleToyotaETNGA::CalculateShiftPosition(const std::string& data)
{
  return static_cast<int>(GetRxBInt8(data, 0));
}

bool OvmsVehicleToyotaETNGA::CalculatePISWStatus(const std::string& data)
{
  // 0x00 = None
  // 0x03 = Charging connector connected
  return GetRxBInt8(data, 0);
}

bool OvmsVehicleToyotaETNGA::CalculateChargingStatus(const std::string& data)
{
  // 0x00 = None
  // 0x03 = Charging / Discharging Mode
  return GetRxBInt8(data, 0);
}

void OvmsVehicleToyotaETNGA::SetBatteryVoltage(const float voltage)
{
  ESP_LOGV(TAG, "Voltage: %f", voltage);
  StdMetrics.ms_v_bat_voltage->SetValue(voltage);
}

void OvmsVehicleToyotaETNGA::SetBatteryCurrent(const float current)
{
  ESP_LOGV(TAG, "Current: %f", current);
  StdMetrics.ms_v_bat_current->SetValue(current);
}

void OvmsVehicleToyotaETNGA::SetBatteryPower(const float power)
{
  ESP_LOGV(TAG, "Power: %f", power);
  StdMetrics.ms_v_bat_power->SetValue(power);
}

void OvmsVehicleToyotaETNGA::SetVehicleSpeed(const float speed)
{
  ESP_LOGV(TAG, "Speed: %f", speed);
  StdMetrics.ms_v_pos_speed->SetValue(speed);
}

void OvmsVehicleToyotaETNGA::SetChargingDoorStatus(const bool chargingDoorStatus)
{
//  ESP_LOGV(TAG, "Charging Door Status: %s", chargingDoorStatus ? "Open" : "Closed");
  StdMetrics.ms_v_door_chargeport->SetValue(chargingDoorStatus);
}

void OvmsVehicleToyotaETNGA::SetReadyStatus(const bool readyStatus)
{
//  ESP_LOGV(TAG, "Ready Status: %s", readyStatus ? "Ready" : "Not Ready");
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

    // There is no single battery temperature value, so I'm using the min Temperature
    StandardMetrics.ms_v_bat_temp->SetValue(minTemperature);
}

void OvmsVehicleToyotaETNGA::SetOdometer(const float odometer)
{
  ESP_LOGV(TAG, "Odometer: %f", odometer);
  StdMetrics.ms_v_pos_odometer->SetValue(odometer);  // Set the odometer metric

  if (m_v_pos_trip_start->IsStale())
  {
    // Update the trip start metric if it is stale
    // It becomes stale when first transitioning to the READY state
    m_v_pos_trip_start->SetValue(odometer);
  }

  // Update the trip odometer
  float tripOdometer = odometer - m_v_pos_trip_start->AsFloat();
  StdMetrics.ms_v_pos_trip->SetValue(tripOdometer); 

}

void OvmsVehicleToyotaETNGA::SetAmbientTemperature(const float temperature)
{
  ESP_LOGV(TAG, "Ambient Temperature: %f", temperature);
  StdMetrics.ms_v_env_temp->SetValue(temperature);
}

void OvmsVehicleToyotaETNGA::SetBatterySOC(const float SOC)
{
  ESP_LOGV(TAG, "SOC: %f", SOC);
  StdMetrics.ms_v_bat_soc->SetValue(SOC);
  
}

void OvmsVehicleToyotaETNGA::SetShiftPosition(const int shiftPosition)
{
  const char* shiftPositionText;
  int gear;

  switch (shiftPosition) {
      case 2:
          shiftPositionText = "Reverse";
          gear = -1;
          break;
      case 4:
          shiftPositionText = "Neutral";
          gear = 0;
          break;
      case 0:
          shiftPositionText = "Park";
          gear = 0;
          break;
      case 6:
          shiftPositionText = "Drive";
          gear = 1;
          break;
      default:
          shiftPositionText = "Unknown";
          gear = -999;
          break;
  }

  ESP_LOGV(TAG, "Gear: %s", shiftPositionText);
  StdMetrics.ms_v_env_gear->SetValue(gear);
  
}

void OvmsVehicleToyotaETNGA::SetPISWStatus(const bool PISWStatus)
{
  ESP_LOGV(TAG, "Pilot Status: %s", PISWStatus ? "Connected" : "Not Connected");
  StdMetrics.ms_v_charge_pilot->SetValue(PISWStatus);
  
}

void OvmsVehicleToyotaETNGA::SetChargingStatus(const bool chargingStatus)
{
  ESP_LOGV(TAG, "Charging Status: %s", chargingStatus ? "Charging" : "Not Charging");
  StdMetrics.ms_v_charge_inprogress->SetValue(chargingStatus);
  
}
