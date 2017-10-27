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

#ifndef __METRICS_H__
#define __METRICS_H__

#include <functional>
#include <map>
#include <list>
#include <string>
#include <bitset>
#include <stdint.h>
#include "ovms_utils.h"

#define METRICS_MAX_MODIFIERS 32

using namespace std;

typedef enum
  {
  Other         = 0,

  Kilometers    = 10,
  Miles         = 11,
  Meters        = 12,

  Celcius       = 20,
  Fahrenheit    = 21,

  kPa           = 30,
  Pa            = 31,
  PSI           = 32,

  Volts         = 40,
  Amps          = 41,
  AmpHours      = 42,
  kWh           = 43,

  Seconds       = 50,
  Minutes       = 51,
  Hours         = 52,

  Degrees       = 60,

  Kph           = 61,
  Mph           = 62,

  Percentage    = 90
  } metric_unit_t;

extern const char* OvmsMetricUnitLabel(metric_unit_t units);

class OvmsMetric
  {
  public:
    OvmsMetric(const char* name, int autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetric();

  public:
    virtual std::string AsString(const char* defvalue = "");
    virtual void SetValue(std::string value);
    virtual uint32_t LastModified();
    virtual bool IsStale();
    virtual void SetStale(bool stale);
    virtual void SetAutoStale(int seconds);
    virtual metric_unit_t GetUnits();
    virtual bool IsModified(size_t modifier);
    virtual bool IsModifiedAndClear(size_t modifier);
    virtual void ClearModified(size_t modifier);
    virtual void SetModified(bool changed=true);

  public:
    const char* m_name;
    bool m_defined;
    bool m_stale;
    int m_autostale;
    metric_unit_t m_units;
    std::bitset<METRICS_MAX_MODIFIERS> m_modified;
    uint32_t m_lastmodified;
  };

class OvmsMetricBool : public OvmsMetric
  {
  public:
    OvmsMetricBool(const char* name, int autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetricBool();

  public:
    std::string AsString(const char* defvalue = "");
    int AsBool(const bool defvalue = false);
    void SetValue(bool value);
    void SetValue(std::string value);

  protected:
    bool m_value;
  };

class OvmsMetricInt : public OvmsMetric
  {
  public:
    OvmsMetricInt(const char* name, int autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetricInt();

  public:
    std::string AsString(const char* defvalue = "");
    int AsInt(const int defvalue = 0);
    void SetValue(int value);
    void SetValue(std::string value);

  protected:
    int m_value;
  };

class OvmsMetricFloat : public OvmsMetric
  {
  public:
    OvmsMetricFloat(const char* name, int autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetricFloat();

  public:
    std::string AsString(const char* defvalue = "");
    float AsFloat(const float defvalue = 0);
    void SetValue(float value);
    void SetValue(std::string value);

  protected:
    float m_value;
  };

class OvmsMetricString : public OvmsMetric
  {
  public:
    OvmsMetricString(const char* name, int autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetricString();

  public:
    std::string AsString(const char* defvalue = "");
    void SetValue(std::string value);

  protected:
    std::string m_value;
  };

typedef std::function<void(OvmsMetric*)> MetricCallback;

class MetricCallbackEntry
  {
  public:
    MetricCallbackEntry(const char* caller, MetricCallback callback);
    virtual ~MetricCallbackEntry();

  public:
    const char *m_caller;
    MetricCallback m_callback;
  };

typedef std::list<MetricCallbackEntry*> MetricCallbackList;
typedef std::map<const char*, MetricCallbackList*, CmpStrOp> MetricCallbackMap;
typedef std::map<const char*, OvmsMetric*, CmpStrOp> MetricMap;

class OvmsMetrics
  {
  public:
    OvmsMetrics();
    virtual ~OvmsMetrics();

  public:
    void RegisterMetric(OvmsMetric* metric, const char* name);

  public:
    bool Set(const char* metric, const char* value);
    bool SetInt(const char* metric, int value);
    bool SetBool(const char* metric, bool value);
    bool SetFloat(const char* metric, float value);
    OvmsMetric* Find(const char* metric);

  public:
    void RegisterListener(const char* caller, const char* name, MetricCallback callback);
    void DeregisterListener(const char* caller);
    void NotifyModified(OvmsMetric* metric);

  protected:
    MetricCallbackMap m_listeners;

  public:
    size_t RegisterModifier();

  protected:
    size_t m_nextmodifier;

  public:
    MetricMap m_metrics;
  };

extern OvmsMetrics MyMetrics;

#endif //#ifndef __METRICS_H__
