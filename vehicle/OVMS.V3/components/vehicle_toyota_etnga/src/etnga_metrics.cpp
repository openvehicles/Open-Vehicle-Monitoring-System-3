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

void OvmsVehicleToyotaETNGA::InitializeMetrics()
{
    m_s_pollstate = MyMetrics.InitInt("xte.s.pollstate", SM_STALE_NONE);  // This variable stores the pollstate
    m_v_bat_heater_status = MyMetrics.InitBool("xte.v.b.heat", SM_STALE_MID);  // This variable stores the status of the battery coolant heater relay
    m_v_bat_soc_bms = MyMetrics.InitFloat("xte.v.b.soc.bms", SM_STALE_MID, 0.0f, Percentage, true);  // This variable stores the SOC as reported by the BMS
    m_v_bat_speed_water_pump = MyMetrics.InitFloat("xte.v.b.speed.waterpump", SM_STALE_MID, 0.0f, Other);  // This variable stores the RPM of the battery water pump
    m_v_bat_temp_coolant = MyMetrics.InitFloat("xte.v.b.temp.coolant", SM_STALE_MID, 0.0f, Celcius);  // This variable stores the temperature of the battery coolant
    m_v_bat_temp_heater = MyMetrics.InitFloat("xte.v.b.temp.heater", SM_STALE_MID, 0.0f, Celcius);  // This variable stores the temperature of the battery coolant
    m_v_pos_trip_start = MyMetrics.InitFloat("xte.v.p.trip.start", SM_STALE_NONE, 0.0f, Kilometers, false);  // This variable stores the odometer reading at the beginning of a trip

    // Set initial values for metrics
    SetReadyStatus(false);

    // Set poll state transition variables to shorter autostale than default
    StandardMetrics.ms_v_env_on->SetAutoStale(SM_STALE_MIN);
    StandardMetrics.ms_v_door_chargeport->SetAutoStale(SM_STALE_MIN);
}

// Data calculation functions
float OvmsVehicleToyotaETNGA::CalculateAmbientTemperature(const std::string& data)
{
    return static_cast<float>(GetRxBInt8(data, 0)) - 40.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryCurrent(const std::string& data)
{
    return static_cast<float>(GetRxBInt16(data, 4)) / 10.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryPower(float voltage, float current)
{
    return voltage * current / 1000.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatterySOC(const std::string& data)
{
    return static_cast<float>(GetRxBInt8(data, 0));
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

float OvmsVehicleToyotaETNGA::CalculateBatteryVoltage(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 2)) / 64.0f;
}

bool OvmsVehicleToyotaETNGA::CalculateChargingDoorStatus(const std::string& data)
{
    return GetRxBBit(data, 1, 1);
}

bool OvmsVehicleToyotaETNGA::CalculateChargingStatus(const std::string& data)
{
    // 0x00 = None
    // 0x03 = Charging / Discharging Mode
    return GetRxBInt8(data, 0);
}

float OvmsVehicleToyotaETNGA::CalculateOdometer(const std::string& data)
{
    return static_cast<float>(GetRxBUint32(data, 0)) / 10.0f;
}

bool OvmsVehicleToyotaETNGA::CalculatePISWStatus(const std::string& data)
{
    // 0x00 = None
    // 0x03 = Charging connector connected
    return GetRxBInt8(data, 0);
}

bool OvmsVehicleToyotaETNGA::CalculateReadyStatus(const std::string& data)
{
    return GetRxBBit(data, 1, 0);
}

int OvmsVehicleToyotaETNGA::CalculateShiftPosition(const std::string& data)
{
    return static_cast<int>(GetRxBInt8(data, 0));
}

float OvmsVehicleToyotaETNGA::CalculateVehicleSpeed(const std::string& data)
{
    return static_cast<float>(GetRxBInt8(data, 0));
}

// Metric setter functions
void OvmsVehicleToyotaETNGA::SetAmbientTemperature(float temperature)
{
    ESP_LOGV(TAG, "Ambient Temperature: %f", temperature);
    StandardMetrics.ms_v_env_temp->SetValue(temperature);
}

void OvmsVehicleToyotaETNGA::SetBatteryCurrent(float current)
{
    ESP_LOGV(TAG, "Current: %f", current);
    StandardMetrics.ms_v_bat_current->SetValue(current);
}

void OvmsVehicleToyotaETNGA::SetBatteryPower(float power)
{
    ESP_LOGV(TAG, "Power: %f", power);
    StandardMetrics.ms_v_bat_power->SetValue(power);
}

void OvmsVehicleToyotaETNGA::SetBatterySOC(float soc)
{
    if (soc == 0.0 && StandardMetrics.ms_v_bat_soc->AsFloat() > 1.0)
    {
        // Ignore zero SOC if previousSOC was greater than 1
        ESP_LOGD(TAG, "Ignoring invalid SOC value: %f", soc);
    }
    else
    {
        ESP_LOGV(TAG, "SOC: %f", soc);
        StandardMetrics.ms_v_bat_soc->SetValue(soc);
    }
}

void OvmsVehicleToyotaETNGA::SetBatteryTemperatures(const std::vector<float>& temperatures)
{
    StandardMetrics.ms_v_bat_cell_temp->SetValue(temperatures);
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
    StandardMetrics.ms_v_bat_pack_tmin->SetValue(minTemperature);
    StandardMetrics.ms_v_bat_pack_tmax->SetValue(maxTemperature);
    StandardMetrics.ms_v_bat_pack_tavg->SetValue(averageTemperature);
    StandardMetrics.ms_v_bat_pack_tstddev->SetValue(standardDeviation);

    // There is no single battery temperature value, so I'm using the min Temperature
    StandardMetrics.ms_v_bat_temp->SetValue(minTemperature);
}

void OvmsVehicleToyotaETNGA::SetBatteryVoltage(float voltage)
{
    ESP_LOGV(TAG, "Voltage: %f", voltage);
    StandardMetrics.ms_v_bat_voltage->SetValue(voltage);
}

void OvmsVehicleToyotaETNGA::SetChargingDoorStatus(bool status)
{
    ESP_LOGV(TAG, "Charging Door Status: %s", status ? "Open" : "Closed");
    StandardMetrics.ms_v_door_chargeport->SetValue(status);
}

void OvmsVehicleToyotaETNGA::SetChargingStatus(bool status)
{
    ESP_LOGV(TAG, "Charging Status: %s", status ? "Charging" : "Not Charging");
    StandardMetrics.ms_v_charge_inprogress->SetValue(status);
}

void OvmsVehicleToyotaETNGA::SetOdometer(float odometer)
{
    ESP_LOGV(TAG, "Odometer: %f", odometer);
    StandardMetrics.ms_v_pos_odometer->SetValue(odometer);  // Set the odometer metric

    if (m_v_pos_trip_start->IsStale())
    {
        // Update the trip start metric if it is stale
        // It becomes stale when first transitioning to the READY state
        m_v_pos_trip_start->SetValue(odometer);
    }

    // Update the trip odometer
    float tripOdometer = odometer - m_v_pos_trip_start->AsFloat();
    StandardMetrics.ms_v_pos_trip->SetValue(tripOdometer);
}

void OvmsVehicleToyotaETNGA::SetPISWStatus(bool status)
{
    ESP_LOGV(TAG, "Pilot Status: %s", status ? "Connected" : "Not Connected");
    StandardMetrics.ms_v_charge_pilot->SetValue(status);
}

void OvmsVehicleToyotaETNGA::SetPollState(int state)
{
    const char* pollStateText;

    switch (state) {
        case PollState::SLEEP:
            pollStateText = "SLEEP";
            break;
        case PollState::ACTIVE:
            pollStateText = "ACTIVE";
            break;
        case PollState::READY:
            pollStateText = "READY";
            break;
        case PollState::CHARGING:
            pollStateText = "CHARGING";
            break;
        default:
            pollStateText = "UNKNOWN";
            ESP_LOGW(TAG, "Unknown poll state: %d", state);
            break;
    }
    
    ESP_LOGI(TAG, "Transitioning to the %s state", pollStateText);

    PollSetState(state);
    m_s_pollstate->SetValue(state);
}

void OvmsVehicleToyotaETNGA::SetReadyStatus(bool status)
{
    ESP_LOGV(TAG, "Ready Status: %s", status ? "Ready" : "Not Ready");
    StandardMetrics.ms_v_env_on->SetValue(status);
}

void OvmsVehicleToyotaETNGA::SetShiftPosition(int position)
{
    const char* shiftPositionText;
    int gear;

    switch (position) {
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
    StandardMetrics.ms_v_env_gear->SetValue(gear);
}

void OvmsVehicleToyotaETNGA::SetVehicleSpeed(float speed)
{
    ESP_LOGV(TAG, "Speed: %f", speed);
    StandardMetrics.ms_v_pos_speed->SetValue(speed);
}

void OvmsVehicleToyotaETNGA::SetVehicleVIN(std::string vin)
{
    ESP_LOGV(TAG, "VIN: %s", vin.c_str());
    StandardMetrics.ms_v_vin->SetValue(std::move(vin));
}
