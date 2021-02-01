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
// static const char *TAG = "vehicle";

#include <stdio.h>
#include <algorithm>
#include <ovms_command.h>
#include <ovms_script.h>
#include <ovms_metrics.h>
#include <ovms_notify.h>
#include <metrics_standard.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_webserver.h>
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_peripherals.h>
#include <string_writer.h>
#include "vehicle.h"

#undef SQR
#define SQR(n) ((n)*(n))
#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))
#undef LIMIT_MIN
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#undef LIMIT_MAX
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))

#undef TRUNCPREC
#define TRUNCPREC(fval,prec) (trunc((fval) * pow(10,(prec))) / pow(10,(prec)))
#undef ROUNDPREC
#define ROUNDPREC(fval,prec) (round((fval) * pow(10,(prec))) / pow(10,(prec)))
#undef CEILPREC
#define CEILPREC(fval,prec)  (ceil((fval)  * pow(10,(prec))) / pow(10,(prec)))


void OvmsVehicle::BmsSetCellArrangementVoltage(int readings, int readingspermodule)
  {
  if (m_bms_voltages != NULL) delete m_bms_voltages;
  m_bms_voltages = new float[readings];
  if (m_bms_vmins != NULL) delete m_bms_vmins;
  m_bms_vmins = new float[readings];
  if (m_bms_vmaxs != NULL) delete m_bms_vmaxs;
  m_bms_vmaxs = new float[readings];
  if (m_bms_vdevmaxs != NULL) delete m_bms_vdevmaxs;
  m_bms_vdevmaxs = new float[readings];
  if (m_bms_valerts != NULL) delete m_bms_valerts;
  m_bms_valerts = new short[readings];
  m_bms_valerts_new = 0;

  m_bms_bitset_v.clear();
  m_bms_bitset_v.reserve(readings);

  m_bms_readings_v = readings;
  m_bms_readingspermodule_v = readingspermodule;

  BmsResetCellVoltages(true);
  }

void OvmsVehicle::BmsSetCellArrangementTemperature(int readings, int readingspermodule)
  {
  if (m_bms_temperatures != NULL) delete m_bms_temperatures;
  m_bms_temperatures = new float[readings];
  if (m_bms_tmins != NULL) delete m_bms_tmins;
  m_bms_tmins = new float[readings];
  if (m_bms_tmaxs != NULL) delete m_bms_tmaxs;
  m_bms_tmaxs = new float[readings];
  if (m_bms_tdevmaxs != NULL) delete m_bms_tdevmaxs;
  m_bms_tdevmaxs = new float[readings];
  if (m_bms_talerts != NULL) delete m_bms_talerts;
  m_bms_talerts = new short[readings];
  m_bms_talerts_new = 0;

  m_bms_bitset_t.clear();
  m_bms_bitset_t.reserve(readings);

  m_bms_readings_t = readings;
  m_bms_readingspermodule_t = readingspermodule;

  BmsResetCellTemperatures(true);
  }

int OvmsVehicle::BmsGetCellArangementVoltage(int* readings, int* readingspermodule)
  {
  if (readings) *readings = m_bms_readings_v;
  if (readingspermodule) *readingspermodule = m_bms_readingspermodule_v;
  return m_bms_readings_v;
  }

int OvmsVehicle::BmsGetCellArangementTemperature(int* readings, int* readingspermodule)
  {
  if (readings) *readings = m_bms_readings_t;
  if (readingspermodule) *readingspermodule = m_bms_readingspermodule_t;
  return m_bms_readings_t;
  }

void OvmsVehicle::BmsSetCellDefaultThresholdsVoltage(float warn, float alert)
  {
  m_bms_defthr_vwarn = warn;
  m_bms_defthr_valert = alert;
  }

void OvmsVehicle::BmsGetCellDefaultThresholdsVoltage(float* warn, float* alert)
  {
  if (warn) *warn = m_bms_defthr_vwarn;
  if (alert) *alert = m_bms_defthr_valert;
  }

void OvmsVehicle::BmsSetCellDefaultThresholdsTemperature(float warn, float alert)
  {
  m_bms_defthr_twarn = warn;
  m_bms_defthr_talert = alert;
  }

void OvmsVehicle::BmsGetCellDefaultThresholdsTemperature(float* warn, float* alert)
  {
  if (warn) *warn = m_bms_defthr_twarn;
  if (alert) *alert = m_bms_defthr_talert;
  }

void OvmsVehicle::BmsSetCellLimitsVoltage(float min, float max)
  {
  m_bms_limit_vmin = min;
  m_bms_limit_vmax = max;
  }

void OvmsVehicle::BmsSetCellLimitsTemperature(float min, float max)
  {
  m_bms_limit_tmin = min;
  m_bms_limit_tmax = max;
  }

void OvmsVehicle::BmsSetCellVoltage(int index, float value)
  {
  // ESP_LOGV(TAG,"BmsSetCellVoltage(%d,%f) c=%d", index, value, m_bms_bitset_cv);
  if ((index<0)||(index>=m_bms_readings_v)) return;
  if ((value<m_bms_limit_vmin)||(value>m_bms_limit_vmax)) return;
  m_bms_voltages[index] = value;

  if (! m_bms_has_voltages)
    {
    m_bms_vmins[index] = value;
    m_bms_vmaxs[index] = value;
    }
  else if (m_bms_vmins[index] > value)
    m_bms_vmins[index] = value;
  else if (m_bms_vmaxs[index] < value)
    m_bms_vmaxs[index] = value;

  if (m_bms_bitset_v[index] == false) m_bms_bitset_cv++;
  if (m_bms_bitset_cv == m_bms_readings_v)
    {
    // get min, max, avg & standard deviation:
    double sum=0, sqrsum=0, avg, stddev=0;
    float min=0, max=0;
    for (int i=0; i<m_bms_readings_v; i++)
      {
      sum += m_bms_voltages[i];
      sqrsum += SQR(m_bms_voltages[i]);
      if (min==0 || m_bms_voltages[i]<min)
        min = m_bms_voltages[i];
      if (max==0 || m_bms_voltages[i]>max)
        max = m_bms_voltages[i];
      }
    avg = sum / m_bms_readings_v;
    stddev = sqrt(LIMIT_MIN((sqrsum / m_bms_readings_v) - SQR(avg), 0));
    // check cell deviations:
    float dev;
    float thr_warn  = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.warn", m_bms_defthr_vwarn);
    float thr_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.alert", m_bms_defthr_valert);
    for (int i=0; i<m_bms_readings_v; i++)
      {
      dev = ROUNDPREC(m_bms_voltages[i] - avg, 5);
      if (ABS(dev) > ABS(m_bms_vdevmaxs[i]))
        m_bms_vdevmaxs[i] = dev;
      if (ABS(dev) >= thr_alert && m_bms_valerts[i] < 2)
        {
        m_bms_valerts[i] = 2;
        m_bms_valerts_new++; // trigger notification
        }
      else if (ABS(dev) >= thr_warn && m_bms_valerts[i] < 1)
        m_bms_valerts[i] = 1;
      }
    // publish to metrics:
    avg = ROUNDPREC(avg, 5);
    stddev = ROUNDPREC(stddev, 5);
    StandardMetrics.ms_v_bat_pack_vmin->SetValue(min);
    StandardMetrics.ms_v_bat_pack_vmax->SetValue(max);
    StandardMetrics.ms_v_bat_pack_vavg->SetValue(avg);
    StandardMetrics.ms_v_bat_pack_vstddev->SetValue(stddev);
    if (stddev > StandardMetrics.ms_v_bat_pack_vstddev_max->AsFloat())
      StandardMetrics.ms_v_bat_pack_vstddev_max->SetValue(stddev);
    StandardMetrics.ms_v_bat_cell_voltage->SetElemValues(0, m_bms_readings_v, m_bms_voltages);
    StandardMetrics.ms_v_bat_cell_vmin->SetElemValues(0, m_bms_readings_v, m_bms_vmins);
    StandardMetrics.ms_v_bat_cell_vmax->SetElemValues(0, m_bms_readings_v, m_bms_vmaxs);
    StandardMetrics.ms_v_bat_cell_vdevmax->SetElemValues(0, m_bms_readings_v, m_bms_vdevmaxs);
    StandardMetrics.ms_v_bat_cell_valert->SetElemValues(0, m_bms_readings_v, m_bms_valerts);
    // complete:
    m_bms_has_voltages = true;
    m_bms_bitset_v.clear();
    m_bms_bitset_v.resize(m_bms_readings_v);
    m_bms_bitset_cv = 0;
    }
  else
    {
    m_bms_bitset_v[index] = true;
    }
  }

void OvmsVehicle::BmsSetCellTemperature(int index, float value)
  {
  // ESP_LOGV(TAG,"BmsSetCellTemperature(%d,%f) c=%d", index, value, m_bms_bitset_ct);
  if ((index<0)||(index>=m_bms_readings_t)) return;
  if ((value<m_bms_limit_tmin)||(value>m_bms_limit_tmax)) return;
  m_bms_temperatures[index] = value;

  if (! m_bms_has_temperatures)
    {
    m_bms_tmins[index] = value;
    m_bms_tmaxs[index] = value;
    }
  else if (m_bms_tmins[index] > value)
    m_bms_tmins[index] = value;
  else if (m_bms_tmaxs[index] < value)
    m_bms_tmaxs[index] = value;

  if (m_bms_bitset_t[index] == false) m_bms_bitset_ct++;
  if (m_bms_bitset_ct == m_bms_readings_t)
    {
    // get min, max, avg & standard deviation:
    double sum=0, sqrsum=0, avg, stddev=0;
    float min=0, max=0;
    for (int i=0; i<m_bms_readings_t; i++)
      {
      sum += m_bms_temperatures[i];
      sqrsum += SQR(m_bms_temperatures[i]);
      if (min==0 || m_bms_temperatures[i]<min)
        min = m_bms_temperatures[i];
      if (max==0 || m_bms_temperatures[i]>max)
        max = m_bms_temperatures[i];
      }
    avg = sum / m_bms_readings_t;
    stddev = sqrt(LIMIT_MIN((sqrsum / m_bms_readings_t) - SQR(avg), 0));
    // check cell deviations:
    float dev;
    float thr_warn  = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.warn", m_bms_defthr_twarn);
    float thr_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.alert", m_bms_defthr_talert);
    for (int i=0; i<m_bms_readings_t; i++)
      {
      dev = ROUNDPREC(m_bms_temperatures[i] - avg, 2);
      if (ABS(dev) > ABS(m_bms_tdevmaxs[i]))
        m_bms_tdevmaxs[i] = dev;
      if (ABS(dev) >= thr_alert && m_bms_talerts[i] < 2)
        {
        m_bms_talerts[i] = 2;
        m_bms_talerts_new++; // trigger notification
        }
      else if (ABS(dev) >= thr_warn && m_bms_valerts[i] < 1)
        m_bms_talerts[i] = 1;
      }
    // publish to metrics:
    avg = ROUNDPREC(avg, 2);
    stddev = ROUNDPREC(stddev, 2);
    StandardMetrics.ms_v_bat_pack_tmin->SetValue(min);
    StandardMetrics.ms_v_bat_pack_tmax->SetValue(max);
    StandardMetrics.ms_v_bat_pack_tavg->SetValue(avg);
    StandardMetrics.ms_v_bat_pack_tstddev->SetValue(stddev);
    if (stddev > StandardMetrics.ms_v_bat_pack_tstddev_max->AsFloat())
      StandardMetrics.ms_v_bat_pack_tstddev_max->SetValue(stddev);
    StandardMetrics.ms_v_bat_cell_temp->SetElemValues(0, m_bms_readings_t, m_bms_temperatures);
    StandardMetrics.ms_v_bat_cell_tmin->SetElemValues(0, m_bms_readings_t, m_bms_tmins);
    StandardMetrics.ms_v_bat_cell_tmax->SetElemValues(0, m_bms_readings_t, m_bms_tmaxs);
    StandardMetrics.ms_v_bat_cell_tdevmax->SetElemValues(0, m_bms_readings_t, m_bms_tdevmaxs);
    StandardMetrics.ms_v_bat_cell_talert->SetElemValues(0, m_bms_readings_t, m_bms_talerts);
    // complete:
    m_bms_has_temperatures = true;
    m_bms_bitset_t.clear();
    m_bms_bitset_t.resize(m_bms_readings_t);
    m_bms_bitset_ct = 0;
    }
  else
    {
    m_bms_bitset_t[index] = true;
    }
  }

void OvmsVehicle::BmsRestartCellVoltages()
  {
  m_bms_bitset_v.clear();
  m_bms_bitset_v.resize(m_bms_readings_v);
  m_bms_bitset_cv = 0;
  }

void OvmsVehicle::BmsRestartCellTemperatures()
  {
  m_bms_bitset_t.clear();
  m_bms_bitset_v.resize(m_bms_readings_t);
  m_bms_bitset_ct = 0;
  }

void OvmsVehicle::BmsResetCellVoltages(bool full /*=false*/)
  {
  if (m_bms_readings_v > 0)
    {
    m_bms_bitset_v.clear();
    m_bms_bitset_v.resize(m_bms_readings_v);
    m_bms_bitset_cv = 0;
    m_bms_has_voltages = false;
    for (int k=0; k<m_bms_readings_v; k++)
      {
      m_bms_vmins[k] = 0;
      m_bms_vmaxs[k] = 0;
      m_bms_vdevmaxs[k] = 0;
      m_bms_valerts[k] = 0;
      }
    m_bms_valerts_new = 0;
    if (full) StandardMetrics.ms_v_bat_cell_voltage->ClearValue();
    StandardMetrics.ms_v_bat_cell_vmin->ClearValue();
    StandardMetrics.ms_v_bat_cell_vmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_vdevmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_valert->ClearValue();
    StandardMetrics.ms_v_bat_pack_tstddev_max->SetValue(StandardMetrics.ms_v_bat_pack_tstddev->AsFloat());
    }
  }

void OvmsVehicle::BmsResetCellTemperatures(bool full /*=false*/)
  {
  if (m_bms_readings_t > 0)
    {
    m_bms_bitset_t.clear();
    m_bms_bitset_t.resize(m_bms_readings_t);
    m_bms_bitset_ct = 0;
    m_bms_has_temperatures = false;
    for (int k=0; k<m_bms_readings_t; k++)
      {
      m_bms_tmins[k] = 0;
      m_bms_tmaxs[k] = 0;
      m_bms_tdevmaxs[k] = 0;
      m_bms_talerts[k] = 0;
      }
    m_bms_talerts_new = 0;
    if (full) StandardMetrics.ms_v_bat_cell_temp->ClearValue();
    StandardMetrics.ms_v_bat_cell_tmin->ClearValue();
    StandardMetrics.ms_v_bat_cell_tmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_tdevmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_talert->ClearValue();
    StandardMetrics.ms_v_bat_pack_tstddev_max->SetValue(StandardMetrics.ms_v_bat_pack_tstddev->AsFloat());
    }
  }

void OvmsVehicle::BmsResetCellStats()
  {
  BmsResetCellVoltages(false);
  BmsResetCellTemperatures(false);
  }

void OvmsVehicle::BmsStatus(int verbosity, OvmsWriter* writer)
  {
  int c;

  if ((! m_bms_has_voltages)||(! m_bms_has_temperatures))
    {
    writer->puts("No BMS status data available");
    return;
    }

  int vwarn=0, valert=0;
  int twarn=0, talert=0;
  for (c=0; c<m_bms_readings_v; c++) {
    if (m_bms_valerts[c]==1) vwarn++;
    if (m_bms_valerts[c]==2) valert++;
  }
  for (c=0; c<m_bms_readings_t; c++) {
    if (m_bms_talerts[c]==1) twarn++;
    if (m_bms_talerts[c]==2) talert++;
  }

  writer->puts("Voltage:");
  writer->printf("    Average: %5.3fV [%5.3fV - %5.3fV]\n",
    StdMetrics.ms_v_bat_pack_vavg->AsFloat(),
    StdMetrics.ms_v_bat_pack_vmin->AsFloat(),
    StdMetrics.ms_v_bat_pack_vmax->AsFloat());
  writer->printf("  Deviation: SD %6.2fmV [max %.2fmV], %d warnings, %d alerts\n",
    StdMetrics.ms_v_bat_pack_vstddev->AsFloat()*1000,
    StdMetrics.ms_v_bat_pack_vstddev_max->AsFloat()*1000,
    vwarn, valert);

  writer->puts("Temperature:");
  writer->printf("    Average: %5.1fC [%5.1fC - %5.1fC]\n",
    StdMetrics.ms_v_bat_pack_tavg->AsFloat(),
    StdMetrics.ms_v_bat_pack_tmin->AsFloat(),
    StdMetrics.ms_v_bat_pack_tmax->AsFloat());
  writer->printf("  Deviation: SD %6.2fC  [max %.2fC], %d warnings, %d alerts\n",
    StdMetrics.ms_v_bat_pack_tstddev->AsFloat(),
    StdMetrics.ms_v_bat_pack_tstddev_max->AsFloat(),
    twarn, talert);

  writer->puts("Cells:");
  int kv = 0;
  int kt = 0;
  for (int module = 0; module < ((m_bms_readings_v+m_bms_readingspermodule_v-1)/m_bms_readingspermodule_v); module++)
    {
    writer->printf("    +");
    for (c=0;c<m_bms_readingspermodule_v;c++) { writer->printf("-------"); }
    writer->printf("-+");
    for (c=0;c<m_bms_readingspermodule_t;c++) { writer->printf("-------"); }
    writer->puts("-+");
    writer->printf("%3d |",module+1);
    for (c=0; c<m_bms_readingspermodule_v; c++)
      {
      if (kv < m_bms_readings_v)
        writer->printf(" %5.3fV",m_bms_voltages[kv++]);
      else
        writer->printf("       ");
      }
    writer->printf(" |");
    for (c=0; c<m_bms_readingspermodule_t; c++)
      {
      if (kt < m_bms_readings_t)
        writer->printf(" %5.1fC",m_bms_temperatures[kt++]);
      else
        writer->printf("       ");
      }
    writer->puts(" |");
    }

  writer->printf("    +");
  for (c=0;c<m_bms_readingspermodule_v;c++) { writer->printf("-------"); }
  writer->printf("-+");
  for (c=0;c<m_bms_readingspermodule_t;c++) { writer->printf("-------"); }
  writer->puts("-+");
  }

bool OvmsVehicle::FormatBmsAlerts(int verbosity, OvmsWriter* writer, bool show_warnings)
  {
  int has_valerts = 0, has_talerts = 0;
  bool verbose = (verbosity > COMMAND_RESULT_SMS);

  writer->puts("Battery cell deviation:");

  // Voltages:
  writer->printf("Voltage: StdDev %dmV", (int)(StdMetrics.ms_v_bat_pack_vstddev_max->AsFloat() * 1000));
  for (int i=0; i<m_bms_readings_v; i++)
    {
    int sts = StdMetrics.ms_v_bat_cell_valert->GetElemValue(i);
    if (sts == 0) continue;
    if (sts == 1 && !show_warnings) continue;
    has_valerts++;
    if (verbose || has_valerts <= 5)
      {
      int dev = StdMetrics.ms_v_bat_cell_vdevmax->GetElemValue(i) * 1000;
      writer->printf("\n %c #%02d: %+4dmV", (sts==1) ? '?' : '!', i+1, dev);
      }
    else
      {
      writer->printf("\n [...]");
      break;
      }
    }
  writer->printf("%s\n", has_valerts ? "" : ", cells OK");

  // Temperatures:
  // (Note: '°' is not SMS safe, so we only output 'C')
  writer->printf("Temperature: StdDev %.1fC", StdMetrics.ms_v_bat_pack_tstddev_max->AsFloat());
  for (int i=0; i<m_bms_readings_v; i++)
    {
    int sts = StdMetrics.ms_v_bat_cell_talert->GetElemValue(i);
    if (sts == 0) continue;
    if (sts == 1 && !show_warnings) continue;
    has_talerts++;
    if (verbose || has_talerts <= 5)
      {
      float dev = StdMetrics.ms_v_bat_cell_tdevmax->GetElemValue(i);
      writer->printf("\n %c #%02d: %+3.1fC", (sts==1) ? '?' : '!', i+1, dev);
      }
    else
      {
      writer->printf("\n [...]");
      break;
      }
    }
  writer->printf("%s\n", has_talerts ? "" : ", cells OK");

  if (verbose || has_valerts >= 5 || has_talerts >= 5)
    {
    writer->puts("Details: bms status");
    }

  return (has_valerts > 0) || (has_talerts > 0);
  }
