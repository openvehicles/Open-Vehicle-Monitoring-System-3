/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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
static const char *TAG = "v-demo";

#include <stdio.h>
#include "vehicle_demo.h"
#include "metrics_standard.h"

OvmsVehicleDemo::OvmsVehicleDemo()
  {
  ESP_LOGI(TAG, "Demonstration vehicle module");

  StandardMetrics.ms_v_type->SetValue("DEMO");
  StandardMetrics.ms_v_vin->SetValue("DEMODEMODEMO");

  StandardMetrics.ms_v_bat_soc->SetValue(50);
  StandardMetrics.ms_v_bat_soh->SetValue(100);
  StandardMetrics.ms_v_bat_cac->SetValue(160);
  StandardMetrics.ms_v_bat_voltage->SetValue(400);
  StandardMetrics.ms_v_bat_current->SetValue(0);
  StandardMetrics.ms_v_bat_power->SetValue(0);
  StandardMetrics.ms_v_bat_energy_used->SetValue(0);
  StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
  StandardMetrics.ms_v_bat_range_full->SetValue(400);
  StandardMetrics.ms_v_bat_range_ideal->SetValue(200);
  StandardMetrics.ms_v_bat_range_est->SetValue(160);
  StandardMetrics.ms_v_bat_12v_voltage->SetValue(12);
  StandardMetrics.ms_v_bat_12v_current->SetValue(0);
  StandardMetrics.ms_v_bat_temp->SetValue(25);
  StandardMetrics.ms_v_charge_voltage->SetValue(0);
  StandardMetrics.ms_v_charge_current->SetValue(0);
  StandardMetrics.ms_v_charge_climit->SetValue(0);
  StandardMetrics.ms_v_charge_kwh->SetValue(0);
  StandardMetrics.ms_v_charge_mode->SetValue("standard");
  StandardMetrics.ms_v_charge_timermode->SetValue(false);
  StandardMetrics.ms_v_charge_timerstart->SetValue(0);
  StandardMetrics.ms_v_charge_state->SetValue("done");
  StandardMetrics.ms_v_charge_substate->SetValue("stopped");
  StandardMetrics.ms_v_charge_type->SetValue("type2");
  StandardMetrics.ms_v_charge_pilot->SetValue(false);
  StandardMetrics.ms_v_charge_inprogress->SetValue(false);
  StandardMetrics.ms_v_charge_limit_range->SetValue(0);
  StandardMetrics.ms_v_charge_limit_soc->SetValue(0);
  StandardMetrics.ms_v_charge_duration_full->SetValue(0);
  StandardMetrics.ms_v_charge_duration_range->SetValue(0);
  StandardMetrics.ms_v_charge_duration_soc->SetValue(0);
  StandardMetrics.ms_v_charge_temp->SetValue(22);
  StandardMetrics.ms_v_inv_temp->SetValue(22);
  StandardMetrics.ms_v_mot_rpm->SetValue(0);
  StandardMetrics.ms_v_mot_temp->SetValue(30);
  StandardMetrics.ms_v_door_fl->SetValue(false);
  StandardMetrics.ms_v_door_fr->SetValue(false);
  StandardMetrics.ms_v_door_rl->SetValue(false);
  StandardMetrics.ms_v_door_rr->SetValue(false);
  StandardMetrics.ms_v_door_chargeport->SetValue(false);
  StandardMetrics.ms_v_door_hood->SetValue(false);
  StandardMetrics.ms_v_door_trunk->SetValue(false);
  StandardMetrics.ms_v_env_drivemode->SetValue(0);
  StandardMetrics.ms_v_env_handbrake->SetValue(false);
  StandardMetrics.ms_v_env_awake->SetValue(false);
  StandardMetrics.ms_v_env_charging12v->SetValue(false);
  StandardMetrics.ms_v_env_cooling->SetValue(false);
  StandardMetrics.ms_v_env_heating->SetValue(false);
  StandardMetrics.ms_v_env_hvac->SetValue(false);
  StandardMetrics.ms_v_env_on->SetValue(false);
  StandardMetrics.ms_v_env_locked->SetValue(false);
  StandardMetrics.ms_v_env_valet->SetValue(false);
  StandardMetrics.ms_v_env_headlights->SetValue(false);
  StandardMetrics.ms_v_env_alarm->SetValue(false);
  StandardMetrics.ms_v_env_ctrl_login->SetValue(false);
  StandardMetrics.ms_v_env_ctrl_config->SetValue(false);
  StandardMetrics.ms_v_env_temp->SetValue(22);
  StandardMetrics.ms_v_pos_gpslock->SetValue(true);
  StandardMetrics.ms_v_pos_satcount->SetValue(12);
  StandardMetrics.ms_v_pos_latitude->SetValue(22.280868);
  StandardMetrics.ms_v_pos_longitude->SetValue(114.160598);
  StandardMetrics.ms_v_pos_direction->SetValue(10);
  StandardMetrics.ms_v_pos_altitude->SetValue(30);
  StandardMetrics.ms_v_pos_speed->SetValue(0);
  StandardMetrics.ms_v_pos_odometer->SetValue(100000);
  StandardMetrics.ms_v_pos_trip->SetValue(0);
  StandardMetrics.ms_v_tpms_fl_t->SetValue(33);
  StandardMetrics.ms_v_tpms_fr_t->SetValue(33);
  StandardMetrics.ms_v_tpms_rr_t->SetValue(34);
  StandardMetrics.ms_v_tpms_rl_t->SetValue(34);
  StandardMetrics.ms_v_tpms_fl_p->SetValue(206.843);
  StandardMetrics.ms_v_tpms_fr_p->SetValue(206.843);
  StandardMetrics.ms_v_tpms_rr_p->SetValue(275.79);
  StandardMetrics.ms_v_tpms_rl_p->SetValue(275.79);
  }

OvmsVehicleDemo::~OvmsVehicleDemo()
  {
  ESP_LOGI(TAG, "Shutdown demonstration vehicle module");
  }

void OvmsVehicleDemo::Ticker1(uint32_t ticker)
  {
  OvmsVehicle::Ticker1(ticker);

  if (StandardMetrics.ms_v_env_on->AsBool())
    {
    // We are driving
    int speed = StandardMetrics.ms_v_pos_speed->AsInt() + (rand()%3) -1;
    if (speed<0) speed = 0;
    else if (speed>100) speed = 100;
    StandardMetrics.ms_v_pos_speed->SetValue(speed);
    StandardMetrics.ms_v_mot_rpm->SetValue(speed*112);
    }
  }

void OvmsVehicleDemo::Ticker10(uint32_t ticker)
  {
  OvmsVehicle::Ticker10(ticker);

  if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    {
    // We are charging
    float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
    if (soc < 100)
      {
      soc += 0.1;
      StandardMetrics.ms_v_bat_soc->SetValue(soc);
      }
    else
      {
      // Car full
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_door_chargeport->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
      StandardMetrics.ms_v_charge_voltage->SetValue(0);
      StandardMetrics.ms_v_charge_current->SetValue(0);
      }
    }
  else if (StandardMetrics.ms_v_env_on->AsBool())
    {
    // We are driving
    float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
    if (soc > 0)
      {
      int speed = StandardMetrics.ms_v_pos_speed->AsInt() + (rand()%3) -1;
      if (speed<0) speed = 0;
      else if (speed>100) speed = 100;
      soc -= 0.1;
      StandardMetrics.ms_v_bat_soc->SetValue(soc);
      StandardMetrics.ms_v_pos_speed->SetValue(speed);
      StandardMetrics.ms_v_mot_rpm->SetValue(speed*112);
      }
    else
      {
      // Battery is empty. Charge the car...
      StandardMetrics.ms_v_pos_speed->SetValue(0);
      StandardMetrics.ms_v_mot_rpm->SetValue(0);
      StandardMetrics.ms_v_env_on->SetValue(false);
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_door_chargeport->SetValue(true);
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      StandardMetrics.ms_v_charge_voltage->SetValue(220);
      StandardMetrics.ms_v_charge_current->SetValue(32);
      }
    }
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandSetChargeMode(vehicle_mode_t mode)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandSetChargeCurrent(uint16_t limit)
  {
  StandardMetrics.ms_v_charge_climit->SetValue(limit);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandStartCharge()
  {
  StandardMetrics.ms_v_pos_speed->SetValue(0);
  StandardMetrics.ms_v_mot_rpm->SetValue(0);
  StandardMetrics.ms_v_env_on->SetValue(false);
  StandardMetrics.ms_v_charge_inprogress->SetValue(true);
  StandardMetrics.ms_v_door_chargeport->SetValue(true);
  StandardMetrics.ms_v_charge_state->SetValue("charging");
  StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
  StandardMetrics.ms_v_charge_pilot->SetValue(true);
  StandardMetrics.ms_v_charge_voltage->SetValue(220);
  StandardMetrics.ms_v_charge_current->SetValue(32);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandStopCharge()
  {
  StandardMetrics.ms_v_charge_inprogress->SetValue(false);
  StandardMetrics.ms_v_door_chargeport->SetValue(false);
  StandardMetrics.ms_v_charge_state->SetValue("done");
  StandardMetrics.ms_v_charge_substate->SetValue("stopped");
  StandardMetrics.ms_v_charge_pilot->SetValue(false);
  StandardMetrics.ms_v_charge_voltage->SetValue(0);
  StandardMetrics.ms_v_charge_current->SetValue(0);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandSetChargeTimer(bool timeron, uint16_t timerstart)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandCooldown(bool cooldownon)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandWakeup()
  {
  StandardMetrics.ms_v_charge_inprogress->SetValue(false);
  StandardMetrics.ms_v_door_chargeport->SetValue(false);
  StandardMetrics.ms_v_charge_state->SetValue("done");
  StandardMetrics.ms_v_charge_substate->SetValue("stopped");
  StandardMetrics.ms_v_charge_pilot->SetValue(false);
  StandardMetrics.ms_v_charge_voltage->SetValue(0);
  StandardMetrics.ms_v_charge_current->SetValue(0);
  StandardMetrics.ms_v_env_on->SetValue(true);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandLock(const char* pin)
  {
  StandardMetrics.ms_v_env_locked->SetValue(true);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandUnlock(const char* pin)
  {
  StandardMetrics.ms_v_env_locked->SetValue(false);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandActivateValet(const char* pin)
  {
  StandardMetrics.ms_v_env_valet->SetValue(true);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandDeactivateValet(const char* pin)
  {
  StandardMetrics.ms_v_env_valet->SetValue(false);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleDemo::CommandHomelink(int button, int durationms)
  {
  return NotImplemented;
  }

class OvmsVehicleDemoInit
  {
  public: OvmsVehicleDemoInit();
} MyOvmsVehicleDemoInit  __attribute__ ((init_priority (9000)));

OvmsVehicleDemoInit::OvmsVehicleDemoInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: DEMO (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleDemo>("DEMO","Demonstration Vehicle");
  }
