/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
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


#include "ovms_bms.h"
#include "ovms_duktape.h"
#include "string_writer.h"
#include "ovms_notify.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

// Voltage stddev running average sample count:
#define VSTDDEV_SMOOTHCNT         5

#ifdef bind
#undef bind
#endif
using namespace std::placeholders;

static const char *TAG = "vehicle-bms";

OvmsBmsMonitor MyBmsMonitor __attribute__ ((init_priority (1900)));

OvmsBmsMonitor::OvmsBmsMonitor()
  : m_bms_voltages(nullptr),
    m_bms_vmins(nullptr),
    m_bms_vmaxs(nullptr),
    m_bms_vdevmaxs(nullptr),
    m_bms_valerts(nullptr),
    m_bms_valerts_new(0),
    m_bms_vstddev_cnt(0),
    m_bms_vstddev_avg(0),
    m_bms_has_voltages(false),
    m_bms_temperatures(nullptr),
    m_bms_tmins(nullptr),
    m_bms_tmaxs(nullptr),
    m_bms_tdevmaxs(nullptr),
    m_bms_talerts(nullptr),
    m_bms_talerts_new(0),
    m_bms_has_temperatures(false),
    m_bms_bitset_cv(0),
    m_bms_bitset_ct(0),
    m_bms_readings_v(0),
    m_bms_readingspermodule_v(0),
    m_bms_readings_t(0),
    m_bms_readingspermodule_t(0),
    m_bms_limit_tmin(-1000),
    m_bms_limit_tmax(1000),
    m_bms_limit_vmin(-1000),
    m_bms_limit_vmax(1000),
    m_bms_defthr_vmaxgrad(BMS_DEFTHR_VMAXGRAD),
    m_bms_defthr_vmaxsddev(BMS_DEFTHR_VMAXSDDEV),
    m_bms_defthr_vwarn(BMS_DEFTHR_VWARN),
    m_bms_defthr_valert(BMS_DEFTHR_VALERT),
    m_bms_defthr_twarn(BMS_DEFTHR_TWARN),
    m_bms_defthr_talert(BMS_DEFTHR_TALERT),
    m_bms_vlog_last(0),
    m_bms_tlog_last(0),
    m_autonotifications(false),
    m_webmodule_enabled(false)
  {
  m_bms_bitset_v.clear();
  m_bms_bitset_t.clear();

  OvmsCommand* cmd_bms = MyCommandApp.RegisterCommand("bms","BMS framework", bms_status, "", 0, 0, false);
  cmd_bms->RegisterCommand("status","Show BMS status",bms_status);
  cmd_bms->RegisterCommand("temp","Show BMS temperature status",bms_status);
  cmd_bms->RegisterCommand("volt","Show BMS voltage status",bms_status);
  cmd_bms->RegisterCommand("reset","Reset BMS statistics",bms_reset);
  cmd_bms->RegisterCommand("alerts","Show BMS alerts",bms_alerts);

  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsBmsMonitor::BmsTicker, this, _1, _2));
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  RegisterBmsDuktape();
#endif
  }

OvmsBmsMonitor::~OvmsBmsMonitor()
  {
  MyEvents.DeregisterEvent(TAG);
  SetWebmoduleEnabled(false);
  ClearVoltages();
  ClearTemperatures();
  }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
void OvmsBmsMonitor::RegisterBmsDuktape()
{
  DuktapeObjectRegistration* dto_bms= new DuktapeObjectRegistration("OvmsBms");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsVoltageSetCellArrangement, 2, "VoltageSetCellArrangement");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsVoltageSetCellDefaultThresholds, DUK_VARARGS, "VoltageSetCellDefaultThresholds");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsVoltageSetCellLimits, 2, "VoltageSetCellLimits");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsVoltageSetCell, 2, "VoltageSetCell");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsVoltageResetCells, DUK_VARARGS, "VoltageResetCells");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsVoltagesRestartCells, 0, "VoltagesRestartCells");

  dto_bms->RegisterDuktapeFunction(DukOvmsBmsTemperatureSetCellArrangement, 2, "TemperatureSetCellArrangement");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsTemperatureSetCellDefaultThresholds,  2, "TemperatureSetCellDefaultThresholds");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsTemperatureSetCellLimits, 2, "TemperatureSetCellLimits");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsTemperatureSetCell, 2, "TemperatureSetCell");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsTemperatureResetCells, DUK_VARARGS, "TemperatureResetCells");
  dto_bms->RegisterDuktapeFunction(DukOvmsBmsTemperatureRestartCells, 0, "TemperatureRestartCells");
  MyDuktape.RegisterDuktapeObject(dto_bms);
  }
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

void OvmsBmsMonitor::ClearVoltages()
  {
  if (m_bms_voltages != nullptr)
    {
    delete [] m_bms_voltages;
    m_bms_voltages = nullptr;
    }
  if (m_bms_vmins != nullptr)
    {
    delete [] m_bms_vmins;
    m_bms_vmins = nullptr;
    }
  if (m_bms_vmaxs != nullptr)
    {
    delete [] m_bms_vmaxs;
    m_bms_vmaxs = nullptr;
    }
  if (m_bms_vdevmaxs != nullptr)
    {
    delete [] m_bms_vdevmaxs;
    m_bms_vdevmaxs = nullptr;
    }
  if (m_bms_valerts != nullptr)
    {
    delete [] m_bms_valerts;
    m_bms_valerts = nullptr;
    }
  m_bms_valerts_new = 0;
  m_bms_bitset_v.clear();
  m_bms_readings_v = 0;
  m_bms_readingspermodule_v = 0;
  m_bms_has_voltages = false;
  }

void OvmsBmsMonitor::SetCellArrangementVoltage(int readings, int readingspermodule)
  {
  bool wasenabled = IsEnabled(bms_status_t::Both);
  ClearVoltages();
  m_bms_voltages = new float[readings];
  m_bms_vmins = new float[readings];
  m_bms_vmaxs = new float[readings];
  m_bms_vdevmaxs = new float[readings];
  m_bms_valerts = new OvmsBmsStatus[readings];
  m_bms_bitset_v.reserve(readings);
  m_bms_readings_v = readings;
  m_bms_readingspermodule_v = readingspermodule;

  ResetCellVoltages(true);
  if (readings > 0)
    m_autonotifications = true;
  bool isenabled = IsEnabled(bms_status_t::Both);
  // Only set web-module status if transitioning from or to 'not Enabled' state
  if (!isenabled || !wasenabled)
    SetWebmoduleEnabled(isenabled);
  }

// Internal entry for changing cell arrangements.
struct reading_entry_t
  {
  int cell_no;
  float entry;
  };

/**
 * Changes the cell arrangement for Voltage, if needs be restoring any readings that were there.
 * Really should only do work the first time it is called.. as for any one car, the number of readings
 * should be consistent.
 * @return true If the cell arrangement was changed.
 */
bool OvmsBmsMonitor::CheckChangeCellArrangementVoltage(int readings, int readingspermodule /*= 0*/)
  {
  bool res = false;
  if (readingspermodule <= 0)
    {
    // Passing in 0 will leave the readings per module unchanged.
    readingspermodule = m_bms_readingspermodule_v;
    }
  if (readings != m_bms_readings_v)
    {
    res = true;
    if (m_bms_bitset_cv == 0)
      {
      SetCellArrangementVoltage(readings, readingspermodule);
      }
    else
      {
      // Store (potentially) sparse readings into a vector.
      std::vector<reading_entry_t> cells;
      // At most we will need the number of voltages set in the bitset.
      cells.reserve(m_bms_bitset_cv);
      reading_entry_t reading;
      int maxv = readings;
      if (m_bms_readings_v < maxv)
        {
        maxv = m_bms_readings_v;
        }
      for (int i = 0; i< maxv; ++i)
        {
        if (m_bms_bitset_v[i])
          {
          reading.cell_no = i;
          reading.entry = m_bms_voltages[i];
          cells.insert(cells.end(),reading);
          }
        }
      SetCellArrangementVoltage(readings, readingspermodule);
      for ( std::vector<reading_entry_t>::iterator iter = cells.begin(); iter != cells.end(); ++iter)
        {
        SetCellVoltage(iter->cell_no, iter->entry);
        }
      }
    }
  else if (readingspermodule != m_bms_readingspermodule_v)
    {
    // This only changes the read-out.
    m_bms_readingspermodule_v = readingspermodule;
    res = true;
    }
  return res;
  }

/**
 * Changes the cell arrangement for Temperature, if needs be restoring any readings that were there.
 * Really should only do work the first time it is called.. as for any one car, the number of readings
 * should be consistent.
 * @return true If the cell arrangement was changed.
 */
bool OvmsBmsMonitor::CheckChangeCellArrangementTemperature(int readings, int readingspermodule /*= 0*/)
  {
  bool res = false;
  if (readingspermodule <= 0)
    {
    // Passing in 0 will leave the readings per module unchanged.
    readingspermodule = m_bms_readingspermodule_t;
    }
  if (readings != m_bms_readings_t)
    {
    res = true;
    if (m_bms_bitset_ct == 0)
      {
      SetCellArrangementTemperature(readings, readingspermodule);
      }
    else
      {
      // Store (potentially) sparse readings into a vector.
      std::vector<reading_entry_t> cells;
      // At most we will need the number of Temperatures set in the bitset.
      cells.reserve(m_bms_bitset_ct);
      reading_entry_t reading;
      int maxv = readings;
      if (m_bms_readings_t < maxv)
        {
        maxv = m_bms_readings_t;
        }
      for (int i = 0; i< maxv; ++i)
        {
        if (m_bms_bitset_t[i])
          {
          reading.cell_no = i;
          reading.entry = m_bms_temperatures[i];
          cells.insert(cells.end(),reading);
          }
        }
      SetCellArrangementTemperature(readings, readingspermodule);
      for ( std::vector<reading_entry_t>::iterator iter = cells.begin(); iter != cells.end(); ++iter)
        {
        SetCellTemperature(iter->cell_no, iter->entry);
        }
      }
    }
  else if (readingspermodule != m_bms_readingspermodule_t)
    {
    // This only changes the read-out.
    m_bms_readingspermodule_t = readingspermodule;
    res = true;
    }
  return res;
  }

void OvmsBmsMonitor::ClearTemperatures()
  {
  if (m_bms_temperatures != nullptr)
    {
    delete [] m_bms_temperatures;
    m_bms_temperatures = nullptr;
    }
  if (m_bms_tmins != nullptr)
    {
    delete [] m_bms_tmins;
    m_bms_tmins = nullptr;
    }
  if (m_bms_tmaxs != nullptr)
    {
    delete [] m_bms_tmaxs;
    m_bms_tmaxs = nullptr;
    }
  if (m_bms_tdevmaxs != nullptr)
    {
    delete [] m_bms_tdevmaxs;
    m_bms_tdevmaxs = nullptr;
    }
  if (m_bms_talerts != nullptr)
    {
    delete [] m_bms_talerts;
    m_bms_talerts = nullptr;
    }
  m_bms_talerts_new = 0;
  m_bms_bitset_t.clear();
  m_bms_readings_t = 0;
  m_bms_readingspermodule_t = 0;
  m_bms_has_temperatures = false;
  }

void OvmsBmsMonitor::SetCellArrangementTemperature(int readings, int readingspermodule)
  {
  ClearTemperatures();
  m_bms_temperatures = new float[readings];
  m_bms_tmins = new float[readings];
  m_bms_tmaxs = new float[readings];
  m_bms_tdevmaxs = new float[readings];
  m_bms_talerts = new OvmsBmsStatus[readings];
  m_bms_bitset_t.reserve(readings);
  m_bms_readings_t = readings;
  m_bms_readingspermodule_t = readingspermodule;

  ResetCellTemperatures(true);
  if (readings > 0)
    m_autonotifications = true;
  SetWebmoduleEnabled(IsEnabled(bms_status_t::Both));
  }

int OvmsBmsMonitor::GetCellArangementVoltage(int* readings/*=NULL*/, int* readingspermodule/*=NULL*/)
  {
  if (readings) *readings = m_bms_readings_v;
  if (readingspermodule) *readingspermodule = m_bms_readingspermodule_v;
  return m_bms_readings_v;
  }

int OvmsBmsMonitor::GetCellArangementTemperature(int* readings/*=NULL*/, int* readingspermodule/*=NULL*/)
  {
  if (readings) *readings = m_bms_readings_t;
  if (readingspermodule) *readingspermodule = m_bms_readingspermodule_t;
  return m_bms_readings_t;
  }

void OvmsBmsMonitor::SetCellDefaultThresholdsVoltage(float warn, float alert,
    float maxgrad /*=-1*/, float maxsddev /*=-1*/)
  {
  m_bms_defthr_vwarn = warn;
  m_bms_defthr_valert = alert;
  m_bms_defthr_vmaxgrad = (maxgrad < 0) ? BMS_DEFTHR_VMAXGRAD : maxgrad;
  m_bms_defthr_vmaxsddev = (maxsddev < 0) ? BMS_DEFTHR_VMAXSDDEV : maxsddev;
  }

void OvmsBmsMonitor::GetCellDefaultThresholdsVoltage(float* warn, float* alert,
    float* maxgrad /*=NULL*/, float* maxsddev/*=NULL*/)
  {
  if (warn) *warn = m_bms_defthr_vwarn;
  if (alert) *alert = m_bms_defthr_valert;
  if (maxgrad) *maxgrad = m_bms_defthr_vmaxgrad;
  if (maxsddev) *maxsddev = m_bms_defthr_vmaxsddev;
  }

void OvmsBmsMonitor::SetCellDefaultThresholdsTemperature(float warn, float alert)
  {
  m_bms_defthr_twarn = warn;
  m_bms_defthr_talert = alert;
  }

void OvmsBmsMonitor::GetCellDefaultThresholdsTemperature(float* warn, float* alert)
  {
  if (warn) *warn = m_bms_defthr_twarn;
  if (alert) *alert = m_bms_defthr_talert;
  }

void OvmsBmsMonitor::SetCellLimitsVoltage(float min, float max)
  {
  m_bms_limit_vmin = min;
  m_bms_limit_vmax = max;
  }

void OvmsBmsMonitor::SetCellLimitsTemperature(float min, float max)
  {
  m_bms_limit_tmin = min;
  m_bms_limit_tmax = max;
  }

void OvmsBmsMonitor::SetCellVoltage(int index, float value)
  {
  // ESP_LOGV(TAG,"SetCellVoltage(%d,%f) c=%d", index, value, m_bms_bitset_cv);
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
    // Series complete, all cell voltages acquired
    float thr_maxgrad  = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.maxgrad",  m_bms_defthr_vmaxgrad);
    float thr_maxsddev = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.maxsddev", m_bms_defthr_vmaxsddev);
    float thr_warn     = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.warn",     m_bms_defthr_vwarn);
    float thr_alert    = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.alert",    m_bms_defthr_valert);

    // Get min, max, avg & standard deviation:
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

    // Get gradient:
    double sumn = 0, sumd = 0;
    for (int i=0; i<m_bms_readings_v; i++)
      {
      sumn += (i - (m_bms_readings_v / 2 - 0.5)) * (m_bms_voltages[i] - avg);
      sumd += SQR(i - (m_bms_readings_v / 2 - 0.5));
      }
    float grad = (sumn / sumd) * m_bms_readings_v;

    // …publish to metrics:
    StandardMetrics.ms_v_bat_pack_vmin->SetValue(min);
    StandardMetrics.ms_v_bat_pack_vmax->SetValue(max);
    StandardMetrics.ms_v_bat_pack_vavg->SetValue(ROUNDPREC(avg, 5));
    StandardMetrics.ms_v_bat_pack_vstddev->SetValue(ROUNDPREC(stddev, 5));
    StandardMetrics.ms_v_bat_pack_vgrad->SetValue(ROUNDPREC(grad, 5));
    StandardMetrics.ms_v_bat_cell_voltage->SetElemValues(0, m_bms_readings_v, m_bms_voltages);
    StandardMetrics.ms_v_bat_cell_vmin->SetElemValues(0, m_bms_readings_v, m_bms_vmins);
    StandardMetrics.ms_v_bat_cell_vmax->SetElemValues(0, m_bms_readings_v, m_bms_vmaxs);

    // Voltages are very volatile and may respond to a load change within the sensor query loop.
    // To detect an inconsistent series, we check for a too high gradient and/or a too high
    // offset of the momentary stddev level from the previously observed average:
    bool series_valid;
    if (ABS(grad) > thr_maxgrad)
      {
      series_valid = false;
      }
    else if (m_bms_vstddev_cnt < VSTDDEV_SMOOTHCNT)
      {
      // skip the first VSTDDEV_SMOOTHCNT series to init the average:
      m_bms_vstddev_cnt++;
      m_bms_vstddev_avg = ((m_bms_vstddev_cnt-1) * m_bms_vstddev_avg + stddev) / m_bms_vstddev_cnt;
      series_valid = false;
      }
    else if (stddev - m_bms_vstddev_avg > thr_maxsddev)
      {
      series_valid = false;
      }
    else
      {
      m_bms_vstddev_avg = ((VSTDDEV_SMOOTHCNT-1) * m_bms_vstddev_avg + stddev) / VSTDDEV_SMOOTHCNT;
      series_valid = true;
      }

    // Check cell deviations only if the series appears to be consistent:
    if (series_valid)
      {
      float dev;
      for (int i=0; i<m_bms_readings_v; i++)
        {
        dev = ROUNDPREC(m_bms_voltages[i] - avg, 5);
        if (ABS(dev) > ABS(m_bms_vdevmaxs[i]))
          m_bms_vdevmaxs[i] = dev;
        if (ABS(dev) >= stddev + thr_alert && m_bms_valerts[i] <= OvmsBmsStatus::Warn)
          {
          m_bms_valerts[i] = OvmsBmsStatus::Alert;
          m_bms_valerts_new++; // trigger notification
          }
        else if (ABS(dev) >= stddev + thr_warn && m_bms_valerts[i] < OvmsBmsStatus::Warn)
          m_bms_valerts[i] = OvmsBmsStatus::Warn;
        }

      // Publish deviation maximums & alerts:
      if (stddev > StandardMetrics.ms_v_bat_pack_vstddev_max->AsFloat())
        StandardMetrics.ms_v_bat_pack_vstddev_max->SetValue(stddev);
      StandardMetrics.ms_v_bat_cell_vdevmax->SetElemValues(0, m_bms_readings_v, m_bms_vdevmaxs);
      StandardMetrics.ms_v_bat_cell_valert->SetElemValues(0, m_bms_readings_v, (short *)m_bms_valerts);
      }

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

void OvmsBmsMonitor::SetCellTemperature(int index, float value)
  {
  // ESP_LOGV(TAG,"SetCellTemperature(%d,%f) c=%d", index, value, m_bms_bitset_ct);
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
    // Series complete, all cell temperatures acquired
    float thr_warn  = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.warn", m_bms_defthr_twarn);
    float thr_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.alert", m_bms_defthr_talert);

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
    for (int i=0; i<m_bms_readings_t; i++)
      {
      dev = ROUNDPREC(m_bms_temperatures[i] - avg, 2);
      if (ABS(dev) > ABS(m_bms_tdevmaxs[i]))
        m_bms_tdevmaxs[i] = dev;
      if (ABS(dev) >= stddev + thr_alert && m_bms_talerts[i] < OvmsBmsStatus::Alert)
        {
        m_bms_talerts[i] = OvmsBmsStatus::Alert;
        m_bms_talerts_new++; // trigger notification
        }
      else if (ABS(dev) >= stddev + thr_warn && m_bms_valerts[i] < OvmsBmsStatus::Warn)
        m_bms_talerts[i] = OvmsBmsStatus::Warn;
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
    StandardMetrics.ms_v_bat_cell_talert->SetElemValues(0, m_bms_readings_t, (short *) m_bms_talerts);

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

void OvmsBmsMonitor::RestartCellVoltages()
  {
  m_bms_bitset_v.clear();
  m_bms_bitset_v.resize(m_bms_readings_v);
  m_bms_bitset_cv = 0;
  }

void OvmsBmsMonitor::RestartCellTemperatures()
  {
  m_bms_bitset_t.clear();
  m_bms_bitset_t.resize(m_bms_readings_t);
  m_bms_bitset_ct = 0;
  }

void OvmsBmsMonitor::ResetCellVoltages(bool full /*=false*/)
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
      m_bms_valerts[k] = OvmsBmsStatus::OK;
      }
    m_bms_valerts_new = 0;
    m_bms_vstddev_cnt = 0;
    m_bms_vstddev_avg = 0;
    if (full) StandardMetrics.ms_v_bat_cell_voltage->ClearValue();
    StandardMetrics.ms_v_bat_cell_vmin->ClearValue();
    StandardMetrics.ms_v_bat_cell_vmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_vdevmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_valert->ClearValue();
    StandardMetrics.ms_v_bat_pack_vstddev_max->SetValue(StandardMetrics.ms_v_bat_pack_vstddev->AsFloat());
    }
  }

void OvmsBmsMonitor::ResetCellTemperatures(bool full /*=false*/)
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
      m_bms_talerts[k] = OvmsBmsStatus::OK;
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

void OvmsBmsMonitor::ResetCellStats()
  {
  ResetCellVoltages(false);
  ResetCellTemperatures(false);
  }

void OvmsBmsMonitor::Status(int verbosity, OvmsWriter* writer, bms_status_t statusmode)
  {
  auto check_max_cols = [](int total_cols, int maximum)
    {
    if (maximum <= 1)
      return 1;
    if (total_cols <= maximum)
      return total_cols;
    if (maximum >= 4 && total_cols % 4 == 0)
      return 4;
    if (total_cols % 3 == 0)
      return 3;
    if (maximum >= 5 && total_cols % 5 == 0)
      return 5;
    return maximum;
    };

  int c;

  bool show_voltage = m_bms_has_voltages;
  bool show_temperature = m_bms_has_temperatures;
  switch (statusmode)
    {
    case bms_status_t::Both:
      break;
    case bms_status_t::Voltage:
      show_temperature = false;
      break;
    case bms_status_t::Temperature:
      show_voltage = false;
      break;
    }
  if ((!show_voltage) && (!show_temperature))
    {
    const char *datatype= "status";
    switch (statusmode)
      {
      case bms_status_t::Both:
        break;
      case bms_status_t::Voltage:
        datatype = "voltage";
        break;
      case bms_status_t::Temperature:
        datatype = "temperature";
        break;
      }
    writer->printf("No BMS %s data available\n", datatype);
    return;
    }
  metric_unit_t user_temp = UnitNotFound;
  std::string temp_unit;
  if (show_temperature)
    {
    user_temp = OvmsMetricGetUserUnit(GrpTemp, Celcius);
    // (To Check: '°' is not SMS safe, so we only output 'C')
    temp_unit = OvmsMetricUnitLabel(user_temp);
    }

  int vwarn=0, valert=0;
  int twarn=0, talert=0;
  for (c=0; c<m_bms_readings_v; c++)
    {
    switch (m_bms_valerts[c])
      {
      case OvmsBmsStatus::OK: break;
      case OvmsBmsStatus::Warn:  vwarn++; break;
      case OvmsBmsStatus::Alert: valert++;break;
      }
    }
  for (c=0; c<m_bms_readings_t; c++)
    {
    switch (m_bms_talerts[c])
      {
      case OvmsBmsStatus::OK: break;
      case OvmsBmsStatus::Warn: twarn++; break;
      case OvmsBmsStatus::Alert: talert++; break;
      }
    }
  if (show_voltage)
    {
    writer->puts("Voltage:");
    writer->printf("    Average: %5.3fV [%5.3fV - %5.3fV]\n",
      StdMetrics.ms_v_bat_pack_vavg->AsFloat(),
      StdMetrics.ms_v_bat_pack_vmin->AsFloat(),
      StdMetrics.ms_v_bat_pack_vmax->AsFloat());
    writer->printf("  Deviation: SD %6.2fmV [max %.2fmV], %d warnings, %d alerts\n",
      StdMetrics.ms_v_bat_pack_vstddev->AsFloat()*1000,
      StdMetrics.ms_v_bat_pack_vstddev_max->AsFloat()*1000,
      vwarn, valert);
    }

  if (show_temperature)
    {
    writer->puts("Temperature:");
    writer->printf("    Average: %5.1f%s [%5.1f%s - %5.1f%s]\n",
      StdMetrics.ms_v_bat_pack_tavg->AsFloat(0, user_temp), temp_unit.c_str(),
      StdMetrics.ms_v_bat_pack_tmin->AsFloat(0, user_temp), temp_unit.c_str(),
      StdMetrics.ms_v_bat_pack_tmax->AsFloat(0, user_temp), temp_unit.c_str());
    writer->printf("  Deviation: SD %6.2f%s  [max %.2f%s], %d warnings, %d alerts\n",
      StdMetrics.ms_v_bat_pack_tstddev->AsFloat(0, user_temp), temp_unit.c_str(),
      StdMetrics.ms_v_bat_pack_tstddev_max->AsFloat(0, user_temp), temp_unit.c_str(),
      twarn, talert);
    }

  writer->puts("Cells:");
  int kv = 0;
  int kt = 0;
  int module_count = 0;
  if (show_voltage)
    module_count = round_up_div(m_bms_readings_v,m_bms_readingspermodule_v);
  if (show_temperature)
    {
    int temp_module_count  = round_up_div(m_bms_readings_t,m_bms_readingspermodule_t);
    if (temp_module_count > module_count)
      module_count = temp_module_count;
    }
  int max_cols_v = 0;
  if (show_voltage)
    max_cols_v = check_max_cols(m_bms_readingspermodule_v, show_temperature?4:5);
  int max_cols_t = 0;
  if (show_temperature)
    max_cols_t = check_max_cols(m_bms_readingspermodule_t, 5-max_cols_v);

  for (int module = 0; module < module_count; ++module)
    {
    writer->printf("    +");
    if (show_voltage)
      {
      for (c=0;c<max_cols_v;c++) { writer->printf("-------"); }
      writer->printf("-+");
      }

      if (show_temperature) {
        for (c=0;c<max_cols_t;c++) { writer->printf("-------"); }
        writer->printf("-+");
      }
      writer->puts("");

    int rows_v = 0, rows_t = 0;
    int reading_left_v = 0, reading_left_t = 0;
    if (show_voltage)
      {
      int items_left_v = m_bms_readings_v - kv;
      reading_left_v = std::min(items_left_v, m_bms_readingspermodule_v);
      rows_v = round_up_div(reading_left_v, max_cols_v);
      }
    if (show_temperature)
      {
      int items_left_t = m_bms_readings_t - kt;
      reading_left_t = std::min(items_left_t, m_bms_readingspermodule_t);
      rows_t = round_up_div(reading_left_t, max_cols_t);
      }
    int rows = std::max(rows_v,rows_t);
    for (int row = 0 ; row < rows; ++row)
      {
      if (row == 0)
        writer->printf("%3d |",module+1);
      else
        writer->printf("    |");
      if (show_voltage)
        {
        for (c=0; c<max_cols_v; c++)
          {
          if (kv < m_bms_readings_v && (reading_left_v > 0))
            {
            writer->printf(" %5.3fV",m_bms_voltages[kv]);
            --reading_left_v;
            ++kv;
            }
          else
            writer->printf("       ");
          }
        writer->printf(" |");
        }
      if (show_temperature)
        {
        for (c=0; c<m_bms_readingspermodule_t; c++)
          {
          if (kt < m_bms_readings_t && (reading_left_t > 0))
            {
            writer->printf(" %5.1f%s",UnitConvert(Celcius, user_temp, m_bms_temperatures[kt]), temp_unit.c_str());
            --reading_left_t;
            ++kt;
            }
          else
            writer->printf("       ");
          }
        writer->printf(" |");
        }
      writer->puts("");
      }
    }

  writer->printf("    +");
  if (show_voltage)
    {
    for (c=0;c<max_cols_v;c++) { writer->printf("-------"); }
    writer->printf("-+");
    }
  if (show_temperature)
    {
    for (c=0;c<max_cols_t;c++) { writer->printf("-------"); }
    writer->printf("-+");
    }
  writer->puts("");
  }

bool OvmsBmsMonitor::FormatBmsAlerts(int verbosity, OvmsWriter* writer, bool show_warnings)
  {
  int has_valerts = 0, has_talerts = 0;
  bool verbose = (verbosity > COMMAND_RESULT_SMS);

  writer->puts("Battery cell deviation:");

  // Voltages:
  if (m_bms_readings_v > 0)
    {
    writer->printf("Voltage: StdDev %dmV", (int)(StdMetrics.ms_v_bat_pack_vstddev_max->AsFloat() * 1000));
    for (int i=0; i<m_bms_readings_v; i++)
      {
      OvmsBmsStatus sts = OvmsBmsStatus(StdMetrics.ms_v_bat_cell_valert->GetElemValue(i));
      switch (sts)
        {
          case OvmsBmsStatus::OK: continue;
          case OvmsBmsStatus::Warn: if (!show_warnings) continue;
          case OvmsBmsStatus::Alert: ;
        }
      has_valerts++;
      if (verbose || has_valerts <= 5)
        {
        int dev = StdMetrics.ms_v_bat_cell_vdevmax->GetElemValue(i) * 1000;
        writer->printf("\n %c #%02d: %+4dmV", (sts==OvmsBmsStatus::Warn) ? '?' : '!', i+1, dev);
        }
      else
        {
        writer->printf("\n [...]");
        break;
        }
      }
    writer->printf("%s\n", has_valerts ? "" : ", cells OK");
    }

  // Temperatures:
  if ( m_bms_readings_t > 0)
    {
    metric_unit_t user_temp =  OvmsMetricGetUserUnit(GrpTemp, Celcius);
    std::string temp_unit = OvmsMetricUnitLabel(user_temp);
    writer->printf("Temperature: StdDev %.1f%s", StdMetrics.ms_v_bat_pack_tstddev_max->AsFloat(0, user_temp), temp_unit.c_str());
    for (int i=0; i<m_bms_readings_v; i++)
      {
      OvmsBmsStatus sts = OvmsBmsStatus(StdMetrics.ms_v_bat_cell_talert->GetElemValue(i));
      switch (sts)
        {
          case OvmsBmsStatus::OK: continue;
          case OvmsBmsStatus::Warn: if (!show_warnings) continue;
          case OvmsBmsStatus::Alert: ;
        }
      has_talerts++;
      if (verbose || has_talerts <= 5)
        {
        float dev = StdMetrics.ms_v_bat_cell_tdevmax->GetElemValue(i, user_temp);
        writer->printf("\n %c #%02d: %+3.1f%s", (sts==OvmsBmsStatus::Warn) ? '?' : '!', i+1, dev, temp_unit.c_str());
        }
      else
        {
        writer->printf("\n [...]");
        break;
        }
      }
    writer->printf("%s\n", has_talerts ? "" : ", cells OK");
    }

  if (verbose || has_valerts >= 5 || has_talerts >= 5)
    {
    writer->puts("Details: bms status");
    }

  return (has_valerts > 0) || (has_talerts > 0);
  }

void OvmsBmsMonitor::NotifyBmsAlerts()
  {
  if (m_bms_readings_v == 0 && m_bms_readings_t == 0)
    return;
  StringWriter buf(200);
  if (FormatBmsAlerts(COMMAND_RESULT_SMS, &buf, false))
    MyNotify.NotifyString("alert", "batt.bms.alert", buf.c_str());
  }

void OvmsBmsMonitor::BmsTicker(std::string event, void* data)
  {
  // Alerts:
  if (m_bms_valerts_new || m_bms_talerts_new)
    {
    ESP_LOGW(TAG, "BMS new alerts: %d voltages, %d temperatures", m_bms_valerts_new, m_bms_talerts_new);
    MyEvents.SignalEvent("vehicle.alert.bms", NULL);
    if (m_autonotifications && MyConfig.GetParamValueBool("vehicle", "bms.alerts.enabled", true))
      NotifyBmsAlerts();
    m_bms_valerts_new = 0;
    m_bms_talerts_new = 0;
    }

  // Log cell voltages:
  int vlog_interval = MyConfig.GetParamValueInt("vehicle", "bms.log.voltage.interval", 0);
  if (vlog_interval > 0 && m_bms_vlog_last + vlog_interval < monotonictime &&
      StdMetrics.ms_v_bat_cell_voltage->LastModified() > m_bms_vlog_last)
    {
    m_bms_vlog_last = monotonictime;
    ESP_LOGI(TAG, "BMS cell voltage: avg=%.3f min=%.3f max=%.3f grad=%.1f stddev=%.1f sdavg=%.1f sdmax=%.1f cells: %s",
      StdMetrics.ms_v_bat_pack_vavg->AsFloat(),
      StdMetrics.ms_v_bat_pack_vmin->AsFloat(),
      StdMetrics.ms_v_bat_pack_vmax->AsFloat(),
      StdMetrics.ms_v_bat_pack_vgrad->AsFloat() * 1000,
      StdMetrics.ms_v_bat_pack_vstddev->AsFloat() * 1000,
      m_bms_vstddev_avg * 1000,
      StdMetrics.ms_v_bat_pack_vstddev_max->AsFloat() * 1000,
      StdMetrics.ms_v_bat_cell_voltage->AsString("", Native, 3).c_str());
    }

  // Log cell temperatures:
  int tlog_interval = MyConfig.GetParamValueInt("vehicle", "bms.log.temp.interval", 0);
  if (tlog_interval > 0 && m_bms_tlog_last + tlog_interval < monotonictime &&
      StdMetrics.ms_v_bat_cell_temp->LastModified() > m_bms_tlog_last)
    {
    m_bms_tlog_last = monotonictime;
    ESP_LOGI(TAG, "BMS cell temperature: avg=%.1f min=%.1f max=%.1f stddev=%.1f sdmax=%.1f cells: %s",
      StdMetrics.ms_v_bat_pack_tavg->AsFloat(),
      StdMetrics.ms_v_bat_pack_tmin->AsFloat(),
      StdMetrics.ms_v_bat_pack_tmax->AsFloat(),
      StdMetrics.ms_v_bat_pack_tstddev->AsFloat(),
      StdMetrics.ms_v_bat_pack_tstddev_max->AsFloat(),
      StdMetrics.ms_v_bat_cell_temp->AsString("", Native, 1).c_str());
    }
  }

bool OvmsBmsMonitor::IsEnabled(bms_status_t statusmode)
  {
  switch (statusmode)
    {
    case bms_status_t::Voltage:
      return (m_bms_readingspermodule_v > 0);
    case bms_status_t::Temperature:
      return (m_bms_readingspermodule_t > 0);
    default:
      return (m_bms_readingspermodule_v > 0) || (m_bms_readingspermodule_t > 0);
    }
  }

void OvmsBmsMonitor::SetWebmoduleEnabled(bool enabled)
  {
  if (enabled == m_webmodule_enabled)
    return;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  if (enabled)
    MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  else
    MyWebServer.DeregisterPage("/bms/cellmon");
#endif
  m_webmodule_enabled = enabled;
  }

void OvmsBmsMonitor::bms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {

  const char* smode = cmd->GetName();
  OvmsBmsMonitor::bms_status_t statusmode = bms_status_t::Both;
  if (strcmp(smode,"volt")==0)
    statusmode = OvmsBmsMonitor::bms_status_t::Voltage;
  else if (strcmp(smode,"temp")==0)
    statusmode = OvmsBmsMonitor::bms_status_t::Temperature;

  MyBmsMonitor.Status(verbosity, writer, statusmode);
  }

void OvmsBmsMonitor::bms_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyBmsMonitor.ResetCellStats();
  writer->puts("BMS cell statistics have been reset.");
  }
void OvmsBmsMonitor::bms_alerts(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyBmsMonitor.IsEnabled(bms_status_t::Both) )
    {
    writer->puts("BMS Statistics Not Recorded");
    return;
    }
  MyBmsMonitor.FormatBmsAlerts(verbosity, writer, true);
  }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
duk_ret_t OvmsBmsMonitor::DukOvmsBmsVoltageSetCellArrangement(duk_context *ctx)
  {
  // VoltageSetCellArrangement( readings, readingspermodule)
  int readings = duk_to_int(ctx, 0);
  int permodule = duk_to_int(ctx, -1);
  MyBmsMonitor.SetCellArrangementVoltage(readings, permodule);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsVoltageSetCellDefaultThresholds(duk_context *ctx)
  {
  // VoltageSetCellDefaultThresholds(warn, alert [, max_grad] [, max_std_dev])``
  duk_idx_t numArgs = duk_get_top(ctx);
  if (numArgs < 2)
    return 0;
  float warn = duk_to_number(ctx, 0);
  float alert = duk_to_number(ctx, -1);
  float maxgrad = -1;
  float maxsddev = -1;
  if (numArgs > 2)
    {
    maxgrad = duk_to_number(ctx, -2);
    if (numArgs > 3)
      maxsddev = duk_to_number(ctx, -3);
    }

  MyBmsMonitor.SetCellDefaultThresholdsVoltage(warn, alert, maxgrad, maxsddev);

  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsVoltageSetCellLimits(duk_context *ctx)
  {
  float minv = duk_to_number(ctx, 0);
  float maxv = duk_to_number(ctx, -1);

  MyBmsMonitor.SetCellLimitsVoltage(minv, maxv);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsVoltageSetCell(duk_context *ctx)
  {
  // VoltageSetCell(int index, float value)
  float index = duk_to_int(ctx, 0);
  float val = duk_to_number(ctx, -1);

  MyBmsMonitor.SetCellVoltage(index, val);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsVoltageResetCells(duk_context *ctx)
  {
  // VoltageResetCells( [full] )
  //

  duk_idx_t numArgs = duk_get_top(ctx);
  bool full = false;

  if (numArgs > 0)
    full = duk_to_boolean(ctx, 0);

  MyBmsMonitor.ResetCellVoltages(full);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsVoltagesRestartCells(duk_context *ctx)
  {
  MyBmsMonitor.RestartCellVoltages();
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsTemperatureSetCellArrangement(duk_context *ctx)
  {
  // TemperatureSetCellArrangement( readings, readingspermodule)
  int readings = duk_to_int(ctx, 0);
  int permodule = duk_to_int(ctx, -1);
  MyBmsMonitor.SetCellArrangementTemperature(readings, permodule);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsTemperatureSetCellDefaultThresholds(duk_context *ctx)
  {
  // TemperatureSetCellDefaultThresholds(warn, alert)
  float warn = duk_to_number(ctx, 0);
  float alert = duk_to_number(ctx, -1);
  MyBmsMonitor.SetCellDefaultThresholdsTemperature(warn, alert);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsTemperatureSetCellLimits(duk_context *ctx)
  {
  // TemperatureSetCellLimits(min, max)
  float minv = duk_to_number(ctx, 0);
  float maxv = duk_to_number(ctx, -1);

  MyBmsMonitor.SetCellLimitsTemperature(minv, maxv);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsTemperatureSetCell(duk_context *ctx)
  {
  // TemperatureSetCell(index, value)
  float index = duk_to_int(ctx, 0);
  float val = duk_to_number(ctx, -1);

  MyBmsMonitor.SetCellTemperature(index, val);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsTemperatureResetCells(duk_context *ctx)
  {
  // TemperatureResetCells( [full] )

  duk_idx_t numArgs = duk_get_top(ctx);
  bool full = false;

  if (numArgs > 0)
    full = duk_to_boolean(ctx, 0);

  MyBmsMonitor.ResetCellTemperatures(full);
  return 0;
  }

duk_ret_t OvmsBmsMonitor::DukOvmsBmsTemperatureRestartCells(duk_context *ctx)
  {
  // TemperatureRestartCells()
  MyBmsMonitor.RestartCellTemperatures();
  return 0;
  }

#endif // CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
