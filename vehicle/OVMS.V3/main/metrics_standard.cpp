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

#include "metrics_standard.h"

MetricsStandard StandardMetrics __attribute__ ((init_priority (1810)));

#define SM_STALE_MIN 10
#define SM_STALE_MID 120
#define SM_STALE_HIGH 3600
#define SM_STALE_MAX 86400

MetricsStandard::MetricsStandard()
  {
  ms_m_version = new OvmsMetricString(MS_M_VERSION);
  ms_m_hardware = new OvmsMetricString(MS_M_HARDWARE);
  ms_m_serial = new OvmsMetricString(MS_M_SERIAL);
  ms_m_tasks = new OvmsMetricInt(MS_M_TASKS, SM_STALE_MID);
  ms_m_freeram = new OvmsMetricInt(MS_M_FREERAM, SM_STALE_MID);
  ms_s_v2_connected = new OvmsMetricBool(MS_S_V2_CONNECTED);
  ms_s_v2_peers = new OvmsMetricInt(MS_S_V2_PEERS);
  ms_v_type = new OvmsMetricString(MS_V_TYPE);
  ms_v_vin = new OvmsMetricString(MS_V_VIN);
  ms_v_bat_soc = new OvmsMetricInt(MS_V_BAT_SOC, SM_STALE_HIGH);
  ms_v_bat_soh = new OvmsMetricInt(MS_V_BAT_SOH, SM_STALE_HIGH);
  ms_v_bat_cac = new OvmsMetricFloat(MS_V_BAT_CAC, SM_STALE_HIGH);
  ms_v_bat_range_ideal = new OvmsMetricInt(MS_V_BAT_RANGE_IDEAL, SM_STALE_HIGH);
  ms_v_bat_range_est = new OvmsMetricInt(MS_V_BAT_RANGE_EST, SM_STALE_HIGH);
  ms_v_bat_12v = new OvmsMetricFloat(MS_V_BAT_12V, SM_STALE_HIGH);
  ms_v_temp_pem = new OvmsMetricInt(MS_V_TEMP_PEM, SM_STALE_MID);
  ms_v_temp_battery = new OvmsMetricInt(MS_V_TEMP_BATTERY, SM_STALE_MID);
  ms_v_temp_motor = new OvmsMetricInt(MS_V_TEMP_MOTOR, SM_STALE_MID);
  ms_v_temp_charger = new OvmsMetricInt(MS_V_TEMP_CHARGER, SM_STALE_MID);
  ms_v_temp_ambient = new OvmsMetricInt(MS_V_TEMP_AMBIENT, SM_STALE_MID);
  ms_v_pos_latitude = new OvmsMetricFloat(MS_V_POS_LATITUDE, SM_STALE_MID);
  ms_v_pos_longitude = new OvmsMetricFloat(MS_V_POS_LONGITUDE, SM_STALE_MID);
  ms_v_pos_direction = new OvmsMetricInt(MS_V_POS_DIRECTION, SM_STALE_MID);
  ms_v_pos_altitude = new OvmsMetricInt(MS_V_POS_ALTITUDE, SM_STALE_MID);
  ms_v_pos_speed = new OvmsMetricInt(MS_V_POS_SPEED, SM_STALE_MID);
  ms_v_pos_odometer = new OvmsMetricFloat(MS_V_POS_ODOMETER, SM_STALE_MID);
  ms_v_pos_trip = new OvmsMetricFloat(MS_V_POS_TRIP, SM_STALE_MID);
  ms_v_tpms_fl_t = new OvmsMetricFloat(MS_V_TPMS_FL_T, SM_STALE_MID);
  ms_v_tpms_fr_t = new OvmsMetricFloat(MS_V_TPMS_FR_T, SM_STALE_MID);
  ms_v_tpms_rr_t = new OvmsMetricFloat(MS_V_TPMS_RR_T, SM_STALE_MID);
  ms_v_tpms_rl_t = new OvmsMetricFloat(MS_V_TPMS_RL_T, SM_STALE_MID);
  ms_v_tpms_fl_p = new OvmsMetricFloat(MS_V_TPMS_FL_P, SM_STALE_MID);
  ms_v_tpms_fr_p = new OvmsMetricFloat(MS_V_TPMS_FR_P, SM_STALE_MID);
  ms_v_tpms_rr_p = new OvmsMetricFloat(MS_V_TPMS_RR_P, SM_STALE_MID);
  ms_v_tpms_rl_p = new OvmsMetricFloat(MS_V_TPMS_RL_P, SM_STALE_MID);
  }

MetricsStandard::~MetricsStandard()
  {
  }
