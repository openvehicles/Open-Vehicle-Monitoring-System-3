/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 ; https://github.com/MyLab-odyssey/ED4scan
 */

#include "vehicle_smarteq.h"

void OvmsVehicleSmartEQ::xsq_trip_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
	
  smarteq->CommandTripStart(verbosity, writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTripStart(int verbosity, OvmsWriter* writer) {
    metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
    writer->puts("Trip start values:");
    writer->printf("  distance: %s\n", (char*) mt_start_distance->AsUnitString("-", rangeUnit, 1).c_str());
    writer->printf("  time: %s\n", (char*) mt_start_time->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
    writer->printf("  CAP: %s\n", (char*) StdMetrics.ms_v_bat_capacity->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyTripStart() {
  StringWriter buf(200);
  CommandTripStart(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.trip.start",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_trip_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
  smarteq->CommandTripReset(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTripReset(int verbosity, OvmsWriter* writer) {
    metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
    writer->puts("Trip reset values:");
    writer->printf("  distance: %s\n", (char*) mt_start_distance->AsUnitString("-", rangeUnit, 1).c_str());
    writer->printf("  time: %s\n", (char*) mt_start_time->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
    writer->printf("  CAP: %s\n", (char*) StdMetrics.ms_v_bat_capacity->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyTripReset() {
  StringWriter buf(200);
  CommandTripReset(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.trip.reset",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_trip_counters(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
	
    smarteq->CommandTripCounters(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTripCounters(int verbosity, OvmsWriter* writer) {
    metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
    writer->puts("Trip counter values:");
    writer->printf("  distance: %s\n", (char*) StdMetrics.ms_v_pos_trip->AsUnitString("-", rangeUnit, 1).c_str());
	  writer->printf("  Energy used: %s\n", (char*) StdMetrics.ms_v_bat_energy_used->AsUnitString("-", Native, 3).c_str());
	  writer->printf("  Energy recd: %s\n", (char*) StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", Native, 3).c_str());
    writer->printf("  Energy grid: %s\n", (char*) StdMetrics.ms_v_charge_kwh_grid->AsUnitString("-", Native, 3).c_str());
    writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
    writer->printf("  CAP: %s\n", (char*) StdMetrics.ms_v_bat_capacity->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyTripCounters() {
  StringWriter buf(200);
  CommandTripCounters(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.trip.counters",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_trip_total(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
    smarteq->CommandTripTotal(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandTripTotal(int verbosity, OvmsWriter* writer) {
    metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
    writer->puts("Trip total values:");
    writer->printf("  distance: %s\n", (char*) mt_pos_odometer_trip_total->AsUnitString("-", rangeUnit, 1).c_str());
	  writer->printf("  Energy used: %s\n", (char*) StdMetrics.ms_v_bat_energy_used_total->AsUnitString("-", Native, 3).c_str());
	  writer->printf("  Energy recd: %s\n", (char*) StdMetrics.ms_v_bat_energy_recd_total->AsUnitString("-", Native, 3).c_str());
    writer->printf("  Energy grid: %s\n", (char*) StdMetrics.ms_v_charge_kwh_grid_total->AsUnitString("-", Native, 3).c_str());
    writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
    writer->printf("  CAP: %s\n", (char*) StdMetrics.ms_v_bat_capacity->AsUnitString("-", Native, 1).c_str());
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyTotalCounters() {
  StringWriter buf(200);
  CommandTripTotal(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.trip.total",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_maintenance(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
    smarteq->CommandMaintenance(verbosity, writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandMaintenance(int verbosity, OvmsWriter* writer) {
    writer->puts("Maintenance values:");    
    if (StdMetrics.ms_v_env_service_time->IsDefined()) {
      time_t service_time = StdMetrics.ms_v_env_service_time->AsInt();
      char service_date[32];
      struct tm service_tm;
      localtime_r(&service_time, &service_tm);
      strftime(service_date, sizeof(service_date), "%d.%m.%Y", &service_tm);
      writer->printf("  service date: %s\n", service_date);
    }
    writer->printf("  days: %d\n", mt_obd_mt_day_usual->AsInt());
    writer->printf("  level: %s\n", mt_obd_mt_level->AsString().c_str());
    writer->printf("  km: %d\n", mt_obd_mt_km_usual->AsInt());    
    writer->printf("  remaining HV contactor cycles: %d / %d\n", 
                   mt_bms_contactor_cycles->GetElemValue(1), 
                   mt_bms_contactor_cycles->GetElemValue(0));
    writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
    return Success;
}
void OvmsVehicleSmartEQ::NotifyMaintenance() {
  StringWriter buf(200);
  CommandMaintenance(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.maintenance",buf.c_str());
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandSOClimit(int verbosity, OvmsWriter* writer) {
  writer->puts("SOC limit reached:");
  writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("  CAP: %s\n", (char*) StdMetrics.ms_v_bat_capacity->AsUnitString("-", Native, 1).c_str());
  writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
  return Success;
}
void OvmsVehicleSmartEQ::NotifySOClimit() {
  StringWriter buf(200);
  CommandSOClimit(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.soclimit",buf.c_str());
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::Command12Vcharge(int verbosity, OvmsWriter* writer) {
  writer->puts("12V charge on:");
 
  writer->printf("  12V: %s\n", (char*) StdMetrics.ms_v_bat_12v_voltage->AsUnitString("-", Native, 1).c_str());
  writer->printf("  SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("  CAP: %s\n", (char*) StdMetrics.ms_v_bat_capacity->AsUnitString("-", Native, 1).c_str());
  writer->printf("  SOH: %s %s\n", StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0).c_str(), StdMetrics.ms_v_bat_health->AsUnitString("-", ToUser, 0).c_str());
  return Success;
}
void OvmsVehicleSmartEQ::Notify12Vcharge() {
  StringWriter buf(200);
  Command12Vcharge(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xsq.12v.charge",buf.c_str());
}

void OvmsVehicleSmartEQ::xsq_ed4scan(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;

  smarteq->CommandED4scan(verbosity, writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandED4scan(int verbosity, OvmsWriter* writer) {
  writer->puts("=== ED4scan-like BMS/EVC/OBL Data Output ===\n");
  writer->printf("  HV Batt Serialnumber:    %s\n", mt_bat_serial->AsString().c_str());
  writer->printf("  BMS Serialnumber:        %s\n", mt_bms_prod_data->AsString().c_str());
  writer->printf("  EVC Traceability:        %s\n", mt_evc_traceability->AsString().c_str());
  writer->printf("  Battery Mileage:         %.0f km\n", mt_bms_mileage->AsFloat());
  writer->printf("  Odometer:                %.0f km\n", StdMetrics.ms_v_pos_odometer->AsFloat()); 

  writer->puts("--- Battery Health (PID 0x61) ---");
  writer->printf("  State of Health:         %.0f%%\n", mt_bms_soh->AsFloat());
  writer->printf("  Usable max Capacity:     %.2f Ah\n", mt_bms_cap->GetElemValue(0));
  
  writer->puts("\n--- HV Contactor Cycles (PID 0x02) ---");
  writer->printf("  Max Cycles:              %d\n", mt_bms_contactor_cycles->GetElemValue(0));
  writer->printf("  remaining Cycles:        %d\n", mt_bms_contactor_cycles->GetElemValue(1));
  
  writer->puts("\n--- SOC Kernel Data (PID 0x08) ---");
  writer->printf("  Open Circuit Voltage:    %.2f V\n", mt_bms_voltages->GetElemValue(5));
  writer->printf("  Real SOC (Min):          %.2f%%\n", mt_bms_soc_values->GetElemValue(2));
  writer->printf("  Real SOC (Max):          %.2f%%\n", mt_bms_soc_values->GetElemValue(3));
  writer->printf("  Kernel SOC:              %.2f%%\n", mt_bms_soc_values->GetElemValue(0));
  writer->printf("  Capacity Loss:           %.2f%%\n", mt_bms_cap->GetElemValue(3));
  writer->printf("  Initial Capacity:        %.2f Ah\n", mt_bms_cap->GetElemValue(1));
  writer->printf("  Estimated Capacity:      %.2f Ah\n", mt_bms_cap->GetElemValue(2));
  writer->printf("  Voltage State:           %s\n", mt_bms_voltage_state->AsString().c_str());
  
  writer->puts("\n--- SOC Recalibration (PID 0x25) ---");
  writer->printf("  Recalibration State:     %s\n", mt_bms_soc_recal_state->AsString().c_str());
  writer->printf("  Display SOC:             %.2f%%\n", mt_bms_soc_values->GetElemValue(4));
  writer->printf("  Usable Capacity:         %.2f Ah\n", mt_bms_cap->GetElemValue(4));
  
  writer->puts("\n--- Battery State (PID 0x07) ---");
  writer->printf("  Cell Voltage Min:        %.3f V\n", mt_bms_voltages->GetElemValue(0));
  writer->printf("  Cell Voltage Max:        %.3f V\n", mt_bms_voltages->GetElemValue(1));
  writer->printf("  Cell Voltage Mean:       %.3f V\n", mt_bms_voltages->GetElemValue(2));
  writer->printf("  Link Voltage:            %.2f V\n", mt_bms_voltages->GetElemValue(3));
  writer->printf("  Pack Voltage:            %.2f V\n", mt_bms_voltages->GetElemValue(4));
  writer->printf("  Pack Current:            %.2f A\n", StdMetrics.ms_v_bat_current->AsFloat());
  writer->printf("  Pack Power:              %.2f kW\n", StdMetrics.ms_v_bat_power->AsFloat());
  writer->printf("  Contactor State:         %s\n", mt_bms_HVcontactStateTXT->AsString().c_str());
  writer->printf("  Vehicle Mode:            %s\n", mt_bms_EVmode_txt->AsString().c_str());
  writer->printf("  BMS 12V Voltage:         %.2f V\n", mt_bms_voltages->GetElemValue(6));

  writer->puts("\n--- EVC Data (0x7EC) ---");
  writer->printf("  HV Energy:               %.3f kWh\n", StdMetrics.ms_v_bat_capacity->AsFloat());
  writer->printf("  DCDC Active Request:     %s\n", StdMetrics.ms_v_env_charging12v->AsBool() ? "Yes" : "No");
  writer->printf("  DCDC Current:            %.2f A\n", mt_evc_dcdc->GetElemValue(6));
  writer->printf("  DCDC Power:              %.0f W\n", mt_evc_dcdc->GetElemValue(2));
  writer->printf("  DCDC Load:               %.2f%%\n", mt_evc_dcdc->GetElemValue(7));
  writer->printf("  Req (int) 12V Voltage:   %.2f V\n", mt_evc_dcdc->GetElemValue(5));
  writer->printf("  DCDC Voltage Request:    %.2f V\n", mt_evc_dcdc->GetElemValue(0));
  writer->printf("  DCDC 12V Voltage:        %.2f V\n", mt_evc_dcdc->GetElemValue(1));
  writer->printf("  USM 12V Voltage:         %.2f V\n", mt_evc_dcdc->GetElemValue(3));
  writer->printf("  Batt 12V Voltage:        %.2f V\n", mt_evc_dcdc->GetElemValue(4));

  writer->puts("\n--- OBL Charger Data (0x793) ---");
  writer->printf("  Fast Charge Active:      %s\n", mt_obl_fastchg->AsBool() ? "Yes" : "No");  
  writer->printf("  Max AC Current:          %.0f A\n", mt_obl_misc->GetElemValue(2));
  writer->printf("  Mains Frequency:         %.2f Hz\n", mt_obl_misc->GetElemValue(0));
  writer->printf("  Ground Resistance:       %.4f Ohm\n", mt_obl_misc->GetElemValue(1));
  writer->printf("  Leakage DC Current:      %.4f mA\n", mt_obl_misc->GetElemValue(3));
  writer->printf("  Leakage HF10kHz:         %.4f mA\n", mt_obl_misc->GetElemValue(4));
  writer->printf("  Leakage HF Current:      %.4f mA\n", mt_obl_misc->GetElemValue(5));
  writer->printf("  Leakage LF Current:      %.4f mA\n", mt_obl_misc->GetElemValue(6));
  writer->printf("  Leakage Diagnostic:      %s\n", mt_obl_main_leakage_diag->AsString().c_str());

  int show = mt_ed4_values->AsInt(10); // number of cells to show

  writer->puts("\n--- Cell Voltage Data (PID 0x41/0x42) ---");
  const std::vector<float> voltage_values = StdMetrics.ms_v_bat_cell_voltage->AsVector();
  if (voltage_values.size() >= show) {
    writer->printf("  Voltages (first %d sensors):\n", show);
    for (int i = 0; i < show && i < (int)voltage_values.size(); i++) {
      writer->printf("    Sensor %02d: %.4f V\n", i + 1, voltage_values[i]);
    }
    writer->printf("  ... (showing %d of 96 cells)\n", show);
  } else {
    writer->puts("  Voltage data not available");
  }
  
  writer->puts("\n--- Cell Temperature Data (PID 0x04) ---");
  const std::vector<float> temp_values = mt_bms_temps->AsVector();
  if (temp_values.size() >= show) {
    writer->printf("  Temperatures (first %d sensors):\n", show);
    for (int i = 0; i < show && i < (int)temp_values.size(); i++) {
      writer->printf("    Sensor %02d: %.1f°C\n", i + 1, temp_values[i]);
    }
    writer->printf("  ... (showing %d of 31 cells)\n", show);
  } else {
    writer->puts("  Temperature data not available");
  }
  
  writer->puts("\n--- Cell Resistance (PID 0x10/0x11) ---");
  const std::vector<float> resistance_values = mt_bms_cell_resistance->AsVector();
  if (resistance_values.size() >= (int)resistance_values.size()) {
    writer->printf("  Relative Resistance (first %d cells):\n", show);
    for (int i = 0; i < show && i < (int)resistance_values.size(); i++) {
      writer->printf("    Cell %02d: %.6f\n", i + 1, resistance_values[i]);
    }
    writer->printf("  ... (showing %d of 96 cells)\n", show);
  } else {
    writer->puts("  Cell resistance data not available");
  }
  
  writer->puts("\n=== End of ED4scan Data ===");
  return Success;
}

void OvmsVehicleSmartEQ::xsq_tpms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq) return;

  bool data_shown = false;
  const char* alertname[] = { "OK", "WARN", "ALERT" };  
  std::vector<std::string> tpms_layout = smarteq->GetTpmsLayout();  
  
  writer->printf("TPMS Data       : ");
  for (auto wheel : tpms_layout)
    writer->printf(" %8s", wheel.c_str());
  writer->puts("");

  if (StandardMetrics.ms_v_tpms_alert->IsDefined())
    {
    writer->printf("Alert level     : ");
    for (auto val : StandardMetrics.ms_v_tpms_alert->AsVector())
      {
      // Bounds check to prevent array overflow
      if (val >= 0 && val <= 2)
        writer->printf(" %8s", alertname[val]);
      else if (val < 0)
        writer->printf(" %8s", "UNKNOWN");
      else
        writer->printf(" %8s", "INVALID");
      }
    writer->puts(StandardMetrics.ms_v_tpms_alert->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (smarteq->mt_tpms_low_batt->IsDefined())
    {
    writer->printf("Battery         : ");
    for (auto val : smarteq->mt_tpms_low_batt->AsVector())
      {
      // Bounds check to prevent array overflow
      if (val >= 0 && val <= 2)
        writer->printf(" %8s", alertname[val]);
      else if (val < 0)
        writer->printf(" %8s", "UNKNOWN");
      else
        writer->printf(" %8s", "INVALID");
      }
    writer->puts(smarteq->mt_tpms_low_batt->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (smarteq->mt_tpms_missing_tx->IsDefined())
    {
    writer->printf("Missing Tx      : ");
    for (auto val : smarteq->mt_tpms_missing_tx->AsVector())
      {
      // Bounds check to prevent array overflow
      if (val >= 0 && val <= 2)
        writer->printf(" %8s", alertname[val]);
      else if (val < 0)
        writer->printf(" %8s", "UNKNOWN");
      else
        writer->printf(" %8s", "INVALID");
      }
    writer->puts(smarteq->mt_tpms_missing_tx->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (!smarteq->m_tpms_index.empty())
    {
    writer->printf("Sensor mapping  : ");
    for (auto val : smarteq->m_tpms_index)
      {
      if (val >= 0)
        {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", val);
        writer->printf(" %8s", buf);
        }
      else
        {
        writer->printf(" %8s", "-");
        }
      }
    writer->puts("");
    data_shown = true;
    }

  if (StandardMetrics.ms_v_tpms_pressure->IsDefined())
    {
    metric_unit_t user_pressure = OvmsMetricGetUserUnit(GrpPressure, kPa);
    writer->printf("Pressure...[%s]: ", OvmsMetricUnitLabel(user_pressure));
    for (auto val : StandardMetrics.ms_v_tpms_pressure->AsVector(user_pressure))
      {
      // Validate pressure (typical range 0-500 kPa)
      if (!std::isnan(val) && val >= 0.0f && val <= 500.0f)
        writer->printf(" %8.1f", val);
      else
        writer->printf(" %8s", "-");
      }
    writer->puts(StandardMetrics.ms_v_tpms_pressure->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (StandardMetrics.ms_v_tpms_temp->IsDefined())
    {
    metric_unit_t user_temp = OvmsMetricGetUserUnit(GrpTemp, Celcius);
    writer->printf("Temperature.[%s]: ", OvmsMetricUnitLabel(user_temp));
    for (auto val : StandardMetrics.ms_v_tpms_temp->AsVector(user_temp))
      {
      // Validate temperature (typical range -40 to +150 °C)
      if (!std::isnan(val) && val >= -50.0f && val <= 200.0f)
        writer->printf(" %8.1f", val);
      else
        writer->printf(" %8s", "-");
      }
    writer->puts(StandardMetrics.ms_v_tpms_temp->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (!data_shown)
    writer->puts("Sorry, no data available. Try switching the vehicle on.");

  writer->puts("");
  }
