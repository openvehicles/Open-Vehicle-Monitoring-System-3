/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: general utilities
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
// static const char *TAG = "v-twizy";

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

#include "vehicle_renaulttwizy.h"

using namespace std;


/**
 * UpdateMaxRange: get MAXRANGE with temperature compensation
 *  (Reminder: this could be a listener on ms_v_bat_temp…)
 */
void OvmsVehicleRenaultTwizy::UpdateMaxRange()
{
  float bat_temp = StdMetrics.ms_v_bat_temp->AsFloat();
  if (bat_temp <= -40)
    bat_temp = 20; // not yet measured
  
  // Temperature compensation:
  //   - assumes standard cfg_maxrange specified at 20°C
  //   - temperature influence approximation: 0.6 km / °C
  twizy_maxrange = cfg_maxrange - ((20 - bat_temp) * 0.6);
  if (twizy_maxrange < 0)
    twizy_maxrange = 0;
  
  *StdMetrics.ms_v_bat_range_full = (float) twizy_maxrange;
}


/**
 * UpdateChargeTimes:
 */
void OvmsVehicleRenaultTwizy::UpdateChargeTimes()
{
  float maxrange;
  
  // get max ideal range:
  UpdateMaxRange();
  if (twizy_maxrange > 0)
    maxrange = twizy_maxrange;
  else
  {
    // estimate max range:
    if (twizy_soc_min && twizy_soc_min_range)
      maxrange = twizy_soc_min_range / twizy_soc_min * 10000;
    else
      maxrange = 0;
  }
  
  // calculate ETR for SUFFSOC:
  if (cfg_suffsoc == 0)
    *StdMetrics.ms_v_charge_duration_soc = (int) -1;
  else
    *StdMetrics.ms_v_charge_duration_soc = (int) CalcChargeTime(cfg_suffsoc * 100);
  
  // calculate ETR for SUFFRANGE:
  if (cfg_suffrange == 0)
    *StdMetrics.ms_v_charge_duration_range = (int) -1;
  else
    *StdMetrics.ms_v_charge_duration_range = (int)
      (maxrange > 0) ? CalcChargeTime(cfg_suffrange * 10000 / maxrange) : 0;
  
  // calculate ETR for full charge:
  *StdMetrics.ms_v_charge_duration_full = (int) CalcChargeTime(10000);
}


/**
 * ChargeTime:
 *  Utility: calculate estimated charge time in minutes
 *  to reach dstsoc from current SOC
 *  (dstsoc in 1/100 %)
 */

// Charge time approximation constants:
// @ ~13 °C  -- temperature compensation needed?
#define CHARGETIME_CVSOC    9400L    // CV phase normally begins at 93..95%
#define CHARGETIME_CC       180L     // CC phase time (160..180 min.)
#define CHARGETIME_CV       40L      // CV phase time (topoff) (20..40 min.)

int OvmsVehicleRenaultTwizy::CalcChargeTime(int dstsoc)
{
  int minutes, d;
  float amps = 0;
  
  if (dstsoc > 10000)
    dstsoc = 10000;
  
  if (twizy_soc >= dstsoc)
    return 0;
  
  minutes = 0;
  
  if (dstsoc > CHARGETIME_CVSOC)
  {
    // CV phase
    if (twizy_soc < CHARGETIME_CVSOC)
      minutes += (((long) dstsoc - CHARGETIME_CVSOC) * CHARGETIME_CV
        + ((10000-CHARGETIME_CVSOC)/2)) / (10000-CHARGETIME_CVSOC);
    else
      minutes += (((long) dstsoc - twizy_soc) * CHARGETIME_CV
        + ((10000-CHARGETIME_CVSOC)/2)) / (10000-CHARGETIME_CVSOC);
    
    dstsoc = CHARGETIME_CVSOC;
  }
  
  // CC phase
  if (twizy_soc < dstsoc)
  {
    // default time:
    d = (((long) dstsoc - twizy_soc) * CHARGETIME_CC
      + (CHARGETIME_CVSOC/2)) / CHARGETIME_CVSOC;

    // correction for reduced/higher charge power:
    if (twizy_flags.Charging) {
      // charging, use actual current reading:
      amps = (float) -twizy_current / 4;
    }
    else if (twizy_flags.EnableWrite && cfg_chargelevel > 0) {
      // charge level control is active, assume:
      if (cfg_chargelevel == 7)
        amps = 32;
      else
        amps = cfg_chargelevel * 5;
    }
    // scale duration linearly from 32A:
    if (amps > 0)
      d *= 32.0f / amps;

    minutes += d;
  }
  
  return minutes;
}


