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

#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include "metrics.h"
#include "metrics_standard.h"
#include "command.h"
#include "string.h"

using namespace std;

OvmsMetrics MyMetrics __attribute__ ((init_priority (1910)));

void metrics_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool found = false;
  for (std::map<std::string, OvmsMetric*>::iterator it=MyMetrics.m_metrics.begin(); it!=MyMetrics.m_metrics.end(); ++it)
    {
    const char* k = it->first.c_str();
    const char* v = it->second->AsString().c_str();
    if ((argc==0)||(strstr(k,argv[0])))
      {
      writer->printf("%-30.30s %s\n",k,v);
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

OvmsMetricFactory MyMetricFactory __attribute__ ((init_priority (1900)));

OvmsMetric* OvmsMetricFactory::NewMetric(const char* MetricType)
  {
  OvmsMetricFactory::map_type::iterator iter = m_map.find(MetricType);
  if (iter != m_map.end())
    {
    return iter->second();
    }
  return NULL;
  }

OvmsMetrics::OvmsMetrics()
  {
  puts("Initialising METRICS Framework");

  // Register our commands
  OvmsCommand* cmd_metric = MyCommandApp.RegisterCommand("metrics","METRICS framework",NULL, "", 1);
  cmd_metric->RegisterCommand("list","Show all metrics",metrics_list, "[metric]", 0, 1);
  cmd_metric->RegisterCommand("set","Set the value of a metric",metrics_set, "<metric> <value>", 2, 2);

  // Register the different types of metric
  MyMetricFactory.RegisterMetric<OvmsMetricInt>("int");
  MyMetricFactory.RegisterMetric<OvmsMetricString>("string");
  MyMetricFactory.RegisterMetric<OvmsMetricFloat>("float");

  // Register all the standard metrics
  for (const MetricStandard_t* p=MetricStandard ; p->name[0] != 0 ; p++)
    {
    OvmsMetric* m = MyMetricFactory.NewMetric(p->type);
    if (m != NULL)
      {
      m_metrics[p->name] = m;
      }
    }
  }

OvmsMetrics::~OvmsMetrics()
  {
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

OvmsMetric* OvmsMetrics::Find(const char* metric)
  {
  auto k = m_metrics.find(metric);
  if (k == m_metrics.end())
    return NULL;
  else
    return k->second;
  }

OvmsMetric::OvmsMetric()
  {
  m_defined = false;
  m_modified = false;
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

void OvmsMetric::SetModified()
  {
  m_defined = true;
  m_modified = true;
  }

OvmsMetricInt::OvmsMetricInt()
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

OvmsMetricFloat::OvmsMetricFloat()
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

OvmsMetricString::OvmsMetricString()
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
