/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy battery monitor
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

#ifndef __rt_battmon_h__
#define __rt_battmon_h__

#include "ovms_metrics.h"
#include "rt_types.h"

using namespace std;


struct battery_pack
{
  UINT volt_act = 0; // current voltage in 1/10 V
  UINT volt_new = 0; // sensor buffer
  UINT volt_min = 1000; // charge cycle min voltage
  UINT volt_max = 0; // charge cycle max voltage
  
  float temp_act = 0; // current temperature in °C + 40
  float temp_min = 240; // charge cycle min temperature
  float temp_max = 0; // charge cycle max temperature
  
  std::bitset<32> volt_watches = 0; // bitfield: cell dev > stddev
  std::bitset<32> volt_alerts = 0; // bitfield: cell dev > BATT_DEV_VOLT_ALERT
  std::bitset<32> last_volt_alerts = 0; // recognize alert state changes
  
  std::bitset<32> temp_watches = 0; // bitfield: dev > stddev
  std::bitset<32> temp_alerts = 0; // bitfield: dev > BATT_DEV_TEMP_ALERT
  std::bitset<32> last_temp_alerts = 0; // recognize alert state changes
  
  UINT cell_volt_min = 2000;            // current cell min voltage [1/200 V]
  UINT cell_volt_max = 0;               // current cell max voltage [1/200 V]
  float cell_volt_avg = 0;              // current cell voltage average [1/200 V]
  float cell_volt_stddev = 0;           // current cell voltage std deviation [1/200 V]
  UINT cell_volt_stddev_max = 0;        // max cell voltage std deviation [1/200 V] => watch/alert bit #15 (1<<15)

  UINT cmod_temp_min = 240;             // current cmod min temperature [°C]
  UINT cmod_temp_max = 0;               // current cmod max temperature [°C]
  float cmod_temp_avg = 0;              // current cmod temperature average [°C] (currently == temp_act)
  float cmod_temp_stddev = 0;           // current cmod temperature std deviation [°C]
  UINT cmod_temp_stddev_max = 0;        // max cmod temperature std deviation [°C] => watch/alert bit #7 (1<<7)
  
  int max_drive_pwr = 0; // in 500 W
  int max_recup_pwr = 0; // in 500 W
};

struct battery_cmod // cell module
{
  UINT8 temp_act = 0; // current temperature in °C + 40
  UINT8 temp_new = 0; // sensor buffer
  UINT8 temp_min = 240; // charge cycle min temperature
  UINT8 temp_max = 0; // charge cycle max temperature
  float temp_maxdev = 0; // charge cycle max temperature deviation
};

struct battery_cell
{
  UINT volt_act = 0; // current voltage in 1/200 V
  UINT volt_new = 0; // sensor buffer
  UINT volt_min = 2000; // charge cycle min voltage
  UINT volt_max = 0; // charge cycle max voltage
  float volt_maxdev = 0; // charge cycle max voltage deviation
};

// Conversion utils:
#define CONV_PackVolt(m) ((int)((m) * (100 / 10)))
#define CONV_CellVolt(m) ((int)((m) * (1000 / 200)))
#define CONV_CellVoltS(m) ((int)((m) * (1000 / 200)))
#define CONV_Temp(m) ((int)(m+0.5) - 40)


#endif // __rt_battmon_h__
