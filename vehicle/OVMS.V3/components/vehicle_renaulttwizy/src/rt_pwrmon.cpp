/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy power monitor
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

#include <math.h>

#include "rt_pwrmon.h"
#include "metrics_standard.h"

#include "vehicle_renaulttwizy.h"


/**
 * PowerInit:
 */
void OvmsVehicleRenaultTwizy::PowerInit()
{
  // init metrics
  
  twizy_speedpwr[0].InitMetrics("x.rt.p.stats.cst.", Kph);
  twizy_speedpwr[1].InitMetrics("x.rt.p.stats.acc.", KphPS);
  twizy_speedpwr[2].InitMetrics("x.rt.p.stats.dec.", KphPS);
  
  twizy_levelpwr[0].InitMetrics("x.rt.p.stats.lup.");
  twizy_levelpwr[1].InitMetrics("x.rt.p.stats.ldn.");
  
}


/**
 * PowerUpdate:
 */
void OvmsVehicleRenaultTwizy::PowerUpdate()
{
  PowerCollectData();
  
  // publish metrics:
  
  unsigned long pwr;
  
  *StdMetrics.ms_v_bat_power = (float) twizy_power * 64 / 10000;
  
  pwr = twizy_speedpwr[CAN_SPEED_CONST].use
      + twizy_speedpwr[CAN_SPEED_ACCEL].use
      + twizy_speedpwr[CAN_SPEED_DECEL].use;
  
  *StdMetrics.ms_v_bat_energy_used = (float) pwr / WH_DIV / 1000;
  
  pwr = twizy_speedpwr[CAN_SPEED_CONST].rec
      + twizy_speedpwr[CAN_SPEED_ACCEL].rec
      + twizy_speedpwr[CAN_SPEED_DECEL].rec;
  
  *StdMetrics.ms_v_bat_energy_recd = (float) pwr / WH_DIV / 1000;
  
  for (speedpwr &stats : twizy_speedpwr)
    stats.UpdateMetrics();
  
  for (levelpwr &stats : twizy_levelpwr)
    stats.UpdateMetrics();
  
}


/**
 * PowerCollectData:
 */
void OvmsVehicleRenaultTwizy::PowerCollectData()
{
  long dist;
  int alt_diff, grade_perc;
  unsigned long coll_use, coll_rec;
  
  bool car_stale_gps = StdMetrics.ms_v_pos_gpslock->IsStale();
  int car_altitude = StdMetrics.ms_v_pos_altitude->AsInt();
  
  if ((twizy_status & CAN_STATUS_KEYON) == 0)
  {
    // CAR is turned off
    return;
  }
  else if (car_stale_gps == 0)
  {
    // no GPS for 2 minutes: reset section
    twizy_level_odo = 0;
    twizy_level_alt = 0;
    
    return;
  }
  else if (twizy_level_odo == 0)
  {
    // start new section:
    
    twizy_level_odo = twizy_odometer;
    twizy_level_alt = car_altitude;
    
    twizy_level_use = 0;
    twizy_level_rec = 0;
    
    return;
  }
  
  // calc section length:
  dist = (twizy_odometer - twizy_level_odo) * 10;
  if (dist < CAN_LEVEL_MINSECTLEN)
  {
    // section too short to collect
    return;
  }
  
  // OK, read + reset collected power sums:
  coll_use = twizy_level_use;
  coll_rec = twizy_level_rec;
  twizy_level_use = 0;
  twizy_level_rec = 0;
  
  // calc grade in percent:
  alt_diff = car_altitude - twizy_level_alt;
  grade_perc = (long) alt_diff * 100L / (long) dist;
  
  // set new section reference:
  twizy_level_odo = twizy_odometer;
  twizy_level_alt = car_altitude;
  
  // collect:
  if (grade_perc >= CAN_LEVEL_THRESHOLD)
  {
    twizy_levelpwr[CAN_LEVEL_UP].dist += dist; // in m
    twizy_levelpwr[CAN_LEVEL_UP].hsum += alt_diff; // in m
    twizy_levelpwr[CAN_LEVEL_UP].use += coll_use;
    twizy_levelpwr[CAN_LEVEL_UP].rec += coll_rec;
  }
  else if (grade_perc <= -CAN_LEVEL_THRESHOLD)
  {
    twizy_levelpwr[CAN_LEVEL_DOWN].dist += dist; // in m
    twizy_levelpwr[CAN_LEVEL_DOWN].hsum += -alt_diff; // in m
    twizy_levelpwr[CAN_LEVEL_DOWN].use += coll_use;
    twizy_levelpwr[CAN_LEVEL_DOWN].rec += coll_rec;
  }
  
}


/**
 * PowerReset:
 */
void OvmsVehicleRenaultTwizy::PowerReset()
{
  for (speedpwr &stats : twizy_speedpwr)
  {
    stats.dist = 0;
    stats.use = 0;
    stats.rec = 0;
    stats.spdsum = 0;
    stats.spdcnt = 0;
  }
  
  for (levelpwr &stats : twizy_levelpwr)
  {
    stats.dist = 0;
    stats.use = 0;
    stats.rec = 0;
    stats.hsum = 0;
  }
  
  twizy_speed_state = CAN_SPEED_CONST;
  twizy_speed_distref = twizy_dist;
  twizy_level_use = 0;
  twizy_level_rec = 0;
  twizy_charge_use = 0;
  twizy_charge_rec = 0;
  
  twizy_level_odo = 0;
  twizy_level_alt = 0;
  
  twizy_cc_charge = 0;
  twizy_cc_soc = 0;
  twizy_cc_power_level = 0;
}




#define PNAME(name) strdup((prefix + name).c_str())

void speedpwr::InitMetrics(const string prefix, metric_unit_t spdunit)
{
  m_dist = MyMetrics.InitFloat(PNAME("dist"), SM_STALE_HIGH, 0, Kilometers);
  m_used = MyMetrics.InitFloat(PNAME("used"), SM_STALE_HIGH, 0, kWh);
  m_recd = MyMetrics.InitFloat(PNAME("recd"), SM_STALE_HIGH, 0, kWh);
  m_spdavg = MyMetrics.InitFloat(PNAME("spdavg"), SM_STALE_HIGH, 0, spdunit);
  m_spdunit = spdunit;
}

void speedpwr::UpdateMetrics()
{
  *m_dist = (float) dist / 10000;
  *m_used = (float) use / WH_DIV / 1000;
  *m_recd = (float) rec / WH_DIV / 1000;
  if (spdcnt > 0)
    *m_spdavg = (float) (spdsum / spdcnt) / ((m_spdunit==Kph) ? 100 : 10);
  else
    *m_spdavg = (float) 0;
}


void levelpwr::InitMetrics(const string prefix)
{
  m_dist = MyMetrics.InitFloat(PNAME("dist"), SM_STALE_HIGH, 0, Kilometers);
  m_hsum = MyMetrics.InitFloat(PNAME("hsum"), SM_STALE_HIGH, 0, Meters);
  m_used = MyMetrics.InitFloat(PNAME("used"), SM_STALE_HIGH, 0, kWh);
  m_recd = MyMetrics.InitFloat(PNAME("recd"), SM_STALE_HIGH, 0, kWh);
}

void levelpwr::UpdateMetrics()
{
  *m_dist = (float) dist / 1000;
  *m_hsum = (float) hsum;
  *m_used = (float) use / WH_DIV / 1000;
  *m_recd = (float) rec / WH_DIV / 1000;
}

