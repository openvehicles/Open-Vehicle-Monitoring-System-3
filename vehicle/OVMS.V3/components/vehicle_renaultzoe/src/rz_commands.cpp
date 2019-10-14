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
  
  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x218;
  frame.data.u8[0] = 0xc9;
  m_can1->Write(&frame);

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
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_3, 1);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_3, 0);
      return Success;
    }
    if (button == 1) {
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_4, 1);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_4, 0);
      return Success;
    }
    if (button == 2) {
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_5, 1);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      MyPeripherals->m_max7317->Output(MAX7317_EGPIO_5, 0);
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
	
	return Success;
}

void OvmsVehicleRenaultZoe::NotifyTrip() {
  StringWriter buf(200);
  CommandTrip(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xrz.trip",buf.c_str());
}
