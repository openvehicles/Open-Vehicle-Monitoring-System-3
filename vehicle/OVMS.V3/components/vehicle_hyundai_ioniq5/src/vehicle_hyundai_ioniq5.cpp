/*
;    Project:       Open Vehicle Monitor System
;    Date:          November 2022
;
;    Changes:
;       0.0.1:  Initial Fork of Kona/Kia module
;       0.0.2:  Load Battery capacity from cell count
;               Fix naming of various metrics.
;               Fix/consolidate power consumption metrics
;       0.0.3:  Fix Battery voltage indicator range.
;               Fix legacy GetFeatures
;               Confirm some more values and move to new decode.
;       0.0.4:  Fix range and battery estimation
;               Add 'unlocked and parked' notification.
;               Improve diagnosis of range estimation.
;               Add Ah / coulomb count to trip metrics.
;               Added RPM measurement.
;               Improve response of speed measurement.
;       0.0.5:  Add requested charge current metric
;               Fix handling of range while charging
;               Improve responsiveness for OBD2ECU
;               Add indicators and warning light metrics
;       0.0.6:  Improve the battery monitor
;
;    (C) 2022,2023 Michael Geddes
; ----- Kona/Kia Module -----
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2019       Geir Øyvind Vælidalo <geir@validalo.net>
;;
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
#define IONIQ5_VERSION "0.0.6"

#include "vehicle_hyundai_ioniq5.h"

#include "ovms_log.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pcp.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"
#include <sys/param.h>
#include "../../vehicle_kiasoulev/src/kia_common.h"
#include <sstream>

const char *OvmsHyundaiIoniqEv::TAG = "v-ioniq5";
const char *OvmsHyundaiIoniqEv::FULL_NAME = "Hyundai Ioniq 5 EV/KIA EV6";
const char *OvmsHyundaiIoniqEv::SHORT_NAME = "Ioniq 5/EV 6";
const char *OvmsHyundaiIoniqEv::VEHICLE_TYPE = "I5";


#ifdef bind
#undef bind
#endif
using namespace std::placeholders;

// Pollstate 0 - car is off
// Pollstate 1 - car is on
// Pollstate 2 - car is charging
// Pollstate 3 - ping : car is off, not charging and something triggers a wake
static const OvmsPoller::poll_pid_t vehicle_ioniq_polls[] = {
  //                                                    0    1    2   3
  //                                                   Off  On  Chrg Ping
  { 0x7b3, 0x7bb, VEHICLE_POLL_TYPE_READDATA, 0x0100, { 0,   2,  10, 30}, 0, ISOTP_STD },   // AirCon and Speed
  { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_READDATA, 0xe004, { 0,   1,   4,  4}, 0, ISOTP_STD },   // VMCU - Drive status + Accellerator
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0101, { 0,   2,   4,  4}, 0, ISOTP_STD },   // BMC Diag page 01 - Inc Battery Pack Temp + RPM
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0102, { 0,  59,   9,  0}, 0, ISOTP_STD },   // Battery 1 - BMC Diag page 02
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0103, { 0,  59,   9,  0}, 0, ISOTP_STD },   // Battery 2 - BMC Diag page 03
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0104, { 0,  59,   9,  0}, 0, ISOTP_STD },   // Battery 3 - BMC Diag page 04
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0105, { 0,  59,   9,  0}, 0, ISOTP_STD },   // Battery 4 - BMC Diag page 05 (Other - Battery Pack Temp)
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x010A, { 0,  59,   9,  0}, 0, ISOTP_STD },   // Battery 5
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x010B, { 0,  59,   9,  0}, 0, ISOTP_STD },   // Battery 6
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x010C, { 0,  59,   9,  0}, 0, ISOTP_STD },   // Battery 7

  //  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0106, { 0,   9,   9,  0}, 0, ISOTP_STD },   // BMC Diag page 06

  { 0x7a0, 0x7a8, VEHICLE_POLL_TYPE_READDATA, 0xB00C, { 0,  29,  29,  0}, 0, ISOTP_STD },   // BCM Heated handle
  { 0x7a0, 0x7a8, VEHICLE_POLL_TYPE_READDATA, 0xb008, { 0,   5,   5,  0}, 0, ISOTP_STD },   // Brake
  { 0x7a0, 0x7a8, VEHICLE_POLL_TYPE_READDATA, 0xC002, { 0,  60,   0,  0}, 0, ISOTP_STD },   // TPMS - ID's
  { 0x7a0, 0x7a8, VEHICLE_POLL_TYPE_READDATA, 0xC00B, { 0,  13,   0,  0}, 0, ISOTP_STD },   // TPMS - Pressure and Temp

  { 0x770, 0x778, VEHICLE_POLL_TYPE_READDATA, 0xbc03, { 0,   4,   7,  2}, 0, ISOTP_STD },  // IGMP Door status + IGN1 & IGN2 - Detects when car is turned on
  { 0x770, 0x778, VEHICLE_POLL_TYPE_READDATA, 0xbc04, { 0,   5,  11,  2}, 0, ISOTP_STD },  // IGMP Door status
  { 0x770, 0x778, VEHICLE_POLL_TYPE_READDATA, 0xbc07, { 0,   2,  13,  0}, 0, ISOTP_STD },  // IGMP Rear/mirror defogger/indicators
  { 0x770, 0x778, VEHICLE_POLL_TYPE_READDATA, 0xbc09, { 0,   5,  10, 20}, 0, ISOTP_STD },  // Lights
  { 0x770, 0x778, VEHICLE_POLL_TYPE_READDATA, 0xbc10, { 0,   5,  10, 20}, 0, ISOTP_STD },  // Lights

  //{0x7b3,0x7bb, VEHICLE_POLL_TYPE_READDATA, 0x0102, { 0,  10,  10,  0} },  // AirCon - No usable values found yet

  { 0x7c6, 0x7ce, VEHICLE_POLL_TYPE_READDATA, 0xB002, { 0,  5, 120,  0}, 0, ISOTP_STD },  // Cluster. ODO

  // TODO 0x7e5 OBC - On Board Charger?

  POLL_LIST_END
};

// Pollstate 4 - Aux Charging car is off but Auxilary is charging
static const OvmsPoller::poll_pid_t vehicle_ioniq_polls_second[] = {
  //                                                      4    5    6   7
  //                                                   AuxCh
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0101, { 300,   0,   0,  0}, 0, ISOTP_STD },   // BMC Diag page 01 - Inc Battery Pack Temp + RPM
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0102, { 300,   0,   0,  0}, 0, ISOTP_STD },   // Battery 1 - BMC Diag page 02
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0103, { 300,   0,   0,  0}, 0, ISOTP_STD },   // Battery 2 - BMC Diag page 03
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0104, { 300,   0,   0,  0}, 0, ISOTP_STD },   // Battery 3 - BMC Diag page 04
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0105, { 300,   0,   0,  0}, 0, ISOTP_STD },   // Battery 4 - BMC Diag page 05 (Other - Battery Pack Temp)
  { 0x770, 0x778, VEHICLE_POLL_TYPE_READDATA, 0xbc03, { 150,   0,   0,  0}, 0, ISOTP_STD },  // IGMP Door status + IGN1 & IGN2 - Detects when car is turned on
  { 0x770, 0x778, VEHICLE_POLL_TYPE_READDATA, 0xbc04, { 150,   0,   0,  0}, 0, ISOTP_STD },  // IGMP Door status
  POLL_LIST_END
};

static const OvmsPoller::poll_pid_t vehicle_ioniq_driving_polls[] = {
  // Check again while driving with ECU only
  { 0x7b3, 0x7bb, VEHICLE_POLL_TYPE_READDATA, 0x0100, { 0,  1,  20,  20}, 0, ISOTP_STD },  // For Speed
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_READDATA, 0x0101, { 0,  1,  20,  20}, 0, ISOTP_STD },  // For RPM
  POLL_LIST_END
};

// Charging profile
//  - Must be from lowest to highest to%.
//  - max watts is at optimal temperature.
//  - MaxWattsUsed = maxChargeWatts * 1-(|battery_temp - tempOptimal| * tempDegrade)

/* 0.0182 means it degrades by 40% of original maxwatts (22deg) by 0 (and 44) degress Celcius
*  - GUESTIMATE ONLY from a graph that shows that at 0 deg it is taking twice(ish)
*   as long to charge than at 25deg
*  Needs data! Might need a +ve and -ve degrade slope.
*/
charging_step_t ioniq5_chargesteps[] = {
  //  to     max  optimal temp degrade
  //   %   watts  deg C   gradient
  {   10, 220000,    22,  0.0182 },
  {   25, 210000,    22,  0.0182 },
  {   45, 200000,    22,  0.0182 },
  {   75, 150000,    22,  0.0182 },
  {   80,  80000,    22,  0.0182 },
  {   85,  60000,    22,  0.0182 },
  {   90,  40000,    22,  0.0182 },
  {   95,  25000,    22,  0.0182 },
  {  100,   7200,    22,  0.0182 },
  { 0, 0, 0, 0},
};
/** Calculate the remaining minutes to charge.
  * Works on the premise that the charge curve applies to the MAXIMUM only.
  * The array is treated as points on a line graph.
  * At each step, 'maxWatts' is degraded based on the battery temperature - this makes the assumption that the battery
  * degrades equally either side of 'optimal'. We're really only after 'good enough'.. so this is not too bad.
  * \param chargewatts the current power being used to charge
  * \param availwatts Is the reported available amount to use (can leave as 0)
  * \param adjustTemp  true if 'tempCelcius' is valid
  * \param tempCelcius The current battery temperature - used to adjust the power based on temperature.
  * \param fromSoc The current state of charge
  * \param toSoc  The target  state of charge
  * \param batterySizeWh The battery size in whatt-hours
  * \param charge_steps Array of steps.
  */
int CalcRemainingChargeMins(float chargewatts, float availwatts, bool adjustTemp, float tempCelcius, int fromSoc, int toSoc, int batterySizeWh, const charging_step_t charge_steps[])
{
  if (chargewatts <= 0 || fromSoc >= toSoc)
    return 0;
  if (availwatts < chargewatts) // encompasses '0' case.
    availwatts = chargewatts;

  int last_to_soc = fromSoc;
  int last_charge_watts = chargewatts;

  // Track the sum of percent/kw.  Reduces floating point operations.
  // The 'batterySizeWh' * 60 / 100 would be used on every sum.. so factored to the end.
  float sumval = 0;
  for (int i = 0; last_to_soc < toSoc; ++i) {
    const charging_step_t &step = charge_steps[i];
    if (step.maxChargeWatts <= 0)
      break;

    if (step.toPercent > last_to_soc) {
      int next_to_soc = step.toPercent;
      float stepMaxCharge = step.maxChargeWatts ;
      if (adjustTemp && step.tempOptimal > 0) {
        stepMaxCharge *= 1-(std::abs(tempCelcius - step.tempOptimal) * step.tempDegrade);
        if (stepMaxCharge < 0) {
          // Shouldn't happen; it means the numbers are wrong.
          stepMaxCharge = chargewatts;
        }
      }
      int percents_In_Step = next_to_soc - last_to_soc;
      if (toSoc < next_to_soc) {
        // This would be the last one. Only really gets used in the odd case where the 'toSoc' isn't one of
        // the points in the array!
        int new_percents_in_step = toSoc - last_to_soc;
        int chargeDiff = stepMaxCharge - last_charge_watts;
        // Calc either way (+ve or -ve) since the check below will cap it.
        stepMaxCharge = last_charge_watts + (new_percents_in_step * chargeDiff / percents_In_Step);
        next_to_soc = toSoc;
        percents_In_Step = new_percents_in_step;
      }
      float end_charge_watts = std::min(availwatts, stepMaxCharge);
      // Keep it as a doubled value here to avoid a /2 which becomes *2 below.
      float curSpeedDbl = last_charge_watts + end_charge_watts;

      // our one main division.
      sumval += ( percents_In_Step * 2) / curSpeedDbl;
      last_to_soc = next_to_soc;
      last_charge_watts = end_charge_watts;
    }
  }
  if (last_to_soc < toSoc) {
    sumval +=  (toSoc - last_to_soc) / last_charge_watts;
  }
  // convert to minutes (summary value to minutes)
  // 60 (hours to mins) / 100 (percent to ratio) -> 0.6
  return (int)((batterySizeWh * sumval * 0.6) + 0.5);
}

/**
 * Constructor for Ioniq 5 EV
 */
OvmsHyundaiIoniqEv::OvmsHyundaiIoniqEv()
{
  XARM("OvmsHyundaiIoniqEv::OvmsHyundaiIoniqEv");

  ESP_LOGI(TAG, "Hyundai Ioniq 5 / KIA EV6 " IONIQ5_VERSION " vehicle module");

#ifdef XIQ_CAN_WRITE
  StopTesterPresentMessages();
#endif

  memset( m_vin, 0, sizeof(m_vin));

  memset( kia_tpms_id, 0, sizeof(kia_tpms_id));

  kia_obc_ac_voltage = 230;
  kia_obc_ac_current = 0;

  kia_last_battery_cum_charge = 0;

  kn_charge_bits.ChargingCCS = false;
  kn_charge_bits.ChargingType2 = false;
  kn_charge_bits.FanStatus = 0;

  kn_battery_fan_feedback = 0;

  kia_send_can.id = 0;
  kia_send_can.status = 0;
  kia_lockDoors = false;
  kia_unlockDoors = false;
  kn_emergency_message_sent = false;


  iq_shift_status = IqShiftStatus::Park;
  iq_car_on = false;


  kia_ready_for_chargepollstate = true;
  kia_secs_with_no_client = 0;
  hif_keep_awake = 0;
  m_checklock_retry = 0;
  m_checklock_start = 0;
  m_checklock_notify = 0;

  memset( kia_send_can.byte, 0, sizeof(kia_send_can.byte));

  hif_maxrange = CFG_DEFAULT_MAXRANGE;

  BmsSetCellArrangementVoltage(192, 12);
  BmsSetCellArrangementTemperature(16, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-35, 90);
  BmsSetCellDefaultThresholdsVoltage(0.1, 0.2);
  BmsSetCellDefaultThresholdsTemperature(4.0, 8.0);

  //Disable BMS alerts by default
  MyConfig.SetParamValueBool("vehicle", "bms.alerts.enabled", false);

  // init metrics:
  m_version = MyMetrics.InitString("xiq.m.version", 0, IONIQ5_VERSION " " __DATE__ " " __TIME__);
  m_b_cell_volt_max = MyMetrics.InitFloat("xiq.v.b.c.voltage.max", 10, 0, Volts);
  m_b_cell_volt_min = MyMetrics.InitFloat("xiq.v.b.c.voltage.min", 10, 0, Volts);
  m_b_cell_volt_max_no = MyMetrics.InitInt("xiq.v.b.c.voltage.max.no", 10, 0);
  m_b_cell_volt_min_no = MyMetrics.InitInt("xiq.v.b.c.voltage.min.no", 10, 0);
  m_b_cell_det_min = MyMetrics.InitFloat("xiq.v.b.c.det.min", 0, 0, Percentage);
  m_b_cell_det_max_no = MyMetrics.InitInt("xiq.v.b.c.det.max.no", 10, 0);
  m_b_cell_det_min_no = MyMetrics.InitInt("xiq.v.b.c.det.min.no", 10, 0);
  m_c_power = MyMetrics.InitFloat("xiq.c.power", 10, 0, kW);
  m_c_speed = MyMetrics.InitFloat("xiq.c.speed", 10, 0, Kph);
  m_b_min_temperature = MyMetrics.InitInt("xiq.b.min.temp", 10, 0, Celcius);
  m_b_max_temperature = MyMetrics.InitInt("xiq.b.max.temp", 10, 0, Celcius);
  m_b_inlet_temperature = MyMetrics.InitInt("xiq.b.inlet.temp", 10, 0, Celcius);
  m_b_heat_1_temperature = MyMetrics.InitInt("xiq.b.heat1.temp", 10, 0, Celcius);
  m_b_bms_soc = MyMetrics.InitFloat("xiq.b.bms.soc", 10, 0, Percentage);
  m_b_aux_soc = MyMetrics.InitInt("xiq.b.aux.soc", 0, 0, Percentage);

  m_b_bms_relay = MyMetrics.InitBool("xiq.b.bms.relay", 30, false, Other);
  m_b_bms_ignition = MyMetrics.InitBool("xiq.b.bms.ignition", 30, false, Other);
  m_b_bms_availpower = MyMetrics.InitInt("xiq.b.bms.power.avail", 30, 0, Watts);
  m_v_bat_calc_cap   = MyMetrics.InitFloat("xiq.v.b.ccap", SM_STALE_NONE, CGF_DEFAULT_BATTERY_CAPACITY, WattHours);

  m_ldc_out_voltage = MyMetrics.InitFloat("xiq.ldc.out.volt", 10, 12, Volts);
  m_ldc_in_voltage = MyMetrics.InitFloat("xiq.ldc.in.volt", 10, 12, Volts);
  m_ldc_out_current = MyMetrics.InitFloat("xiq.ldc.out.amps", 10, 0, Amps);
  m_ldc_temperature = MyMetrics.InitFloat("xiq.ldc.temp", 10, 0, Celcius);

  m_obc_pilot_duty = MyMetrics.InitFloat("xiq.obc.pilot.duty", 10, 0, Percentage);

  m_obc_timer_enabled = MyMetrics.InitBool("xiq.obc.timer.enabled", 10, false);

  m_v_env_lowbeam = MyMetrics.InitBool("xiq.e.lowbeam", 10, false);
  m_v_env_highbeam = MyMetrics.InitBool("xiq.e.highbeam", 10, false);
  m_v_env_parklights = MyMetrics.InitBool("xiq.e.parklights", 10, false);
  m_v_env_indicator_l = MyMetrics.InitBool("xiq.e.indicator.l",SM_STALE_MIN, false);
  m_v_env_indicator_r = MyMetrics.InitBool("xiq.e.indicator.r",SM_STALE_MIN, false);
  m_v_emergency_lights = MyMetrics.InitBool("xiq.e.indicator.e",SM_STALE_MIN, false);

  m_v_preheat_timer1_enabled = MyMetrics.InitBool("xiq.e.preheat.timer1.enabled", 10, false);
  m_v_preheat_timer2_enabled = MyMetrics.InitBool("xiq.e.preheat.timer2.enabled", 10, false);
  m_v_preheating = MyMetrics.InitBool("xiq.e.preheating", 10, false);

  m_v_heated_handle = MyMetrics.InitBool("xiq.e.heated.steering", 10, false);
  m_v_rear_defogger = MyMetrics.InitBool("xiq.e.rear.defogger", 10, false);

  m_v_traction_control = MyMetrics.InitBool("xiq.v.traction.control", 10, false);

  ms_v_pos_trip = MyMetrics.InitFloat("xiq.e.trip", 10, 0, Kilometers);
  ms_v_trip_energy_used = MyMetrics.InitFloat("xiq.e.trip.energy.used", 10, 0, kWh);
  ms_v_trip_energy_recd = MyMetrics.InitFloat("xiq.e.trip.energy.recuperated", 10, 0, kWh);

  m_v_seat_belt_driver = MyMetrics.InitBool("xiq.v.sb.driver", 10, false);
  m_v_seat_belt_passenger = MyMetrics.InitBool("xiq.v.sb.passenger", 10, false);
  m_v_seat_belt_back_right = MyMetrics.InitBool("xiq.v.sb.back.right", 10, false);
  m_v_seat_belt_back_middle = MyMetrics.InitBool("xiq.v.sb.back.middle", 10, false);
  m_v_seat_belt_back_left = MyMetrics.InitBool("xiq.v.sb.back.left", 10, false);

  // m_v_power_usage = MyMetrics.InitFloat("xiq.v.power.usage", 10, 0, kW);

  m_v_trip_consumption = MyMetrics.InitFloat("xiq.v.trip.consumption", 10, 0, kWhP100K);

  m_v_door_lock_fl = MyMetrics.InitBool("xiq.v.d.l.fl", 10, false);
  m_v_door_lock_fr = MyMetrics.InitBool("xiq.v.d.l.fr", 10, false);
  m_v_door_lock_rl = MyMetrics.InitBool("xiq.v.d.l.rl", 10, false);
  m_v_door_lock_rr = MyMetrics.InitBool("xiq.v.d.l.rr", 10, false);

  m_v_accum_op_time           = MyMetrics.InitInt("xiq.v.accum.op.time",         SM_STALE_MAX, 0, Seconds );

  m_v_charge_current_request = MyMetrics.InitFloat("xiq.v.c.current.req", SM_STALE_MID, 0, Amps);

  // Setting a minimum trip size of 5km as the granuality of trips is +/- 1km.
  // The default range and battery size are overridden below in ConfigChanged() .. and
  // when the battery size is loaded from OBD.
  iq_range_calc = new RangeCalculator(5/*minTrip*/, 4/*weightCurrent*/, 450, 74);

  m_b_cell_det_min->SetValue(0);

  StdMetrics.ms_v_charge_inprogress->SetValue(false);
  StdMetrics.ms_v_env_on->SetValue(false);

  // Mirror Average battery temperature to battery temperature metric.
  if (StdMetrics.ms_v_bat_pack_tavg->IsDefined()) {
    StdMetrics.ms_v_bat_temp->SetValue(StdMetrics.ms_v_bat_pack_tavg->AsFloat());
  }
  MyMetrics.RegisterListener(TAG, MS_V_BAT_PACK_TAVG, std::bind(&OvmsHyundaiIoniqEv::UpdatedAverageTemp, this, _1));

  // init commands:
  cmd_hiq = MyCommandApp.RegisterCommand("xhiq", "Hyundai Ioniq 5 EV/Kia EV6", nullptr, "", 0,0, true,
      nullptr, OvmsCommandType::SystemAllowUsrDir);
  cmd_hiq->RegisterCommand("trip", "Show trip info since last parked", xiq_trip_since_parked);
  cmd_hiq->RegisterCommand("tripch", "Show trip info since last charge", xiq_trip_since_charge);
  cmd_hiq->RegisterCommand("tpms", "Tire pressure monitor", xiq_tpms);

  OvmsCommand *cmd_aux = cmd_hiq->RegisterCommand("aux", "Aux battery", xiq_aux);
  cmd_aux->RegisterCommand("status", "Aux Battery Status", xiq_aux);
  cmd_aux->RegisterCommand("monitor", "Aux Battery Monitor", xiq_aux_monitor);

  cmd_hiq->RegisterCommand("vin", "VIN information", xiq_vin);

  OvmsCommand *cmd_trip = cmd_hiq->RegisterCommand("range", "Show Range information");
  cmd_trip->RegisterCommand("status", "Show Status of Range Calculator", xiq_range_stat);
  cmd_trip->RegisterCommand("reset", "Reset ranage calculation stats", xiq_range_reset);

  //TODO cmd_hiq->RegisterCommand("trunk", "Open trunk", CommandOpenTrunk, "<pin>", 1, 1);

  MyConfig.SetParamValueBool("modem", "enable.gps", true);
  MyConfig.SetParamValueBool("modem", "enable.gpstime", true);
  MyConfig.SetParamValueBool("modem", "enable.net", true);
  MyConfig.SetParamValueBool("modem", "enable.sms", true);

  // Require GPS.
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);

#ifdef XIQ_CAN_WRITE
#else
  kia_enable_write = false;
#endif

  MyEvents.RegisterEvent(TAG, "app.connected", std::bind(&OvmsHyundaiIoniqEv::EventListener, this, _1, _2));

  MyConfig.RegisterParam("xiq", "Ioniq 5/EV6 specific settings.", true, true);
  ConfigChanged(NULL);

  m_ecu_lockout = -1;
  m_ecu_status_on = false;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif

  RestoreStatus();

  // D-Bus
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  PollState_Off();
  kia_secs_with_no_client = 0;
  PollSetPidList(m_can1, vehicle_ioniq_polls);

  auto secondary_series = std::shared_ptr<OvmsPoller::StandardPollSeries>(
      new OvmsPoller::StandardVehiclePollSeries(nullptr, GetPollerSignal(), 4));
  secondary_series->PollSetPidList(1, vehicle_ioniq_polls_second);

  PollRequest(m_can1, "!v.secondary", secondary_series);

  // Initially throttling to 4.
  PollSetThrottling(4);

  ESP_LOGD(TAG, "PollState->Ping for 30 (Init)");
  PollState_Ping(30);

  EnableAuxMonitor();

  XDISARM;
}

static const char *ECU_POLL = "!v.xiq.ecu";

const char* OvmsHyundaiIoniqEv::VehicleShortName()
  {
  return SHORT_NAME;
  }
const char* OvmsHyundaiIoniqEv::VehicleType()
  {
  return VEHICLE_TYPE;
  }

void OvmsHyundaiIoniqEv::ECUStatusChange(bool run)
{
  if (m_ecu_status_on == run) {
    return;
  }
  if (!run)
    m_ecu_lockout = -1;
  m_ecu_status_on = run;
  // When ECU is running - be more agressive.
  int newThrottle =  run ? 10 : 5;
  bool subtick = run && MyConfig.GetParamValueBool("xiq", "poll_subtick", true);
  ESP_LOGD(TAG, "run=%d throttle=%d subtick=%d", run, newThrottle, subtick);
  PollSetThrottling(newThrottle);

  if (!run) {
    RemovePollRequest(m_can1, ECU_POLL);
  } else {
    // Add an extra set of polling.
    auto poll_series = std::shared_ptr<OvmsPoller::StandardPacketPollSeries>(
        new OvmsPoller::StandardPacketPollSeries(nullptr, 3/*repeats*/,
              std::bind(&OvmsHyundaiIoniqEv::Incoming_Full, this, _1, _2, _3, _4, _5, _6),
              nullptr));
    poll_series->PollSetPidList(1, vehicle_ioniq_driving_polls);

    PollRequest(m_can1, ECU_POLL, poll_series);
  }
  if (subtick) {
    PollSetTimeBetweenSuccess(80); // 80ms Gap between each successfull poll.
    PollSetResponseSeparationTime(5); // Faster bursts of messages.
    PollSetTicker(300, 3);
  }
  else {
    // Defaults.
    PollSetTimeBetweenSuccess(0);
    PollSetResponseSeparationTime(25);
    PollSetTicker(1000, 1);
  }
}

/**
 * Destructor
 */
OvmsHyundaiIoniqEv::~OvmsHyundaiIoniqEv()
{
  XARM("OvmsHyundaiIoniqEv::~OvmsHyundaiIoniqEv");
  ESP_LOGI(TAG, "Shutdown Hyundai Ioniq 5 EV vehicle module");
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.DeregisterPage("/bms/cellmon");
#endif
  MyEvents.DeregisterEvent(TAG);
  MyMetrics.DeregisterListener(TAG);
  delete iq_range_calc;
  XDISARM;
}

int OvmsHyundaiIoniqEv::GetNotifyChargeStateDelay(const char *state)
{
  if (!StdMetrics.ms_v_charge_inprogress->AsBool())
    return KiaVehicle::GetNotifyChargeStateDelay(state);

  std::string charge_type = StdMetrics.ms_v_charge_type->AsString();
  if (charge_type == "ccs") {
    // CCS charging needs some time to ramp up the current/power level:
    return MyConfig.GetParamValueInt("xiq", "notify.charge.delay.ccs", 15);
  }
  else {
    return MyConfig.GetParamValueInt("xiq", "notify.charge.delay.type2", 10);
  }
}

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsHyundaiIoniqEv::ConfigChanged(OvmsConfigParam *param)
{
  XARM("OvmsHyundaiIoniqEv::ConfigChanged");
  ESP_LOGD(TAG, "Hyundai Ioniq 5 EV reload configuration");

  // Instances:
  // xiq
  //    cap_act_kwh     Battery capacity in kwH
  //  suffsoc           Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange         Sufficient range [km] (Default: 0=disabled)
  //  maxrange          Maximum ideal range at 20 °C [km] (Default: 160)
  //  canwrite          Enable commands
  int newcap = MyConfig.GetParamValueInt("xiq", "cap_act_kwh", 0);

  hif_override_capacity = newcap > 0;
  if (hif_override_capacity) {
    if (newcap > 10000)
      hif_battery_capacity = newcap;
    else
      hif_battery_capacity = (float)newcap * 1000;
  }
  else
    hif_battery_capacity = (BmsGetCellArangementVoltage() * HIF_CELL_PAIR_CAPACITY);
  m_v_bat_calc_cap->SetValue(hif_battery_capacity, WattHours);
  iq_range_calc->updateCapacity(hif_battery_capacity / 1000);
  UpdateMaxRangeAndSOH();

  hif_maxrange = MyConfig.GetParamValueInt("xiq", "maxrange", CFG_DEFAULT_MAXRANGE);
  if (hif_maxrange <= 0) {
    hif_maxrange = CFG_DEFAULT_MAXRANGE;
  }

  std::string suff = MyConfig.GetParamValue("xiq", "suffsoc");

  if (suff.empty())
    StdMetrics.ms_v_charge_limit_soc->Clear();
  else {
    auto range = atoi(suff.c_str());
    if (range <=0 )
      StdMetrics.ms_v_charge_limit_soc->Clear();
    else
      *StdMetrics.ms_v_charge_limit_soc = (float)range;
   }

  suff = MyConfig.GetParamValue("xiq", "suffrange");
  if (suff.empty())
    StdMetrics.ms_v_charge_limit_range->Clear();
  else {
    auto range = atoi(suff.c_str());
    if (range <= 0)
      StdMetrics.ms_v_charge_limit_range->Clear();
    else
      *StdMetrics.ms_v_charge_limit_range = (float)range;
  }
#ifdef XIQ_CAN_WRITE
  kia_enable_write = MyConfig.GetParamValueBool("xiq", "canwrite", false);
#endif
  XDISARM;
}

void OvmsHyundaiIoniqEv::UpdatedAverageTemp(OvmsMetric* metric)
{
  if (metric == StdMetrics.ms_v_bat_pack_tavg) {
    // Mirror battery average temperature to battery temp
    if (metric->IsDefined()) {
      StdMetrics.ms_v_bat_temp->SetValue(metric->AsFloat());
    } else {
      StdMetrics.ms_v_bat_temp->Clear();
    }
  }
}

/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsHyundaiIoniqEv::vehicle_ioniq5_car_on(bool isOn)
{
  XARM("OvmsHyundaiIoniqEv::vehicle_ioniq5_car_on");
  iq_car_on = isOn;
  StdMetrics.ms_v_env_awake->SetValue(isOn);
  float charg_rec = StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh);
  float charg_used = StdMetrics.ms_v_bat_energy_used_total->AsFloat(kWh);
  float coulomb_rec = StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat(kWh);
  float coulomb_used = StdMetrics.ms_v_bat_coulomb_used_total->AsFloat(kWh);
  bool isCharge = StdMetrics.ms_v_charge_inprogress->AsBool();
  if (isOn) {
    // Car is ON
    ESP_LOGI(TAG, "CAR IS ON");
    ESP_LOGD(TAG, "PollState->Running (on)");
    PollState_Running();
    StdMetrics.ms_v_env_charging12v->SetValue( true );
    kia_ready_for_chargepollstate = true;

    kia_park_trip_counter.Reset(POS_ODO, charg_used, charg_rec, coulomb_used, coulomb_rec, isCharge);
    BmsResetCellStats();
  }
  else if (!isOn) {
    // Car is OFF
    ESP_LOGI(TAG, "CAR IS OFF");
    if (kia_park_trip_counter.Started()) {
      kia_park_trip_counter.Update(POS_ODO, charg_used, charg_rec, coulomb_used, coulomb_rec);
      if (isCharge != kia_park_trip_counter.Charging()) {
        kia_park_trip_counter.StartCharge(charg_rec, coulomb_rec);
      }
    } else {
      kia_park_trip_counter.Reset(POS_ODO, charg_used, charg_rec, coulomb_used, coulomb_rec, isCharge);
    }
    kia_secs_with_no_client = 0;
    StdMetrics.ms_v_pos_speed->SetValue( 0 );
    StdMetrics.ms_v_pos_trip->SetValue( kia_park_trip_counter.GetDistance() );
    StdMetrics.ms_v_env_charging12v->SetValue( false );
    kia_ready_for_chargepollstate = true;
    iq_range_calc->tripEnded(kia_park_trip_counter.GetDistance(), kia_park_trip_counter.GetEnergyUsed());
    SaveStatus();
  }
  XDISARM;
}

/**
 * Ticker1: Called every second
 */
void OvmsHyundaiIoniqEv::Ticker1(uint32_t ticker)
{
  XARM("OvmsHyundaiIoniqEv::Ticker1");
  //ESP_LOGD(TAG,"Pollstate: %d sec with no client: %d ",m_poll_state, kn_secs_with_no_client);
  // Register car as locked only if all doors are locked
  bool isLocked =
    m_v_door_lock_fr->AsBool() &
    m_v_door_lock_fl->AsBool() &
    m_v_door_lock_rr->AsBool() &
    m_v_door_lock_rl->AsBool() &
    !StdMetrics.ms_v_door_trunk->AsBool();
  StdMetrics.ms_v_env_locked->SetValue( isLocked);

  if (iq_car_on != StdMetrics.ms_v_env_on->AsBool()) {
    vehicle_ioniq5_car_on(StdMetrics.ms_v_env_on->AsBool());
  }

  UpdateMaxRangeAndSOH();

  // Update trip data
  if (StdMetrics.ms_v_env_on->AsBool()) {
    float energy_used = StdMetrics.ms_v_bat_energy_used_total->AsFloat(kWh);
    float energy_recovered = StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh);
    float coulomb_used = StdMetrics.ms_v_bat_coulomb_used_total->AsFloat(kWh);
    float coulomb_recovered = StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat(kWh);
    float odo =  StdMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers);

    if (kia_park_trip_counter.Started()) {
      kia_park_trip_counter.Update(odo, energy_used, energy_recovered, coulomb_used, coulomb_recovered);
      if (StdMetrics.ms_v_charge_inprogress->AsBool() != kia_park_trip_counter.Charging()) {
        kia_park_trip_counter.StartCharge( energy_recovered, coulomb_recovered );
      }

      StdMetrics.ms_v_pos_trip->SetValue( kia_park_trip_counter.GetDistance(), Kilometers);
      if ( kia_park_trip_counter.HasEnergyData()) {
        StdMetrics.ms_v_bat_energy_used->SetValue( kia_park_trip_counter.GetEnergyUsed(), kWh );
        StdMetrics.ms_v_bat_energy_recd->SetValue( kia_park_trip_counter.GetEnergyRecuperated(), kWh );
      }
      if ( kia_park_trip_counter.HasChargeData()) {
        StdMetrics.ms_v_bat_coulomb_used->SetValue( kia_park_trip_counter.GetChargeUsed(), kWh );
        StdMetrics.ms_v_bat_coulomb_recd->SetValue( kia_park_trip_counter.GetChargeRecuperated(), kWh );
      }
    }
    if (kia_charge_trip_counter.Started()) {
      kia_charge_trip_counter.Update(odo, energy_used, energy_recovered, coulomb_used, coulomb_recovered);
      ms_v_pos_trip->SetValue( kia_charge_trip_counter.GetDistance(), Kilometers);
      if ( kia_charge_trip_counter.HasEnergyData()) {
        ms_v_trip_energy_used->SetValue( kia_charge_trip_counter.GetEnergyUsed(), kWh );
        ms_v_trip_energy_recd->SetValue( kia_charge_trip_counter.GetEnergyRecuperated(), kWh );
      }
    }
  } else {
    if (kia_park_trip_counter.Started() && (StdMetrics.ms_v_charge_inprogress->AsBool() != kia_park_trip_counter.Charging())) {
      float energy_recovered = StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh);
      float coulomb_recovered = StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat(kWh);
      kia_park_trip_counter.StartCharge( energy_recovered, coulomb_recovered);
    }
  }

  float trip_km = StdMetrics.ms_v_pos_trip->AsFloat(Kilometers);
  float trip_energy = StdMetrics.ms_v_bat_energy_used->AsFloat(kWh);
  if ( trip_km > 0 && trip_energy > 0 ) {
    m_v_trip_consumption->SetValue( trip_energy * 100 / trip_km, kWhP100K );
  }

  StdMetrics.ms_v_bat_power->SetValue( StdMetrics.ms_v_bat_voltage->AsFloat(400, Volts) * StdMetrics.ms_v_bat_current->AsFloat(1, Amps) / 1000, kW );

  //Calculate charge current and "guess" charging type
  if (StdMetrics.ms_v_bat_power->AsFloat(0, kW) < 0 ) {
    // We are charging! Now lets calculate which type! (This is a hack until we find this information elsewhere)
    StdMetrics.ms_v_charge_current->SetValue(
      -StdMetrics.ms_v_bat_current->AsFloat(1, Amps));
    if (StdMetrics.ms_v_bat_power->AsFloat(0, kW) < -12 /*7.36*/) {
      kn_charge_bits.ChargingCCS = true;
      kn_charge_bits.ChargingType2 = false;
    }
    else {
      kn_charge_bits.ChargingCCS = false;
      kn_charge_bits.ChargingType2 = true;
    }
  }
  else {
    StdMetrics.ms_v_charge_current->SetValue(0);
    kn_charge_bits.ChargingCCS = false;
    kn_charge_bits.ChargingType2 = false;
  }

  // AC charge current on kona not yet found, so we'll fake it by looking at battery power
  kia_obc_ac_current = -StdMetrics.ms_v_bat_power->AsFloat(0, Watts) / kia_obc_ac_voltage;

  //Keep charging metrics up to date
  if (kn_charge_bits.ChargingType2) {   // **** Type 2  charging ****
    SetChargeMetrics(kia_obc_ac_voltage, kia_obc_ac_current, 32, false);
  }
  else if (kn_charge_bits.ChargingCCS) { // **** CCS charging ****
    SetChargeMetrics(StdMetrics.ms_v_bat_voltage->AsFloat(400, Volts), StdMetrics.ms_v_charge_current->AsFloat(1, Amps), 200, true);
  }

  // Check for charging status changes:
  bool isCharging = false;
  if (m_b_bms_relay->IsStale() || m_b_bms_ignition->IsStale()) {
    isCharging = false;
  }
  else {
    // See https://docs.google.com/spreadsheets/d/1JyJnXh7DOvzTl0cbWZRpW9qB_OgEOM-w_XOiAY_a64o/edit#gid=1990128420
    isCharging = (m_b_bms_relay->AsBool(false) - m_b_bms_ignition->AsBool(false)) == 1;
  }
  // Still a flag we are missing for low-power charging.
  if (!isCharging && m_b_bms_availpower->AsInt() > 0) {
    isCharging = true;
  }
  // fake charge port door TODO: Fix for Ioniq5
  StdMetrics.ms_v_door_chargeport->SetValue(isCharging);

  if (isCharging) {
    HandleCharging();
  }
  else if (StdMetrics.ms_v_charge_inprogress->AsBool()) {
    HandleChargeStop();
  }

  if ( !(ticker & 1) && m_aux_battery_mon.state() == OvmsBatteryState::Charging) {
    BatteryStateStillCharging();
  }
  if (IsPollState_Off() && StdMetrics.ms_v_door_chargeport->AsBool() && kia_ready_for_chargepollstate) {
    //Set pollstate charging if car is off and chargeport is open.
    ESP_LOGI(TAG, "CHARGEDOOR OPEN. READY FOR CHARGING.");
    ESP_LOGD(TAG, "PollState->Charging (Charge Door Open)");
    PollState_Charging();
    kia_ready_for_chargepollstate = false;
    kia_secs_with_no_client = 0; //Reset no client counter
  }

  // Wake up if car starts charging again
  int accum_power_wh = StdMetrics.ms_v_bat_energy_recd_total->AsInt();
  if (IsPollState_Off() && kia_last_battery_cum_charge < accum_power_wh ) {
    kia_secs_with_no_client = 0;
    kia_last_battery_cum_charge = accum_power_wh;
    if (StdMetrics.ms_v_env_on->AsBool()) {
      if (!IsPollState_Running()) {
        ESP_LOGD(TAG, "PollState->Running (ON)");
        PollState_Running();
      }
    }
    else if (!IsPollState_Charging()) {
      ESP_LOGD(TAG, "PollState->Charging (Not On - Other)");
      PollState_Charging();
    }
  }

  bool wasPaused = hif_keep_awake > 0;
  if (hif_keep_awake > 0) {
    --hif_keep_awake;
  }

  //**** AUX Battery drain prevention code ***
  bool isRunning = StdMetrics.ms_v_env_on->AsBool();
  if (StdMetrics.ms_v_bat_12v_voltage_alert->AsBool()) {
    ESP_LOGV(TAG, "12V Battery Alert");
    if (hif_keep_awake == 0) {
      ESP_LOGD(TAG, "PollState->Off (Aux Battery Alert)");
      PollState_Off();
    }
  }
  else {
    // If no clients are connected for 60 seconds, we'll turn off polling.
    uint32_t clients = 0;
    if (StdMetrics.ms_s_v2_connected->AsBool()) {
      clients += StdMetrics.ms_s_v2_peers->AsInt();
    }
    if (StdMetrics.ms_s_v3_connected->AsBool()) {
      clients += StdMetrics.ms_s_v3_peers->AsInt();
    }
    if (clients == 0) {
      if (!isRunning && !isCharging ) {
        kia_secs_with_no_client++;
        if (kia_secs_with_no_client > 60) {
          if (!IsPollState_Off() ) {
            if (!IsPollState_Ping() && !isLocked) {
              ESP_LOGI(TAG, "NO CLIENTS - Not locked so poll for a bit more.");
              ESP_LOGD(TAG, "PollState->Ping (No Clients - unlocked)");
              PollState_Ping(120);

            }
            else if ( hif_keep_awake == 0) {
              ESP_LOGI(TAG, "NO CLIENTS. Turning off polling.");
              ESP_LOGD(TAG, "PollState->Off (No Clients)");
              PollState_Off();
            }
            else if (!IsPollState_Ping()) {
              ESP_LOGI(TAG, "NO CLIENTS. Going into Ping polling state for %d more secs.", hif_keep_awake);
              ESP_LOGD(TAG, "PollState->Ping (No Clients)");
              PollState_Ping();
            }
          }
        }
      }
    }
    else {
      if (!isRunning && !isCharging) {
        if (!IsPollState_Ping() && !IsPollState_Off()) {
          ESP_LOGI(TAG, "CLIENT CONNECTED. Keep awake for a bit");
          if (isLocked) {
            ESP_LOGD(TAG, "PollState->Ping for 60 (Client Connected - Locked)");
            PollState_Ping(60);
          }
          else {
            ESP_LOGD(TAG, "PollState->Ping for 300 (Client Connected - Unlocked)");
            PollState_Ping(300);
          }
        }
        else if (isLocked && IsPollState_Ping()) {
          // locked already - cap the pollstate to 60 secs
          Poll_CapAwake(60);
        }
      }
      else if (IsPollState_Off() || IsPollState_Ping()) {
        //If client connects while poll state is off, we set the appropriate poll state

        hif_keep_awake = 0;
        kia_secs_with_no_client = 0;
        ESP_LOGI(TAG, "CLIENT CONNECTED. Turning on polling.");
        if (isRunning) {
          if (!IsPollState_Running()) {
            ESP_LOGD(TAG, "PollState->Running (Client Connect - running)");
            PollState_Running();
          }
        }
        else if (!IsPollState_Charging()) {
          ESP_LOGD(TAG, "PollState->Charging (Client Connect - not running)");
          PollState_Charging();
        }
      }
    }
  }
  if (!isRunning && !isCharging && wasPaused && (hif_keep_awake == 0)) {
    if (!IsPollState_Off()) {
      ESP_LOGD(TAG, "PollState->Off (Timed out)");
      PollState_Off();
    }
  }
  //**** End of AUX Battery drain prevention code ***

  // Reset emergency light if it is stale.
  if ( m_v_emergency_lights->IsStale() ) {
    m_v_emergency_lights->SetValue(false);
  } else {

    // Notify if emergency light are turned on or off.
    if ( m_v_emergency_lights->AsBool() && !kn_emergency_message_sent) {
      kn_emergency_message_sent = true;
      RequestNotify(SEND_EmergencyAlert);
    }
    else if ( !m_v_emergency_lights->AsBool() && kn_emergency_message_sent) {
      kn_emergency_message_sent = false;
      RequestNotify(SEND_EmergencyAlertOff);
    }
  }

  // Let the busy time of starting the car happen before we
  // ramp up the speed of the polls to support obd2ecu.
  // Otherwise we can see the car reporting system failures.

  if (StandardMetrics.ms_v_env_on->AsBool()
      && StandardMetrics.ms_m_obd2ecu_on->AsBool()
      && (StdMetrics.ms_v_env_gear->AsInt() > 0)) {

    if (m_ecu_lockout > 0)
      --m_ecu_lockout;
    else if (m_ecu_lockout < 0)
      m_ecu_lockout = 10;

    ECUStatusChange(m_ecu_lockout==0);
  } else {
    ECUStatusChange(false);
  }

#ifdef XIQ_CAN_WRITE
  // Send tester present
  SendTesterPresentMessages();
#endif

  DoNotify();

  XDISARM;
}

void OvmsHyundaiIoniqEv::NotifiedVehicleAux12vStateChanged(OvmsBatteryState new_state, const OvmsBatteryMon &monitor)
{
#ifdef OVMS_DEBUG_BATTERYMON
  ESP_LOGV(TAG, "Aux Battery: %s", monitor.to_string().c_str());
#endif
  switch (new_state) {
    case OvmsBatteryState::Unknown:
      break;
    case OvmsBatteryState::Normal:
      ESP_LOGD(TAG, "Aux Battery state returned to normal");
      m_b_aux_soc->SetValue( CalcAUXSoc(monitor.average_lastf()), Percentage );
      break;
    case OvmsBatteryState::Charging:
      ESP_LOGD(TAG, "Aux Battery state: Charging %g" , monitor.average_lastf());
      break;
    case OvmsBatteryState::ChargingDip:
      ESP_LOGD(TAG, "Aux Battery state: Charging %g Dip %g",
          monitor.average_lastf(), monitor.diff_lastf());
      if ( IsPollState_Off()) {
        ESP_LOGD(TAG, "PollState->Ping for 30 (Charge Dip)");
        PollState_Ping(30);
      }
      break;
    case OvmsBatteryState::ChargingBlip:
      ESP_LOGD(TAG, "Aux Battery state: Charging %g Blip %g",
          monitor.average_lastf(), monitor.diff_lastf());
      if ( IsPollState_Off()) {
        ESP_LOGD(TAG, "PollState->Ping for 30 (Charge Blip)");
        PollState_Ping(30);
      }
      break;
    case OvmsBatteryState::Blip: {
      ESP_LOGD(TAG, "Aux Battery state: Blip %g", monitor.diff_lastf());
      if ( IsPollState_Off()) {
        ESP_LOGD(TAG, "PollState->Ping for 30 (Blip)");
        PollState_Ping(30);
      }
    }
    break;
    case OvmsBatteryState::Dip: {
      ESP_LOGD(TAG, "Aux Battery state: Dip %g", monitor.diff_lastf());
      if ( IsPollState_Off()) {
        ESP_LOGD(TAG, "PollState->Ping for 30 (Dip)");
        PollState_Ping(30);
      }
    }
    break;
    case OvmsBatteryState::Low: {
      ESP_LOGD(TAG, "Aux Battery state: Low %g", monitor.diff_lastf());
      if (!IsPollState_Off()) {
        ESP_LOGD(TAG, "PollState->Off (Aux Battery state Low)");
        PollState_Off();
        // ?? Turn other things off ??
      }
      m_b_aux_soc->SetValue( CalcAUXSoc(monitor.average_lastf()), Percentage );
      XDISARM;
      return;
    }
  }
}

void OvmsHyundaiIoniqEv::BatteryStateStillCharging()
{
  if (IsPollState_Off()) {
    ESP_LOGD(TAG, "PollState->PingAux for 30 (Charging)");
    PollState_PingAux(30);
  }
}

/**
 * Ticker10: Called every ten seconds
 */
void OvmsHyundaiIoniqEv::Ticker10(uint32_t ticker)
{
  if (m_vin[0] == 0 && IsPollState_Running() && (m_vin_retry < 10)) {
    ESP_LOGI(TAG, "Checking for VIN.");
    if (PollRequestVIN()) {
      ++m_vin_retry;
    }
  }
}

void OvmsHyundaiIoniqEv::MetricModified(OvmsMetric* metric)
{
  KiaVehicle::MetricModified(metric);

  if (metric == StdMetrics.ms_v_env_locked || metric == StdMetrics.ms_v_env_on) {
    if (StdMetrics.ms_v_env_locked->AsBool(false) || StdMetrics.ms_v_env_on->AsBool(false)) {
      CheckResetDoorCheck();
    }
  }
}
void OvmsHyundaiIoniqEv::CheckResetDoorCheck()
{
  if (m_checklock_start > 0) {
    if (m_checklock_notify > 0) {
      if ((monotonictime - m_checklock_notify) < (30*60)) {
        if (StdMetrics.ms_v_env_locked->AsBool(false)) {
          MyNotify.NotifyString("info", "unlock.ok", "Vehicle is now locked");
        }
      }
      m_checklock_notify = 0;
    }
    m_checklock_start = 0;
  }
  m_checklock_retry = 0;
}

void OvmsHyundaiIoniqEv::Ticker60(uint32_t ticker)
{
  if (!StdMetrics.ms_v_env_locked->AsBool(false) && !StdMetrics.ms_v_env_on->AsBool(false)) {
    if (m_checklock_start == 0) {
      m_checklock_start = monotonictime;
      m_checklock_notify = 0;
    }
    bool notify = false;
    int diffsecs = (monotonictime - m_checklock_start);
    if ( m_checklock_notify == 0) {
      notify = diffsecs >= (5*60);
    } else if (diffsecs <= (45*60)) {
      notify = (monotonictime - m_checklock_notify) > 600;
    }
    if (notify) {
      m_checklock_notify = monotonictime;
      MyNotify.NotifyString("alert", "unlock.alert", "Vehicle is still unlocked and off");
    }
  } else {
    CheckResetDoorCheck();
  }
}

/**
 * Ticker300: Called every five minutes
 */
void OvmsHyundaiIoniqEv::Ticker300(uint32_t ticker)
{
  XARM("OvmsHyundaiIoniqEv::Ticker300");
  Save12VHistory();
  if (!StdMetrics.ms_v_env_locked->AsBool(false) && !StdMetrics.ms_v_env_on->AsBool(false)) {
    if (m_checklock_start == 0) {
      m_checklock_start = monotonictime;
      m_checklock_notify = 0;
    }
    if (!IsPollState_Ping()) {
      ++m_checklock_retry;
      if (m_checklock_retry < 5) {
        PollState_Ping(30);
      } else if ((m_checklock_retry <= 20) && ((m_checklock_retry & 3) == 0)) // every 4th
        PollState_Ping(10);
    }
  } else {
    CheckResetDoorCheck();
  }

  XDISARM;
}

void OvmsHyundaiIoniqEv::EventListener(std::string event, void *data)
{
  XARM("OvmsHyundaiIoniqEv::EventListener");
  if (event == "app.connected") {
    kia_secs_with_no_client = 0;
  }
  XDISARM;
}


/**
 * Update metrics when charging
 */
void OvmsHyundaiIoniqEv::HandleCharging()
{
  XARM("OvmsHyundaiIoniqEv::HandleCharging");

  float bat_soc = StdMetrics.ms_v_bat_soc->AsFloat(100);
  if (!StdMetrics.ms_v_charge_inprogress->AsBool() ) {
    ESP_LOGI(TAG, "Charging starting");
    kia_park_trip_counter.StartCharge(
      StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh),
      StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat(kWh)
    );

    // ******* Charging started: **********
    StdMetrics.ms_v_charge_duration_full->SetValue( 1440 * (100 - bat_soc), Minutes ); // Lets assume 24H to full.
    if ( StdMetrics.ms_v_charge_timermode->AsBool()) {
      SET_CHARGE_STATE("charging", "scheduledstart");
    }
    else {
      SET_CHARGE_STATE("charging", "onrequest");
    }
    StdMetrics.ms_v_charge_kwh->SetValue( 0, kWh );  // kWh charged
    kia_cum_charge_start = StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh); // Battery charge base point
    StdMetrics.ms_v_charge_inprogress->SetValue( true );
    StdMetrics.ms_v_env_charging12v->SetValue( true);

    BmsResetCellStats();

    ESP_LOGD(TAG, "PollState->Charging (Charging Event)");
    PollState_Charging();
  }
  else {
    // ******* Charging continues: *******
    float limit_soc = StdMetrics.ms_v_charge_limit_soc->AsFloat(0);
    float est_range = StdMetrics.ms_v_bat_range_est->AsFloat(400, Kilometers);
    float limit_range = StdMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers);
    float ideal_range = StdMetrics.ms_v_bat_range_ideal->AsFloat(450, Kilometers);
    kia_park_trip_counter.Update(
      POS_ODO,
      StdMetrics.ms_v_bat_energy_used_total->AsFloat(kWh), StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh),
      StdMetrics.ms_v_bat_coulomb_used_total->AsFloat(kWh), StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat(kWh)
    );

    if (((bat_soc > 0) && (limit_soc > 0) && (bat_soc >= limit_soc) && (kia_last_soc < limit_soc))
      || ((est_range > 0) && (limit_range > 0)
        && (ideal_range >= limit_range )
        && (kia_last_ideal_range < limit_range ))) {
      // ...enter state 2=topping off when we've reach the needed range / SOC:
      SET_CHARGE_STATE("topoff");
    }
    else if (bat_soc >= 95) { // ...else set "topping off" from 94% SOC:
      SET_CHARGE_STATE("topoff");
    }
  }

  // Check if we have what is needed to calculate remaining minutes
  float charge_current = StdMetrics.ms_v_charge_current->AsFloat(0, Amps);
  float charge_voltage = StdMetrics.ms_v_charge_voltage->AsFloat(0, Volts);
  if (charge_voltage > 0 && charge_current > 0) {
    //Calculate remaining charge time
    float chargeTarget_full   = 100;
    if (StdMetrics.ms_v_bat_soh->IsDefined()) {
      float bat_soh = StdMetrics.ms_v_bat_soh->AsFloat(100);
      if (bat_soh > 0) {
        chargeTarget_full = bat_soh;
      }
    }

    auto batTemp = StdMetrics.ms_v_charge_temp->AsFloat(0);
    bool useTemp = StdMetrics.ms_v_charge_temp->IsDefined();
    if (!useTemp && StandardMetrics.ms_v_bat_pack_tavg->IsDefined()) {
      useTemp = true;
      batTemp = StandardMetrics.ms_v_bat_pack_tavg->AsFloat(0);
    }

    float charge_power = charge_voltage * charge_current;
    auto calc_remain_mins = [&]( int target_soc ) {
        return CalcRemainingChargeMins(charge_power, 0/*availPower*/,
          useTemp, batTemp,  bat_soc, target_soc, hif_battery_capacity, ioniq5_chargesteps);
      };

    if (!StdMetrics.ms_v_charge_limit_soc->IsDefined()) {
      StdMetrics.ms_v_charge_duration_soc->Clear();
    } else {
      float chargeTarget_soc = StdMetrics.ms_v_charge_limit_soc->AsFloat(0);
      if (chargeTarget_soc <= 0) {
        StdMetrics.ms_v_charge_duration_soc->Clear();
      } else {
        int dur_mins = calc_remain_mins(chargeTarget_soc);
        StdMetrics.ms_v_charge_duration_soc->SetValue(dur_mins, Minutes);
      }
    }

    if (!StdMetrics.ms_v_charge_limit_range->IsDefined() || !StdMetrics.ms_v_bat_range_full->IsDefined()) {
      StdMetrics.ms_v_charge_duration_range->Clear();
    } else {
      float chargeTarget_range = StdMetrics.ms_v_charge_limit_range->AsFloat(0);
      float bat_range_full = StdMetrics.ms_v_bat_range_full->AsFloat(0);

      if (chargeTarget_range <= 0 || bat_range_full <= 0) {
        StdMetrics.ms_v_charge_duration_range->Clear();
      } else {
        float targetPercent = chargeTarget_range * 100 / bat_range_full;
        int dur_mins = calc_remain_mins(targetPercent);
        StdMetrics.ms_v_charge_duration_range->SetValue(dur_mins, Minutes);
      }
    }
    int dur_mins = calc_remain_mins(chargeTarget_full);
    StdMetrics.ms_v_charge_duration_full->SetValue(dur_mins, Minutes);
  }
  else {
    if ( m_v_preheating->AsBool()) {
      SET_CHARGE_STATE("heating", "scheduledstart");
    }
    else {
      SET_CHARGE_STATE("charging");
    }
  }
  StdMetrics.ms_v_charge_kwh->SetValue(StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh) - kia_cum_charge_start, kWh); // kWh charged
  kia_last_soc = bat_soc;
  kia_last_battery_cum_charge = StdMetrics.ms_v_bat_energy_recd_total->AsInt();
  kia_last_ideal_range = StdMetrics.ms_v_bat_range_ideal->AsFloat(100, Kilometers);
  StdMetrics.ms_v_charge_pilot->SetValue(true);
  XDISARM;
}

/**
 * Update metrics when charging stops
 */
void OvmsHyundaiIoniqEv::HandleChargeStop()
{
  XARM("OvmsHyundaiIoniqEv::HandleChargeStop");
  ESP_LOGI(TAG, "Charging done...");

  // ** Charge completed or interrupted: **
  StdMetrics.ms_v_charge_current->SetValue( 0 );
  StdMetrics.ms_v_charge_climit->SetValue( 0 );
  if (BAT_SOC == 100 || (BAT_SOC > 82 && kn_charge_bits.ChargingCCS)) {
    SET_CHARGE_STATE("done", "stopped");
  }
  else if (BAT_SOC == 80 && kn_charge_bits.ChargingType2) {
    SET_CHARGE_STATE("stopped", "scheduledstop");
  }
  else {
    SET_CHARGE_STATE("stopped", "interrupted");
  }
  StdMetrics.ms_v_charge_kwh->SetValue( StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh) - kia_cum_charge_start, kWh );  // kWh charged

  kia_cum_charge_start = 0;
  StdMetrics.ms_v_charge_inprogress->SetValue( false );
  StdMetrics.ms_v_env_charging12v->SetValue( false );
  StdMetrics.ms_v_charge_pilot->SetValue(false);
  kn_charge_bits.ChargingCCS = false;
  kn_charge_bits.ChargingType2 = false;
  m_c_speed->SetValue(0);

  StdMetrics.ms_v_charge_duration_full->Clear();
  StdMetrics.ms_v_charge_duration_soc->Clear();
  StdMetrics.ms_v_charge_duration_range->Clear();

  // Reset trip counter for this charge
  kia_charge_trip_counter.Reset(POS_ODO,
    StdMetrics.ms_v_bat_energy_used_total->AsFloat(kWh), StdMetrics.ms_v_bat_energy_recd_total->AsFloat(kWh),
    StdMetrics.ms_v_bat_coulomb_used_total->AsFloat(kWh), StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat(kWh)
    );
  kia_secs_with_no_client = 0;
  SaveStatus();
  XDISARM;
}

/**
 *  Sets the charge metrics
 */
void OvmsHyundaiIoniqEv::SetChargeMetrics(float voltage, float current, float climit, bool ccs)
{
  XARM("OvmsHyundaiIoniqEv::SetChargeMetrics");
  StdMetrics.ms_v_charge_voltage->SetValue( voltage, Volts );
  StdMetrics.ms_v_charge_current->SetValue( current, Amps );
  StdMetrics.ms_v_charge_mode->SetValue( ccs ? "performance" : "standard");
  StdMetrics.ms_v_charge_climit->SetValue( climit, Amps);
  StdMetrics.ms_v_charge_type->SetValue( ccs ? "ccs" : "type2");
  StdMetrics.ms_v_charge_substate->SetValue("onrequest");

  //ESP_LOGI(TAG, "SetChargeMetrics: volt=%1f current=%1f chargeLimit=%1f", voltage, current, climit);

  //"Typical" consumption based on battery temperature and ambient temperature.
  float temperature = StdMetrics.ms_v_bat_temp->AsFloat(20, Celcius);
  float temp =  temperature;
  if (StdMetrics.ms_v_env_temp->IsDefined()) {
    if (StdMetrics.ms_v_bat_temp->IsDefined()) {
      temp = (( temp * 3) + StdMetrics.ms_v_env_temp->AsFloat(Celcius)) / 4;
    } else {
      temp = StdMetrics.ms_v_env_temp->AsFloat(Celcius);
    }
  }
  float consumption = 15 + (20 - temp) * 3.0 / 8.0; //kWh/100km
  m_c_speed->SetValue( (voltage * current) / (consumption * 10), Kph);
  XDISARM;
}

/**
 * Updates the maximum real world range at current temperature.
 * Also updates the State of Health
 */
void OvmsHyundaiIoniqEv::UpdateMaxRangeAndSOH(void)
{
  XARM("OvmsHyundaiIoniqEv::UpdateMaxRangeAndSOH");
  //Update State of Health using following assumption: 10% buffer
  float bat_soh = StdMetrics.ms_v_bat_soh->AsFloat(100);
  float bat_soc = StdMetrics.ms_v_bat_soc->AsFloat(100);
  StdMetrics.ms_v_bat_cac->SetValue( (hif_battery_capacity * bat_soh * bat_soc / 10000.0) / 400, AmpHours);

  iq_range_calc->updateTrip(kia_park_trip_counter.GetDistance(), kia_park_trip_counter.GetEnergyUsed());

  float maxRange = hif_maxrange;// * MIN(BAT_SOH,100) / 100.0;
  float wltpRange = iq_range_calc->getRange();
  float amb_temp = StdMetrics.ms_v_env_temp->AsFloat(20, Celcius);
  float bat_temp = 20;
  if (StdMetrics.ms_v_bat_pack_tavg->IsDefined()) {
    bat_temp = StdMetrics.ms_v_bat_pack_tavg->AsFloat();
  } else if ( StdMetrics.ms_v_bat_temp->IsDefined()) {
    bat_temp = StdMetrics.ms_v_bat_temp->AsFloat();
  } else {
    bat_temp  = amb_temp;
  }
  float use_temp = bat_temp;
  if (StdMetrics.ms_v_env_temp->IsDefined()) {
    use_temp = (amb_temp + (bat_temp * 3))  / 4;
  }

  // Temperature compensation:
  //   - Assumes standard maxRange specified at 20 degrees C
  //   - Range halved at -20C.
  if (maxRange != 0) {
    maxRange = (maxRange * (100.0 - (int) (ABS(20.0 - use_temp) * 1.25))) / 100.0;
    StdMetrics.ms_v_bat_range_ideal->SetValue( maxRange * BAT_SOC / 100.0, Kilometers);
  }
  StdMetrics.ms_v_bat_range_full->SetValue(maxRange, Kilometers);

  //TODO How to find the range as displayed in the cluster? Use the WLTP until we find it
  //wltpRange = (wltpRange * (100.0 - (int) (ABS(20.0 - (amb_temp+bat_temp * 3)/4)* 1.25))) / 100.0;
  StdMetrics.ms_v_bat_range_est->SetValue( wltpRange * BAT_SOC / 100.0, Kilometers);
  XDISARM;
}


/**
 * Open or lock the doors
 */
bool OvmsHyundaiIoniqEv::SetDoorLock(bool open, const char *password)
{
  XARM("OvmsHyundaiIoniqEv::SetDoorLock");
  bool result = false;
  /*
  if ( iq_shift_status == IqShiftStatus::Park ) {
    if ( PinCheck((char *)password) ) {
      ACCRelay(true, password  );
      DriverIndicatorOn();
      result = Send_IGMP_Command(0xbc, open ? 0x11 : 0x10, 0x03);
      ACCRelay(false, password );
    }
  }
  */
  XDISARM;
  return result;
}

/**
 * Open trunk door
 */
bool OvmsHyundaiIoniqEv::OpenTrunk(const char *password)
{
  XARM("OvmsHyundaiIoniqEv::OpenTrunk");
  /*
  if ( iq_shift_status == IqShiftStatus::Park ) {
    if ( PinCheck((char *)password) ) {
      StartRelay(true, password);
      DriverIndicatorOn();
      bool res = Send_IGMP_Command(0xbc, 0x09, 0x03);
      StartRelay(false, password);
      XDISARM;
      return res;
    }
  }
  */
  XDISARM;
  return false;
}

/**
 * Turn on and off left indicator light
 */
bool OvmsHyundaiIoniqEv::LeftIndicator(bool on)
{
  XARM("OvmsHyundaiIoniqEv::LeftIndicator");
  bool res =  false;
  /*Not at this address.
  if ( iq_shift_status == IqShiftStatus::Park ) {
    Set_IGMP_TP_TimeOut(on, 10);//Keep IGMP in test mode for up to 10 seconds
    res = Send_IGMP_Command(0xbc, 0x15, on ? 0x03 : 0x00);
  }
  */
  XDISARM;
  return res;
}

/**
 * Turn on and off right indicator light
 */
bool OvmsHyundaiIoniqEv::RightIndicator(bool on)
{
  XARM("OvmsHyundaiIoniqEv::RightIndicator");
  bool res = false;
  /* Not at this address.
  if ( iq_shift_status == IqShiftStatus::Park ) {
    Set_IGMP_TP_TimeOut(on, 10);    //Keep IGMP in test mode for up to 10 seconds
    res = Send_IGMP_Command(0xbc, 0x16, on ? 0x03 : 0x00);
  }
  */
  XDISARM;
  return res;
}

/**
 * Turn on and off rear defogger
 * Needs to send tester present or something... Turns of immediately.
 * 770 04 2f bc 0c 0[3:0]
 */
bool OvmsHyundaiIoniqEv::RearDefogger(bool on)
{
  XARM("OvmsHyundaiIoniqEv::RearDefogger");
  bool res = false;
  /*
  if ( iq_shift_status == IqShiftStatus::Park ) {
    Set_IGMP_TP_TimeOut(on, 900);  //Keep IGMP in test mode for up to 15 minutes
    res = Send_IGMP_Command(0xbc, 0x0c, on ? 0x03 : 0x00);
  }
  */
  XDISARM;
  return res;
}

/**
 * Fold or unfold mirrors.
 *
 * 7a0 04 2f b0 5[b:c] 03
 */
bool OvmsHyundaiIoniqEv::FoldMirrors(bool on)
{
  XARM("OvmsHyundaiIoniqEv::FoldMirrors");
  bool res = false;
  /*
  if ( iq_shift_status == IqShiftStatus::Park ) {
    Set_BCM_TP_TimeOut(on, 10);  //Keep BCM in test mode for up to 10 seconds
    res = Send_BCM_Command(0xb0, on ? 0x5b : 0x5c, 0x03);
  }
  */
  XDISARM;
  return res;
}

/**
 * ACC - relay
 */
bool OvmsHyundaiIoniqEv::ACCRelay(bool on, const char *password)
{
  XARM("OvmsHyundaiIoniqEv::ACCRelay");
  bool res = false;
  /*
  if (PinCheck((char *)password)) {
    if ( iq_shift_status == IqShiftStatus::Park ) {
      if ( on ) {
        res = Send_SMK_Command(7, 0xb1, 0x08, 0x03, 0x0a, 0x0a, 0x05);
      }
      else {
        res = Send_SMK_Command(4, 0xb1, 0x08, 0, 0, 0, 0);
      }
    }
  }
  */
  XDISARM;
  return res;
}

/**
 * IGN1 - relay
 */
bool OvmsHyundaiIoniqEv::IGN1Relay(bool on, const char *password)
{
  XARM("OvmsHyundaiIoniqEv::IGN1Relay");
  bool res = false;
  /*
  if (PinCheck((char *)password)) {
    if ( iq_shift_status == IqShiftStatus::Park ) {
      if ( on ) {
        res = Send_SMK_Command(7, 0xb1, 0x09, 0x03, 0x0a, 0x0a, 0x05);
      }
      else {
        res = Send_SMK_Command(4, 0xb1, 0x09, 0, 0, 0, 0);
      }
    }
  }
  */
  XDISARM;
  return res;
}

/**
 * IGN2 - relay
 */
bool OvmsHyundaiIoniqEv::IGN2Relay(bool on, const char *password)
{
  XARM("OvmsHyundaiIoniqEv::IGN2Relay");
  bool res = false;
  /*
  if (PinCheck((char *)password)) {
    if ( iq_shift_status == IqShiftStatus::Park ) {
      if ( on ) {
        res = Send_SMK_Command(7, 0xb1, 0x0a, 0x03, 0x0a, 0x0a, 0x05);
      }
      else {
        res = Send_SMK_Command(4, 0xb1, 0x0a, 0, 0, 0, 0);
      }
    }
  }
  */
  XDISARM;
  return res;
}

/**
 * Start - relay
 */
bool OvmsHyundaiIoniqEv::StartRelay(bool on, const char *password)
{
  XARM("OvmsHyundaiIoniqEv::StartRelay");
  bool res = false;
  /*
  if (PinCheck((char *)password)) {
    if ( iq_shift_status == IqShiftStatus::Park ) {
      if ( on ) {
        res = Send_SMK_Command(7, 0xb1, 0x0b, 0x03, 0x02, 0x02, 0x01);
      }
      else {
        res = Send_SMK_Command(4, 0xb1, 0x0b, 0, 0, 0, 0);
      }
    }
  }
  */
  XDISARM;
  return res;
}

/**
 * RequestNotify: send notifications / alerts / data updates
 */
void OvmsHyundaiIoniqEv::RequestNotify(unsigned int which)
{
  XARM("OvmsHyundaiIoniqEv::RequestNotify");
  kn_notifications |= which;
  XDISARM;
}

void OvmsHyundaiIoniqEv::DoNotify()
{
  XARM("OvmsHyundaiIoniqEv::DoNotify");
  unsigned int which = kn_notifications;

  if (which & SEND_EmergencyAlert) {
    //TODO MyNotify.NotifyCommand("alert", "Emergency.Alert","Emergency alert signals are turned on");
    kn_notifications &= ~SEND_EmergencyAlert;
  }

  if (which & SEND_EmergencyAlertOff) {
    //TODO MyNotify.NotifyCommand("alert", "Emergency.Alert","Emergency alert signals are turned off");
    kn_notifications &= ~SEND_EmergencyAlertOff;
  }
  XDISARM;

}


/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsHyundaiIoniqEv::SetFeature(int key, const char *value)
{
  XARM("OvmsHyundaiIoniqEv::SetFeature");
  switch (key) {
    case 10:
      MyConfig.SetParamValue("xiq", "suffsoc", value);
      XDISARM;
      return true;
    case 11:
      MyConfig.SetParamValue("xiq", "suffrange", value);
      XDISARM;
      return true;
    case 12:
      MyConfig.SetParamValue("xiq", "maxrange", value);
      XDISARM;
      return true;
    case 15: {
#ifdef XIQ_CAN_WRITE
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xiq", "canwrite",  (bits & 1) != 0);
      XDISARM;
      return true;
#else
      XDISARM;
      return false;
#endif
    }
    default:
      XDISARM;
      return OvmsVehicle::SetFeature(key, value);
  }
  XDISARM;
}


/**
 * GetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
const std::string OvmsHyundaiIoniqEv::GetFeature(int key)
{
  XARM("OvmsHyundaiIoniqEv::GetFeature");
  std::string res;
  switch (key) {
    case 0:
    case 10:
      res = MyConfig.GetParamValue("xiq", "suffsoc", STR(0));
      break;
    case 11:
      res = MyConfig.GetParamValue("xiq", "suffrange", STR(0));
      break;
    case 12:
      res = MyConfig.GetParamValue("xiq", "maxrange", STR(CFG_DEFAULT_MAXRANGE));
      break;
    case 15: {
#ifdef XIQ_CAN_WRITE
      int bits =
        ( MyConfig.GetParamValueBool("xiq", "canwrite",  false) ?  1 : 0);
      char buf[4];
      snprintf(buf, 4, "%d", bits);
      res = std::string(buf);
      break;
#else
      res = std::string("unavailable");
      break;
#endif
    }
    default:
      res = OvmsVehicle::GetFeature(key);
      break;
  }
  XDISARM;
  return res;
}


class OvmsHyundaiIoniqEvInit
{
public:
  OvmsHyundaiIoniqEvInit();
} MyOvmsHyundaiIoniqEvInit  __attribute__ ((init_priority (9000)));

OvmsHyundaiIoniqEvInit::OvmsHyundaiIoniqEvInit()
{
  ESP_LOGI(OvmsHyundaiIoniqEv::TAG, "Registering Vehicle: %s (9000)", OvmsHyundaiIoniqEv::FULL_NAME);

  MyVehicleFactory.RegisterVehicle<OvmsHyundaiIoniqEv>(OvmsHyundaiIoniqEv::VEHICLE_TYPE, OvmsHyundaiIoniqEv::FULL_NAME);
}
