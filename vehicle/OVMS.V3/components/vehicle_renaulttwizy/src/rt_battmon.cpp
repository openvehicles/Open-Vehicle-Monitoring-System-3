/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy battery monitor
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

#include "rt_battmon.h"
#include "metrics_standard.h"

#include "vehicle_renaulttwizy.h"


// Battery cell/cmod deviation alert thresholds:
#define BATT_DEV_TEMP_ALERT         3       // = 3 °C
#define BATT_DEV_VOLT_ALERT         6       // = 30 mV

// ...thresholds for overall stddev:
#define BATT_STDDEV_TEMP_WATCH      2       // = 2 °C
#define BATT_STDDEV_TEMP_ALERT      3       // = 3 °C
#define BATT_STDDEV_VOLT_WATCH      3       // = 15 mV
#define BATT_STDDEV_VOLT_ALERT      5       // = 25 mV

// watch/alert flags for overall stddev:
#define BATT_STDDEV_TEMP_FLAG       31  // bit #31
#define BATT_STDDEV_VOLT_FLAG       31  // bit #31



/**
 * BatteryInit:
 */
void OvmsVehicleRenaultTwizy::BatteryInit()
{
  int i;
  char prefix[50];
  
  // init metrics
  
  m_batt_pack_count = MyMetrics.InitInt("x.rt.b.pack.cnt", SM_STALE_HIGH, batt_pack_count);
  for (i = 0; i < BATT_PACKS; i++)
  {
    snprintf(prefix, sizeof prefix, "x.rt.b.pack.%d.", i+1);
    twizy_batt[i].InitMetrics(prefix);
  }
  
  m_batt_cmod_count = MyMetrics.InitInt("x.rt.b.cmod.cnt", SM_STALE_HIGH, batt_cmod_count);
  for (i = 0; i < BATT_CMODS; i++)
  {
    snprintf(prefix, sizeof prefix, "x.rt.b.cmod.%02d.", i+1);
    twizy_cmod[i].InitMetrics(prefix);
  }
  
  m_batt_cell_count = MyMetrics.InitInt("x.rt.b.cell.cnt", SM_STALE_HIGH, batt_cell_count);
  for (i = 0; i < BATT_CELLS; i++)
  {
    snprintf(prefix, sizeof prefix, "x.rt.b.cell.%02d.", i+1);
    twizy_cell[i].InitMetrics(prefix);
  }
  
}


/**
 * BatteryUpdate:
 *  - update pack layout
 *  - calculate voltage & temperature deviations
 *  - publish internal state to metrics
 */
void OvmsVehicleRenaultTwizy::BatteryUpdate()
{
  // update pack layout:
  
  if (twizy_cell[15].volt_act != 0 && twizy_cell[15].volt_act != 0x0fff)
    batt_cell_count = 16;
  if (twizy_cell[14].volt_act != 0 && twizy_cell[14].volt_act != 0x0fff)
    batt_cell_count = 15;
  else
    batt_cell_count = 14;
  *m_batt_cell_count = (int) batt_cell_count;
  
  if (twizy_cmod[7].temp_act != 0 && twizy_cmod[7].temp_act != 0xff)
    batt_cmod_count = 8;
  else
    batt_cmod_count = 7;
  *m_batt_cmod_count = (int) batt_cmod_count;
  
  
  // calculate voltage & temperature deviations:
  
  BatteryCheckDeviations();
  
  
  // publish internal state to metrics:
  
  *StdMetrics.ms_v_bat_soc = (float) twizy_soc / 100;
  
  *StdMetrics.ms_v_bat_soh = (float) twizy_soh;
  *StdMetrics.ms_v_bat_cac = (float) cfg_bat_cap_actual_prc / 100 * cfg_bat_cap_nominal_ah;
  // … add metrics for net capacity and cap_prc?
  
  *StdMetrics.ms_v_bat_voltage = (float) twizy_batt[0].volt_act / 10;
  *StdMetrics.ms_v_bat_current = (float) twizy_current / 4;
  
  for (battery_pack &pack : twizy_batt)
    pack.UpdateMetrics();
  
  for (battery_cmod &cmod : twizy_cmod)
    cmod.UpdateMetrics();
  
  for (battery_cell &cell : twizy_cell)
    cell.UpdateMetrics();
  
}


/**
 * BatteryReset: reset deviations, alerts & watches
 */
void OvmsVehicleRenaultTwizy::BatteryReset()
{
  for (battery_cell &cell : twizy_cell)
  {
    cell.volt_max = cell.volt_act;
    cell.volt_min = cell.volt_act;
    cell.volt_maxdev = 0;
  }
  
  for (battery_cmod &cmod : twizy_cmod)
  {
    cmod.temp_max = cmod.temp_act;
    cmod.temp_min = cmod.temp_act;
    cmod.temp_maxdev = 0;
  }
  
  for (battery_pack &pack : twizy_batt)
  {
    pack.volt_min = pack.volt_act;
    pack.volt_max = pack.volt_act;
    pack.cell_volt_stddev_max = 0;
    pack.cmod_temp_stddev_max = 0;
    pack.temp_watches = 0;
    pack.temp_alerts = 0;
    pack.volt_watches = 0;
    pack.volt_alerts = 0;
    pack.last_temp_alerts = 0;
    pack.last_volt_alerts = 0;
  }
}


/**
 * BatteryCheckDeviations:
 *  - calculate voltage & temperature deviations
 *  - set watch/alert flags accordingly
 */

void OvmsVehicleRenaultTwizy::BatteryCheckDeviations(void)
{
  UINT i;
  float stddev, absdev, dev, m;
  UINT32 sum, sqrsum;
  
  
  // *********** Temperatures: ************
  
  // build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;
  
  for (i = 0; i < batt_cmod_count; i++)
  {
    // Validate:
    if ((twizy_cmod[i].temp_act == 0) || (twizy_cmod[i].temp_act >= 0x0f0))
      break;
    
    // Remember min:
    if ((twizy_cmod[i].temp_min == 0) || (twizy_cmod[i].temp_act < twizy_cmod[i].temp_min))
      twizy_cmod[i].temp_min = twizy_cmod[i].temp_act;
    
    // Remember max:
    if ((twizy_cmod[i].temp_max == 0) || (twizy_cmod[i].temp_act > twizy_cmod[i].temp_max))
      twizy_cmod[i].temp_max = twizy_cmod[i].temp_act;
    
    // build sums:
    sum += twizy_cmod[i].temp_act;
    sqrsum += SQR((UINT32) twizy_cmod[i].temp_act);
  }
  
  if (i == batt_cmod_count)
  {
    // All values valid, process:
    
    m = (float) sum / batt_cmod_count;
    
    *StdMetrics.ms_v_bat_temp = (float) m - 40;
    
    stddev = sqrtf( ((float)sqrsum/batt_cmod_count) - SQR((float)sum/batt_cmod_count) );
    
    // check max stddev:
    if (stddev > twizy_batt[0].cmod_temp_stddev_max)
    {
      twizy_batt[0].cmod_temp_stddev_max = stddev;
      
      // switch to overall stddev alert mode?
      // (resetting cmod flags to build new alert set)
      if (stddev >= BATT_STDDEV_TEMP_ALERT)
      {
        twizy_batt[0].temp_alerts.reset();
        twizy_batt[0].temp_alerts.set(BATT_STDDEV_TEMP_FLAG);
      }
      else if (stddev >= BATT_STDDEV_TEMP_WATCH)
      {
        twizy_batt[0].temp_watches.reset();
        twizy_batt[0].temp_watches.set(BATT_STDDEV_TEMP_FLAG);
      }
    }
    
    // check cmod deviations:
    for (i = 0; i < batt_cmod_count; i++)
    {
      // deviation:
      dev = twizy_cmod[i].temp_act - m;
      absdev = ABS(dev);
      
      // Set watch/alert flags:
      // (applying overall thresholds only in stddev alert mode)
      if ((twizy_batt[0].temp_alerts[BATT_STDDEV_TEMP_FLAG]) && (absdev >= BATT_STDDEV_TEMP_ALERT))
        twizy_batt[0].temp_alerts.set(i);
      else if (absdev >= BATT_DEV_TEMP_ALERT)
        twizy_batt[0].temp_alerts.set(i);
      else if ((twizy_batt[0].temp_watches[BATT_STDDEV_TEMP_FLAG]) && (absdev >= BATT_STDDEV_TEMP_WATCH))
        twizy_batt[0].temp_watches.set(i);
      else if (absdev > stddev)
        twizy_batt[0].temp_watches.set(i);
      
      // Remember max deviation:
      if (absdev > ABS(twizy_cmod[i].temp_maxdev))
        twizy_cmod[i].temp_maxdev = dev;
    }
    
  } // if( i == batt_cmod_count )
  
  
  // ********** Voltages: ************
  
  // Battery pack:
  
  if ((twizy_batt[0].volt_min == 0) || (twizy_batt[0].volt_act < twizy_batt[0].volt_min))
    twizy_batt[0].volt_min = twizy_batt[0].volt_act;
  if ((twizy_batt[0].volt_max == 0) || (twizy_batt[0].volt_act > twizy_batt[0].volt_max))
    twizy_batt[0].volt_max = twizy_batt[0].volt_act;
  
  // Cells: build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;
  
  for (i = 0; i < batt_cell_count; i++)
  {
    // Validate:
    if ((twizy_cell[i].volt_act == 0) || (twizy_cell[i].volt_act >= 0x0f00))
      break;
    
    // Remember min:
    if ((twizy_cell[i].volt_min == 0) || (twizy_cell[i].volt_act < twizy_cell[i].volt_min))
      twizy_cell[i].volt_min = twizy_cell[i].volt_act;
    
    // Remember max:
    if ((twizy_cell[i].volt_max == 0) || (twizy_cell[i].volt_act > twizy_cell[i].volt_max))
      twizy_cell[i].volt_max = twizy_cell[i].volt_act;
    
    // build sums:
    sum += twizy_cell[i].volt_act;
    sqrsum += SQR((UINT32) twizy_cell[i].volt_act);
  }
  
  if (i == batt_cell_count)
  {
    // All values valid, process:
    
    m = (float) sum / batt_cell_count;
    
    stddev = sqrtf( ((float)sqrsum/batt_cell_count) - SQR((float)sum/batt_cell_count) );
    
    // check max stddev:
    if (stddev > twizy_batt[0].cell_volt_stddev_max)
    {
      twizy_batt[0].cell_volt_stddev_max = stddev;
      
      // switch to overall stddev alert mode?
      // (resetting cell flags to build new alert set)
      if (stddev >= BATT_STDDEV_VOLT_ALERT)
      {
        twizy_batt[0].volt_alerts.reset();
        twizy_batt[0].volt_alerts.set(BATT_STDDEV_VOLT_FLAG);
      }
      else if (stddev >= BATT_STDDEV_VOLT_WATCH)
      {
        twizy_batt[0].volt_watches.reset();
        twizy_batt[0].volt_watches.set(BATT_STDDEV_VOLT_FLAG);
      }
    }
    
    // check cell deviations:
    for (i = 0; i < batt_cell_count; i++)
    {
      // deviation:
      dev = twizy_cell[i].volt_act - m;
      absdev = ABS(dev);
      
      // Set watch/alert flags:
      // (applying overall thresholds only in stddev alert mode)
      if ((twizy_batt[0].volt_alerts[BATT_STDDEV_VOLT_FLAG]) && (absdev >= BATT_STDDEV_VOLT_ALERT))
        twizy_batt[0].volt_alerts.set(i);
      else if (absdev >= BATT_DEV_VOLT_ALERT)
        twizy_batt[0].volt_alerts.set(i);
      else if ((twizy_batt[0].volt_watches[BATT_STDDEV_VOLT_FLAG]) && (absdev >= BATT_STDDEV_VOLT_WATCH))
        twizy_batt[0].volt_watches.set(i);
      else if (absdev > stddev)
        twizy_batt[0].volt_watches.set(i);
      
      // Remember max deviation:
      if (absdev > ABS(twizy_cell[i].volt_maxdev))
        twizy_cell[i].volt_maxdev = dev;
    }
    
  } // if( i == batt_cell_count )
  
  
  // Battery monitor update/alert:
  if ((twizy_batt[0].volt_alerts != twizy_batt[0].last_volt_alerts)
    || (twizy_batt[0].temp_alerts != twizy_batt[0].last_temp_alerts))
  {
    // TODO twizy_notify(SEND_BatteryAlert | SEND_BatteryStats);
  }
  
  
}



#define PNAME(name) strdup((prefix + name).c_str())

void battery_pack::InitMetrics(const string prefix)
{
  m_volt_min = MyMetrics.InitFloat(PNAME("voltage.min"), SM_STALE_HIGH, 0, Volts);
  m_volt_max = MyMetrics.InitFloat(PNAME("voltage.max"), SM_STALE_HIGH, 0, Volts);
  
  //ESP_LOGI("battery_pack", "sizeof(bitset<32>) = %d", sizeof(volt_watches));
  
  m_volt_watches = MyMetrics.InitBitset<32>(PNAME("voltage.watches"), SM_STALE_HIGH);
  m_volt_alerts = MyMetrics.InitBitset<32>(PNAME("voltage.alerts"), SM_STALE_HIGH);
  
  m_temp_watches = MyMetrics.InitBitset<32>(PNAME("temp.watches"), SM_STALE_HIGH);
  m_temp_alerts = MyMetrics.InitBitset<32>(PNAME("temp.alerts"), SM_STALE_HIGH);
  
  m_cell_volt_stddev_max = MyMetrics.InitFloat(PNAME("voltage.stddev.max"), SM_STALE_HIGH, 0, Volts);
  m_cmod_temp_stddev_max = MyMetrics.InitFloat(PNAME("temp.stddev.max"), SM_STALE_HIGH, 0, Celcius);
}

void battery_pack::UpdateMetrics()
{
  *m_volt_min = (float) volt_min / 10;
  *m_volt_max = (float) volt_max / 10;
  
  *m_volt_watches = volt_watches;
  *m_volt_alerts = volt_alerts;
  
  *m_temp_watches = temp_watches;
  *m_temp_alerts = temp_alerts;

  *m_cell_volt_stddev_max = (float) cell_volt_stddev_max / 200;
  *m_cmod_temp_stddev_max = (float) cmod_temp_stddev_max;
}


void battery_cmod::InitMetrics(const string prefix)
{
  m_temp_act = MyMetrics.InitFloat(PNAME("temp.act"), SM_STALE_HIGH, 0, Celcius);
  m_temp_min = MyMetrics.InitFloat(PNAME("temp.min"), SM_STALE_HIGH, 0, Celcius);
  m_temp_max = MyMetrics.InitFloat(PNAME("temp.max"), SM_STALE_HIGH, 0, Celcius);
  m_temp_maxdev = MyMetrics.InitFloat(PNAME("temp.maxdev"), SM_STALE_HIGH, 0, Celcius);
}

void battery_cmod::UpdateMetrics()
{
  *m_temp_act = (float) temp_act - 40;
  *m_temp_min = (float) temp_min - 40;
  *m_temp_max = (float) temp_max - 40;
  *m_temp_maxdev = (float) temp_maxdev;
}


void battery_cell::InitMetrics(const string prefix)
{
  m_volt_act = MyMetrics.InitFloat(PNAME("voltage.act"), SM_STALE_HIGH, 0, Volts);
  m_volt_min = MyMetrics.InitFloat(PNAME("voltage.min"), SM_STALE_HIGH, 0, Volts);
  m_volt_max = MyMetrics.InitFloat(PNAME("voltage.max"), SM_STALE_HIGH, 0, Volts);
  m_volt_maxdev = MyMetrics.InitFloat(PNAME("voltage.maxdev"), SM_STALE_HIGH, 0, Volts);
}

void battery_cell::UpdateMetrics()
{
  *m_volt_act = (float) volt_act / 200;
  *m_volt_min = (float) volt_min / 200;
  *m_volt_max = (float) volt_max / 200;
  *m_volt_maxdev = (float) volt_maxdev / 200;
}


