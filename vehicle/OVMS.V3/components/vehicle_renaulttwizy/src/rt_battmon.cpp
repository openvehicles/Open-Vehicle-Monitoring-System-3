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

#include "ovms_log.h"
static const char *TAG = "v-twizy";

#include <math.h>

#include "rt_battmon.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "string_writer.h"

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
 * vehicle_twizy_batt: command wrapper for CommandBatt
 */
void vehicle_twizy_batt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = (OvmsVehicleRenaultTwizy*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();

  if (!twizy || type != "RT")
  {
    writer->puts("Error: Twizy vehicle module not selected");
    return;
  }

  twizy->CommandBatt(verbosity, writer, cmd, argc, argv);
}


/**
 * BatteryInit:
 */
void OvmsVehicleRenaultTwizy::BatteryInit()
{
  ESP_LOGI(TAG, "battmon subsystem init");

  twizy_batt_sensors_state = BATT_SENSORS_START;
  m_batt_doreset = false;

  // init sensor lock:

  m_batt_sensors = xSemaphoreCreateBinary();
  xSemaphoreGive(m_batt_sensors);

  // init metrics

  m_batt_use_soc_min = MyMetrics.InitFloat("xrt.b.u.soc.min", SM_STALE_HIGH, (float) twizy_soc_min / 100, Percentage);
  m_batt_use_soc_max = MyMetrics.InitFloat("xrt.b.u.soc.max", SM_STALE_HIGH, (float) twizy_soc_max / 100, Percentage);
  m_batt_use_volt_min = MyMetrics.InitFloat("xrt.b.u.volt.min", SM_STALE_HIGH, 0, Volts);
  m_batt_use_volt_max = MyMetrics.InitFloat("xrt.b.u.volt.max", SM_STALE_HIGH, 0, Volts);
  m_batt_use_temp_min = MyMetrics.InitFloat("xrt.b.u.temp.min", SM_STALE_HIGH, 0, Celcius);
  m_batt_use_temp_max = MyMetrics.InitFloat("xrt.b.u.temp.max", SM_STALE_HIGH, 0, Celcius);

  twizy_bms_type  = BMS_TYPE_ORIG;
  m_bms_type      = MyMetrics.InitInt("xrt.bms.type", SM_STALE_HIGH, BMS_TYPE_ORIG);
  m_bms_state1    = MyMetrics.InitInt("xrt.bms.state1", SM_STALE_HIGH, 0);
  m_bms_state2    = MyMetrics.InitInt("xrt.bms.state2", SM_STALE_HIGH, 0);
  m_bms_error     = MyMetrics.InitInt("xrt.bms.error", SM_STALE_HIGH, 0);
  m_bms_balancing = MyMetrics.InitBitset<16>("xrt.bms.balancing", SM_STALE_HIGH, 0);
  m_bms_temp      = MyMetrics.InitFloat("xrt.bms.temp", SM_STALE_HIGH, 0, Celcius);

  // BMS configuration:
  //    Note: layout initialized to 2 voltages + 1 temperature per module,
  //          custom layouts will be configured by CAN frame 0x700 (see rt_can)
  BmsSetCellArrangementVoltage(batt_cell_count, 2);
  BmsSetCellArrangementTemperature(batt_cmod_count, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

  // init commands

  cmd_batt = cmd_xrt->RegisterCommand("batt", "Battery monitor");
  {
    cmd_batt->RegisterCommand("reset", "Reset alerts & watches", vehicle_twizy_batt);
    cmd_batt->RegisterCommand("status", "Status report", vehicle_twizy_batt, "[<pack>]", 0, 1);
    cmd_batt->RegisterCommand("volt", "Show voltages", vehicle_twizy_batt);
    cmd_batt->RegisterCommand("vdev", "Show voltage deviations", vehicle_twizy_batt);
    cmd_batt->RegisterCommand("temp", "Show temperatures", vehicle_twizy_batt);
    cmd_batt->RegisterCommand("tdev", "Show temperature deviations", vehicle_twizy_batt);
    cmd_batt->RegisterCommand("data-pack", "Output pack record", vehicle_twizy_batt, "[<pack>]", 0, 1);
    cmd_batt->RegisterCommand("data-cell", "Output cell record", vehicle_twizy_batt, "<cell>", 1, 1);
  }
}


bool OvmsVehicleRenaultTwizy::BatteryLock(int maxwait_ms)
{
  if (m_batt_sensors && xSemaphoreTake(m_batt_sensors, pdMS_TO_TICKS(maxwait_ms)) == pdTRUE)
    return true;
  return false;
}

void OvmsVehicleRenaultTwizy::BatteryUnlock()
{
  if (m_batt_sensors)
    xSemaphoreGive(m_batt_sensors);
}


/**
 * BatteryUpdateMetrics: publish internal state to metrics
 */
void OvmsVehicleRenaultTwizy::BatteryUpdateMetrics()
{
  *StdMetrics.ms_v_bat_temp = (float) twizy_batt[0].temp_act - 40;
  *StdMetrics.ms_v_bat_voltage = (float) twizy_batt[0].volt_act / 10;

  *m_batt_use_volt_min = (float) twizy_batt[0].volt_min / 10;
  *m_batt_use_volt_max = (float) twizy_batt[0].volt_max / 10;

  *m_batt_use_temp_min = (float) twizy_batt[0].temp_min - 40;
  *m_batt_use_temp_max = (float) twizy_batt[0].temp_max - 40;

  float vmin = MyConfig.GetParamValueFloat("xrt", "cell_volt_min", 3.165);
  float vmax = MyConfig.GetParamValueFloat("xrt", "cell_volt_max", 4.140);
  *StdMetrics.ms_v_bat_pack_level_min = (float) TRUNCPREC((((float)twizy_batt[0].cell_volt_min/200)-vmin) / (vmax-vmin) * 100, 3);
  *StdMetrics.ms_v_bat_pack_level_max = (float) TRUNCPREC((((float)twizy_batt[0].cell_volt_max/200)-vmin) / (vmax-vmin) * 100, 3);
  *StdMetrics.ms_v_bat_pack_level_avg = (float) TRUNCPREC(((twizy_batt[0].cell_volt_avg/200)-vmin) / (vmax-vmin) * 100, 3);
  *StdMetrics.ms_v_bat_pack_level_stddev = (float) TRUNCPREC((twizy_batt[0].cell_volt_stddev/200) / (vmax-vmin) * 100, 3);
  
  int i;
  for (i = 0; i < batt_cmod_count; i++)
    BmsSetCellTemperature(i, (float) twizy_cmod[i].temp_act - 40);
  for (i = 0; i < batt_cell_count; i++)
    BmsSetCellVoltage(i, (float) twizy_cell[i].volt_act / 200);
}


/**
 * BatteryUpdate:
 *  - executed in CAN RX task when sensor data is complete
 *  - commit sensor RX buffers
 *  - calculate voltage & temperature deviations
 *  - publish internal state to metrics
 */
void OvmsVehicleRenaultTwizy::BatteryUpdate()
{
  // reset sensor fetch cycle:
  twizy_batt_sensors_state = BATT_SENSORS_START;

  // try to lock, else drop this update:
  if (!BatteryLock(0)) {
    ESP_LOGV(TAG, "BatteryUpdate: no lock, update dropped");
    return;
  }

  // copy RX buffers:
  for (battery_cell &cell : twizy_cell)
    cell.volt_act = cell.volt_new;
  for (battery_cmod &cmod : twizy_cmod)
    cmod.temp_act = cmod.temp_new;
  for (battery_pack &pack : twizy_batt)
    pack.volt_act = pack.volt_new;

  // reset min/max if due:
  if (m_batt_doreset) {
    BatteryReset();
    m_batt_doreset = false;
  }

  // calculate voltage & temperature deviations:
  BatteryCheckDeviations();

  // publish internal state to metrics:
  BatteryUpdateMetrics();

  // done, start next fetch cycle:
  BatteryUnlock();
}


/**
 * BatteryReset: reset deviations, alerts & watches
 */
void OvmsVehicleRenaultTwizy::BatteryReset()
{
  ESP_LOGD(TAG, "battmon reset");

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
    pack.temp_min = pack.temp_act;
    pack.temp_max = pack.temp_act;
    pack.cmod_temp_stddev_max = 0;
    pack.temp_watches = 0;
    pack.temp_alerts = 0;
    pack.volt_watches = 0;
    pack.volt_alerts = 0;
    pack.last_temp_alerts = 0;
    pack.last_volt_alerts = 0;
  }
  
  BmsResetCellStats();
  BatteryUpdateMetrics();
}


/**
 * BatteryCheckDeviations:
 *  - calculate voltage & temperature deviations
 *  - set watch/alert flags accordingly
 */

void OvmsVehicleRenaultTwizy::BatteryCheckDeviations(void)
{
  int i;
  int stddev, absdev;
  int dev;
  double sd, m;
  long sum, sqrsum;
  int min, max;


  // *********** Temperatures: ************

  // build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;
  min = 240;
  max = 0;

  for (i = 0; i < batt_cmod_count; i++)
  {
    // Validate:
    if ((twizy_cmod[i].temp_act == 0) || (twizy_cmod[i].temp_act >= 0x0f0))
      break;

    // Remember cell min/max:
    if ((twizy_cmod[i].temp_min == 0) || (twizy_cmod[i].temp_act < twizy_cmod[i].temp_min))
      twizy_cmod[i].temp_min = twizy_cmod[i].temp_act;
    if ((twizy_cmod[i].temp_max == 0) || (twizy_cmod[i].temp_act > twizy_cmod[i].temp_max))
      twizy_cmod[i].temp_max = twizy_cmod[i].temp_act;
    
    // Set pack min/max:
    if (twizy_cmod[i].temp_act < min)
      min = twizy_cmod[i].temp_act;
    if (twizy_cmod[i].temp_act > max)
      max = twizy_cmod[i].temp_act;

    // build sums:
    sum += twizy_cmod[i].temp_act;
    sqrsum += SQR(twizy_cmod[i].temp_act);
  }

  if (i == batt_cmod_count)
  {
    // All values valid, process:

    m = (double) sum / batt_cmod_count;

    twizy_batt[0].temp_act = m;

    // Battery pack usage cycle min/max:
    if ((twizy_batt[0].temp_min == 0) || (twizy_batt[0].temp_act < twizy_batt[0].temp_min))
      twizy_batt[0].temp_min = twizy_batt[0].temp_act;
    if ((twizy_batt[0].temp_max == 0) || (twizy_batt[0].temp_act > twizy_batt[0].temp_max))
      twizy_batt[0].temp_max = twizy_batt[0].temp_act;

    sd = sqrt( ((double)sqrsum/batt_cmod_count) - SQR((double)sum/batt_cmod_count) );
    stddev = sd + 0.5;
    if (stddev == 0)
      stddev = 1; // not enough precision to allow stddev 0

    twizy_batt[0].cmod_temp_min = min;
    twizy_batt[0].cmod_temp_max = max;
    twizy_batt[0].cmod_temp_avg = m;
    twizy_batt[0].cmod_temp_stddev = sd;

    // check max stddev:
    if (stddev > twizy_batt[0].cmod_temp_stddev_max)
    {
      twizy_batt[0].cmod_temp_stddev_max = stddev;
      twizy_batt[0].temp_alerts.reset();
      twizy_batt[0].temp_watches.reset();

      // switch to overall stddev alert mode?
      if (stddev >= BATT_STDDEV_TEMP_ALERT)
        twizy_batt[0].temp_alerts.set(BATT_STDDEV_TEMP_FLAG);
      else if (stddev >= BATT_STDDEV_TEMP_WATCH)
        twizy_batt[0].temp_watches.set(BATT_STDDEV_TEMP_FLAG);
    }

    // check cmod deviations:
    for (i = 0; i < batt_cmod_count; i++)
    {
      // deviation:
      dev = (twizy_cmod[i].temp_act - m)
              + ((twizy_cmod[i].temp_act >= m) ? 0.5 : -0.5);
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

  // Battery pack usage cycle min/max:

  if ((twizy_batt[0].volt_min == 0) || (twizy_batt[0].volt_act < twizy_batt[0].volt_min))
    twizy_batt[0].volt_min = twizy_batt[0].volt_act;
  if ((twizy_batt[0].volt_max == 0) || (twizy_batt[0].volt_act > twizy_batt[0].volt_max))
    twizy_batt[0].volt_max = twizy_batt[0].volt_act;

  // Cells: build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;
  min = 2000;
  max = 0;

  for (i = 0; i < batt_cell_count; i++)
  {
    // Validate:
    if ((twizy_cell[i].volt_act == 0) || (twizy_cell[i].volt_act >= 0x0f00))
      break;

    // Remember cell min/max:
    if ((twizy_cell[i].volt_min == 0) || (twizy_cell[i].volt_act < twizy_cell[i].volt_min))
      twizy_cell[i].volt_min = twizy_cell[i].volt_act;
    if ((twizy_cell[i].volt_max == 0) || (twizy_cell[i].volt_act > twizy_cell[i].volt_max))
      twizy_cell[i].volt_max = twizy_cell[i].volt_act;

    // Set pack min/max:
    if (twizy_cell[i].volt_act < min)
      min = twizy_cell[i].volt_act;
    if (twizy_cell[i].volt_act > max)
      max = twizy_cell[i].volt_act;

    // build sums:
    sum += twizy_cell[i].volt_act;
    sqrsum += SQR(twizy_cell[i].volt_act);
  }

  if (i == batt_cell_count)
  {
    // All values valid, process:

    m = (double) sum / batt_cell_count;

    sd = sqrt( ((double)sqrsum/batt_cell_count) - SQR((double)sum/batt_cell_count) );
    stddev = sd + 0.5;
    if (stddev == 0)
      stddev = 1; // not enough precision to allow stddev 0
    
    twizy_batt[0].cell_volt_min = min;
    twizy_batt[0].cell_volt_max = max;
    twizy_batt[0].cell_volt_avg = m;
    twizy_batt[0].cell_volt_stddev = sd;

    // check max stddev:
    if (stddev > twizy_batt[0].cell_volt_stddev_max)
    {
      twizy_batt[0].cell_volt_stddev_max = stddev;
      twizy_batt[0].volt_alerts.reset();
      twizy_batt[0].volt_watches.reset();

      // switch to overall stddev alert mode?
      if (stddev >= BATT_STDDEV_VOLT_ALERT)
        twizy_batt[0].volt_alerts.set(BATT_STDDEV_VOLT_FLAG);
      else if (stddev >= BATT_STDDEV_VOLT_WATCH)
        twizy_batt[0].volt_watches.set(BATT_STDDEV_VOLT_FLAG);
    }

    // check cell deviations:
    for (i = 0; i < batt_cell_count; i++)
    {
      // deviation:
      dev = (twizy_cell[i].volt_act - m)
              + ((twizy_cell[i].volt_act >= m) ? 0.5 : -0.5);
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
    RequestNotify(SEND_BatteryAlert | SEND_BatteryStats);
    twizy_batt[0].last_volt_alerts = twizy_batt[0].volt_alerts;
    twizy_batt[0].last_temp_alerts = twizy_batt[0].temp_alerts;
  }
}


/**
 * CommandBatt: batt reset|status|volt|vdev|temp|tdev|data-pack|data-cell
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandBatt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  const char *subcmd = cmd->GetName();

  ESP_LOGV(TAG, "command batt %s, verbosity=%d", subcmd, verbosity);

  if (!BatteryLock(100))
  {
    writer->puts("ERROR: can't lock battery state, please retry");
    return Fail;
  }

  OvmsVehicleRenaultTwizy::vehicle_command_t res = Success;

  if (strcmp(subcmd, "reset") == 0)
  {
    BatteryReset();
    writer->puts("Battery monitor reset.");
  }

  else if (strcmp(subcmd, "status") == 0)
  {
    int pack = (argc > 0) ? atoi(argv[0]) : 1;
    if (pack < 1 || pack > batt_pack_count) {
      writer->printf("ERROR: pack number out of range [1-%d]\n", batt_pack_count);
      res = Fail;
    } else {
      FormatBatteryStatus(verbosity, writer, pack-1);
    }
  }

  else if (subcmd[0] == 'v')
  {
    // "volt"=absolute values, "vdev"=deviations
    FormatBatteryVolts(verbosity, writer, (subcmd[1]=='d') ? true : false);
  }

  else if (subcmd[0] == 't')
  {
    // "temp"=absolute values, "tdev"=deviations
    FormatBatteryTemps(verbosity, writer, (subcmd[1]=='d') ? true : false);
  }

  else if (strcmp(subcmd, "data-pack") == 0)
  {
    int pack = (argc > 0) ? atoi(argv[0]) : 1;
    if (pack < 1 || pack > batt_pack_count) {
      writer->printf("ERROR: pack number out of range [1-%d]\n", batt_pack_count);
      res = Fail;
    } else {
      FormatPackData(verbosity, writer, pack-1);
    }
  }

  else if (strcmp(subcmd, "data-cell") == 0)
  {
    int cell = (argc > 0) ? atoi(argv[0]) : 1;
    if (cell < 1 || cell > batt_cell_count) {
      writer->printf("ERROR: cell number out of range [1-%d]\n", batt_cell_count);
      res = Fail;
    } else {
      FormatCellData(verbosity, writer, cell-1);
    }
  }

  BatteryUnlock();

  return res;
}


/**
 * FormatBatteryStatus: output status report (alerts & watches)
 */
void OvmsVehicleRenaultTwizy::FormatBatteryStatus(int verbosity, OvmsWriter* writer, int pack)
{
  int capacity = verbosity;
  const char *em;
  int c, val;

  // Voltage deviations:
  capacity -= writer->printf("Volts: ");

  // standard deviation:
  val = CONV_CellVolt(twizy_batt[pack].cell_volt_stddev_max);
  if (twizy_batt[pack].volt_alerts.test(BATT_STDDEV_VOLT_FLAG))
    em = "!";
  else if (twizy_batt[pack].volt_watches.test(BATT_STDDEV_VOLT_FLAG))
    em = "?";
  else
    em = "";
  capacity -= writer->printf("%sSD:%dmV ", em, val);

  if ((twizy_batt[pack].volt_alerts.none()) && (twizy_batt[pack].volt_watches.none()))
  {
    capacity -= writer->printf("OK ");
  }
  else
  {
    for (c = 0; c < batt_cell_count; c++)
    {
      // check length:
      if (capacity < 12)
      {
        writer->puts("...");
        return;
      }

      // Alert / Watch?
      if (twizy_batt[pack].volt_alerts.test(c))
        em = "!";
      else if (twizy_batt[pack].volt_watches.test(c))
        em = "?";
      else
        continue;

      val = CONV_CellVoltS(twizy_cell[c].volt_maxdev);

      capacity -= writer->printf("%sC%d:%+dmV ", em, c+1, val);
    }
  }

  // check length:
  if (capacity < 20)
  {
    writer->puts("...");
    return;
  }

  // Temperature deviations:
  capacity -= writer->printf("Temps: ");

  // standard deviation:
  val = twizy_batt[pack].cmod_temp_stddev_max;
  if (twizy_batt[pack].temp_alerts.test(BATT_STDDEV_TEMP_FLAG))
    em = "!";
  else if (twizy_batt[pack].temp_watches.test(BATT_STDDEV_TEMP_FLAG))
    em = "?";
  else
    em = "";
  capacity -= writer->printf("%sSD:%dC ", em, val);

  if ((twizy_batt[pack].temp_alerts.none()) && (twizy_batt[pack].temp_watches.none()))
  {
    capacity -= writer->printf("OK ");
  }
  else
  {
    for (c = 0; c < batt_cmod_count; c++)
    {
      // check length:
      if (capacity < 8)
      {
        writer->puts("...");
        return;
      }

      // Alert / Watch?
      if (twizy_batt[pack].temp_alerts.test(c))
        em = "!";
      else if (twizy_batt[pack].temp_watches.test(c))
        em = "?";
      else
        continue;

      val = twizy_cmod[c].temp_maxdev;

      capacity -= writer->printf("%sM%d:%+dC ", em, c+1, val);
    }
  }

  writer->puts("");
}


/**
 * FormatBatteryVolts: output voltage report (absolute / deviations)
 */
void OvmsVehicleRenaultTwizy::FormatBatteryVolts(int verbosity, OvmsWriter* writer, bool show_deviations)
{
  int capacity = verbosity;
  const char *em;

  // Output pack status:
  for (int p = 0; p < batt_pack_count; p++)
  {
    // output capacity reached?
    if (capacity < 13)
      break;

    if (show_deviations)
    {
      if (twizy_batt[p].volt_alerts.test(BATT_STDDEV_VOLT_FLAG))
        em = "!";
      else if (twizy_batt[p].volt_watches.test(BATT_STDDEV_VOLT_FLAG))
        em = "?";
      else
        em = "";
      capacity -= writer->printf("%sSD:%dmV ", em, CONV_CellVolt(twizy_batt[p].cell_volt_stddev_max));
    }
    else
    {
      capacity -= writer->printf("P:%.2fV ", (float) CONV_PackVolt(twizy_batt[p].volt_act) / 100);
    }
  }

  // Output cell status:
  for (int c = 0; c < batt_cell_count; c++)
  {
    int p = 0; // fixed for now…

    // output capacity reached?
    if (capacity < 13)
      break;

    // Alert?
    if (twizy_batt[p].volt_alerts.test(c))
      em = "!";
    else if (twizy_batt[p].volt_watches.test(c))
      em = "?";
    else
      em = "";

    if (show_deviations)
      capacity -= writer->printf("%s%d:%+dmV ", em, c+1, CONV_CellVoltS(twizy_cell[c].volt_maxdev));
    else
      capacity -= writer->printf("%s%d:%.3fV ", em, c+1, (float) CONV_CellVolt(twizy_cell[c].volt_act) / 1000);
  }

  writer->puts("");
}


/**
 * FormatBatteryTemps: output temperature report (absolute / deviations)
 */
void OvmsVehicleRenaultTwizy::FormatBatteryTemps(int verbosity, OvmsWriter* writer, bool show_deviations)
{
  int capacity = verbosity;
  const char *em;

  // Output pack status:
  for (int p = 0; p < batt_pack_count; p++)
  {
    if (capacity < 17)
      break;

    if (show_deviations)
    {
      if (twizy_batt[p].temp_alerts.test(BATT_STDDEV_TEMP_FLAG))
        em = "!";
      else if (twizy_batt[p].temp_watches.test(BATT_STDDEV_TEMP_FLAG))
        em = "?";
      else
        em = "";
      capacity -= writer->printf("%sSD:%dC ", em, twizy_batt[0].cmod_temp_stddev_max);
    }
    else
    {
      capacity -= writer->printf("P:%dC (%dC..%dC) ",
        CONV_Temp(twizy_batt[p].temp_act),
        CONV_Temp(twizy_batt[p].temp_min),
        CONV_Temp(twizy_batt[p].temp_max));
    }
  }

  // Output cmod status:
  for (int c = 0; c < batt_cmod_count; c++)
  {
    int p = 0; // fixed for now…

    if (capacity < 8)
      break;

    // Alert?
    if (twizy_batt[p].temp_alerts.test(c))
      em = "!";
    else if (twizy_batt[p].temp_watches.test(c))
      em = "?";
    else
      em = "";

    if (show_deviations)
      capacity -= writer->printf("%s%d:%+dC ", em, c+1, twizy_cmod[c].temp_maxdev);
    else
      capacity -= writer->printf("%s%d:%dC ", em, c+1, CONV_Temp(twizy_cmod[c].temp_act));
  }

  writer->puts("");
}


/**
 * FormatPackData: output RT-BAT-P record
 */
void OvmsVehicleRenaultTwizy::FormatPackData(int verbosity, OvmsWriter* writer, int pack)
{
  if (verbosity < 200)
    return;

  int volt_alert, temp_alert;

  if (twizy_batt[pack].volt_alerts.any())
    volt_alert = 3;
  else if (twizy_batt[pack].volt_watches.any())
    volt_alert = 2;
  else
    volt_alert = 1;

  if (twizy_batt[pack].temp_alerts.any())
    temp_alert = 3;
  else if (twizy_batt[pack].temp_watches.any())
    temp_alert = 2;
  else
    temp_alert = 1;

  // Output pack status:
  //  RT-BAT-P,0,86400
  //  ,<volt_alertstatus>,<temp_alertstatus>
  //  ,<soc>,<soc_min>,<soc_max>
  //  ,<volt_act>,<volt_min>,<volt_max>
  //  ,<temp_act>,<temp_min>,<temp_max>
  //  ,<cell_volt_stddev_max>,<cmod_temp_stddev_max>
  //  ,<max_drive_pwr>,<max_recup_pwr>

  writer->printf(
    "RT-BAT-P,%d,86400"
    ",%d,%d"
    ",%d,%d,%d"
    ",%d,%d,%d"
    ",%d,%d,%d"
    ",%d,%d"
    ",%d,%d",
    pack+1,
    volt_alert, temp_alert,
    twizy_soc, twizy_soc_min, twizy_soc_max,
    twizy_batt[pack].volt_act,
    twizy_batt[pack].volt_min,
    twizy_batt[pack].volt_max,
    CONV_Temp(twizy_batt[pack].temp_act),
    CONV_Temp(twizy_batt[pack].temp_min),
    CONV_Temp(twizy_batt[pack].temp_max),
    CONV_CellVolt(twizy_batt[pack].cell_volt_stddev_max),
    (int) (twizy_batt[pack].cmod_temp_stddev_max + 0.5),
    (int) twizy_batt[pack].max_drive_pwr * 5,
    (int) twizy_batt[pack].max_recup_pwr * 5);

}


/**
 * FormatCellData: output RT-BAT-C record
 */
void OvmsVehicleRenaultTwizy::FormatCellData(int verbosity, OvmsWriter* writer, int cell)
{
  if (verbosity < 200)
    return;

  int pack = 0; // currently fixed, TODO for addon packs: determine pack index for cell
  int volt_alert, temp_alert;
  int cmod = cell / m_bms_readingspermodule_v;

  if (twizy_batt[pack].volt_alerts.test(cell))
    volt_alert = 3;
  else if (twizy_batt[pack].volt_watches.test(cell))
    volt_alert = 2;
  else
    volt_alert = 1;

  if (twizy_batt[pack].temp_alerts.test(cmod))
    temp_alert = 3;
  else if (twizy_batt[pack].temp_watches.test(cmod))
    temp_alert = 2;
  else
    temp_alert = 1;

  // Output cell status:
  //  RT-BAT-C,<cellnr>,86400
  //  ,<volt_alertstatus>,<temp_alertstatus>,
  //  ,<volt_act>,<volt_min>,<volt_max>,<volt_maxdev>
  //  ,<temp_act>,<temp_min>,<temp_max>,<temp_maxdev>

  writer->printf(
    "RT-BAT-C,%d,86400"
    ",%d,%d"
    ",%d,%d,%d,%d"
    ",%d,%d,%d,%d",
    cell+1,
    volt_alert, temp_alert,
    CONV_CellVolt(twizy_cell[cell].volt_act),
    CONV_CellVolt(twizy_cell[cell].volt_min),
    CONV_CellVolt(twizy_cell[cell].volt_max),
    CONV_CellVoltS(twizy_cell[cell].volt_maxdev),
    CONV_Temp(twizy_cmod[cmod].temp_act),
    CONV_Temp(twizy_cmod[cmod].temp_min),
    CONV_Temp(twizy_cmod[cmod].temp_max),
    (int) (twizy_cmod[cmod].temp_maxdev + 0.5));

}


/**
 * BatterySendDataUpdate: send data notifications for modified packs & cells
 */
void OvmsVehicleRenaultTwizy::BatterySendDataUpdate(bool force)
{
  bool modified = force |
    StdMetrics.ms_v_bat_soc->IsModifiedAndClear(m_modifier) |
    m_batt_use_soc_min->IsModifiedAndClear(m_modifier) |
    m_batt_use_soc_max->IsModifiedAndClear(m_modifier) |
    m_batt_use_volt_min->IsModifiedAndClear(m_modifier) |
    m_batt_use_volt_max->IsModifiedAndClear(m_modifier) |
    m_batt_use_temp_min->IsModifiedAndClear(m_modifier) |
    m_batt_use_temp_max->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_voltage->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_voltage->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_vdevmax->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_valert->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_temp->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_temp->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_tdevmax->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_talert->IsModifiedAndClear(m_modifier);

  if (!modified)
    return;

  StringWriter buf(100);

  for (int pack=0; pack < batt_pack_count; pack++) {
    buf.clear();
    FormatPackData(COMMAND_RESULT_NORMAL, &buf, pack);
    MyNotify.NotifyString("data", "xrt.battery.log", buf.c_str());
  }

  for (int cell=0; cell < batt_cell_count; cell++) {
    buf.clear();
    FormatCellData(COMMAND_RESULT_NORMAL, &buf, cell);
    MyNotify.NotifyString("data", "xrt.battery.log", buf.c_str());
  }
}
