/*
;    Project:       Open Vehicle Monitor System
;    Date:          1st April 2021
;
;    Changes:
;    0.0.1  Initial release
;
;    (C) 2021       Didier Ernotte
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
static const char *TAG = "v-jaguaripace";

#include <stdio.h>
#include "vehicle_jaguaripace.h"
#include "ipace_poll_states.h"

#include "ipace_obd_pids.h"

static const OvmsVehicle::poll_pid_t obdii_polls[] = {
// BECM PIDs
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoCPid, {  0, 30, 30, 60 }, 0, ISOTP_STD }, 
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHCapacityPid, {  0, 30, 30, 60 }, 0, ISOTP_STD }, 
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHCapacityMinPid, {  0, 30, 30, 60 }, 0, ISOTP_STD }, 
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHCapacityMaxPid, {  0, 30, 30, 60 }, 0, ISOTP_STD }, 
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPowerPid, {  0, 30, 30, 60 }, 0, ISOTP_STD }, 
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPowerMinPid, {  0, 30, 30, 60 }, 0, ISOTP_STD }, 
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPowerMaxPid, {  0, 30, 30, 60 }, 0, ISOTP_STD }, 
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPowerPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCellMinVoltPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryCellMaxVoltPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryVoltPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryTempMinPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryTempMaxPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryTempAvgPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batteryMaxRegenPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, odometerPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cabinTempPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { becmId, becmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vinPid, {  0, 30, 30, 30 }, 0, ISOTP_STD },

// HVAC PIDs
    { hvacId, hvacId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, ambientTempPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },

// BCM/GWM PIDs
    { bcmId, bcmId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vehicleSpeedPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },

// TPMS PIDs
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, frontLeftTirePressurePid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, frontRightTirePressurePid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, rearLeftTirePressurePid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, rearRightTirePressurePid, {  0, 30, 30, 60 }, 0, ISOTP_STD },

    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, frontLeftTireTempPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, frontRightTireTempPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, rearLeftTireTempPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },
    { tpmsId, tpmsId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, rearRightTireTempPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },

// TCU PIDs
    { tcuId, tcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, localizationPid, {  0, 30, 30, 60 }, 0, ISOTP_STD },

    POLL_LIST_END
};

OvmsVehicleJaguarIpace::OvmsVehicleJaguarIpace() {
  ESP_LOGI(TAG, "Jaguar Ipace vehicle module");

  memset(m_vin,0,sizeof(m_vin));

  xji_v_bat_capacity_soh_min = MyMetrics.InitFloat("xji.v.bat.capacity.sohmin", 0, SM_STALE_HIGH, Percentage);
  xji_v_bat_capacity_soh_max = MyMetrics.InitFloat("xji.v.bat.capacity.sohmax", 0, SM_STALE_HIGH, Percentage);
  xji_v_bat_power_soh = MyMetrics.InitFloat("xji.v.bat.power.soh", 0, SM_STALE_HIGH, Percentage);
  xji_v_bat_power_soh_min = MyMetrics.InitFloat("xji.v.bat.power.sohmin", 0, SM_STALE_HIGH, Percentage);
  xji_v_bat_power_soh_max = MyMetrics.InitFloat("xji.v.bat.power.sohmax", 0, SM_STALE_HIGH, Percentage);
  xji_v_bat_max_regen = MyMetrics.InitFloat("xji.v.bat.max.regen", 0, SM_STALE_HIGH, kW);


  // init configs:
  MyConfig.RegisterParam("xji", "Jaguar Ipace", true, true);
  ConfigChanged(NULL);

  // init commands:
  cmd_xrt = MyCommandApp.RegisterCommand("xji", "Jaguar Ipace");

  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  PollSetPidList(m_can1, obdii_polls);
  ESP_LOGD(TAG, "OvmsVehicleJaguarIpace, PollSetPidList");

  PollSetState(PollStateRunning);
  ESP_LOGD(TAG, "OvmsVehicleJaguarIpace, PollSetState(0)");
}

OvmsVehicleJaguarIpace::~OvmsVehicleJaguarIpace() {
  ESP_LOGI(TAG, "Shutdown Jaguar Ipace vehicle module");
}

class OvmsVehicleJaguarIpaceInit {
  public: OvmsVehicleJaguarIpaceInit();
} MyOvmsVehicleJaguarIpaceInit  __attribute__ ((init_priority (9000)));

OvmsVehicleJaguarIpaceInit::OvmsVehicleJaguarIpaceInit() {
  ESP_LOGI(TAG, "Registering Vehicle: Jaguar Ipace (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleJaguarIpace>("JLRI","Jaguar Ipace");
}
