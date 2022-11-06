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
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
#include "ovms_script.h"
#endif

#include "ovms_log.h"
#define TAG ((const char*)"metric")

#define METRICS_MAX_MODIFIERS 32

using namespace std;

typedef enum : uint8_t
  {
  Other         = 0,
  Native        = Other,

  Kilometers    = 10,
  Miles         = 11,
  Meters        = 12,
  Feet          = 13,

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
  TimeUTC       = 53,
  TimeLocal     = 54,

  Degrees       = 60,

  Kph           = 61,
  Mph           = 62,

  // Acceleration:
  KphPS         = 71,   // Kph per second
  MphPS         = 72,   // Mph per second
  MetersPSS     = 73,   // Meters per second^2
  FeetPSS       = 74,   // Feet per second^2

  dbm           = 80,   // Signal Quality (in dBm)
  sq            = 81,   // Signal Quality (in SQ units)

  Percentage    = 90,

  // Energy consumption:
  WattHoursPK   = 100,  // Wh/km
  WattHoursPM   = 101,  // Wh/mi
  kWhP100K      = 102,  // Kwh/100km
  KPkWh         = 103,  // Km/kwH
  MPkWh         = 104,  // mi/kwH

  // Torque:
  Nm            = 110,
  // ^^^^ Last ^^^^ MetricUnitLast below.

  UnitNotFound  = 255 // Used for errors in Search.
  } metric_unit_t;

const metric_unit_t MetricUnitFirst = Other;
const metric_unit_t MetricUnitLast  = Nm;

typedef enum : uint8_t
  {
  NeverDefined   = 0,
  FirstDefined,
  Defined
} metric_defined_t;

extern const char* OvmsMetricUnitLabel(metric_unit_t units);
extern const char* OvmsMetricUnitName(metric_unit_t units);
extern metric_unit_t OvmsMetricUnitFromName(const char* unit);

extern int UnitConvert(metric_unit_t from, metric_unit_t to, int value);
extern float UnitConvert(metric_unit_t from, metric_unit_t to, float value);

typedef uint32_t persistent_value_t;

struct persistent_values
  {
  std::size_t                 namehash;
  persistent_value_t          value;
  };

struct persistent_metrics
  {
  u_long                      magic;
  int                         version;
  unsigned int                serial;
  size_t                      size;
  int                         used;
  persistent_values           values[100];
  };

extern persistent_values *pmetrics_find(const char *name);
extern persistent_values *pmetrics_register(const char *name);

class OvmsMetric
  {
  public:
    OvmsMetric(const char* name, uint16_t autostale = 0, metric_unit_t units = Other, bool persist = false);
    virtual ~OvmsMetric();

  public:
    virtual std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    std::string AsUnitString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual float AsFloat(const float defvalue = 0, metric_unit_t units = Other);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    virtual void DukPush(DukContext &dc, metric_unit_t units = Other);
#endif
    virtual bool SetValue(std::string value, metric_unit_t units = Other);
    virtual bool SetValue(dbcNumber& value);
    virtual void operator=(std::string value);
    virtual uint32_t LastModified();
    virtual uint32_t Age();
    virtual bool CheckPersist();
    virtual bool IsDefined();
    virtual bool IsFirstDefined();
    virtual bool IsPersistent();
    virtual bool IsStale();
    virtual bool IsString() { return false; };
    virtual bool IsFresh();
    virtual void RefreshPersist();
    virtual void SetStale(bool stale);
    virtual void SetAutoStale(uint16_t seconds);
    virtual metric_unit_t GetUnits();
    virtual bool IsModified(size_t modifier);
    virtual bool IsModifiedAndClear(size_t modifier);
    virtual void ClearModified(size_t modifier);
    virtual void SetModified(bool changed=true);
    virtual void Clear();

  public:
    OvmsMetric* m_next;
    const char* m_name;
    std::atomic_ulong m_modified;
    uint32_t m_lastmodified;
    uint16_t m_autostale;
    metric_unit_t m_units;
    metric_defined_t m_defined;
    bool m_stale;
    bool m_persist;
  };

class OvmsMetricBool : public OvmsMetric
  {
  public:
    OvmsMetricBool(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);
    virtual ~OvmsMetricBool();

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other);
    int AsBool(const bool defvalue = false);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    bool SetValue(bool value);
    void operator=(bool value) { SetValue(value); }
    bool SetValue(std::string value, metric_unit_t units = Other) override;
    bool SetValue(dbcNumber& value) override;
    void operator=(std::string value) { SetValue(value); }
    void Clear();
    bool CheckPersist();
    void RefreshPersist();

  protected:
    bool m_value;
    bool* m_valuep;
  };

class OvmsMetricInt : public OvmsMetric
  {
  public:
    OvmsMetricInt(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);
    virtual ~OvmsMetricInt();

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other);
    int AsInt(const int defvalue = 0, metric_unit_t units = Other);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    bool SetValue(int value, metric_unit_t units = Other);
    void operator=(int value) { SetValue(value); }
    bool SetValue(std::string value, metric_unit_t units = Other) override;
    bool SetValue(dbcNumber& value) override;
    void operator=(std::string value) { SetValue(value); }
    void Clear();
    bool CheckPersist();
    void RefreshPersist();

  protected:
    int m_value;
    int* m_valuep;
  };

class OvmsMetricFloat : public OvmsMetric
  {
  public:
    OvmsMetricFloat(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);
    virtual ~OvmsMetricFloat();

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other);
    int AsInt(const int defvalue = 0, metric_unit_t units = Other);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    bool SetValue(float value, metric_unit_t units = Other);
    void operator=(float value) { SetValue(value); }
    bool SetValue(std::string value, metric_unit_t units = Other) override;
    bool SetValue(dbcNumber& value) override;
    void operator=(std::string value) { SetValue(value); }
    void Clear();
    virtual bool CheckPersist();
    virtual void RefreshPersist();

  protected:
    float m_value;
    float* m_valuep;
  };

class OvmsMetricString : public OvmsMetric
  {
  public:
    OvmsMetricString(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);
    virtual ~OvmsMetricString();

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    bool SetValue(std::string value, metric_unit_t units = Other) override;
    void operator=(std::string value) { SetValue(value); }
    void Clear();
    virtual bool IsString() { return true; };

  protected:
    OvmsMutex m_mutex;
    std::string m_value;
  };


/**
 * OvmsMetricBitset<bits>: metric wrapper for std::bitset<bits>
 *  - string representation as comma separated bit positions (beginning at startpos) of set bits
 */
template <size_t N, int startpos=1>
class OvmsMetricBitset : public OvmsMetric
  {
  public:
    OvmsMetricBitset(const char* name, uint16_t autostale=0, metric_unit_t units = Other)
      : OvmsMetric(name, autostale, units, false)
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
          ss << startpos + i;
          }
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

    bool SetValue(std::string value, metric_unit_t units = Other) override
      {
      std::bitset<N> n_value;
      std::istringstream vs(value);
      std::string token;
      int elem;
      while(std::getline(vs, token, ','))
        {
        std::istringstream ts(token);
        ts >> elem;
        elem -= startpos;
        if (elem >= 0 && elem < N)
          n_value[elem] = 1;
        }
      return SetValue(n_value, units);
      }
    void operator=(std::string value) { SetValue(value); }

    std::bitset<N> AsBitset(const std::bitset<N> defvalue = std::bitset<N>(0), metric_unit_t units = Other)
      {
      if (!IsDefined())
        return defvalue;
      OvmsMutexLock lock(&m_mutex);
      return m_value;
      }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override
      {
      std::bitset<N> value;
        {
        OvmsMutexLock lock(&m_mutex);
        value = m_value;
        }
      dc.PushArray();
      int cnt = 0;
      for (int i = 0; i < N; i++)
        {
        if (value[i])
          {
          dc.Push(i);
          dc.PutProp(-2, cnt++);
          }
        }
      }
#endif

    bool SetValue(std::bitset<N> value, metric_unit_t units = Other)
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
        return modified;
        }
      return false;
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
      : OvmsMetric(name, autostale, units, false)
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

    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      std::string json = "[";
      json += AsString(defvalue, units, precision);
      json += "]";
      return json;
      }

    bool SetValue(std::string value, metric_unit_t units = Other) override
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
      return SetValue(n_value, units);
      }
    void operator=(std::string value) { SetValue(value); }

    std::set<ElemType> AsSet(const std::set<ElemType> defvalue = std::set<ElemType>(), metric_unit_t units = Other)
      {
      if (!IsDefined())
        return defvalue;
      OvmsMutexLock lock(&m_mutex);
      return m_value;
      }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override
      {
      std::set<ElemType> value;
        {
        OvmsMutexLock lock(&m_mutex);
        value = m_value;
        }
      dc.PushArray();
      int cnt = 0;
      for (auto i = value.begin(); i != value.end(); i++)
        {
        dc.Push(*i);
        dc.PutProp(-2, cnt++);
        }
      }
#endif

    bool SetValue(std::set<ElemType> value, metric_unit_t units = Other)
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
        return modified;
        }
      return false;
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
 *
 * Persistence can only be used on ElemTypes fitting into a pmetrics storage container.
 * A persistent vector will need 1+size pmetrics slots. It will allocate new slots as
 * needed when growing, but the slots will remain used when shrinking the vector. If any
 * new element cannot allocate a pmetrics slot, the whole vector loses its persistence.
 *
 * Unit conversion currently casts to and from float for the conversion, it's assumed to
 * only be necessary for floating point values here. If you need int conversion, rework
 * UnitConvert() into a template.
 */
template
  <
  typename ElemType,
  class Allocator = std::allocator<ElemType>
  >
class OvmsMetricVector : public OvmsMetric
  {
  public:
    OvmsMetricVector(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false)
      : OvmsMetric(name, autostale, units, persist)
      {
      m_valuep_size = NULL;
      if (!persist)
        return;

      // Ensure ElemType fits into persistent value:
      if (sizeof(ElemType) > sizeof(persistent_value_t))
        return;

      // Vector persistence is currently implemented by a size entry with the metric name
      //  + one additional entry per element using the metric name extended by the element index.
      // This may be optimized when the persistence system supports arrays.
      struct persistent_values *vp = pmetrics_register(m_name);
      if (!vp)
        return;
      m_valuep_size = reinterpret_cast<std::size_t*>(&vp->value);
      std::size_t psize = *m_valuep_size;
      if (SetPersistSize(psize))
        {
        SetModified(true);
        ESP_LOGI(TAG, "persist %s = %s", m_name, AsUnitString().c_str());
        }
      }
    virtual ~OvmsMetricVector()
      {
      }

  private:
    bool SetPersistSize(std::size_t new_size)
      {
      // Used by the constructor for initial read and by later SetValue() calls
      // on vector size changes:
      //  m_persist false => register & read persistent values
      //  m_persist true  => only register new elements
      //
      // Persistent element pointers will be retained on shrinking, and reused
      // when the size grows again. Overall vector persistence is cancelled if any
      // new element fails to register.

      if (!m_valuep_size)
        return false;

      std::size_t old_size = m_valuep_elem.size();
      if (new_size <= old_size)
        {
        *m_valuep_size = new_size;
        m_persist = true;
        return false;
        }

      m_valuep_elem.resize(new_size);
      if (!m_persist)
        m_value.resize(new_size);

      struct persistent_values *vp;
      char elem_name[100];
      for (std::size_t i = old_size; i < new_size; i++)
        {
        snprintf(elem_name, sizeof(elem_name), "%s_%u", m_name, i);
        vp = pmetrics_register(elem_name);
        if (!vp)
          {
          // if any element fails to register, the whole vector persistence fails:
          ESP_LOGE(TAG, "%s persistence lost: can't register slot for elem index %u", m_name, i);
          *m_valuep_size = 0;
          m_valuep_size = NULL;
          m_valuep_elem.resize(0);
          if (!m_persist)
            m_value.resize(0);
          m_persist = false;
          return false;
          }
        else
          {
          m_valuep_elem[i] = reinterpret_cast<ElemType*>(&vp->value);
          if (!m_persist)
            m_value[i] = *m_valuep_elem[i];
          }
        }

      *m_valuep_size = new_size;
      m_persist = true;
      return true;
      }

  public:
    bool CheckPersist()
      {
      if (!m_persist || !m_valuep_size || !IsDefined())
        return true;
      if (*m_valuep_size != m_value.size())
        {
        ESP_LOGE(TAG, "CheckPersist: bad value for %s", m_name);
        return false;
        }
      persistent_values *vp = pmetrics_find(m_name);
      if (vp == NULL)
        {
        ESP_LOGE(TAG, "CheckPersist: can't find %s", m_name);
        return false;
        }
      if (m_valuep_size != reinterpret_cast<std::size_t*>(&vp->value))
        {
        ESP_LOGE(TAG, "CheckPersist: bad address for %s", m_name);
        return false;
        }
      for (int i = 0; i < m_value.size(); i++)
        {
        if (*m_valuep_elem[i] != m_value[i])
          {
          ESP_LOGE(TAG, "CheckPersist: bad value for %s[%d]", m_name, i);
          return false;
          }
        }
      return true;
      }

    void RefreshPersist()
      {
      if (m_persist && m_valuep_size && IsDefined())
        {
        *m_valuep_size = m_value.size();
        for (int i = 0; i < m_value.size(); i++)
          *m_valuep_elem[i] = m_value[i];
        }
      }

  public:
    virtual std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      if (!IsDefined())
        return std::string(defvalue);
      std::ostringstream ss;
      if (precision >= 0)
        {
        ss.precision(precision);
        ss << fixed;
        }
      OvmsMutexLock lock(&m_mutex);
      for (auto i = m_value.begin(); i != m_value.end(); i++)
        {
        if (ss.tellp() > 0)
          ss << ',';
        if (units != Other && units != m_units)
          ss << (ElemType) UnitConvert(m_units, units, (float)*i);
        else
          ss << *i;
        }
      return ss.str();
      }

    virtual std::string ElemAsString(size_t n, const char* defvalue = "", metric_unit_t units = Other, int precision = -1, bool addunitlabel = false)
      {
      OvmsMutexLock lock(&m_mutex);
      if (!IsDefined() || m_value.size() <= n)
        return std::string(defvalue);
      std::ostringstream ss;
      if (precision >= 0)
        {
        ss.precision(precision);
        ss << fixed;
        }
      if (units != Other && units != m_units)
        ss << (ElemType) UnitConvert(m_units, units, (float)m_value[n]);
      else
        ss << m_value[n];
      if (addunitlabel)
        ss << OvmsMetricUnitLabel(units == Native ? GetUnits() : units);
      return ss.str();
      }

    std::string ElemAsUnitString(size_t n, const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      return ElemAsString(n, defvalue, units, precision, true);
      }

    virtual std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      std::string json = "[";
      json += AsString(defvalue, units, precision);
      json += "]";
      return json;
      }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override
      {
      std::vector<ElemType, Allocator> value;
        {
        OvmsMutexLock lock(&m_mutex);
        value = m_value;
        }
      dc.PushArray();
      int cnt = 0;
      for (auto i = value.begin(); i != value.end(); i++)
        {
        dc.Push(*i);
        dc.PutProp(-2, cnt++);
        }
      }
#endif

    virtual bool SetValue(std::string value)
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
      return SetValue(n_value);
      }
    void operator=(std::string value) { SetValue(value); }

    bool SetValue(const std::vector<ElemType, Allocator>& value, metric_unit_t units = Other)
      {
      bool modified = false, resized = false;
      if (m_mutex.Lock())
        {
        if (m_value.size() != value.size())
          {
          m_value.resize(value.size());
          if (m_persist)
            SetPersistSize(value.size());
          resized = true;
          }
        for (size_t i = 0; i < value.size(); i++)
          {
          ElemType ivalue;
          if (units != Other && units != m_units)
            ivalue = (ElemType) UnitConvert(units, m_units, (float)value[i]);
          else
            ivalue = value[i];
          if (resized || m_value[i] != ivalue)
            {
            m_value[i] = ivalue;
            modified = true;
            if (m_persist)
              *m_valuep_elem[i] = ivalue;
            }
          }
        m_mutex.Unlock();
        SetModified(modified);
        }
      return modified;
      }
    void operator=(std::vector<ElemType, Allocator> value) { SetValue(value); }

    void ClearValue()
      {
      if (IsDefined())
        {
        if (m_mutex.Lock())
          {
          if (m_persist)
            SetPersistSize(0);
          m_value.clear();
          m_mutex.Unlock();
          SetModified(true);
          }
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

    void SetElemValue(size_t n, const ElemType nvalue, metric_unit_t units = Other)
      {
      ElemType value;
      if (units != Other && units != m_units)
        value = (ElemType) UnitConvert(units, m_units, (float)nvalue);
      else
        value = nvalue;
      bool modified = false, resized = false;
      if (m_mutex.Lock())
        {
        if (m_value.size() < n+1)
          {
          m_value.resize(n+1);
          if (m_persist)
            SetPersistSize(n+1);
          resized = true;
          }
        if (resized || m_value[n] != value)
          {
          m_value[n] = value;
          modified = true;
          if (m_persist)
            *m_valuep_elem[n] = value;
          }
        m_mutex.Unlock();
        }
      SetModified(modified);
      }

    void SetElemValues(size_t start, size_t cnt, const ElemType* values, metric_unit_t units = Other)
      {
      bool modified = false, resized = false;
      if (m_mutex.Lock())
        {
        if (m_value.size() < start+cnt)
          {
          m_value.resize(start+cnt);
          if (m_persist)
            SetPersistSize(start+cnt);
          resized = true;
          }
        for (size_t i = 0; i < cnt; i++)
          {
          ElemType ivalue;
          if (units != Other && units != m_units)
            ivalue = (ElemType) UnitConvert(units, m_units, (float)values[i]);
          else
            ivalue = values[i];
          if (resized || m_value[start+i] != ivalue)
            {
            m_value[start+i] = ivalue;
            modified = true;
            if (m_persist)
              *m_valuep_elem[start+i] = ivalue;
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
    std::size_t* m_valuep_size;
    std::vector<ElemType*, Allocator> m_valuep_elem;
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
    bool Set(const char* metric, const char* value, const char *unit = NULL);
    bool SetInt(const char* metric, int value);
    bool SetBool(const char* metric, bool value);
    bool SetFloat(const char* metric, float value);
    std::string GetUnitStr(const char* metric, const char *unit = NULL);
    OvmsMetric* Find(const char* metric);

    OvmsMetricInt *InitInt(const char* metric, uint16_t autostale=0, int value=0, metric_unit_t units = Other, bool persist = false);
    OvmsMetricBool *InitBool(const char* metric, uint16_t autostale=0, bool value=0, metric_unit_t units = Other, bool persist = false);
    OvmsMetricFloat *InitFloat(const char* metric, uint16_t autostale=0, float value=0, metric_unit_t units = Other, bool persist = false);
    OvmsMetricString *InitString(const char* metric, uint16_t autostale=0, const char* value=NULL, metric_unit_t units = Other);
    template <size_t N>
    OvmsMetricBitset<N> *InitBitset(const char* metric, uint16_t autostale=0, const char* value=NULL, metric_unit_t units = Other)
      {
      OvmsMetricBitset<N> *m = (OvmsMetricBitset<N> *)Find(metric);
      if (m==NULL) m = new OvmsMetricBitset<N>(metric, autostale, units);
      if (value && !m->IsDefined())
        m->SetValue(value);
      return m;
      }
    template <typename ElemType>
    OvmsMetricSet<ElemType> *InitSet(const char* metric, uint16_t autostale=0, const char* value=NULL, metric_unit_t units = Other)
      {
      OvmsMetricSet<ElemType> *m = (OvmsMetricSet<ElemType> *)Find(metric);
      if (m==NULL) m = new OvmsMetricSet<ElemType>(metric, autostale, units);
      if (value && !m->IsDefined())
        m->SetValue(value);
      return m;
      }
    template <typename ElemType>
    OvmsMetricVector<ElemType> *InitVector(const char* metric, uint16_t autostale=0, const char* value=NULL, metric_unit_t units = Other, bool persist = false)
      {
      OvmsMetricVector<ElemType> *m = (OvmsMetricVector<ElemType> *)Find(metric);
      if (m==NULL) m = new OvmsMetricVector<ElemType>(metric, autostale, units, persist);
      if (value && !m->IsDefined())
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

  public:
    void EventSystemShutDown(std::string event, void* data);

  protected:
    size_t m_nextmodifier;

  public:
    OvmsMetric* m_first;
    bool m_trace;
  };

extern OvmsMetrics MyMetrics;

#undef TAG

#endif //#ifndef __METRICS_H__
