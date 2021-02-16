/*
;    Project:       Open Vehicle Monitor System
;    Date:          11th Sep 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2018       Marcos Mezo
;    (C) 2019       Thomas Heuer @Dimitrie78
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
static const char *TAG = "v-zoe";

#include <stdio.h>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_peripherals.h"

#include "vehicle_renaultzoe.h"

void OvmsVehicleRenaultZoe::zoe_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleRenaultZoe* zoe = GetInstance(writer);
  if (!zoe)
    return;
	
  zoe->CommandTrip(verbosity, writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandClimateControl(bool climatecontrolon) {
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandWakeup() {
  if(!m_enable_write)
    return Fail;
  
  ESP_LOGI(TAG, "Send Wakeup Command");
  
  uint8_t data[8] = {0xf1, 0x04, 0x1f, 0xc5, 0x35, 0xfe, 0x65, 0x08};
  
  canbus *obd;
  obd = m_can1;
  
  obd->WriteStandard(0x35C, 8, data);
  data[1] = 0x06;
  data[7] = 0x61;
  vTaskDelay(50 / portTICK_PERIOD_MS);
  obd->WriteStandard(0x35C, 8, data);

  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandLock(const char* pin) {
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandUnlock(const char* pin) {
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandActivateValet(const char* pin) {
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandDeactivateValet(const char* pin) {
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandHomelink(int button, int durationms) {
#ifdef CONFIG_OVMS_COMP_MAX7317  
  if(m_enable_egpio) {
    if (button == 0) {
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_3, 0);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_3, 1);
      return Success;
    }
    if (button == 1) {
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_4, 0);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_4, 1);
      return Success;
    }
    if (button == 2) {
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_5, 0);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_5, 1);
      return Success;
    }
  }
#endif
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandTrip(int verbosity, OvmsWriter* writer) {
	metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
	
	writer->printf("Driven: %s\n", (char*) StdMetrics.ms_v_pos_trip->AsUnitString("-", rangeUnit, 1).c_str());
	writer->printf("Energy used: %s\n", (char*) StdMetrics.ms_v_bat_energy_used->AsUnitString("-", Native, 3).c_str());
	writer->printf("Energy recd: %s\n", (char*) StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", Native, 3).c_str());
	writer->printf("Energy Available: %s\n", (char*) mt_available_energy->AsUnitString("-", Native, 1).c_str());
	
	return Success;
}

void OvmsVehicleRenaultZoe::NotifyTrip() {
  StringWriter buf(200);
  CommandTrip(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xrz.trip",buf.c_str());
}

OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoe::CommandStat(int verbosity, OvmsWriter* writer) {
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  if (chargeport_open)
    {
    std::string charge_mode = StdMetrics.ms_v_charge_mode->AsString();
    std::string charge_state = StdMetrics.ms_v_charge_state->AsString();
    bool show_details = !(charge_state == "done" || charge_state == "stopped");

    // Translate mode codes:
    if (charge_mode == "standard")
      charge_mode = "Standard";
    else if (charge_mode == "storage")
      charge_mode = "Storage";
    else if (charge_mode == "range")
      charge_mode = "Range";
    else if (charge_mode == "performance")
      charge_mode = "Performance";

    // Translate state codes:
    if (charge_state == "charging")
      charge_state = "Charging";
    else if (charge_state == "topoff")
      charge_state = "Topping off";
    else if (charge_state == "done")
      charge_state = "Charge Done";
    else if (charge_state == "preparing")
      charge_state = "Preparing";
    else if (charge_state == "heating")
      charge_state = "Charging, Heating";
    else if (charge_state == "stopped")
      charge_state = "Charge Stopped";

    writer->printf("%s - %s\n", charge_mode.c_str(), charge_state.c_str());

    if (show_details)
      {
      writer->printf("%s/%s\n",
        (char*) StdMetrics.ms_v_charge_voltage->AsUnitString("-", Native, 1).c_str(),
        (char*) StdMetrics.ms_v_charge_current->AsUnitString("-", Native, 1).c_str());

      int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
      if (duration_full > 0)
        writer->printf("Full: %d mins\n", duration_full);

      int duration_soc = StdMetrics.ms_v_charge_duration_soc->AsInt();
      if (duration_soc > 0)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_soc->AsUnitString("SOC", Native, 0).c_str(),
          duration_soc);

      int duration_range = StdMetrics.ms_v_charge_duration_range->AsInt();
      if (duration_range > 0)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_range->AsUnitString("Range", rangeUnit, 0).c_str(),
          duration_range);
      }
    }
  else
    {
    writer->puts("Not charging");
    }

  writer->printf("SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());

  const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_ideal != '-')
    writer->printf("Ideal range: %s\n", range_ideal);

  const char* range_est = StdMetrics.ms_v_bat_range_est->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_est != '-')
    writer->printf("Est. range: %s\n", range_est);

  const char* chargedkwh = StdMetrics.ms_v_charge_kwh->AsUnitString("-", Native, 3).c_str();
  if (*chargedkwh != '-')
    writer->printf("Energy charged: %s\n", chargedkwh);

  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf("ODO: %s\n", odometer);

  const char* cac = StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str();
  if (*cac != '-')
    writer->printf("CAC: %s\n", cac);

  const char* soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 1).c_str();
  if (*soh != '-')
    writer->printf("SOH: %s\n", soh);
  
  const char* avai_energy = mt_available_energy->AsUnitString("-", Native, 1).c_str();
  if (*avai_energy != '-')
    writer->printf("Energy Available: %s\n", avai_energy);
  
  return Success;
}
