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
#include "ovms_mutex.h"
#include "dbc_number.h"
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
#include "ovms_script.h"
#endif

#include "ovms_command.h"

#include "ovms_log.h"
#define TAG ((const char*)"metric")

#define METRICS_MAX_MODIFIERS 32

using namespace std;

typedef enum : uint8_t
  {
  Other         = 0,
  Native        = Other,
  ToMetric      = 1,
  ToImperial    = 2,
  ToUser        = 3,

  Kilometers    = 10,
  Miles         = 11,
  Meters        = 12,
  Feet          = 13,

  Celcius       = 20,
  Fahrenheit    = 21,

  kPa           = 30,
  Pa            = 31,
  PSI           = 32,
  Bar           = 33,

  Volts         = 40,
  Amps          = 41,
  AmpHours      = 42,
  kW            = 43,
  kWh           = 44,
  Watts         = 45,
  WattHours     = 46,
  Kilocoulombs  = 47,
  MegaJoules    = 48,

  Seconds       = 50,
  Minutes       = 51,
  Hours         = 52,
  TimeUTC       = 53,
  TimeLocal     = 54,

  Degrees       = 60,

  Kph           = 61,
  Mph           = 62,
  MetersPS      = 63,
  FeetPS        = 64,

  // Acceleration:
  KphPS         = 71,   // Kph per second
  MphPS         = 72,   // Mph per second
  MetersPSS     = 73,   // Meters per second^2
  FeetPSS       = 74,   // Feet per second^2

  dbm           = 80,   // Signal Quality (in dBm)
  sq            = 81,   // Signal Quality (in SQ units)

  DateUnix      = 85,
  DateUTC       = 86,
  DateLocal     = 87,

  Percentage    = 90,
  Permille      = 91,

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

// Mask for folding "Short groups" to their equivalent "Long Group"
const uint8_t GrpFoldMask = 0x1f;
const uint8_t GrpUnfold = 0x20;
typedef enum : uint8_t
  {
  GrpNone = 0,
  GrpOther = 1,
  GrpDistance = 2,
  GrpSpeed = 3,
  GrpAccel = 4,
  GrpPower = 5,
  GrpEnergy = 6,
  GrpConsumption = 7,
  GrpTemp = 8,
  GrpPressure = 9,
  GrpTime = 10,
  GrpSignal = 11,
  GrpTorque = 12,
  GrpDirection = 13,
  GrpRatio = 14,
  GrpCharge = 15,
  GrpDate = 16,
  // These are where a dimension group is split and allows
  // easily folding the 'short distances' back onto their equivalents.
  GrpDistanceShort = GrpDistance + GrpUnfold,
  GrpAccelShort = GrpAccel + GrpUnfold
  } metric_group_t;
const metric_group_t MetricGroupLast = GrpAccelShort;

extern const char* OvmsMetricUnitLabel(metric_unit_t units);
extern const char* OvmsMetricUnitName(metric_unit_t units);
extern metric_unit_t OvmsMetricUnitFromName(const char* unit, bool allowUniquePrefix = false);
extern metric_unit_t OvmsMetricUnitFromLabel(const char* unit);
int OvmsMetricUnit_Validate(OvmsWriter* writer, int argc, const char* token, bool complete, metric_group_t group = GrpNone);
const char *OvmsMetricUnit_FindUniquePrefix(const char* token);

bool CheckTargetUnit(metric_unit_t from, metric_unit_t &to, bool full_check);
extern int UnitConvert(metric_unit_t from, metric_unit_t to, int value);
extern float UnitConvert(metric_unit_t from, metric_unit_t to, float value);

typedef std::vector<metric_group_t> metric_group_list_t;
typedef std::set<metric_unit_t> metric_unit_set_t;

// Groups in order of display.
extern bool OvmsMetricGroupConfigList(metric_group_list_t& groups);
extern const char* OvmsMetricGroupLabel(metric_group_t group);
extern const char* OvmsMetricGroupName(metric_group_t group);
extern bool OvmsMetricGroupUnits(metric_group_t group, metric_unit_set_t& units);

// Get/Set Metric default config
extern std::string OvmsMetricGetUserConfig(metric_group_t group);
extern void OvmsMetricSetUserConfig(metric_group_t group, std::string value);
extern void OvmsMetricSetUserConfig(metric_group_t group, metric_unit_t unit);
extern metric_unit_t OvmsMetricGetUserUnit(metric_group_t group, metric_unit_t defaultUnit = Native);
extern metric_group_t GetMetricGroup(metric_unit_t unit);
metric_unit_t OvmsMetricCheckUnit(metric_unit_t fromUnit, metric_unit_t toUnit);

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
extern persistent_values *pmetrics_find(const std::string &name);
extern persistent_values *pmetrics_register(const char *name);
extern persistent_values *pmetrics_register(const std::string &name);

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
    virtual bool SetValue(const dbcNumber& value, metric_unit_t units = Other);
    virtual void operator=(std::string value);
    virtual bool CheckPersist();
    virtual void RefreshPersist();
    virtual bool IsString() { return false; };
    virtual void Clear();

    uint32_t LastModified() const;
    uint32_t Age() const;
    bool IsDefined() const;
    bool IsFirstDefined() const;
    bool IsPersistent() const;
    bool IsStale();
    bool IsFresh();
    void SetStale(bool stale)
      {
      m_stale = stale;
      }
    void SetAutoStale(uint16_t seconds)
      {
      m_autostale = seconds;
      }
    metric_unit_t GetUnits() const
      {
      return m_units;
      }
    bool IsModified(size_t modifier) const;
    bool IsModifiedAndClear(size_t modifier);
    void ClearModified(size_t modifier);
    void SetModified(bool changed=true);

    bool IsUnitSend(size_t modifier) const;
    bool IsUnitSendAndClear(size_t modifier);
    void ClearUnitSend(size_t modifier);
    void SetUnitSend(size_t modifier);
    void SetUnitSendAll();

  public:
    OvmsMetric* m_next;
    const char* m_name;
    std::atomic_ulong m_modified, m_sendunit;
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
    ~OvmsMetricBool() override;

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;
    std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other) override;
    int AsBool(const bool defvalue = false);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    bool SetValue(bool value);
    void operator=(bool value) { SetValue(value); }
    bool SetValue(std::string value, metric_unit_t units = Other) override;
    bool SetValue(const dbcNumber& value, metric_unit_t units = Other) override;
    void operator=(std::string value) override { SetValue(value); }
    void Clear() override;
    bool CheckPersist() override;
    void RefreshPersist() override;

  protected:
    bool m_value;
    bool* m_valuep;
  };

class OvmsMetricInt : public OvmsMetric
  {
  public:
    OvmsMetricInt(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);
    ~OvmsMetricInt() override;

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;
    std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other) override;
    int AsInt(const int defvalue = 0, metric_unit_t units = Other);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    bool SetValue(int value, metric_unit_t units = Other);
    void operator=(int value) { SetValue(value); }
    bool SetValue(std::string value, metric_unit_t units = Other) override;
    bool SetValue(const dbcNumber& value, metric_unit_t units = Other) override;
    void operator=(std::string value) override { SetValue(value); }
    void Clear() override;
    bool CheckPersist() override;
    void RefreshPersist() override;

  protected:
    int m_value;
    int* m_valuep;
  };

class OvmsMetricFloat : public OvmsMetric
  {
  public:
    OvmsMetricFloat(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);
    ~OvmsMetricFloat() override;

  public:
    void SetFormat(int precision = -1, bool fixed = false) { m_fmt_prec = precision; m_fmt_fixed = fixed; }
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;
    std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;
    float AsFloat(const float defvalue = 0, metric_unit_t units = Other) override;
    int AsInt(const int defvalue = 0, metric_unit_t units = Other);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    bool SetValue(float value, metric_unit_t units = Other);
    void operator=(float value) { SetValue(value); }
    bool SetValue(std::string value, metric_unit_t units = Other) override;
    bool SetValue(const dbcNumber& value, metric_unit_t units = Other) override;
    void operator=(std::string value) override { SetValue(value); }
    void Clear() override;
    bool CheckPersist() override;
    void RefreshPersist() override;

  protected:
    float m_value;
    float* m_valuep;
    int m_fmt_prec;
    bool m_fmt_fixed;
  };

class OvmsMetricString : public OvmsMetric
  {
  public:
    OvmsMetricString(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);
    ~OvmsMetricString() override;

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    bool SetValue(std::string value, metric_unit_t units = Other) override;
    void operator=(std::string value) override { SetValue(value); }
    void Clear() override;
    bool IsString() override { return true; };

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
    ~OvmsMetricBitset() override
      {
      }

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override
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

    std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override
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
    void operator=(std::string value) override { SetValue(value); }

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

    // Bring other overridden SetValue into scope from  OVMSMetric
    using OvmsMetric::SetValue;

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
    ~OvmsMetricSet() override
      {
      }

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override
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

    std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override
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
    void operator=(std::string value) override { SetValue(value); }

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
  class Allocator = std::allocator<ElemType>,
  class AllocatorStar = std::allocator<ElemType*>
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
    ~OvmsMetricVector() override
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
    bool CheckPersist() override
      {
      if (!m_persist || !m_valuep_size || !IsDefined())
        return true;
      if (*m_valuep_size != m_value.size())
        {
        ESP_LOGE(TAG, "CheckPersist: bad value for %s[] size", m_name);
        return false;
        }
      persistent_values *vp = pmetrics_find(m_name);
      if (vp == NULL)
        {
        ESP_LOGE(TAG, "CheckPersist: can't find %s[] size", m_name);
        return false;
        }
      if (m_valuep_size != reinterpret_cast<std::size_t*>(&vp->value))
        {
        ESP_LOGE(TAG, "CheckPersist: bad address for %s[] size", m_name);
        return false;
        }
      for (std::size_t i = 0; i < m_value.size(); i++)
        {
        if (*m_valuep_elem[i] != m_value[i])
          {
          ESP_LOGE(TAG, "CheckPersist: bad value for %s[%d]", m_name, i);
          return false;
          }
        }
      char elem_name[100];
      for (std::size_t i = 0; i < m_value.size(); ++i)
        {
        snprintf(elem_name, sizeof(elem_name), "%s_%u", m_name, i);
        vp = pmetrics_find(elem_name);
        if (!vp)
          {
          ESP_LOGE(TAG, "CheckPersist: can't find %s[%d]", m_name, i);
          return false;
          }
        if (m_valuep_elem[i] != reinterpret_cast<ElemType*>(&vp->value))
          {
          ESP_LOGE(TAG, "CheckPersist: bad address for %s[%d]", m_name, i);
          return false;
          }
        }
      return true;
      }

    void RefreshPersist() override
      {
      if (m_persist && m_valuep_size && IsDefined())
        {
        *m_valuep_size = m_value.size();
        for (int i = 0; i < m_value.size(); i++)
          *m_valuep_elem[i] = m_value[i];
        }
      }

  public:
    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override
      {
      if (!IsDefined())
        return std::string(defvalue);
      std::ostringstream ss;
      if (precision >= 0)
        {
        ss.precision(precision);
        ss << fixed;
        }
      CheckTargetUnit(m_units, units, false);
        {
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
        }
      return ss.str();
      }

    std::string ElemAsString(size_t n, const char* defvalue = "", metric_unit_t units = Other, int precision = -1, bool addunitlabel = false)
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
      auto currentUnits = GetUnits();
      CheckTargetUnit(currentUnits, units, true);
      if (units == Native)
        units = currentUnits;

      ElemType value = m_value[n];
      if (units != currentUnits)
        value = (ElemType)UnitConvert(currentUnits, units, (float)value);
      ss << value;

      if (addunitlabel)
        ss << OvmsMetricUnitLabel( units );
      return ss.str();
      }

    std::string ElemAsUnitString(size_t n, const char* defvalue = "", metric_unit_t units = Other, int precision = -1)
      {
      return ElemAsString(n, defvalue, units, precision, true);
      }

    std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override
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

    bool SetValue(std::string value, metric_unit_t units = Other) override
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
      return SetValue(n_value, units);
      }
    void operator=(std::string value) override { SetValue(value); }

    // Bring other overridden SetValue into scope from OVMSMetric
    using OvmsMetric::SetValue;

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
      // Translate any 'Metric' or 'Imperial' type units.. see if we can do a simple return first..
      CheckTargetUnit(m_units, units, false);
      if ((units == Native) || (units == m_units) )
        {
        OvmsMutexLock lock(&m_mutex);
        return m_value;
        }
      else
        {
        std::vector<ElemType, Allocator> res;
          {
          OvmsMutexLock lock(&m_mutex);
          res = m_value;
          }
        for ( auto it = res.begin(); it != res.end(); ++it)
          *it = UnitConvert(m_units, units, *it);
        return res;
        }
      }
    inline std::vector<ElemType, Allocator> AsVector(metric_unit_t units)
      {
      return AsVector(std::vector<ElemType, Allocator>(), units);
      }

    ElemType GetElemValue(size_t n)
      {
      ElemType val{};
      OvmsMutexLock lock(&m_mutex);
      if (m_value.size() > n)
        val = m_value[n];
      return val;
      }

    ElemType GetElemValue(size_t n, metric_unit_t units)
      {
      ElemType val = GetElemValue(n);
      return UnitConvert(m_units, units, val);
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
    std::vector<ElemType*, AllocatorStar> m_valuep_elem;
  };

/* Base class for 64 bit persisted metrics.
 */
class OvmsMetric64 : public OvmsMetric
 {
  public:
    OvmsMetric64(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);

    bool CheckPersist() override;
    void RefreshPersist() override;

    using OvmsMetric::operator=;
    void operator=(std::string value) override { SetValue(value); }
  protected:
    // Get the value as low/high parts.
    virtual void GetValueParts( persistent_value_t &value_low, persistent_value_t &value_hi) = 0;
    // Set the value as low/high parts. Return true on changed
    virtual bool SetValueParts( persistent_value_t value_low, persistent_value_t value_hi) = 0;

    // Needs to be called by overriding constructor.
    void InitPersist();

    persistent_value_t *m_valuep_lo;
    persistent_value_t *m_valuep_hi;
 };

class OvmsMetricInt64 : public OvmsMetric64
  {
  protected:
    // Get the value as low/high parts.
    void GetValueParts( persistent_value_t &value_low, persistent_value_t &value_hi) override;
    // Set the value as low/high parts. Return true on changed
    bool SetValueParts( persistent_value_t value_low, persistent_value_t value_hi) override;

    int64_t m_value;

  public:
    OvmsMetricInt64(const char* name, uint16_t autostale=0, metric_unit_t units = Other, bool persist = false);

    std::string AsString(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;
    std::string AsJSON(const char* defvalue = "", metric_unit_t units = Other, int precision = -1) override;

    float AsFloat(const float defvalue = 0, metric_unit_t units = Other) override; // TODO !?!?!?

    int64_t AsInt(const int64_t defvalue = 0, metric_unit_t units = Other);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    void DukPush(DukContext &dc, metric_unit_t units = Other) override;
#endif
    // Bring other overridden SetValue into scope from  OVMSMetric64
    using OvmsMetric64::SetValue;
    bool SetValue(int64_t value, metric_unit_t units = Other);
    bool SetValue(const dbcNumber& value, metric_unit_t units = Other) override;
    bool SetValue(std::string value, metric_unit_t units = Other) override;

    // Bring other overridden = operator into scope from  OVMSMetric64
    using OvmsMetric64::operator=;
    void operator=(int64_t value) { SetValue(value); }

    void Clear() override;

  };

typedef std::function<void(OvmsMetric*)> MetricCallback;

class MetricCallbackEntry
  {
  public:
    MetricCallbackEntry(std::string caller, MetricCallback callback);
    ~MetricCallbackEntry();

  public:
    std::string m_caller;
    MetricCallback m_callback;
  };

class UnitConfigMap
  {
  protected:
    std::array<metric_unit_t, static_cast<uint8_t>(MetricGroupLast)+1> m_map;
    std::array<std::atomic_ulong, static_cast<uint8_t>(MetricGroupLast)+1> m_modified;
    OvmsMutex m_store_lock;
  public:
    UnitConfigMap();
    void Load();

    void ConfigEventListener(std::string event, void* data);
    void ConfigMountedListener(std::string event, void* data);

    metric_unit_t GetUserUnit( metric_group_t group, metric_unit_t defaultUnit = UnitNotFound );
    metric_unit_t GetUserUnit( metric_unit_t unit);

    bool IsModified( metric_group_t group, size_t modifier);
    bool IsModifiedAndClear(metric_group_t group, size_t modifier);
    bool HasModified(size_t modifier);
    void ConfigList(metric_group_list_t& groups);

    metric_group_list_t config_groups;

    void InitialiseSlot(size_t modifier);
  };

typedef std::list<MetricCallbackEntry*> MetricCallbackList;
typedef std::map<std::string, MetricCallbackList*> MetricCallbackMap;

class OvmsMetrics
  {
  public:
    OvmsMetrics();
    ~OvmsMetrics();

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

    OvmsMetric* FindUniquePrefix(const char* token) const;
    bool GetCompletion(OvmsWriter* writer, const char* token) const;
    int Validate(OvmsWriter* writer, int argc, const char* token, bool complete) const;

    OvmsMetricInt *InitInt(const char* metric, uint16_t autostale=0, int value=0, metric_unit_t units = Other, bool persist = false);
    OvmsMetricInt64 *InitInt64(const char* metric, uint16_t autostale=0, int64_t value=0, metric_unit_t units = Other, bool persist = false);
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
    void SetAllUnitSend(size_t modifier);
    void SetAllGroupUnitSend(metric_group_t group);

    // Return bitmask of unit streams that need to be sent
    unsigned long GetUnitSendAll();

  public:
    void RegisterListener(std::string caller, std::string name, MetricCallback callback);
    void DeregisterListener(std::string caller);
    void NotifyModified(OvmsMetric* metric);
  protected:
    MetricCallbackMap m_listeners;

  public:
    size_t RegisterModifier();
    void InitialiseSlot(size_t modifier);

  public:
    void EventSystemShutDown(std::string event, void* data);

  protected:
    size_t m_nextmodifier;

  public:
    OvmsMetric* m_first;
    bool m_trace;
  };

extern OvmsMetrics MyMetrics;
extern UnitConfigMap MyUnitConfig;

#undef TAG

#endif //#ifndef __METRICS_H__
