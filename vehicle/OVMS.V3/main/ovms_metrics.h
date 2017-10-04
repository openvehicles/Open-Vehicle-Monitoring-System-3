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

#define METRICS_MAX_MODIFIERS 32

using namespace std;

class OvmsMetric
  {
  public:
    OvmsMetric(std::string name, int autostale=0);
    virtual ~OvmsMetric();

  public:
    virtual std::string AsString();
    virtual void SetValue(std::string value);
    virtual uint32_t LastModified();
    virtual bool IsStale();
    virtual void SetStale(bool stale);
    virtual void SetAutoStale(int seconds);
    virtual bool IsModified(size_t modifier);
    virtual bool IsModifiedAndClear(size_t modifier);
    virtual void ClearModified(size_t modifier);
    virtual void SetModified(bool changed=true);

  public:
    std::string m_name;
    bool m_defined;
    bool m_stale;
    int m_autostale;
    std::bitset<METRICS_MAX_MODIFIERS> m_modified;
    uint32_t m_lastmodified;
  };

class OvmsMetricBool : public OvmsMetric
  {
  public:
    OvmsMetricBool(std::string name, int autostale=0);
    virtual ~OvmsMetricBool();

  public:
    std::string AsString();
    int AsBool();
    void SetValue(bool value);
    void SetValue(std::string value);

  protected:
    bool m_value;
  };

class OvmsMetricInt : public OvmsMetric
  {
  public:
    OvmsMetricInt(std::string name, int autostale=0);
    virtual ~OvmsMetricInt();

  public:
    std::string AsString();
    int AsInt();
    void SetValue(int value);
    void SetValue(std::string value);

  protected:
    int m_value;
  };

class OvmsMetricFloat : public OvmsMetric
  {
  public:
    OvmsMetricFloat(std::string name, int autostale=0);
    virtual ~OvmsMetricFloat();

  public:
    std::string AsString();
    float AsFloat();
    void SetValue(float value);
    void SetValue(std::string value);

  protected:
    float m_value;
  };

class OvmsMetricString : public OvmsMetric
  {
  public:
    OvmsMetricString(std::string name, int autostale=0);
    virtual ~OvmsMetricString();

  public:
    std::string AsString();
    void SetValue(std::string value);

  protected:
    std::string m_value;
  };

typedef std::function<void(OvmsMetric*)> MetricCallback;

class MetricCallbackEntry
  {
  public:
    MetricCallbackEntry(std::string caller, MetricCallback callback);
    virtual ~MetricCallbackEntry();

  public:
    std::string m_caller;
    MetricCallback m_callback;
  };

typedef std::list<MetricCallbackEntry*> MetricCallbackList;
typedef std::map<std::string, MetricCallbackList*> MetricMap;

class OvmsMetrics
  {
  public:
    OvmsMetrics();
    virtual ~OvmsMetrics();

  public:
    void RegisterMetric(OvmsMetric* metric, std::string name);

  public:
    bool Set(const char* metric, const char* value);
    bool SetInt(const char* metric, int value);
    bool SetBool(const char* metric, bool value);
    bool SetFloat(const char* metric, float value);
    OvmsMetric* Find(const char* metric);

  public:
    void RegisterListener(std::string caller, std::string name, MetricCallback callback);
    void DeregisterListener(std::string caller);
    void NotifyModified(OvmsMetric* metric);

  protected:
    MetricMap m_listeners;

  public:
    size_t RegisterModifier();

  protected:
    size_t m_nextmodifier;

  public:
    std::map<std::string, OvmsMetric*> m_metrics;
  };

extern OvmsMetrics MyMetrics;

#endif //#ifndef __METRICS_H__
