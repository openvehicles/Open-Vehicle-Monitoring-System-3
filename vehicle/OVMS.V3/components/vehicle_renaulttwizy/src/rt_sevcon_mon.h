/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy SEVCON monitor
 * 
 * (c) 2018  Michael Balzer <dexter@dexters-web.de>
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

#ifndef __rt_sevcon_mon_h__
#define __rt_sevcon_mon_h__

#include "ovms_metrics.h"
#include "ovms_mutex.h"
#include "rt_types.h"

#define SCMON_MAX_KPH         120

using namespace std;

#if 0
template <typename T> struct minmax {
  T min, max;
  minmax() {
    reset();
  }
  void reset() {
    min = std::numeric_limits<T>::max();
    max = std::numeric_limits<T>::min();
  }
  void addval(T val) {
    if (val < min)
      min = val;
    if (val > max)
      max = val;
  }
};
#endif

struct sc_mondata
{
  //
  // Direct SDO metrics
  //
  
  // 4600.0c Actual AC Motor Current
  int16_t               mot_current_raw;                    // [A]
  OvmsMetricFloat*      mot_current;                        // [A]
  // 4600.0d Actual AC Motor Voltage
  int16_t               mot_voltage_raw;                    // [1/16 V]
  OvmsMetricFloat*      mot_voltage;                        // [V]
  // 4600.0b Voltage modulation
  int16_t               mot_voltmod_raw;                    // [100/255 %]
  OvmsMetricFloat*      mot_voltmod;                        // [%]

  // 4600.01 Slip Frequency
  int16_t               mot_slipfreq_raw;                   // [1/256 rad/s]
  OvmsMetricFloat*      mot_slipfreq;                       // [rad/s]
  // 4600.0f Electrical output frequency
  int16_t               mot_outputfreq_raw;                 // [1/16 rad/s]
  OvmsMetricFloat*      mot_outputfreq;                     // [rad/s]

  // 4602.0b Torque demand value (U.T_d)
  int16_t               mot_torque_demand_raw;              // [1/16 Nm]
  OvmsMetricFloat*      mot_torque_demand;                  // [Nm]
  // 4602.0c Torque actual value (DWork.Td)
  int16_t               mot_torque_raw;                     // [1/16 Nm]
  OvmsMetricFloat*      mot_torque;                         // [Nm]
  // 4602.0e Maximum power limit torque (Y.T_power_limit)
  int16_t               mot_torque_limit_raw;               // [1/16 Nm]
  OvmsMetricFloat*      mot_torque_limit;                   // [Nm]

  // 4602.11 Battery Voltage
  uint16_t              bat_voltage_raw;                    // [1/16 V]
  OvmsMetricFloat*      bat_voltage;                        // [V]
  // 4602.12 Capacitor Voltage
  uint16_t              cap_voltage_raw;                    // [1/16 V]
  OvmsMetricFloat*      cap_voltage;                        // [V]

  //
  // Derived metrics
  //
  
  OvmsMetricFloat*      mot_power;                          // current * voltage [kW]
  
  //
  // Speed maps
  //
  
  int16_t               bat_power_drv[SCMON_MAX_KPH];       // [W]
  int16_t               bat_power_rec[SCMON_MAX_KPH];       // [W]
  OvmsMetricVector<float>*      m_bat_power_drv;            // [kW]
  OvmsMetricVector<float>*      m_bat_power_rec;            // [kW]

  float                 mot_torque_drv[SCMON_MAX_KPH];      // [Nm]
  float                 mot_torque_rec[SCMON_MAX_KPH];      // [Nm]
  OvmsMetricVector<float>*      m_mot_torque_drv;           // [Nm]
  OvmsMetricVector<float>*      m_mot_torque_rec;           // [Nm]

  void reset_speedmaps();
};

#endif // __rt_sevcon_mon_h__
