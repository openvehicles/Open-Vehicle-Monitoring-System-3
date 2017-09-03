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

#ifndef __METRICS_STANDARD_H__
#define __METRICS_STANDARD_H__

#define MS_M_VERSION                "m.version"
#define MS_M_HARDWARE               "m.hardware"
#define MS_M_SERIAL                 "m.serial"
#define MS_M_TASKS                  "m.tasks"
#define MS_M_FREERAM                "m.freeram"

#define MS_V_TYPE                   "v.type"
#define MS_V_VIN                    "v.vin"

#define MS_V_BAT_SOC                "v.b.soc"
#define MS_V_BAT_SOH                "v.b.soh"
#define MS_V_BAT_CAC                "v.b.cac"
#define MS_V_BAT_RANGE_IDEAL        "v.b.range.i"
#define MS_V_BAT_RANGE_EST          "v.b.range.e"
#define MS_V_BAT_12V                "v.b.12v"

#define MS_V_TEMP_PEM               "v.b.temp.pem"
#define MS_V_TEMP_BATTERY           "v.b.temp.battery"
#define MS_V_TEMP_MOTOR             "v.b.temp.motor"
#define MS_V_TEMP_CHARGER           "v.b.temp.charger"
#define MS_V_TEMP_AMBIENT           "v.b.temp.ambient"

#define MS_V_POS_LATITUDE           "v.p.latitude"
#define MS_V_POS_LONGITUDE          "v.p.longitude"
#define MS_V_POS_DIRECTION          "v.p.direction"
#define MS_V_POS_ALTITUDE           "v.p.altitude"
#define MS_V_POS_SPEED              "v.p.speed"
#define MS_V_POS_ODOMETER           "v.p.odometer"
#define MS_V_POS_TRIP               "v.p.trip"

#define MS_V_TPMS_FL_T              "v.tp.fl.t"
#define MS_V_TPMS_FR_T              "v.tp.fr.t"
#define MS_V_TPMS_RR_T              "v.tp.rr.t"
#define MS_V_TPMS_RL_T              "v.tp.rl.t"
#define MS_V_TPMS_FL_P              "v.tp.fl.p"
#define MS_V_TPMS_FR_P              "v.tp.fr.p"
#define MS_V_TPMS_RR_P              "v.tp.rr.p"
#define MS_V_TPMS_RL_P              "v.tp.rl.p"

typedef struct
  {
  const char* name;
  const char* type;
  } MetricStandard_t;

const MetricStandard_t MetricStandard[] =
  {
  { MS_M_VERSION,                 "string" },
  { MS_M_HARDWARE,                "string" },
  { MS_M_SERIAL,                  "string" },
  { MS_M_TASKS,                   "int" },
  { MS_M_FREERAM,                 "int" },
  { MS_V_TYPE,                    "string" },
  { MS_V_VIN,                     "string" },
  { MS_V_BAT_SOC,                 "int" },
  { MS_V_BAT_SOH,                 "int" },
  { MS_V_BAT_CAC,                 "float" },
  { MS_V_BAT_RANGE_IDEAL,         "int" },
  { MS_V_BAT_RANGE_EST,           "int" },
  { MS_V_BAT_12V,                 "float" },
  { MS_V_TEMP_PEM,                "int" },
  { MS_V_TEMP_BATTERY,            "int" },
  { MS_V_TEMP_MOTOR,              "int" },
  { MS_V_TEMP_CHARGER,            "int" },
  { MS_V_TEMP_AMBIENT,            "int" },
  { MS_V_POS_LATITUDE,            "int" },
  { MS_V_POS_LONGITUDE,           "int" },
  { MS_V_POS_DIRECTION,           "int" },
  { MS_V_POS_ALTITUDE,            "int" },
  { MS_V_POS_SPEED,               "int" },
  { MS_V_POS_ODOMETER,            "float" },
  { MS_V_POS_TRIP,                "float" },
  { MS_V_TPMS_FL_T,               "float" },
  { MS_V_TPMS_FR_T,               "float" },
  { MS_V_TPMS_RR_T,               "float" },
  { MS_V_TPMS_RL_T,               "float" },
  { MS_V_TPMS_FL_P,               "float" },
  { MS_V_TPMS_FR_P,               "float" },
  { MS_V_TPMS_RR_P,               "float" },
  { MS_V_TPMS_RL_P,               "float" },
  { "", "" }
  };

#endif //#ifndef __METRICS_STANDARD_H__
