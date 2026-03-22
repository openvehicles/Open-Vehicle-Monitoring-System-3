/*
;    Project:       Open Vehicle Monitor System
;    Module:        Renault Zoe Ph2 - Energy & Statistics
;
;    (C) 2022-2026  Carsten Schmiemann
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
#include "vehicle_renaultzoe_ph2.h"

void OvmsVehicleRenaultZoePh2::CalculateEfficiency()
{
  if (!m_vcan_enabled)
  {
    float speed = StdMetrics.ms_v_pos_speed->AsFloat();
    float power = StdMetrics.ms_v_bat_power->AsFloat(0, Watts);
    float distanceTraveled = speed / 3600.0f; // km/h to km/s

    float consumption = 0;
    if (speed >= 10)
    {
      consumption = power / speed; // Wh/km
    }

    // Only record valid entries
    if (distanceTraveled > 0 && std::isfinite(consumption))
    {
      consumptionHistory.push_back(std::make_pair(distanceTraveled, consumption));
      totalConsumption += consumption * distanceTraveled;
      totalDistance += distanceTraveled;
    }

    // Trim excess distance from front of buffer when over 100 km
    // Use 101.0 km as safe buffer to avoid edge hover
    while (totalDistance > 101.0f && !consumptionHistory.empty())
    {
      auto oldest = consumptionHistory.front();
      totalDistance -= oldest.first;
      totalConsumption -= oldest.first * oldest.second;
      consumptionHistory.pop_front();
    }

    // Ensure numbers stay clean
    if (totalDistance < 0 || !std::isfinite(totalDistance))
      totalDistance = 0;
    if (totalConsumption < 0 || !std::isfinite(totalConsumption))
      totalConsumption = 0;

    // Calculate and set average
    float avgConsumption = (totalDistance > 0) ? totalConsumption / totalDistance : 0;
    StdMetrics.ms_v_bat_consumption->SetValue(TRUNCPREC(avgConsumption, 1));

    // Logging
    ESP_LOGD_RZ2(TAG, "Average battery consumption: %.1f Wh/km (over %.1f km, buffer size: %zu)",
             avgConsumption, totalDistance, consumptionHistory.size());

    // Hard reset if buffer grows out of control
    if (consumptionHistory.size() > 10000)
    {
      ESP_LOGW(TAG, "Consumption history too large (%.1f km stored, %zu entries) — resetting!",
               totalDistance, consumptionHistory.size());
      consumptionHistory.clear();
      totalDistance = 0;
      totalConsumption = 0;
    }
  }
}

// Handle Charge ernergy, charge time remaining and efficiency calucation
void OvmsVehicleRenaultZoePh2::ChargeStatistics()
{
  float charge_current = fabs(StandardMetrics.ms_v_bat_current->AsFloat(0, Amps));
  float charge_voltage = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float battery_power = fabs(StandardMetrics.ms_v_bat_power->AsFloat(0, kW));
  float charger_power = StandardMetrics.ms_v_charge_power->AsFloat(1, kW);
  float ac_current = StandardMetrics.ms_v_charge_current->AsFloat(0, Amps);
  string ac_phases = mt_main_phases->AsString();
  int minsremaining = 0;

  if (CarIsCharging != CarLastCharging)
  {
    if (CarIsCharging)
    {
      StandardMetrics.ms_v_charge_kwh->SetValue(0);
      StandardMetrics.ms_v_charge_inprogress->SetValue(CarIsCharging);
      if (m_UseBMScalculation)
      {
        mt_bat_chg_start->SetValue(StandardMetrics.ms_v_charge_kwh_grid_total->AsFloat());
      }
    }
    else
    {
      StandardMetrics.ms_v_charge_inprogress->SetValue(CarIsCharging);
    }
  }
  CarLastCharging = CarIsCharging;

  if (!StandardMetrics.ms_v_charge_pilot->AsBool() || !StandardMetrics.ms_v_charge_inprogress->AsBool() || (charge_current <= 0.0))
  {
    return;
  }

  if (charge_voltage > 0 && charge_current > 0)
  {
    float power = charge_voltage * charge_current / 1000.0;
    float energy = power / 3600.0 * 10.0;
    float c_efficiency;

    if (m_UseBMScalculation)
    {
      StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh_grid_total->AsFloat() - mt_bat_chg_start->AsFloat());
    }
    else
    {
      StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat() + energy);
    }

    // Calculate charge remaining time if we cannot get it from V-CAN
    if (m_vcan_enabled)
    {
      if (vcan_hvBat_ChargeTimeRemaining < 2550) // If charging is not started we got 2555 value from V-CAN
      {
        StandardMetrics.ms_v_charge_duration_full->SetValue(vcan_hvBat_ChargeTimeRemaining, Minutes);
        minsremaining = vcan_hvBat_ChargeTimeRemaining;
      }
    }
    else
    {
      if (charge_voltage != 0 && charge_current != 0)
      {
        minsremaining = calcMinutesRemaining(charge_voltage, charge_current);
        StandardMetrics.ms_v_charge_duration_full->SetValue(minsremaining, Minutes);
      }
    }

    if (StandardMetrics.ms_v_charge_type->AsString() == "type2")
    {
      if (charger_power != 0)
      {
        c_efficiency = (battery_power / charger_power) * 100.0;
        if (c_efficiency < 100.0)
        {
          StandardMetrics.ms_v_charge_efficiency->SetValue(c_efficiency);
        }
      }
      ESP_LOGI(TAG, "Charge time remaining: %d mins, AC Charge at %.2f kW with %.1f amps, %s at %.1f efficiency, %.2f powerfactor", minsremaining, charger_power, ac_current, ac_phases.c_str(), StandardMetrics.ms_v_charge_efficiency->AsFloat(100), ACInputPowerFactor);
    }
    else if (StandardMetrics.ms_v_charge_type->AsString() == "ccs" || StandardMetrics.ms_v_charge_type->AsString() == "chademo")
    {
      StandardMetrics.ms_v_charge_power->SetValue(battery_power);
      ESP_LOGI(TAG, "Charge time remaining: %d mins, DC Charge at %.2f kW", minsremaining, battery_power);
    }
  }
}

// Charge time remaining - helper function to get remaining charge time for ChargeStatistics
int OvmsVehicleRenaultZoePh2::calcMinutesRemaining(float charge_voltage, float charge_current)
{
  float bat_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
  float real_capacity = m_battery_capacity * (StandardMetrics.ms_v_bat_soh->AsFloat(100) / 100);

  float remaining_wh = real_capacity - (real_capacity * bat_soc / 100.0);
  float remaining_hours = remaining_wh / (charge_current * charge_voltage);
  float remaining_mins = remaining_hours * 60.0;

  ESP_LOGD_RZ2(TAG, "SOC: %f, BattCap:%d, Current: %f, Voltage: %f, RemainWH: %f, RemainHour: %f, RemainMin: %f", bat_soc, m_battery_capacity, charge_current, charge_voltage, remaining_wh, remaining_hours, remaining_mins);
  return MIN(1440, (int)remaining_mins);
}

// Energy values calculation withing OVMS
void OvmsVehicleRenaultZoePh2::EnergyStatisticsOVMS()
{
  float voltage = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float current = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);

  float power = voltage * current / 1000.0;

  if (power != 0.0 && StandardMetrics.ms_v_env_on->AsBool())
  {
    float energy = power / 3600.0 * 10.0;
    if (energy > 0.0f)
      StandardMetrics.ms_v_bat_energy_used->SetValue(StandardMetrics.ms_v_bat_energy_used->AsFloat() + energy);
    else
      StandardMetrics.ms_v_bat_energy_recd->SetValue(StandardMetrics.ms_v_bat_energy_recd->AsFloat() - energy);
  }
}

// Energy values from BMS
void OvmsVehicleRenaultZoePh2::EnergyStatisticsBMS()
{
  if (StandardMetrics.ms_v_bat_energy_used_total->AsFloat() != 0 && StandardMetrics.ms_v_bat_energy_recd_total->AsFloat() != 0)
  {
    StandardMetrics.ms_v_bat_energy_used->SetValue(StandardMetrics.ms_v_bat_energy_used_total->AsFloat() - mt_bat_used_start->AsFloat());
    StandardMetrics.ms_v_bat_energy_recd->SetValue(StandardMetrics.ms_v_bat_energy_recd_total->AsFloat() - mt_bat_recd_start->AsFloat());
  }
}
