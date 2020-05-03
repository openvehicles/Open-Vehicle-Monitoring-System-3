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
 ; http://ed.no-limit.de/wiki/index.php/Hauptseite
 */

#include "ovms_log.h"
static const char *TAG = "v-smarted";

#include <stdio.h>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_events.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_peripherals.h"

#include "vehicle_smarted.h"


/**
 * Set Recu wippen.
 */
void OvmsVehicleSmartED::xse_recu(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartED* smart = GetInstance(writer);
  if (!smart)
    return;
  
  bool on;
	if (strcmp("up", argv[0]) == 0) {
    on = true;
  }
  else if (strcmp("down", argv[0]) == 0) {
    on = false;
  }
  else {
    writer->puts("Error: argument must be 'up' or 'down'");
    return;
  }
  bool status = smart->CommandSetRecu( on );
  if (status)
    writer->printf("Recu set to: %s\n", (on == true ? "up" : "down"));
  else
    writer->puts("Error: Function need CAN-Write enable");
}

/**
 * Set ChargeTimer.
 */
void OvmsVehicleSmartED::xse_chargetimer(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartED* smart = GetInstance(writer);
  if (!smart)
    return;
  
  bool enable = false;
  int hours    = strtol(argv[0], NULL, 10);
  int minutes  = strtol(argv[1], NULL, 10);
  if (hours < 0 || hours > 23) {
    writer->puts("ERROR: Hour invalid (0-23)");
    return;
  }
  if (minutes < 0 || minutes > 59) {
    writer->puts("ERROR: Minute invalid (0-59)");
    return;
  }
  if (strcmp("on", argv[2]) == 0) {
    enable = true;
  }
  else if (strcmp("off", argv[2]) == 0) {
    enable = false;
  }
  else {
    writer->puts("Error: last argument must be 'on' or 'off'");
    return;
  }
  if( smart->CommandSetChargeTimer(enable, hours, minutes) == OvmsVehicle::Success ) {
    writer->printf("Charging Time set to: %d:%d\n", hours, minutes);
  }
  else {
    writer->puts("Error: Function need CAN-Write enable");
  }
}

/**
 * Trip values.
 */
void OvmsVehicleSmartED::xse_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartED* smart = GetInstance(writer);
  if (!smart)
    return;
	
  smart->CommandTrip(verbosity, writer);
}

/**
 * Show BMS diag values.
 */
void OvmsVehicleSmartED::xse_bmsdiag(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartED* smart = GetInstance(writer);
  if (!smart)
    return;
	
  smart->BmsDiag(verbosity, writer);
}

/**
 * Show BMS diag values.
 */
void OvmsVehicleSmartED::xse_RPTdata(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartED* smart = GetInstance(writer);
  if (!smart)
    return;
	
  smart->printRPTdata(verbosity, writer);
}

void OvmsVehicleSmartED::TempPoll() {
  vTaskDelay(300 / portTICK_PERIOD_MS);
  uint8_t data[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  canbus *obd;
  obd = m_can1;
  
  data[0] = 0x02;
  data[1] = 0x10;
  data[2] = 0x92;
  obd->WriteStandard(0x7A2, 8, data);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  
  data[0] = 0x02;
  data[1] = 0xe3;
  data[2] = 0x01;
  obd->WriteStandard(0x7A2, 8, data);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  
  data[0] = 0x02;
  data[1] = 0x21;
  data[2] = 0x12;
  obd->WriteStandard(0x7A2, 8, data);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  
  data[0] = 0x02;
  data[1] = 0x21;
  data[2] = 0x12;
  obd->WriteStandard(0x7A2, 8, data);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  
  data[0] = 0x02;
  data[1] = 0xe3;
  data[2] = 0x01;
  obd->WriteStandard(0x7A2, 8, data);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  
  data[0] = 0x02;
  data[1] = 0x10;
  data[2] = 0x81;
  obd->WriteStandard(0x7A2, 8, data);
}

bool OvmsVehicleSmartED::CommandSetRecu(bool on) {
  if(!m_enable_write)
    return false;
  
  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x236;
  frame.data.u8[0] = (on == true ? 0x02 : 0x04);
  frame.Write();

  return true;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandSetChargeCurrent(uint16_t limit) {
  /*The charging current changes with
   0x512 00 00 1F FF 00 7C 00 00 auf 12A.
   8A = 0x74, 12A = 0x7C and 32A = 0xA4 (as max. at 22kW).
   So calculate an offset 0xA4 = d164: e.g. 12A = 0xA4 - 0x7C = 0x28 = d40 -> subtract from the maximum (0xA4 = 32A)
    then divide by two to get the value for 20A (with 0.5A resolution).
  */
  if(!m_enable_write)
    return Fail;
  
  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x512;
  frame.data.u8[0] = 0x00;
  frame.data.u8[1] = 0x00;
  frame.data.u8[2] = 0x1F;
  frame.data.u8[3] = 0xFF;
  frame.data.u8[4] = 0x00;
  frame.data.u8[5] = (uint8_t) (100 + 2 * limit);
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandWakeup() {
  /*So we still have to get the car to wake up. Can someone please test these two queries when the car is asleep:
  0x218 00 00 00 00 00 00 00 00 or
  0x210 00 00 00 01 00 00 00 00
  Both patterns should comply with the Bosch CAN bus spec. for waking up a sleeping bus with recessive bits.
  0x423 01 00 00 00 will wake up the CAN bus but not the right on.
  */
  if(!m_enable_write)
    return Fail;
  
  ESP_LOGI(TAG, "Send Wakeup Command");
  
  if(!mt_bus_awake->AsBool()) {
    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can2;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 4;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x210;
    frame.callback = NULL;
    frame.data.u8[0] = 0x01;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = 0x00;
    frame.data.u8[3] = 0x00;
    m_can2->Write(&frame);
  }
  TempPoll();

  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandClimateControl(bool enable) {
  time_t rawtime;
  time ( &rawtime );
  rawtime += m_preclima_time*60; // add 15 minutes
  struct tm* tmu = localtime(&rawtime);
  int hours = tmu->tm_hour;
  int minutes = tmu->tm_min;
  minutes = (minutes - (minutes % 5));
  return CommandSetChargeTimer(enable, hours, minutes);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandSetChargeTimer(bool timeron, int hours, int minutes) {
#ifdef CONFIG_OVMS_COMP_MAX7317
  //Set the charge start time as seconds since midnight or 8 Bit hour and 8 bit minutes?
  /*With
   0x512 00 00 12 1E 00 00 00 00
   if one sets e.g. the time at 18:30. If you now mask byte 3 (0x12) with 0x40 (and set the second bit to high there), the A / C function is also activated.
  */
  if(!m_enable_write)
    return Fail;
  
  // Try first Wakeup can if car sleep
  if(!mt_bus_awake->AsBool()) {
    CommandWakeup();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  // Try unlock/lock doors to Wakeup Can
  if(!mt_bus_awake->AsBool()) {
    if (!MyConfig.IsDefined("password","pin")) return Fail;
    
    std::string vpin = MyConfig.GetParamValue("password","pin");
    CommandUnlock(vpin.c_str());
    vTaskDelay(600 / portTICK_PERIOD_MS);
    CommandLock(vpin.c_str());
    vTaskDelay(600 / portTICK_PERIOD_MS);
    if(!mt_bus_awake->AsBool()) return Fail;
  }
  
  ESP_LOGI(TAG,"ClimaStartTime: %d:%d", hours, minutes);
  
  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x512;
  frame.data.u8[0] = 0x00;
  frame.data.u8[1] = 0x00;
  frame.data.u8[2] = ((uint8_t) hours & 0xff);
  if (timeron) {
      frame.data.u8[3] = ((uint8_t) minutes & 0xff) + 0x40; //(timerstart >> 8) & 0xff;
  } else {
      frame.data.u8[3] = ((uint8_t) minutes & 0xff); //((timerstart >> 8) & 0xff) + 0x40;
  }
  frame.data.u8[4] = 0x00;
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can1->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can1->Write(&frame);
  
  ESP_LOGI(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", frame.MsgID, frame.data.u8[0], frame.data.u8[1], frame.data.u8[2], frame.data.u8[3], frame.data.u8[4], frame.data.u8[5], frame.data.u8[6], frame.data.u8[7]);
  return Success;
#endif
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandHomelink(int button, int durationms) {
  bool enable;
  time_t rawtime;
  time ( &rawtime );
  rawtime += m_preclima_time*60; // add 15 minutes
  struct tm* tmu = localtime(&rawtime);
  int hours = tmu->tm_hour;
  int minutes = tmu->tm_min;
  minutes = (minutes - (minutes % 5));
  if (button == 0) {
      enable = true;
      return CommandSetChargeTimer(enable, hours, minutes);
  }
  if (button == 1) {
      enable = false;
      return CommandSetChargeTimer(enable, hours, minutes);
  }
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandLock(const char* pin) {
#ifdef CONFIG_OVMS_COMP_MAX7317
  //switch 12v to GEP 1
  MyPeripherals->m_max7317->Output(m_doorlock_port, (m_gpio_highlow ? 0 : 1));
  vTaskDelay(500 / portTICK_PERIOD_MS);
  MyPeripherals->m_max7317->Output(m_doorlock_port, (m_gpio_highlow ? 1 : 0));
  StandardMetrics.ms_v_env_locked->SetValue(true);
  return Success;
#endif
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandUnlock(const char* pin) {
#ifdef CONFIG_OVMS_COMP_MAX7317
  //switch 12v to GEP 2 
  MyPeripherals->m_max7317->Output(m_doorunlock_port, (m_gpio_highlow ? 0 : 1));
  vTaskDelay(500 / portTICK_PERIOD_MS);
  MyPeripherals->m_max7317->Output(m_doorunlock_port, (m_gpio_highlow ? 1 : 0));
  StandardMetrics.ms_v_env_locked->SetValue(false);
  return Success;
#endif
  return NotImplemented;  
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandActivateValet(const char* pin) {
  if (StandardMetrics.ms_v_bat_soc->AsFloat() > 20) {
#ifdef CONFIG_OVMS_COMP_MAX7317
    ESP_LOGI(TAG,"Ignition EGPIO on port: %d", m_ignition_port);
    MyPeripherals->m_max7317->Output(m_ignition_port, 1);
    m_egpio_timer = m_egpio_timout;
    StandardMetrics.ms_v_env_valet->SetValue(true);
    MyNotify.NotifyString("info", "valet.enabled", "Ignition on");
    return Success;
#endif
  } else {
    MyNotify.NotifyString("info", "valet.enabled", "Soc below 20%, can't activate Ignition");
    return Fail;
  }
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandDeactivateValet(const char* pin) {
#ifdef CONFIG_OVMS_COMP_MAX7317
  ESP_LOGI(TAG,"Ignition EGPIO off port: %d", m_ignition_port);
  MyPeripherals->m_max7317->Output(m_ignition_port, 0);
  m_egpio_timer = 0;
  StandardMetrics.ms_v_env_valet->SetValue(false);
  MyNotify.NotifyString("info", "valet.disabled", "Ignition off");
  return Success;
#endif
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandStat(int verbosity, OvmsWriter* writer) {
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
  writer->printf("realSOC: %s\n", (char*) mt_real_soc->AsUnitString("-", Native, 1).c_str());

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

  const char* days = mt_v_bat_LastMeas_days->AsUnitString("-", Native, 0).c_str();
  if (*days != '-') {
    writer->printf("Last measurement      : %d day(s)\n", mt_v_bat_LastMeas_days->AsInt());
    writer->printf("Measurement estimation: %.3f\n", mt_v_bat_Cap_meas_quality->AsFloat());
    writer->printf("Actual estimation     : %.3f\n", mt_v_bat_Cap_combined_quality->AsFloat());
    writer->printf("CAP mean: %5.0f As/10, %2.1f Ah\n", mt_v_bat_Cap_As_avg->AsFloat(), mt_v_bat_Cap_As_avg->AsFloat()/360.0);
    writer->printf("CAP min : %5.0f As/10, %2.1f Ah\n", mt_v_bat_Cap_As_min->AsFloat(), mt_v_bat_Cap_As_min->AsFloat()/360.0);
    writer->printf("CAP max : %5.0f As/10, %2.1f Ah\n", mt_v_bat_Cap_As_max->AsFloat(), mt_v_bat_Cap_As_max->AsFloat()/360.0);
  }

  const char* soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 1).c_str();
  if (*soh != '-')
    writer->printf("SOH: %s\n", soh);

  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandTrip(int verbosity, OvmsWriter* writer) {
	metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

	writer->printf("Driven: %s\n", (char*) StdMetrics.ms_v_pos_trip->AsUnitString("-", rangeUnit, 1).c_str());
	writer->printf("Energy used: %s\n", (char*) StdMetrics.ms_v_bat_energy_used->AsUnitString("-", Native, 3).c_str());
	writer->printf("Energy recd: %s\n", (char*) StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", Native, 3).c_str());
	writer->printf("SOC: %s, realSOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str(), (char*) mt_real_soc->AsUnitString("-", Native, 1).c_str());
  writer->printf("Acceleration: %s\n", (char*) mt_ed_eco_accel->AsUnitString("-", Percentage, 1).c_str());
  writer->printf("Steady Driving: %s\n", (char*) mt_ed_eco_const->AsUnitString("-", Percentage, 1).c_str());
  writer->printf("Coasting: %s\n", (char*) mt_ed_eco_coast->AsUnitString("-", Percentage, 1).c_str());
  writer->printf("ECO Score: %s\n", (char*) mt_ed_eco_score->AsUnitString("-", Percentage, 1).c_str());

	return Success;
}

void OvmsVehicleSmartED::NotifyTrip() {
  StringWriter buf(200);
  CommandTrip(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","xse.trip",buf.c_str());
}

void OvmsVehicleSmartED::NotifyValetEnabled() {
  // MyNotify.NotifyString("info", "valet.enabled", "Ignition on");
}

void OvmsVehicleSmartED::NotifyValetDisabled() {
  // MyNotify.NotifyString("info", "valet.disabled", "Ignition off");
}
