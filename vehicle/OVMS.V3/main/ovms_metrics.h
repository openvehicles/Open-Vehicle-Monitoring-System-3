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

#include <map>
#include <string>

using namespace std;

class OvmsMetric
  {
  public:
    OvmsMetric();
    virtual ~OvmsMetric();

  public:
    virtual std::string AsString();
    virtual void SetValue(std::string value);

  protected:
    void SetModified();

  public:
    bool m_defined;
    bool m_modified;
  };

class OvmsMetricInt : public OvmsMetric
  {
  public:
    OvmsMetricInt();
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
    OvmsMetricFloat();
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
    OvmsMetricString();
    virtual ~OvmsMetricString();

  public:
    std::string AsString();
    void SetValue(std::string value);

  protected:
    std::string m_value;
  };

template<typename Type> OvmsMetric* CreateMetric()
  {
  return new Type;
  }

class OvmsMetricFactory
  {
  public:
    OvmsMetricFactory();
    virtual ~OvmsMetricFactory();

  public:
    typedef OvmsMetric* (*FactoryFuncPtr)();
    typedef map<const char*, FactoryFuncPtr> map_type;

    map_type m_map;

  public:
    template<typename Type>
    short RegisterMetric(const char* MetricType)
      {
      FactoryFuncPtr function = &CreateMetric<Type>;
        m_map.insert(std::make_pair(MetricType, function));
      return 0;
      };
    OvmsMetric* NewMetric(const char* MetricType);
  };

extern OvmsMetricFactory MyMetricFactory;

class OvmsMetrics
  {
  public:
    OvmsMetrics();
    virtual ~OvmsMetrics();

  public:
    bool Set(const char* metric, const char* value);
    bool SetInt(const char* metric, int value);
    bool SetFloat(const char* metric, float value);
    OvmsMetric* Find(const char* metric);

  public:
    std::map<std::string, OvmsMetric*> m_metrics;
  };

extern OvmsMetrics MyMetrics;

#endif //#ifndef __METRICS_H__
