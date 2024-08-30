/*
;    Project:       Open Vehicle Monitor System
;    Date:          23rd August 2023
;
;    Changes:
;    1.0  Initial release
;    1.1  Adding Min Cell Temperature, AC volts and AC amp metrics
;
;    (C) 2024       Jamie Jones
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
static const char *TAG = "v-zombie-vcu";

#include <stdio.h>
#include "vehicle_zombie_vcu.h"

//Polstate 0 asleep, 1 awake
static const OvmsPoller::poll_pid_t zombie_polls[]
  =
  {
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT , 0x05, {  0, 5, 0 }, 0, ISOTP_STD }, // motor temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0c, {  0, 1, 0 }, 0, ISOTP_STD }, // Engine RPM
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x07DF, { 0, 15, 10 }, 0, ISOTP_STD }, // SoC
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x07D6, { 0, 30, 60 }, 0, ISOTP_STD }, // udc
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x07DC, { 0, 1, 10 }, 0, ISOTP_STD }, // idc
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x07D2, { 1, 1, 1 }, 0, ISOTP_STD }, // opmode
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x07EC, { 0, 5, 0 }, 0, ISOTP_STD }, // inverter
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x0827, { 0, 30, 30 }, 0, ISOTP_STD }, // battery cell max temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x0826, { 0, 30, 30 }, 0, ISOTP_STD }, // battery cell min temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x0824, { 0, 30, 30 }, 0, ISOTP_STD }, // battery cell min mV
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x0825, { 0, 30, 30}, 0, ISOTP_STD }, // battery cell max mV
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x81E, { 0, 30, 30 }, 0, ISOTP_STD }, // charger temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x81F, { 0, 0, 30}, 0, ISOTP_STD }, // AC_VOLTS
    { 0x7df, 0, VEHICLE_POLL_TYPE_ZOMBIE_VCU_16, 0x829, { 0, 0, 10}, 0, ISOTP_STD }, // AC_AMPS

    POLL_LIST_END
  };
int16_t current = 0;

OvmsVehicleZombieVcu::OvmsVehicleZombieVcu()
{
  ESP_LOGI(TAG, "ZombieVerter VCU vehicle module");

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  PollSetPidList(m_can1, zombie_polls);
  //state 0 off, state 1 drive, state 2 charging
  PollSetState(0);
}

OvmsVehicleZombieVcu::~OvmsVehicleZombieVcu()
{
  ESP_LOGI(TAG, "Shutdown ZombieVerter VCU vehicle module");
}

void OvmsVehicleZombieVcu::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
  {
  int value1 = (int)data[0];

  switch (job.pid)
  {

    case 0x05:  // Motor temperature
      StandardMetrics.ms_v_mot_temp->SetValue(value1 - 0x28);
      break;
    case 0x0c:  // Engine RPM
      //TODO
      break;
    case 0x07DF: //Soc
    {
      int32_t soc = ((int32_t)data[2] << 8) + (int)data[3];
      StdMetrics.ms_v_bat_soc->SetValue(soc / 32);
      break;
    }
    case 0x07D6: //udc
    {
      int32_t udc = ((int32_t)data[2] << 8) + (int)data[3];
      StdMetrics.ms_v_bat_voltage->SetValue(udc / 32, Volts);
      break;
    }
    case 0x07DC: //idc
    {
      int32_t idc = ((int32_t)data[0] << 24) + (  (int32_t)data[1] << 16) + ((int32_t)data[2] << 8) + (int32_t)data[3];
      current = idc / 32;

      //current in OVMS is discharge positive, charge negative. Flip
      current = current * -1;
      StdMetrics.ms_v_bat_current->SetValue(current, Amps);
      break;
    }
    case 0x07D2: //opmode
    {
      int opmode = data[3] / 32;

      if (opmode == 0) { //off
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_env_drivemode->SetValue(false);
        StandardMetrics.ms_v_door_chargeport->SetValue(false);
        StdMetrics.ms_v_charge_type->SetValue("");
        StdMetrics.ms_v_charge_state->SetValue("");
        StdMetrics.ms_v_env_on->SetValue(false);
        StandardMetrics.ms_v_charge_current->SetValue(0, Amps);
        StandardMetrics.ms_v_charge_voltage->SetValue(0, Volts);

        PollSetState(0);
        
      } else if (opmode == 1) { // running
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_env_drivemode->SetValue(true);
        StandardMetrics.ms_v_door_chargeport->SetValue(false);
        StdMetrics.ms_v_charge_type->SetValue("");
        StdMetrics.ms_v_charge_state->SetValue("");
        StdMetrics.ms_v_env_on->SetValue(true);
        StandardMetrics.ms_v_charge_current->SetValue(0, Amps);
        StandardMetrics.ms_v_charge_voltage->SetValue(0, Volts);
        PollSetState(1);

      } else if (opmode == 3) { // precharge
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_env_drivemode->SetValue(false);
        StandardMetrics.ms_v_door_chargeport->SetValue(false);
        StdMetrics.ms_v_charge_type->SetValue("");
        StdMetrics.ms_v_charge_state->SetValue("");
        StandardMetrics.ms_v_charge_current->SetValue(0, Amps);
        StandardMetrics.ms_v_charge_voltage->SetValue(0, Volts);


        StdMetrics.ms_v_env_on->SetValue(false);
        PollSetState(1);

      } else if (opmode == 4) { // charge
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_env_drivemode->SetValue(false);
        StandardMetrics.ms_v_charge_type->SetValue("Standard");
        StandardMetrics.ms_v_charge_mode->SetValue("Type2");
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        StandardMetrics.ms_v_door_chargeport->SetValue(true);
        StdMetrics.ms_v_env_on->SetValue(false);
        PollSetState(2);
      }

      break;
    }
    case 0x07EC: //inverter
    {
      int32_t temperature = ((int32_t)data[2] << 8) + (int)data[3];
      StdMetrics.ms_v_inv_temp->SetValue(temperature / 32, Volts);
      break;
    }
    case 0x0827: //battery Tmax
    {
      int32_t temperature = ((int32_t)data[2] << 8) + (int)data[3];
      StdMetrics.ms_v_bat_temp->SetValue(temperature / 32, Celcius);
      StandardMetrics.ms_v_bat_pack_tmax->SetValue(temperature/32);
      break;
    }
    case 0x0826: //battery Tmin
    {
      int32_t temperature = ((int32_t)data[2] << 8) + (int)data[3];
      StandardMetrics.ms_v_bat_pack_tmin->SetValue(temperature/32);

      break;
    }
    case 0x0824: //battery Vmin
    {
      int32_t milliVolt = ((int32_t)data[1] << 16) + ((int32_t)data[2] << 8) + (int)data[3];
      milliVolt = milliVolt / 32;
      float volts = (float)milliVolt / 1000;

      StandardMetrics.ms_v_bat_pack_vmin->SetValue(volts, Volts);
      break;
    }
    case 0x0825: //battery Vmax
    {
      int32_t milliVolt = ((int32_t)data[1] << 16) + ((int32_t)data[2] << 8) + (int)data[3];
      milliVolt = milliVolt / 32;
      float volts = (float)milliVolt / 1000;

      StandardMetrics.ms_v_bat_pack_vmax->SetValue(volts, Volts);
      break;
    }
    case 0x81E: //charger temp
    {
      int32_t temperature = ((int32_t)data[2] << 8) + (int)data[3];
      StandardMetrics.ms_v_charge_temp->SetValue(temperature/32);

      break;
    }
    case 0x81F: //AC_VOLTS
    {
      int32_t volts = ((int32_t)data[0] << 24) + (  (int32_t)data[1] << 16) + ((int32_t)data[2] << 8) + (int32_t)data[3];
      StandardMetrics.ms_v_charge_voltage->SetValue(volts/32, Volts);

      break;
    }
    case 0x829: //AC_AMPS
    {
       int32_t amps = ((int32_t)data[0] << 24) + (  (int32_t)data[1] << 16) + ((int32_t)data[2] << 8) + (int32_t)data[3];
      StandardMetrics.ms_v_charge_current->SetValue(amps/32, Amps);

      break;
    }
  }    
}

class OvmsVehicleZombieVcuInit
{
  public: OvmsVehicleZombieVcuInit();
} MyOvmsVehicleZombieVcuInit  __attribute__ ((init_priority (9000)));

OvmsVehicleZombieVcuInit::OvmsVehicleZombieVcuInit()
{
  ESP_LOGI(TAG, "Registering Vehicle: ZombieVerter VCU (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleZombieVcu>("ZOM","ZOMBIE VCU");
}
