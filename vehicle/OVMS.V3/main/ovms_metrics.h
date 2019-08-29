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
#include <sstream>
#include <set>
#include <vector>
#include <atomic>
#include "ovms_utils.h"
#include "ovms_mutex.h"
#include "dbc_number.h"

#define METRICS_MAX_MODIFIERS 32

using namespace std;

typedef enum : uint8_t
  {
  Other         = 0,
  Native        = Other,

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
  kW            = 43,
  kWh           = 44,
  Watts         = 45,
  WattHours     = 46,

  Seconds       = 50,
  Minutes       = 51,
  Hours         = 52,

  Degrees       = 60,

  Kph           = 61,
  Mph           = 62,

  // Acceleration:
  KphPS         = 71,   // Kph per second
  MphPS         = 72,   // Mph per second
  MetersPSS     = 73,   // Meters per second^2

  dbm           = 80,   // Signal Quality (in dBm)
  sq            = 81,   // Signal Quality (in SQ units)

  Percentage    = 90,

  // Energy consumption:
  WattHoursPK   = 100,  // Wh/km
  WattHoursPM   = 101,  // Wh/mi

  // Torque:
  Nm            = 110,
  } metric_unit_t;

typedef enum : uint8_t
  {
  NeverDefined   = 0,
  FirstDefined,
  Defined
} metric_defined_t;

extern const char* OvmsMetricUnitLabel(metric_unit_t units);
extern int UnitConvert(metric_unit_t from, metric_unit_t to, int value);
extern float UnitConvert(metric_unit_t from, metric_unit_t to, float value);

class OvmsMetric
  {
  public:
    OvmsMetric(const char* name, uint16_t autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetric();

  public:
    virtual std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    std::string AsUnitString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual float AsFloat(const float defvalue = 0, metric_unit_t units = Other);
    virtual void SetValue(std::string value);
    virtual void SetValue(dbcNumber& value);
    virtual void operator=(std::string value);
    virtual uint32_t LastModified();
    virtual uint32_t Age();
    virtual bool IsDefined();
    virtual bool IsFirstDefined();
    virtual bool IsStale();
    virtual void SetStale(bool stale);
    virtual void SetAutoStale(uint16_t seconds);
    virtual metric_unit_t GetUnits();
    virtual bool IsModified(size_t modifier);
    virtual bool IsModifiedAndClear(size_t modifier);
    virtual void ClearModified(size_t modifier);
    virtual void SetModified(bool changed=true);

  public:
    OvmsMetric* m_next;
    const char* m_name;
    std::atomic_ulong m_modified;
    uint32_t m_lastmodified;
    uint16_t m_autostale;
    metric_unit_t m_units;
    metric_defined_t m_defined;
    bool m_stale;
  };

class OvmsMetricBool : public OvmsMetric
  {
  public:
    OvmsMetricBool(const char* name, uint16_t autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetricBool();

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other);
    int AsBool(const bool defvalue = false);
    void SetValue(bool value);
    void operator=(bool value) { SetValue(value); }
    void SetValue(std::string value);
    void SetValue(dbcNumber& value);
    void operator=(std::string value) { SetValue(value); }

  protected:
    bool m_value;
  };

class OvmsMetricInt : public OvmsMetric
  {
  public:
    OvmsMetricInt(const char* name, uint16_t autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetricInt();

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other);
    int AsInt(const int defvalue = 0, metric_unit_t units = Other);
    void SetValue(int value, metric_unit_t units = Other);
    void operator=(int value) { SetValue(value); }
    void SetValue(std::string value);
    void SetValue(dbcNumber& value);
    void operator=(std::string value) { SetValue(value); }

  protected:
    int m_value;
  };

class OvmsMetricFloat : public OvmsMetric
  {
  public:
    OvmsMetricFloat(const char* name, uint16_t autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetricFloat();

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other);
    int AsInt(const int defvalue = 0, metric_unit_t units = Other);
    void SetValue(float value, metric_unit_t units = Other);
    void operator=(float value) { SetValue(value); }
    void SetValue(std::string value);
    void SetValue(dbcNumber& value);
    void operator=(std::string value) { SetValue(value); }

  protected:
    float m_value;
  };

class OvmsMetricString : public OvmsMetric
  {
  public:
    OvmsMetricString(const char* name, uint16_t autostale=0, metric_unit_t units = Other);
    virtual ~OvmsMetricString();

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    void SetValue(std::string value);
    void operator=(std::string value) { SetValue(value); }

  protected:
    OvmsMutex m_mutex;
    std::string m_value;
  };


/**
 * OvmsMetricBitset<bits>: metric wrapper for std::bitset<bits>
 *  - string representation as comma separated bit positions (beginning at 1) of set bits
 */
template <size_t N>
class OvmsMetricBitset : public OvmsMetric
  {
  public:
    OvmsMetricBitset(const char* name, uint16_t autostale=0, metric_unit_t units = Other)
      : OvmsMetric(name, autostale, units)
      {
      }
    virtual ~OvmsMetricBitset()
      {
      }

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      if (!IsDefined())
        return std::string(defvalue);
      std::ostringstream ss;
      OvmsMutexLock lock(&m_mutex);
      for (int i = 0; i < N; i++)
        {
        if (m_value[i])
          {
          if (ss.tellp() > 0)
            ss << ',';
          ss << i+1;
          }
        }
      return ss.str();
      }

    void SetValue(std::string value)
      {
      std::bitset<N> n_value;
      std::istringstream vs(value);
      std::string token;
      int elem;
      while(std::getline(vs, token, ','))
        {
        std::istringstream ts(token);
        ts >> elem;
        if (elem > 0 && elem <= N)
          n_value[elem-1] = 1;
        }
      SetValue(n_value);
      }
    void operator=(std::string value) { SetValue(value); }

    std::bitset<N> AsBitset(const std::bitset<N> defvalue = std::bitset<N>(0), metric_unit_t units = Other)
      {
      if (!IsDefined())
        return defvalue;
      OvmsMutexLock lock(&m_mutex);
      return m_value;
      }

    void SetValue(std::bitset<N> value, metric_unit_t units = Other)
      {
      if (m_mutex.Lock())
        {
        bool modified = false;
        if (m_value != value)
          {
          m_value = value;
          modified = true;
          }
        m_mutex.Unlock();
        SetModified(modified);
        }
      }
    void operator=(std::bitset<N> value) { SetValue(value); }

  protected:
    OvmsMutex m_mutex;
    std::bitset<N> m_value;
  };


/**
 * OvmsMetricSet<type>: metric wrapper for std::set<type>
 *  - string representation as comma separated values
 *  - be aware this is a memory eater, only use if necessary
 */
template <typename ElemType>
class OvmsMetricSet : public OvmsMetric
  {
  public:
    OvmsMetricSet(const char* name, uint16_t autostale=0, metric_unit_t units = Other)
      : OvmsMetric(name, autostale, units)
      {
      }
    virtual ~OvmsMetricSet()
      {
      }

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      if (!IsDefined())
        return std::string(defvalue);
      std::ostringstream ss;
      OvmsMutexLock lock(&m_mutex);
      for (auto i = m_value.begin(); i != m_value.end(); i++)
        {
        if (ss.tellp() > 0)
          ss << ',';
        ss << *i;
        }
      return ss.str();
      }

    void SetValue(std::string value)
      {
      std::set<ElemType> n_value;
      std::istringstream vs(value);
      std::string token;
      ElemType elem;
      while(std::getline(vs, token, ','))
        {
        std::istringstream ts(token);
        ts >> elem;
        n_value.insert(elem);
        }
      SetValue(n_value);
      }
    void operator=(std::string value) { SetValue(value); }

    std::set<ElemType> AsSet(const std::set<ElemType> defvalue = std::set<ElemType>(), metric_unit_t units = Other)
      {
      if (!IsDefined())
        return defvalue;
      OvmsMutexLock lock(&m_mutex);
      return m_value;
      }

    void SetValue(std::set<ElemType> value, metric_unit_t units = Other)
      {
      if (m_mutex.Lock())
        {
        bool modified = false;
        if (m_value != value)
          {
          m_value = value;
          modified = true;
          }
        m_mutex.Unlock();
        SetModified(modified);
        }
      }
    void operator=(std::set<ElemType> value) { SetValue(value); }

  protected:
    OvmsMutex m_mutex;
    std::set<ElemType> m_value;
  };


/**
 * OvmsMetricVector<type>: metric wrapper for std::vector<type>
 *  - string representation as comma separated values
 *  - TODO: escaping / string encoding for non-numeric types
 *  - TODO: unit conversions
 *
 * Usage example:
 *  OvmsMetricVector<float>* vf = new OvmsMetricVector<float>("test.volts", SM_STALE_MIN, Volts);
 *  vf->SetElemValue(3, 1.23);
 *  vf->SetElemValue(17, 2.34);
 *  float myvals[3] = { 5.5, 6.6, 7.7 };
 *  vf->SetElemValues(10, 3, myvals);
 *
 * Note: use ExtRamAllocator<type> for large vectors (= use SPIRAM)
 */
template
  <
  typename ElemType,
  class Allocator = std::allocator<ElemType>
  >
class OvmsMetricVector : public OvmsMetric
  {
  public:
    OvmsMetricVector(const char* name, uint16_t autostale=0, metric_unit_t units = Other)
      : OvmsMetric(name, autostale, units)
      {
      }
    virtual ~OvmsMetricVector()
      {
      }

  public:
    virtual std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      if (!IsDefined())
        return std::string(defvalue);
      std::ostringstream ss;
      if (precision >= 0)
        {
        ss.precision(precision); // Set desired precision
        ss << fixed;
        }
      OvmsMutexLock lock(&m_mutex);
      for (auto i = m_value.begin(); i != m_value.end(); i++)
        {
        if (ss.tellp() > 0)
          ss << ',';
        ss << *i;
        }
      return ss.str();
      }

    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      std::string json = "[";
      json += AsString(defvalue, units, precision);
      json += "]";
      return json;
      }

    virtual void SetValue(std::string value)
      {
      std::vector<ElemType, Allocator> n_value;
      std::istringstream vs(value);
      std::string token;
      ElemType elem;
      while(std::getline(vs, token, ','))
        {
        std::istringstream ts(token);
        ts >> elem;
        n_value.push_back(elem);
        }
      SetValue(n_value);
      }
    void operator=(std::string value) { SetValue(value); }

    void SetValue(std::vector<ElemType, Allocator> value, metric_unit_t units = Other)
      {
      if (m_mutex.Lock())
        {
        bool modified = false;
        if (m_value != value)
          {
          m_value = value;
          modified = true;
          }
        m_mutex.Unlock();
        SetModified(modified);
        }
      }
    void operator=(std::vector<ElemType, Allocator> value) { SetValue(value); }

    void ClearValue()
      {
      if (IsDefined())
        {
        if (m_mutex.Lock())
          {
          m_value.clear();
          m_mutex.Unlock();
          }
        SetModified(true);
        }
      }

    std::vector<ElemType, Allocator> AsVector(const std::vector<ElemType, Allocator> defvalue = std::vector<ElemType, Allocator>(), metric_unit_t units = Other)
      {
      if (!IsDefined())
        return defvalue;
      OvmsMutexLock lock(&m_mutex);
      return m_value;
      }

    ElemType GetElemValue(size_t n)
      {
      ElemType val{};
      OvmsMutexLock lock(&m_mutex);
      if (m_value.size() > n)
        val = m_value[n];
      return val;
      }

    void SetElemValue(size_t n, ElemType value)
      {
      bool modified = false;
      if (m_mutex.Lock())
        {
        if (m_value.size() < n+1)
          m_value.resize(n+1);
        if (m_value[n] != value)
          {
          m_value[n] = value;
          modified = true;
          }
        m_mutex.Unlock();
        }
      SetModified(modified);
      }

    void SetElemValues(size_t start, size_t cnt, ElemType* values)
      {
      bool modified = false;
      if (m_mutex.Lock())
        {
        if (m_value.size() < start+cnt)
          m_value.resize(start+cnt);
        for (size_t i = 0; i < cnt; i++)
          {
          if (m_value[start+i] != values[i])
            {
            m_value[start+i] = values[i];
            modified = true;
            }
          }
        m_mutex.Unlock();
        }
      SetModified(modified);
      }

    uint32_t GetSize()
      {
      return m_value.size();
      }

  protected:
    OvmsMutex m_mutex;
    std::vector<ElemType, Allocator> m_value;
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

class OvmsMetrics
  {
  public:
    OvmsMetrics();
    virtual ~OvmsMetrics();

  public:
    void RegisterMetric(OvmsMetric* metric);
    void DeregisterMetric(OvmsMetric* metric);

  public:
    bool Set(const char* metric, const char* value);
    bool SetInt(const char* metric, int value);
    bool SetBool(const char* metric, bool value);
    bool SetFloat(const char* metric, float value);
    OvmsMetric* Find(const char* metric);

    OvmsMetricString *InitString(const char* metric, uint16_t autostale=0, const char* value=NULL, metric_unit_t units = Other);
    OvmsMetricInt *InitInt(const char* metric, uint16_t autostale=0, int value=0, metric_unit_t units = Other);
    OvmsMetricBool *InitBool(const char* metric, uint16_t autostale=0, bool value=0, metric_unit_t units = Other);
    OvmsMetricFloat *InitFloat(const char* metric, uint16_t autostale=0, float value=0, metric_unit_t units = Other);
    template <size_t N>
    OvmsMetricBitset<N> *InitBitset(const char* metric, uint16_t autostale=0, const char* value=NULL, metric_unit_t units = Other)
      {
      OvmsMetricBitset<N> *m = (OvmsMetricBitset<N> *)Find(metric);
      if (m==NULL) m = new OvmsMetricBitset<N>(metric, autostale, units);
      if (value)
        m->SetValue(value);
      return m;
      }
    template <typename ElemType>
    OvmsMetricSet<ElemType> *InitSet(const char* metric, uint16_t autostale=0, const char* value=NULL, metric_unit_t units = Other)
      {
      OvmsMetricSet<ElemType> *m = (OvmsMetricSet<ElemType> *)Find(metric);
      if (m==NULL) m = new OvmsMetricSet<ElemType>(metric, autostale, units);
      if (value)
        m->SetValue(value);
      return m;
      }
    template <typename ElemType>
    OvmsMetricVector<ElemType> *InitVector(const char* metric, uint16_t autostale=0, const char* value=NULL, metric_unit_t units = Other)
      {
      OvmsMetricVector<ElemType> *m = (OvmsMetricVector<ElemType> *)Find(metric);
      if (m==NULL) m = new OvmsMetricVector<ElemType>(metric, autostale, units);
      if (value)
        m->SetValue(value);
      return m;
      }

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
    OvmsMetric* m_first;
    bool m_trace;
  };

extern OvmsMetrics MyMetrics;

#endif //#ifndef __METRICS_H__
