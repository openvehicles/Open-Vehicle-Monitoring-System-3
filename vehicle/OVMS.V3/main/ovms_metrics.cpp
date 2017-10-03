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

#include "esp_log.h"
static const char *TAG = "metrics";

#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "string.h"

using namespace std;

OvmsMetrics       MyMetrics       __attribute__ ((init_priority (1800)));

void metrics_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool found = false;
  for (std::map<std::string, OvmsMetric*>::iterator it=MyMetrics.m_metrics.begin(); it!=MyMetrics.m_metrics.end(); ++it)
    {
    const char* k = it->first.c_str();
    std::string v = it->second->AsString();
    if ((argc==0)||(strstr(k,argv[0])))
      {
      writer->printf("%-30.30s %s\n",k,v.c_str());
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

OvmsMetrics::OvmsMetrics()
  {
  ESP_LOGI(TAG, "Initialising METRICS (1810)");

  m_nextmodifier = 1;

  // Register our commands
  OvmsCommand* cmd_metric = MyCommandApp.RegisterCommand("metrics","METRICS framework",NULL, "", 1);
  cmd_metric->RegisterCommand("list","Show all metrics",metrics_list, "[metric]", 0, 1);
  cmd_metric->RegisterCommand("set","Set the value of a metric",metrics_set, "<metric> <value>", 2, 2);
  }

OvmsMetrics::~OvmsMetrics()
  {
  }

void OvmsMetrics::RegisterMetric(OvmsMetric* metric, std::string name)
  {
  m_metrics[name] = metric;
  }

bool OvmsMetrics::Set(const char* metric, const char* value)
  {
  auto k = m_metrics.find(metric);
  if (k == m_metrics.end())
    return false;

  OvmsMetric* m = k->second;
  m->SetValue(std::string(value));
  return true;
  }

bool OvmsMetrics::SetInt(const char* metric, int value)
  {
  auto k = m_metrics.find(metric);
  if (k == m_metrics.end())
    return false;

  OvmsMetricInt* m = (OvmsMetricInt*)k->second;
  m->SetValue(value);
  return true;
  }

bool OvmsMetrics::SetBool(const char* metric, bool value)
  {
  auto k = m_metrics.find(metric);
  if (k == m_metrics.end())
    return false;

  OvmsMetricBool* m = (OvmsMetricBool*)k->second;
  m->SetValue(value);
  return true;
  }

bool OvmsMetrics::SetFloat(const char* metric, float value)
  {
  auto k = m_metrics.find(metric);
  if (k == m_metrics.end())
    return false;

  OvmsMetricFloat* m = (OvmsMetricFloat*)k->second;
  m->SetValue(value);
  return true;
  }

OvmsMetric* OvmsMetrics::Find(const char* metric)
  {
  auto k = m_metrics.find(metric);
  if (k == m_metrics.end())
    return NULL;
  else
    return k->second;
  }

void OvmsMetrics::RegisterListener(std::string caller, std::string name, MetricCallback callback)
  {
  auto k = m_listeners.find(name);
  if (k == m_listeners.end())
    {
    m_listeners[name] = new MetricCallbackList();
    k = m_listeners.find(name);
    }
  if (k == m_listeners.end())
    {
    ESP_LOGE(TAG, "Problem registering metric %s for caller %s",name.c_str(),caller.c_str());
    return;
    }

  MetricCallbackList *ml = k->second;
  ml->push_back(new MetricCallbackEntry(caller,callback));
  }

void OvmsMetrics::DeregisterListener(std::string caller)
  {
  for (MetricMap::iterator itm=m_listeners.begin(); itm!=m_listeners.end(); ++itm)
    {
    MetricCallbackList* ml = itm->second;
    for (MetricCallbackList::iterator itc=ml->begin(); itc!=ml->end(); ++itc)
      {
      MetricCallbackEntry* ec = *itc;
      if (ec->m_caller == caller)
        {
        ml->erase(itc);
        }
      }
    if (ml->empty())
      {
      delete ml;
      m_listeners.erase(itm);
      }
    }
  }

void OvmsMetrics::NotifyModified(OvmsMetric* metric)
  {
  auto k = m_listeners.find(metric->m_name);
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
  }

size_t OvmsMetrics::RegisterModifier()
  {
  return m_nextmodifier++;
  }

OvmsMetric::OvmsMetric(std::string name)
  {
  m_defined = false;
  m_modified.reset();
  m_name = name;
  m_lastmodified = 0;
  MyMetrics.RegisterMetric(this, name);
  }

OvmsMetric::~OvmsMetric()
  {
  }

std::string OvmsMetric::AsString()
  {
  return std::string("invalid");
  }

void OvmsMetric::SetValue(std::string value)
  {
  }

time_t OvmsMetric::LastModified()
  {
  return m_lastmodified;
  }

void OvmsMetric::SetModified()
  {
  m_defined = true;
  m_modified.set();
  m_lastmodified = time(0);
  MyMetrics.NotifyModified(this);
  }

bool OvmsMetric::IsStale()
  {
  return m_stale;
  }

void OvmsMetric::SetStale(bool stale)
  {
  m_stale = stale;
  }

bool OvmsMetric::IsModified(size_t modifier)
  {
  return m_modified[modifier];
  }

void OvmsMetric::ClearModified(size_t modifier)
  {
  m_modified.reset(modifier);
  }

OvmsMetricInt::OvmsMetricInt(std::string name)
  : OvmsMetric(name)
  {
  m_value = 0;
  }

OvmsMetricInt::~OvmsMetricInt()
  {
  }

std::string OvmsMetricInt::AsString()
  {
  if (m_defined)
    {
    char buffer[33];
    itoa (m_value,buffer,10);
    return buffer;
    }
  else
    {
    return std::string("");
    }
  }

int OvmsMetricInt::AsInt()
  {
  return m_value;
  }

void OvmsMetricInt::SetValue(int value)
  {
  m_value = value;
  SetModified();
  }

void OvmsMetricInt::SetValue(std::string value)
  {
  m_value = atoi(value.c_str());
  SetModified();
  }

OvmsMetricBool::OvmsMetricBool(std::string name)
  : OvmsMetric(name)
  {
  m_value = false;
  }

OvmsMetricBool::~OvmsMetricBool()
  {
  }

std::string OvmsMetricBool::AsString()
  {
  if (m_defined)
    {
    if (m_value)
      return std::string("yes");
    else
      return std::string("no");
    }
  else
    {
    return std::string("");
    }
  }

int OvmsMetricBool::AsBool()
  {
  return m_value;
  }

void OvmsMetricBool::SetValue(bool value)
  {
  m_value = value;
  SetModified();
  }

void OvmsMetricBool::SetValue(std::string value)
  {
  if ((value == "yes")||(value == "1")||(value == "true"))
    m_value = true;
  else
    m_value = false;
  SetModified();
  }

OvmsMetricFloat::OvmsMetricFloat(std::string name)
  : OvmsMetric(name)
  {
  m_value = 0;
  }

OvmsMetricFloat::~OvmsMetricFloat()
  {
  }

std::string OvmsMetricFloat::AsString()
  {
  if (m_defined)
    {
    std::ostringstream ss;
    ss << m_value;
    std::string s(ss.str());
    return s;
    }
  else
    {
    return std::string("");
    }
  }

float OvmsMetricFloat::AsFloat()
  {
  return m_value;
  }

void OvmsMetricFloat::SetValue(float value)
  {
  m_value = value;
  SetModified();
  }

void OvmsMetricFloat::SetValue(std::string value)
  {
  m_value = atof(value.c_str());
  SetModified();
  }

OvmsMetricString::OvmsMetricString(std::string name)
  : OvmsMetric(name)
  {
  }

OvmsMetricString::~OvmsMetricString()
  {
  }

std::string OvmsMetricString::AsString()
  {
  return m_value;
  }

void OvmsMetricString::SetValue(std::string value)
  {
  m_value = value;
  SetModified();
  }
