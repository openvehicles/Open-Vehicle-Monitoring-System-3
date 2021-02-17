/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy power monitor
 * 
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __rt_pwrmon_h__
#define __rt_pwrmon_h__

#include "ovms_metrics.h"
#include "rt_types.h"

using namespace std;

struct speedpwr // speed+power usage statistics for const/accel/decel
{
  // Model
  
  unsigned long dist = 0;   // distance sum in 1/10 m
  unsigned long use = 0;    // sum of power used
  unsigned long rec = 0;    // sum of power recovered (recuperation)
  
  unsigned long spdcnt = 0; // count of speed sum entries
  unsigned long spdsum = 0; // sum of speeds/deltas during driving
  
  // Metrics
  
  void InitMetrics(int i, metric_unit_t spdunit);
  void DeleteMetrics();
  void UpdateMetrics();
  bool IsModified(size_t modifier);
  
  metric_unit_t m_spdunit;
  OvmsMetricFloat *m_dist;
  OvmsMetricFloat *m_used;
  OvmsMetricFloat *m_recd;
  OvmsMetricFloat *m_spdavg;
};

struct levelpwr // power usage statistics for level up/down
{
  // Model
  
  unsigned long dist = 0;   // distance sum in 1 m
  unsigned int hsum = 0;    // height sum in 1 m
  unsigned long use = 0;    // sum of power used
  unsigned long rec = 0;    // sum of power recovered (recuperation)
  
  // Metrics
  
  void InitMetrics(int i);
  void DeleteMetrics();
  void UpdateMetrics();
  bool IsModified(size_t modifier);
  
  OvmsMetricFloat *m_dist;
  OvmsMetricFloat *m_hsum;
  OvmsMetricFloat *m_used;
  OvmsMetricFloat *m_recd;
};


#endif
