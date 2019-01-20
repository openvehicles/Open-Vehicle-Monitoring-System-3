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
static const char *TAG = "metrics";

#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include "ovms.h"
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "ovms_script.h"
#include "string.h"

using namespace std;

OvmsMetrics       MyMetrics       __attribute__ ((init_priority (1800)));

void metrics_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool found = false;
  for (OvmsMetric* m=MyMetrics.m_first; m != NULL; m=m->m_next)
    {
    const char *k = m->m_name;
    std::string v = m->AsString();
    if ((argc==0)||(strstr(k,argv[0])))
      {
      if (v.empty())
        writer->printf("%s\n",k);
      else
        writer->printf("%-40.40s %s%s\n",k,v.c_str(),OvmsMetricUnitLabel(m->GetUnits()));
      found = true;
      }
    }
  if (!found)
    puts("Unrecognised metric name");
  }

void metrics_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyMetrics.Set(argv[0],argv[1]))
    writer->puts("Metric set");
  else
    writer->puts("Metric could not be set");
  }

void metrics_trace(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (strcmp(cmd->GetName(),"on")==0)
    MyMetrics.m_trace = true;
  else
    MyMetrics.m_trace = false;

  writer->printf("Metric tracing is now %s\n",cmd->GetName());
  }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

static duk_ret_t DukOvmsMetricValue(duk_context *ctx)
  {
  const char *mn = duk_to_string(ctx,0);
  OvmsMetric *m = MyMetrics.Find(mn);
  if (m)
    {
    duk_push_string(ctx, m->AsString().c_str());
    return 1;  /* one return value */
    }
  else
    return 0;
  }

static duk_ret_t DukOvmsMetricFloat(duk_context *ctx)
  {
  const char *mn = duk_to_string(ctx,0);
  OvmsMetric *m = MyMetrics.Find(mn);
  if (m)
    {
    duk_push_number(ctx, m->AsFloat());
    return 1;  /* one return value */
    }
  else
    return 0;
  }

#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

MetricCallbackEntry::MetricCallbackEntry(const char* caller, MetricCallback callback)
  {
  m_caller = caller;
  m_callback = callback;
  }

MetricCallbackEntry::~MetricCallbackEntry()
  {
  }

OvmsMetrics::OvmsMetrics()
  {
  ESP_LOGI(TAG, "Initialising METRICS (1810)");

  m_nextmodifier = 1;
  m_first = NULL;
  m_trace = false;

  // Register our commands
  OvmsCommand* cmd_metric = MyCommandApp.RegisterCommand("metrics","METRICS framework",NULL, "", 0, 0, true);
  cmd_metric->RegisterCommand("list","Show all metrics",metrics_list, "[<metric>]", 0, 1, true);
  cmd_metric->RegisterCommand("set","Set the value of a metric",metrics_set, "<metric> <value>", 2, 2, true);
  OvmsCommand* cmd_metrictrace = cmd_metric->RegisterCommand("trace","METRIC trace framework", NULL, "", 0, 0, true);
  cmd_metrictrace->RegisterCommand("on","Turn metric tracing ON",metrics_trace,"", 0, 0, true);
  cmd_metrictrace->RegisterCommand("off","Turn metric tracing OFF",metrics_trace,"", 0, 0, true);

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  ESP_LOGI(TAG, "Expanding DUKTAPE javascript engine");
  DuktapeObjectRegistration* dto = new DuktapeObjectRegistration("OvmsMetrics");
  dto->RegisterDuktapeFunction(DukOvmsMetricValue, 1, "Value");
  dto->RegisterDuktapeFunction(DukOvmsMetricFloat, 1, "AsFloat");
  MyScripts.RegisterDuktapeObject(dto);
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

OvmsMetrics::~OvmsMetrics()
  {
  for (OvmsMetric* m = m_first; m!=NULL;)
    {
    OvmsMetric* c = m;
    m = m->m_next;
    delete c;
    }
  }

void OvmsMetrics::RegisterMetric(OvmsMetric* metric)
  {
  // Quick simple check for if we are the first metric.
  if (m_first == NULL)
    {
    m_first = metric;
    return;
    }

  // Now, check if we are before the first
  if (strcmp(m_first->m_name,metric->m_name)>=0)
    {
    metric->m_next = m_first;
    m_first = metric;
    return;
    }

  // Search for the correct place to insert
  for (OvmsMetric*m = m_first; m!=NULL; m=m->m_next)
    {
    if (m->m_next == NULL)
      {
      m->m_next = metric;
      return;
      }
    if (strcmp(m->m_next->m_name,metric->m_name)>=0)
      {
      metric->m_next = m->m_next;
      m->m_next = metric;
      return;
      }
    }
  }

void OvmsMetrics::DeregisterMetric(OvmsMetric* metric)
  {
  if (m_first == metric)
    {
    m_first = metric->m_next;
    delete metric;
    return;
    }

  for (OvmsMetric* m=m_first;m!=NULL;m=m->m_next)
    {
    if (m->m_next == metric)
      {
      m->m_next = metric->m_next;
      delete metric;
      return;
      }
    }
  }

bool OvmsMetrics::Set(const char* metric, const char* value)
  {
  OvmsMetric* m = Find(metric);
  if (m == NULL) return false;

  m->SetValue(std::string(value));
  return true;
  }

bool OvmsMetrics::SetInt(const char* metric, int value)
  {
  OvmsMetricInt* m = (OvmsMetricInt*)Find(metric);
  if (m == NULL) return false;

  m->SetValue(value);
  return true;
  }

bool OvmsMetrics::SetBool(const char* metric, bool value)
  {
  OvmsMetricBool* m = (OvmsMetricBool*)Find(metric);
  if (m == NULL) return false;

  m->SetValue(value);
  return true;
  }

bool OvmsMetrics::SetFloat(const char* metric, float value)
  {
  OvmsMetricFloat* m = (OvmsMetricFloat*)Find(metric);
  if (m == NULL) return false;

  m->SetValue(value);
  return true;
  }

OvmsMetric* OvmsMetrics::Find(const char* metric)
  {
  for (OvmsMetric* m=m_first; m != NULL; m=m->m_next)
    {
    if (strcmp(m->m_name,metric)==0) return m;
    }
  return NULL;
  }

OvmsMetricString* OvmsMetrics::InitString(const char* metric, uint16_t autostale, const char* value, metric_unit_t units)
  {
  OvmsMetricString *m = (OvmsMetricString*)Find(metric);
  if (m==NULL) m = new OvmsMetricString(metric, autostale, units);

  if (value)
    m->SetValue(value);
  return m;
  }

OvmsMetricInt* OvmsMetrics::InitInt(const char* metric, uint16_t autostale, int value, metric_unit_t units)
  {
  OvmsMetricInt *m = (OvmsMetricInt*)Find(metric);
  if (m==NULL) m = new OvmsMetricInt(metric, autostale, units);

  m->SetValue(value);
  return m;
  }

OvmsMetricBool* OvmsMetrics::InitBool(const char* metric, uint16_t autostale, bool value, metric_unit_t units)
  {
  OvmsMetricBool *m = (OvmsMetricBool*)Find(metric);

  if (m==NULL) m = new OvmsMetricBool(metric, autostale, units);

  m->SetValue(value);
  return m;
  }

OvmsMetricFloat* OvmsMetrics::InitFloat(const char* metric, uint16_t autostale, float value, metric_unit_t units)
  {
  OvmsMetricFloat *m = (OvmsMetricFloat*)Find(metric);

  if (m==NULL) m = new OvmsMetricFloat(metric, autostale, units);

  m->SetValue(value);
  return m;
  }

void OvmsMetrics::RegisterListener(const char* caller, const char* name, MetricCallback callback)
  {
  auto k = m_listeners.find(name);
  if (k == m_listeners.end())
    {
    m_listeners[name] = new MetricCallbackList();
    k = m_listeners.find(name);
    }
  if (k == m_listeners.end())
    {
    ESP_LOGE(TAG, "Problem registering metric %s for caller %s",name,caller);
    return;
    }

  MetricCallbackList *ml = k->second;
  ml->push_back(new MetricCallbackEntry(caller,callback));
  }

void OvmsMetrics::DeregisterListener(const char* caller)
  {
  MetricCallbackMap::iterator itm=m_listeners.begin();
  while (itm!=m_listeners.end())
    {
    MetricCallbackList* ml = itm->second;
    MetricCallbackList::iterator itc=ml->begin();
    while (itc!=ml->end())
      {
      MetricCallbackEntry* ec = *itc;
      if (ec->m_caller == caller)
        {
        itc = ml->erase(itc);
        delete ec;
        }
      else
        {
        ++itc;
        }
      }
    if (ml->empty())
      {
      itm = m_listeners.erase(itm);
      delete ml;
      }
    else
      {
      ++itm;
      }
    }
  }

void OvmsMetrics::NotifyModified(OvmsMetric* metric)
  {
  if (m_trace &&
      strcmp(metric->m_name, "m.monotonic") != 0 &&
      strcmp(metric->m_name, "m.time.utc") != 0 &&
      strcmp(metric->m_name, "v.e.parktime") != 0 &&
      strcmp(metric->m_name, "v.e.drivetime") != 0 &&
      strcmp(metric->m_name, "v.c.time") != 0)
    {
    ESP_LOGI(TAG, "Modified metric %s: %s",
      metric->m_name, metric->AsUnitString().c_str());
    }

  auto k = m_listeners.find("*");
  for (int x=0;x<2;x++)
    {
    if (k != m_listeners.end())
      {
      MetricCallbackList* ml = k->second;
      if (ml)
        {
        for (MetricCallbackList::iterator itc=ml->begin(); itc!=ml->end(); ++itc)
          {
          MetricCallbackEntry* ec = *itc;
          ec->m_callback(metric);
          }
        }
      }
    k = m_listeners.find(metric->m_name);
    }
  }

size_t OvmsMetrics::RegisterModifier()
  {
  return m_nextmodifier++;
  }

OvmsMetric::OvmsMetric(const char* name, uint16_t autostale, metric_unit_t units)
  {
  m_defined = NeverDefined;
  m_modified.reset();
  m_name = name;
  m_lastmodified = 0;
  m_autostale = autostale;
  m_units = units;
  m_next = NULL;
  MyMetrics.RegisterMetric(this);
  }

OvmsMetric::~OvmsMetric()
  {
  MyMetrics.DeregisterMetric(this);

  // Warning: pointers to a deleted OvmsMetric can still be held locally in
  //  other modules. If you delete metrics, take care to inform all readers
  //  (i.e. by broadcasting a module shutdown event).
  }

std::string OvmsMetric::AsString(const char* defvalue, metric_unit_t units, int precision)
  {
  return std::string(defvalue);
  }

std::string OvmsMetric::AsUnitString(const char* defvalue, metric_unit_t units, int precision)
  {
  if (!IsDefined())
    return std::string(defvalue);
  return AsString(defvalue, units, precision) + OvmsMetricUnitLabel(units==Native ? GetUnits() : units);
  }

std::string OvmsMetric::AsJSON(const char* defvalue, metric_unit_t units, int precision)
  {
  std::string buf = "\"";
  buf.append(json_encode(AsString(defvalue, units, precision)));
  buf.append("\"");
  return buf;
  }

float OvmsMetric::AsFloat(const float defvalue, metric_unit_t units)
  {
  return defvalue;
  }

void OvmsMetric::SetValue(std::string value)
  {
  }

void OvmsMetric::operator=(std::string value)
  {
  }

uint32_t OvmsMetric::LastModified()
  {
  return m_lastmodified;
  }

uint32_t OvmsMetric::Age()
  {
  return monotonictime - m_lastmodified;
  }

void OvmsMetric::SetModified(bool changed)
  {
  if (m_defined == NeverDefined)
    m_defined = FirstDefined;
  else
    m_defined = Defined;
  m_stale = false;
  m_lastmodified = monotonictime;
  if (changed)
    {
    m_modified.set();
    MyMetrics.NotifyModified(this);
    }
  }

bool OvmsMetric::IsDefined()
  {
  return (m_defined != NeverDefined);
  }

bool OvmsMetric::IsFirstDefined()
  {
  return (m_defined == FirstDefined);
  }

bool OvmsMetric::IsStale()
  {
  if (m_autostale>0)
    {
    if (m_lastmodified < (monotonictime-m_autostale))
      m_stale=true;
    else
      m_stale=false;
    }
  return m_stale;
  }

void OvmsMetric::SetStale(bool stale)
  {
  m_stale = stale;
  }

void OvmsMetric::SetAutoStale(uint16_t seconds)
  {
  m_autostale = seconds;
  }

metric_unit_t OvmsMetric::GetUnits()
  {
  return m_units;
  }

bool OvmsMetric::IsModified(size_t modifier)
  {
  return m_modified[modifier];
  }

bool OvmsMetric::IsModifiedAndClear(size_t modifier)
  {
  bool modified = m_modified[modifier];
  if (modified) m_modified.reset(modifier);
  return modified;
  }

void OvmsMetric::ClearModified(size_t modifier)
  {
  m_modified.reset(modifier);
  }

OvmsMetricInt::OvmsMetricInt(const char* name, uint16_t autostale, metric_unit_t units)
  : OvmsMetric(name, autostale, units)
  {
  m_value = 0;
  }

OvmsMetricInt::~OvmsMetricInt()
  {
  }

std::string OvmsMetricInt::AsString(const char* defvalue, metric_unit_t units, int precision)
  {
  if (IsDefined())
    {
    char buffer[33];
    if ((units != Other)&&(units != m_units))
      itoa(UnitConvert(m_units,units,m_value),buffer,10);
    else
      itoa (m_value,buffer,10);
    return buffer;
    }
  else
    {
    return std::string(defvalue);
    }
  }

std::string OvmsMetricInt::AsJSON(const char* defvalue, metric_unit_t units, int precision)
  {
  if (IsDefined())
    return AsString(defvalue, units, precision);
  else
    return std::string((defvalue && *defvalue) ? defvalue : "0");
  }

float OvmsMetricInt::AsFloat(const float defvalue, metric_unit_t units)
  {
  return (float)AsInt((int)defvalue, units);
  }

int OvmsMetricInt::AsInt(const int defvalue, metric_unit_t units)
  {
  if (IsDefined())
    {
    if ((units != Other)&&(units != m_units))
      return UnitConvert(m_units,units,m_value);
    else
      return m_value;
    }
  else
    return defvalue;
  }

void OvmsMetricInt::SetValue(int value, metric_unit_t units)
  {
  int nvalue = value;
  if ((units != Other)&&(units != m_units)) nvalue=UnitConvert(units,m_units,value);

  if (m_value != nvalue)
    {
    m_value = nvalue;
    SetModified(true);
    }
  else
    SetModified(false);
  }

void OvmsMetricInt::SetValue(std::string value)
  {
  int nvalue = atoi(value.c_str());
  if (m_value != nvalue)
    {
    m_value = nvalue;
    SetModified(true);
    }
  else
    SetModified(false);
  }

OvmsMetricBool::OvmsMetricBool(const char* name, uint16_t autostale, metric_unit_t units)
  : OvmsMetric(name, autostale, units)
  {
  m_value = false;
  }

OvmsMetricBool::~OvmsMetricBool()
  {
  }

std::string OvmsMetricBool::AsString(const char* defvalue, metric_unit_t units, int precision)
  {
  if (IsDefined())
    {
    if (m_value)
      return std::string("yes");
    else
      return std::string("no");
    }
  else
    {
    return std::string(defvalue);
    }
  }

std::string OvmsMetricBool::AsJSON(const char* defvalue, metric_unit_t units, int precision)
  {
  if (IsDefined())
    {
    if (m_value)
      return std::string("true");
    else
      return std::string("false");
    }
  else
    {
    if (strtobool(defvalue) == true)
      return std::string("true");
    else
      return std::string("false");
    }
  }

float OvmsMetricBool::AsFloat(const float defvalue, metric_unit_t units)
  {
  return (float)AsBool((bool)defvalue);
  }

int OvmsMetricBool::AsBool(const bool defvalue)
  {
  if (IsDefined())
    return m_value;
  else
    return defvalue;
  }

void OvmsMetricBool::SetValue(bool value)
  {
  if (m_value != value)
    {
    m_value = value;
    SetModified(true);
    }
  else
    SetModified(false);
  }

void OvmsMetricBool::SetValue(std::string value)
  {
  bool nvalue = strtobool(value);
  if (m_value != nvalue)
    {
    m_value = nvalue;
    SetModified(true);
    }
  else
    SetModified(false);
  }

OvmsMetricFloat::OvmsMetricFloat(const char* name, uint16_t autostale, metric_unit_t units)
  : OvmsMetric(name, autostale, units)
  {
  m_value = 0;
  }

OvmsMetricFloat::~OvmsMetricFloat()
  {
  }

std::string OvmsMetricFloat::AsString(const char* defvalue, metric_unit_t units, int precision)
  {
  if (IsDefined())
    {
    std::ostringstream ss;
    if (precision >= 0)
      {
      ss.precision(precision); // Set desired precision
      ss << fixed;
      }
    if ((units != Other)&&(units != m_units))
      ss << UnitConvert(m_units,units,m_value);
    else
      ss << m_value;
    std::string s(ss.str());
    return s;
    }
  else
    {
    return std::string(defvalue);
    }
  }

std::string OvmsMetricFloat::AsJSON(const char* defvalue, metric_unit_t units, int precision)
  {
  if (IsDefined())
    return AsString(defvalue, units, precision);
  else
    return std::string((defvalue && *defvalue) ? defvalue : "0");
  }

float OvmsMetricFloat::AsFloat(const float defvalue, metric_unit_t units)
  {
  if (IsDefined())
    {
    if ((units != Other)&&(units != m_units))
      return UnitConvert(m_units,units,m_value);
    else
      return m_value;
    }
  else
    return defvalue;
  }

int OvmsMetricFloat::AsInt(const int defvalue, metric_unit_t units)
  {
  return (int) AsFloat((float) defvalue, units);
  }

void OvmsMetricFloat::SetValue(float value, metric_unit_t units)
  {
  float nvalue = value;
  if ((units != Other)&&(units != m_units)) nvalue=UnitConvert(units,m_units,value);

  if (m_value != nvalue)
    {
    m_value = nvalue;
    SetModified(true);
    }
  else
    SetModified(false);
  }

void OvmsMetricFloat::SetValue(std::string value)
  {
  float nvalue = atof(value.c_str());
  if (m_value != nvalue)
    {
    m_value = nvalue;
    SetModified(true);
    }
  else
    SetModified(false);
  }

OvmsMetricString::OvmsMetricString(const char* name, uint16_t autostale, metric_unit_t units)
  : OvmsMetric(name, autostale, units)
  {
  }

OvmsMetricString::~OvmsMetricString()
  {
  }

std::string OvmsMetricString::AsString(const char* defvalue, metric_unit_t units, int precision)
  {
  if (IsDefined())
    return m_value;
  else
    return std::string(defvalue);
  }

void OvmsMetricString::SetValue(std::string value)
  {
  if (m_value.compare(value)!=0)
    {
    m_value = value;
    SetModified(true);
    }
  else
    SetModified(false);
  }

const char* OvmsMetricUnitLabel(metric_unit_t units)
  {
  switch (units)
    {
    case Kilometers:   return "km";
    case Miles:        return "M";
    case Meters:       return "m";
    case Celcius:      return "°C";
    case Fahrenheit:   return "°F";
    case kPa:          return "kPa";
    case Pa:           return "Pa";
    case PSI:          return "psi";
    case Volts:        return "V";
    case Amps:         return "A";
    case AmpHours:     return "Ah";
    case kW:           return "kW";
    case kWh:          return "kWh";
    case Watts:        return "W";
    case WattHours:    return "Wh";
    case Seconds:      return "Sec";
    case Minutes:      return "Min";
    case Hours:        return "Hour";
    case Degrees:      return "°";
    case Kph:          return "km/h";
    case Mph:          return "Mph";
    case KphPS:        return "km/h/s";
    case MphPS:        return "Mph/s";
    case MetersPSS:    return "m/s²";
    case dbm:          return "dBm";
    case sq:           return "sq";
    case Percentage:   return "%";
    case WattHoursPK:  return "Wh/km";
    case WattHoursPM:  return "Wh/mi";
    case Nm:           return "Nm";
    default:           return "";
    }
  }

int UnitConvert(metric_unit_t from, metric_unit_t to, int value)
  {
  switch (from)
    {
    case Kilometers:
      if (to == Miles) return (value*5)/8;
      else if (to == Meters) return value/1000;
      break;
    case Miles:
      if (to == Kilometers) return (value*8)/5;
      else if (to == Meters) return (value*8000)/5;
      break;
    case KphPS:
      if (to == MphPS) return (value*5)/8;
      else if (to == MetersPSS) return value/1000;
      break;
    case MphPS:
      if (to == KphPS) return (value*8)/5;
      else if (to == MetersPSS) return (value*8000)/5;
      break;
    case kW:
      if (to == Watts) return (value*1000);
      break;
    case Watts:
      if (to == kW) return (value/1000);
      break;
    case kWh:
      if (to == WattHours) return (value*1000);
      break;
    case WattHours:
      if (to == kWh) return (value/1000);
      break;
    case WattHoursPK:
      if (to == WattHoursPM) return (value*8)/5;
      break;
    case WattHoursPM:
      if (to == WattHoursPK) return (value*5)/8;
      break;
    case Celcius:
      if (to == Fahrenheit) return ((value*9)/5) + 32;
      break;
    case Fahrenheit:
      if (to == Celcius) return ((value-32)*5)/9;
      break;
    case kPa:
      if (to == Pa) return value*1000;
      else if (to == PSI) return int((float)value * 0.14503773773020923);
    case Pa:
      if (to == kPa) return value/1000;
      else if (to == PSI) return int((float)value * 0.00014503773773020923);
    case PSI:
      if (to == kPa) return int((float)value * 6.894757293168361);
      else if (to == Pa) return int((float)value * 0.006894757293168361);
      break;
    case Seconds:
      if (to == Minutes) return value/60;
      else if (to == Hours) return value/3600;
      break;
    case Minutes:
      if (to == Seconds) return value*60;
      else if (to == Hours) return value/60;
      break;
    case Hours:
      if (to == Seconds) return value*3600;
      else if (to == Minutes) return value*60;
      break;
    case Kph:
      if (to == Mph) return (value*5)/8;
      break;
    case Mph:
      if (to == Kph) return (value*8)/5;
      break;
    case dbm:
      if (to == sq) return (value <= -51)?((value + 113)/2):0;
      break;
    case sq:
      if (to == dbm) return (value <= 31)?(-113 + (value*2)):0;
      break;
    default:
      return value;
    }
  return value;
  }

float UnitConvert(metric_unit_t from, metric_unit_t to, float value)
  {
  switch (from)
    {
    case Kilometers:
      if (to == Miles) return (value/1.60934);
      else if (to == Meters) return value/1000;
      break;
    case Miles:
      if (to == Kilometers) return (value*1.60934);
      else if (to == Meters) return (value*1609.34);
      break;
    case KphPS:
      if (to == MphPS) return (value/1.60934);
      else if (to == MetersPSS) return value/1000;
      break;
    case MphPS:
      if (to == KphPS) return (value*8)/5;
      else if (to == MetersPSS) return (value*1.60934);
      break;
    case kW:
      if (to == Watts) return (value*1000);
      break;
    case Watts:
      if (to == kW) return (value/1000);
      break;
    case kWh:
      if (to == WattHours) return (value*1000);
      break;
    case WattHours:
      if (to == kWh) return (value/1000);
      break;
    case WattHoursPK:
      if (to == WattHoursPM) return (value*1.60934);
      break;
    case WattHoursPM:
      if (to == WattHoursPK) return (value/1.60934);
      break;
    case Celcius:
      if (to == Fahrenheit) return ((value*9)/5) + 32;
      break;
    case Fahrenheit:
      if (to == Celcius) return ((value-32)*5)/9;
      break;
    case kPa:
      if (to == Pa) return value*1000;
      else if (to == PSI) return value * 0.14503773773020923;
    case Pa:
      if (to == kPa) return value/1000;
      else if (to == PSI) return value * 0.00014503773773020923;
    case PSI:
      if (to == kPa) return value * 6.894757293168361;
      else if (to == Pa) return value * 0.006894757293168361;
      break;
    case Seconds:
      if (to == Minutes) return value/60;
      else if (to == Hours) return value/3600;
      break;
    case Minutes:
      if (to == Seconds) return value*60;
      else if (to == Hours) return value/60;
      break;
    case Hours:
      if (to == Seconds) return value*3600;
      else if (to == Minutes) return value*60;
      break;
    case Kph:
      if (to == Mph) return (value/1.60934);
      break;
    case Mph:
      if (to == Kph) return (value*1.60934);
      break;
    case dbm:
      if (to == sq) return int((value <= -51)?((value + 113)/2):0);
      break;
    case sq:
      if (to == dbm) return int((value <= 31)?(-113 + (value*2)):0);
      break;
    default:
      return value;
    }
  return value;
  }
