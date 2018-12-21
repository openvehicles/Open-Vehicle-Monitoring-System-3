/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: general notifications, alerts & data updates
 *                              (see rt_battmon & rt_pwrmon for specific notifications)
 *
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ovms_log.h"
//static const char *TAG = "v-twizy";

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
#include "string_writer.h"

#include "vehicle_renaulttwizy.h"

using namespace std;


/**
 * RequestNotify & DoNotify: send notifications / alerts / data updates
 */

void OvmsVehicleRenaultTwizy::RequestNotify(unsigned int which)
{
  twizy_notifications |= which;
}

void OvmsVehicleRenaultTwizy::DoNotify()
{
  unsigned int which = twizy_notifications;

  // Send battery alert?
  if (which & SEND_BatteryAlert) {
    if (BatteryLock(0)) {
      StringWriter buf(200);
      FormatBatteryStatus(COMMAND_RESULT_NORMAL, &buf, 0);
      MyNotify.NotifyString("alert", "battery.status", buf.c_str());
      BatteryUnlock();
      twizy_notifications &= ~SEND_BatteryAlert;
    }
  }

  // Send sufficient charge notifications?
  if (which & SEND_SuffCharge) {
    StringWriter buf(200);
    CommandStat(COMMAND_RESULT_NORMAL, &buf);
    MyNotify.NotifyString("info", "charge.status.sufficient", buf.c_str());
    twizy_notifications &= ~SEND_SuffCharge;
  }

  // Send power usage statistics?
  if (which & SEND_PowerNotify) {
    MyNotify.NotifyCommand("info", "xrt.trip.report", "xrt power report");
    twizy_notifications &= ~SEND_PowerNotify;
  }

  // Send drive log?
  if (which & SEND_TripLog) {
    SendTripLog();
    twizy_notifications &= ~SEND_TripLog;
  }

  // Send GPS log update?
  if (which & SEND_GPSLog) {
    SendGPSLog();
    twizy_notifications &= ~SEND_GPSLog;
  }

  // Send battery status update?
  if (which & SEND_BatteryStats) {
    if (BatteryLock(0)) {
      //uint32_t ts = esp_log_timestamp();
      BatterySendDataUpdate(false);
      //ESP_LOGD(TAG, "time SEND_BatteryStats: %d ms", esp_log_timestamp()-ts);
      BatteryUnlock();
      twizy_notifications &= ~SEND_BatteryStats;
    }
  }

  // Send power usage update?
  if (which & SEND_PowerStats) {
    if (PowerIsModified())
      MyNotify.NotifyCommand("data", "xrt.power.log", "xrt power stats");
    twizy_notifications &= ~SEND_PowerStats;
  }

  // Send SEVCON SDO log update?
  if (which & SEND_SDOLog) {
    // TODO
    // stat = vehicle_twizy_sdolog_msgp(stat, 0x4600);
    // stat = vehicle_twizy_sdolog_msgp(stat, 0x4602);
    twizy_notifications &= ~SEND_SDOLog;
  }

}


/**
 * GetNotifyChargeStateDelay: framework hook
 */
int OvmsVehicleRenaultTwizy::GetNotifyChargeStateDelay(const char* state)
{
  // no need for a delay on the Twizy:
  return 0;
}


/**
 * NotifiedVehicleChargeState: framework hook
 */
void OvmsVehicleRenaultTwizy::NotifiedVehicleChargeState(const char* state)
{
  // add charge record to trip log:
  RequestNotify(SEND_TripLog);
}


/**
 * CommandStat: Twizy implementation of vehicle status output
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandStat(int verbosity, OvmsWriter* writer)
{
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  if (chargeport_open)
  {
    std::string charge_state = StdMetrics.ms_v_charge_state->AsString();

    if (charge_state != "done") {
      if (cfg_suffsoc > 0 && twizy_soc >= cfg_suffsoc*100 && cfg_suffrange > 0 && twizy_range_ideal >= cfg_suffrange)
        writer->puts("Sufficient SOC and range reached.");
      else if (cfg_suffsoc > 0 && twizy_soc >= cfg_suffsoc*100)
        writer->puts("Sufficient SOC level reached.");
      else if (cfg_suffrange > 0 && twizy_range_ideal >= cfg_suffrange)
        writer->puts("Sufficient range reached.");
    }
    
    // Translate state codes:
    if (charge_state == "charging")
      charge_state = "Charging";
    else if (charge_state == "topoff")
      charge_state = "Topping off";
    else if (charge_state == "done")
      charge_state = "Charge Done";
    else if (charge_state == "stopped")
      charge_state = "Charge Stopped";

    writer->puts(charge_state.c_str());

    // Power sums: battery input:
    float pwr_batt = StdMetrics.ms_v_bat_energy_recd->AsFloat() * 1000;
    // Grid drain estimation:
    //  charge efficiency is ~88.1% w/o 12V charge and ~86.4% with
    //  (depending on when to cut the grid connection)
    //  so we're using 87.2% as an average efficiency:
    float pwr_grid = pwr_batt / 0.872;
    writer->printf("CHG: %d (~%d) Wh\n", (int) pwr_batt, (int) pwr_grid);
  }
  else
  {
    // Charge port door is closed, not charging
    writer->puts("Not charging");
  }

  // Estimated charge time for 100%:
  if (twizy_soc < 10000)
  {
    int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
    if (duration_full)
      writer->printf("Full: %d min.\n", duration_full);
  }

  // Estimated + Ideal Range:
  const char* range_est = StdMetrics.ms_v_bat_range_est->AsString("?", rangeUnit, 0).c_str();
  const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("?", rangeUnit, 0).c_str();
  writer->printf("Range: %s - %s\n", range_est, range_ideal);

  // SOC + min/max:
  writer->printf("SOC: %s (%s..%s)\n",
    (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str(),
    (char*) m_batt_soc_min->AsString("-", Native, 1).c_str(),
    (char*) m_batt_soc_max->AsUnitString("-", Native, 1).c_str());

  // ODOMETER:
  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf("ODO: %s\n", odometer);

  // BATTERY CAPACITY:
  if (cfg_bat_cap_actual_prc > 0)
  {
    writer->printf("CAP: %.1f%% %s\n",
      cfg_bat_cap_actual_prc,
      StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
  }

  // BATTERY SOH:
  if (twizy_soh > 0)
  {
    writer->printf("SOH: %s\n",
      StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 0).c_str());
  }

  return Success;
}


/**
 * SendGPSLog: send "RT-GPS-Log" history record
 *  (GPS track log with extended power usage data)
 */
void OvmsVehicleRenaultTwizy::SendGPSLog()
{
  bool modified =
    StdMetrics.ms_v_pos_odometer->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_latitude->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_longitude->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_altitude->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_direction->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_gpsspeed->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_speed->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_power->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_energy_used->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_energy_recd->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_current->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_coulomb_used->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_coulomb_recd->IsModifiedAndClear(m_modifier);

  if (!modified)
    return;

  unsigned long pwr_dist, pwr_use, pwr_rec;
  signed int pwr_min, pwr_max, curr_min, curr_max;

  // Read power stats:

  pwr_dist = twizy_speedpwr[CAN_SPEED_CONST].dist
          + twizy_speedpwr[CAN_SPEED_ACCEL].dist
          + twizy_speedpwr[CAN_SPEED_DECEL].dist;

  pwr_use = twizy_speedpwr[CAN_SPEED_CONST].use
          + twizy_speedpwr[CAN_SPEED_ACCEL].use
          + twizy_speedpwr[CAN_SPEED_DECEL].use;

  pwr_rec = twizy_speedpwr[CAN_SPEED_CONST].rec
          + twizy_speedpwr[CAN_SPEED_ACCEL].rec
          + twizy_speedpwr[CAN_SPEED_DECEL].rec;

  // read & reset GPS section power & current min/max:
  pwr_min = twizy_power_min;
  pwr_max = twizy_power_max;
  curr_min = twizy_current_min;
  curr_max = twizy_current_max;
  twizy_power_min = twizy_current_min = 32767;
  twizy_power_max = twizy_current_max = -32768;

  // H type "RT-GPS-Log", recno = odometer, keep for 1 day
  ostringstream buf;
  buf
    << "RT-GPS-Log,"
    << (long) (StdMetrics.ms_v_pos_odometer->AsFloat(0, Miles) * 10) // in 1/10 mi
    << ",86400"
    << setprecision(8)
    << fixed
    << "," << StdMetrics.ms_v_pos_latitude->AsFloat()
    << "," << StdMetrics.ms_v_pos_longitude->AsFloat()
    << setprecision(0)
    << "," << StdMetrics.ms_v_pos_altitude->AsFloat()
    << "," << StdMetrics.ms_v_pos_direction->AsFloat()
    << "," << StdMetrics.ms_v_pos_speed->AsFloat()
    << "," << (int) StdMetrics.ms_v_pos_gpslock->AsBool()
    << "," << 120 - MAX(120, StdMetrics.ms_v_pos_latitude->Age())
    << "," << StdMetrics.ms_m_net_sq->AsInt()
    << "," << twizy_power * 64 / 10
    << "," << pwr_use / WH_DIV
    << "," << pwr_rec / WH_DIV
    << "," << pwr_dist / 10;

  if (pwr_min == 32767) {
    buf
      << ",0,0";
  } else {
    buf
      << "," << (pwr_min * 64 + 5) / 10
      << "," << (pwr_max * 64 + 5) / 10;
  }

  buf
    << setbase(16)
    << "," << (unsigned int) twizy_status
    << setbase(10)
    << "," << twizy_batt[0].max_drive_pwr * 500
    << "," << twizy_batt[0].max_recup_pwr * -500
    << setprecision(3)
    << "," << (float) m_sevcon->twizy_autodrive_level / 1000
    << "," << (float) m_sevcon->twizy_autorecup_level / 1000;

  if (curr_min == 32767) {
    buf
      << ",0,0";
  } else {
    buf
      << setprecision(2)
      << "," << (float) curr_min / 4
      << "," << (float) curr_max / 4;
  }

  MyNotify.NotifyString("data", "xrt.gps.log", buf.str().c_str());
}


/**
 * SendTripLog: send "RT-PWR-Log" history record
 */
void OvmsVehicleRenaultTwizy::SendTripLog()
{
  unsigned long pwr_dist, pwr_use, pwr_rec;

  // Read power stats:

  pwr_dist = twizy_speedpwr[CAN_SPEED_CONST].dist
          + twizy_speedpwr[CAN_SPEED_ACCEL].dist
          + twizy_speedpwr[CAN_SPEED_DECEL].dist;

  pwr_use = twizy_speedpwr[CAN_SPEED_CONST].use
          + twizy_speedpwr[CAN_SPEED_ACCEL].use
          + twizy_speedpwr[CAN_SPEED_DECEL].use;

  pwr_rec = twizy_speedpwr[CAN_SPEED_CONST].rec
          + twizy_speedpwr[CAN_SPEED_ACCEL].rec
          + twizy_speedpwr[CAN_SPEED_DECEL].rec;

  // H type "RT-PWR-Log"
  //   ,odometer_km,latitude,longitude,altitude
  //   ,chargestate,parktime_min
  //   ,soc,soc_min,soc_max
  //   ,power_used_wh,power_recd_wh,power_distance
  //   ,range_estim_km,range_ideal_km
  //   ,batt_volt,batt_volt_min,batt_volt_max
  //   ,batt_temp,batt_temp_min,batt_temp_max
  //   ,motor_temp,pem_temp
  //   ,trip_length_km,trip_soc_usage
  //   ,trip_avg_speed_kph,trip_avg_accel_kps,trip_avg_decel_kps
  //   ,charge_used_ah,charge_recd_ah,batt_capacity_prc
  //   ,chg_temp

  ostringstream buf;
  buf
    << "RT-PWR-Log,0,31536000" // recno = 0, keep for 365 days
    << fixed
    << setprecision(2)
    << "," << StdMetrics.ms_v_pos_odometer->AsFloat()
    << setprecision(8)
    << "," << StdMetrics.ms_v_pos_latitude->AsFloat()
    << "," << StdMetrics.ms_v_pos_longitude->AsFloat()
    << setprecision(0)
    << "," << StdMetrics.ms_v_pos_altitude->AsFloat()
    << "," << (twizy_flags.ChargePort ? twizy_chargestate : 0)
    << "," << StandardMetrics.ms_v_env_parktime->AsInt(0, Minutes)
    << setprecision(2)
    << "," << (float) twizy_soc / 100
    << "," << (float) twizy_soc_min / 100
    << "," << (float) twizy_soc_max / 100
    << setprecision(0)
    << "," << pwr_use / WH_DIV
    << "," << pwr_rec / WH_DIV
    << "," << pwr_dist / 10
    << "," << twizy_range_est
    << "," << twizy_range_ideal
    << setprecision(1)
    << "," << (float) twizy_batt[0].volt_act / 10
    << "," << (float) twizy_batt[0].volt_min / 10
    << "," << (float) twizy_batt[0].volt_max / 10
    << setprecision(0)
    << "," << CONV_Temp(twizy_batt[0].temp_act)
    << "," << CONV_Temp(twizy_batt[0].temp_min)
    << "," << CONV_Temp(twizy_batt[0].temp_max)
    << "," << StdMetrics.ms_v_mot_temp->AsInt()
    << "," << StdMetrics.ms_v_inv_temp->AsInt()
    << setprecision(2)
    << "," << (float) (twizy_odometer - twizy_odometer_tripstart) / 100
    << "," << (float) ABS((long)twizy_soc_tripstart - (long)twizy_soc_tripend) / 100

    << "," << (float) ((twizy_speedpwr[CAN_SPEED_CONST].spdcnt > 0)
          ? twizy_speedpwr[CAN_SPEED_CONST].spdsum / twizy_speedpwr[CAN_SPEED_CONST].spdcnt
          : 0) / 100 // avg speed kph
    << "," << (float) ((twizy_speedpwr[CAN_SPEED_ACCEL].spdcnt > 0)
          ? (twizy_speedpwr[CAN_SPEED_ACCEL].spdsum * 10) / twizy_speedpwr[CAN_SPEED_ACCEL].spdcnt
          : 0) / 100 // avg accel kph/s
    << "," << (float) ((twizy_speedpwr[CAN_SPEED_DECEL].spdcnt > 0)
          ? (twizy_speedpwr[CAN_SPEED_DECEL].spdsum * 10) / twizy_speedpwr[CAN_SPEED_DECEL].spdcnt
          : 0) / 100 // avg decel kph/s

    << "," << (float) twizy_charge_use / AH_DIV
    << "," << (float) twizy_charge_rec / AH_DIV
    << "," << (float) cfg_bat_cap_actual_prc
    << "," << StdMetrics.ms_v_charge_temp->AsInt()
    ;

  MyNotify.NotifyString("data", "xrt.trip.log", buf.str().c_str());
}
