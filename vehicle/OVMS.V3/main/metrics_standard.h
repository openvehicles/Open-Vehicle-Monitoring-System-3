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

#include "ovms_metrics.h"

#define SM_STALE_NONE   0
#define SM_STALE_MIN    10
#define SM_STALE_MID    120
#define SM_STALE_HIGH   3600
#define SM_STALE_MAX    65535


#define MS_M_VERSION                "m.version"
#define MS_M_HARDWARE               "m.hardware"
#define MS_M_SERIAL                 "m.serial"
#define MS_M_TASKS                  "m.tasks"
#define MS_M_FREERAM                "m.freeram"
#define MS_M_MONOTONIC              "m.monotonic"
#define MS_M_TIME_UTC               "m.time.utc"

#define MS_N_TYPE                   "m.net.type"
#define MS_N_SQ                     "m.net.sq"
#define MS_N_PROVIDER               "m.net.provider"
#define MS_N_MDM_ICCID              "m.net.mdm.iccid"
#define MS_N_MDM_MODEL              "m.net.mdm.model"

#define MS_S_V2_CONNECTED           "s.v2.connected"
#define MS_S_V2_PEERS               "s.v2.peers"

#define MS_S_V3_CONNECTED           "s.v3.connected"
#define MS_S_V3_PEERS               "s.v3.peers"

#define MS_V_TYPE                   "v.type"
#define MS_V_VIN                    "v.vin"

#define MS_V_BAT_SOC                "v.b.soc"
#define MS_V_BAT_SOH                "v.b.soh"
#define MS_V_BAT_CAC                "v.b.cac"
#define MS_V_BAT_HEALTH             "v.b.health"
#define MS_V_BAT_VOLTAGE            "v.b.voltage"
#define MS_V_BAT_CURRENT            "v.b.current"
#define MS_V_BAT_COULOMB_USED       "v.b.coulomb.used"
#define MS_V_BAT_COULOMB_RECD       "v.b.coulomb.recd"
#define MS_V_BAT_POWER              "v.b.power"
#define MS_V_BAT_CONSUMPTION        "v.b.consumption"
#define MS_V_BAT_ENERGY_USED        "v.b.energy.used"
#define MS_V_BAT_ENERGY_RECD        "v.b.energy.recd"
#define MS_V_BAT_RANGE_FULL         "v.b.range.full"
#define MS_V_BAT_RANGE_IDEAL        "v.b.range.ideal"
#define MS_V_BAT_RANGE_EST          "v.b.range.est"
#define MS_V_BAT_12V_VOLTAGE        "v.b.12v.voltage"
#define MS_V_BAT_12V_CURRENT        "v.b.12v.current"
#define MS_V_BAT_12V_VOLTAGE_REF    "v.b.12v.voltage.ref"
#define MS_V_BAT_12V_VOLTAGE_ALERT  "v.b.12v.voltage.alert"
#define MS_V_BAT_TEMP               "v.b.temp"

#define MS_V_BAT_PACK_LEVEL_MIN     "v.b.p.level.min"
#define MS_V_BAT_PACK_LEVEL_MAX     "v.b.p.level.max"
#define MS_V_BAT_PACK_LEVEL_AVG     "v.b.p.level.avg"
#define MS_V_BAT_PACK_LEVEL_STDDEV  "v.b.p.level.stddev"
#define MS_V_BAT_PACK_VMIN          "v.b.p.voltage.min"
#define MS_V_BAT_PACK_VMAX          "v.b.p.voltage.max"
#define MS_V_BAT_PACK_VAVG          "v.b.p.voltage.avg"
#define MS_V_BAT_PACK_VSTDDEV       "v.b.p.voltage.stddev"
#define MS_V_BAT_PACK_VSTDDEVMAX    "v.b.p.voltage.stddev.max"
#define MS_V_BAT_PACK_TMIN          "v.b.p.temp.min"
#define MS_V_BAT_PACK_TMAX          "v.b.p.temp.max"
#define MS_V_BAT_PACK_TAVG          "v.b.p.temp.avg"
#define MS_V_BAT_PACK_TSTDDEV       "v.b.p.temp.stddev"
#define MS_V_BAT_PACK_TSTDDEVMAX    "v.b.p.temp.stddev.max"
#define MS_V_BAT_CELL_VOLTAGE       "v.b.c.voltage"
#define MS_V_BAT_CELL_VMIN          "v.b.c.voltage.min"
#define MS_V_BAT_CELL_VMAX          "v.b.c.voltage.max"
#define MS_V_BAT_CELL_VDEVMAX       "v.b.c.voltage.dev.max"
#define MS_V_BAT_CELL_VALERT        "v.b.c.voltage.alert"
#define MS_V_BAT_CELL_TEMP          "v.b.c.temp"
#define MS_V_BAT_CELL_TMIN          "v.b.c.temp.min"
#define MS_V_BAT_CELL_TMAX          "v.b.c.temp.max"
#define MS_V_BAT_CELL_TDEVMAX       "v.b.c.temp.dev.max"
#define MS_V_BAT_CELL_TALERT        "v.b.c.temp.alert"

#define MS_V_CHARGE_VOLTAGE         "v.c.voltage"
#define MS_V_CHARGE_CURRENT         "v.c.current"
#define MS_V_CHARGE_CLIMIT          "v.c.climit"
#define MS_V_CHARGE_TIME            "v.c.time"
#define MS_V_CHARGE_KWH             "v.c.kwh"
#define MS_V_CHARGE_MODE            "v.c.mode"
#define MS_V_CHARGE_TIMERMODE       "v.c.timermode"
#define MS_V_CHARGE_TIMERSTART      "v.c.timerstart"
#define MS_V_CHARGE_STATE           "v.c.state"
#define MS_V_CHARGE_SUBSTATE        "v.c.substate"
#define MS_V_CHARGE_TYPE            "v.c.type"
#define MS_V_CHARGE_PILOT           "v.c.pilot"
#define MS_V_CHARGE_INPROGRESS      "v.c.charging"
#define MS_V_CHARGE_LIMIT_RANGE     "v.c.limit.range"
#define MS_V_CHARGE_LIMIT_SOC       "v.c.limit.soc"
#define MS_V_CHARGE_DURATION_FULL   "v.c.duration.full"
#define MS_V_CHARGE_DURATION_RANGE  "v.c.duration.range"
#define MS_V_CHARGE_DURATION_SOC    "v.c.duration.soc"
#define MS_V_CHARGE_TEMP            "v.c.temp"

#define MS_V_INV_TEMP               "v.i.temp"

#define MS_V_MOT_RPM                "v.m.rpm"
#define MS_V_MOT_TEMP               "v.m.temp"

#define MS_V_DOOR_FL                "v.d.fl"
#define MS_V_DOOR_FR                "v.d.fr"
#define MS_V_DOOR_RL                "v.d.rl"
#define MS_V_DOOR_RR                "v.d.rr"
#define MS_V_DOOR_CHARGEPORT        "v.d.cp"
#define MS_V_DOOR_HOOD              "v.d.hood"
#define MS_V_DOOR_TRUNK             "v.d.trunk"

#define MS_V_ENV_DRIVEMODE          "v.e.drivemode"
#define MS_V_ENV_GEAR               "v.e.gear"
#define MS_V_ENV_THROTTLE           "v.e.throttle"
#define MS_V_ENV_FOOTBRAKE          "v.e.footbrake"
#define MS_V_ENV_HANDBRAKE          "v.e.handbrake"
#define MS_V_ENV_AWAKE              "v.e.awake"
#define MS_V_ENV_CHARGING12V        "v.e.charging12v"
#define MS_V_ENV_COOLING            "v.e.cooling"
#define MS_V_ENV_HEATING            "v.e.heating"
#define MS_V_ENV_HVAC               "v.e.hvac"
#define MS_V_ENV_ON                 "v.e.on"
#define MS_V_ENV_LOCKED             "v.e.locked"
#define MS_V_ENV_VALET              "v.e.valet"
#define MS_V_ENV_HEADLIGHTS         "v.e.headlights"
#define MS_V_ENV_ALARM              "v.e.alarm"
#define MS_V_ENV_PARKTIME           "v.e.parktime"
#define MS_V_ENV_DRIVETIME          "v.e.drivetime"
#define MS_V_ENV_CTRL_LOGIN         "v.e.c.login"
#define MS_V_ENV_CTRL_CONFIG        "v.e.c.config"
#define MS_V_ENV_TEMP               "v.e.temp"
#define MS_V_ENV_CABINTEMP          "v.e.cabintemp"

#define MS_V_POS_GPSLOCK            "v.p.gpslock"
#define MS_V_POS_GPSSTALE           "v.p.gpsstale"
#define MS_V_POS_GPSMODE            "v.p.gpsmode"
#define MS_V_POS_GPSHDOP            "v.p.gpshdop"
#define MS_V_POS_SATCOUNT           "v.p.satcount"
#define MS_V_POS_LATITUDE           "v.p.latitude"
#define MS_V_POS_LONGITUDE          "v.p.longitude"
#define MS_V_POS_DIRECTION          "v.p.direction"
#define MS_V_POS_ALTITUDE           "v.p.altitude"
#define MS_V_POS_SPEED              "v.p.speed"
#define MS_V_POS_GPSSPEED           "v.p.gpsspeed"
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

class MetricsStandard
  {
  public:
    MetricsStandard();
    virtual ~MetricsStandard();

  public:
    OvmsMetricString* ms_m_version;
    OvmsMetricString* ms_m_hardware;
    OvmsMetricString* ms_m_serial;
    OvmsMetricInt*    ms_m_tasks;
    OvmsMetricInt*    ms_m_freeram;
    OvmsMetricInt*    ms_m_monotonic;
    OvmsMetricInt*    ms_m_timeutc;

    OvmsMetricString* ms_m_net_type;                // none, wifi, modem
    OvmsMetricInt*    ms_m_net_sq;                  // Network signal quality [dbm]
    OvmsMetricString* ms_m_net_provider;            // Network provider name
    OvmsMetricString* ms_m_net_mdm_iccid;
    OvmsMetricString* ms_m_net_mdm_model;

    OvmsMetricBool*   ms_s_v2_connected;            // True = V2 server connected [1]
    OvmsMetricInt*    ms_s_v2_peers;                // V2 clients connected [1]

    OvmsMetricBool*   ms_s_v3_connected;            // True = V3 server connected [1]
    OvmsMetricInt*    ms_s_v3_peers;                // V3 clients connected [1]

    OvmsMetricString* ms_v_type;                    // Vehicle type code
    OvmsMetricString* ms_v_vin;                     // Vehicle identification number

    OvmsMetricFloat*  ms_v_bat_soc;                 // State of charge [%]
    OvmsMetricFloat*  ms_v_bat_soh;                 // State of health [%]
    OvmsMetricFloat*  ms_v_bat_cac;                 // Calculated capacity [Ah]
    OvmsMetricString* ms_v_bat_health;              // General textual description of battery health
    OvmsMetricFloat*  ms_v_bat_voltage;             // Main battery momentary voltage [V]
    OvmsMetricFloat*  ms_v_bat_current;             // Main battery momentary current [A]
    OvmsMetricFloat*  ms_v_bat_coulomb_used;        // Main battery coulomb used on trip [Ah]
    OvmsMetricFloat*  ms_v_bat_coulomb_recd;        // Main battery coulomb recovered on trip [Ah]
    OvmsMetricFloat*  ms_v_bat_power;               // Main battery momentary power [kW]
    OvmsMetricFloat*  ms_v_bat_consumption;         // Main battery momentary consumption [Wh/km]
    OvmsMetricFloat*  ms_v_bat_energy_used;         // Main battery energy used on trip [kWh]
    OvmsMetricFloat*  ms_v_bat_energy_recd;         // Main battery energy recovered on trip [kWh]
    OvmsMetricFloat*  ms_v_bat_range_full;          // Ideal range at 100% SOC & current conditions [km]
    OvmsMetricFloat*  ms_v_bat_range_ideal;         // Ideal range [km]
    OvmsMetricFloat*  ms_v_bat_range_est;           // Estimated range [km]
    OvmsMetricFloat*  ms_v_bat_12v_voltage;         // Auxiliary 12V battery momentary voltage [V]
    OvmsMetricFloat*  ms_v_bat_12v_current;         // Auxiliary 12V battery momentary current [A]
    OvmsMetricFloat*  ms_v_bat_12v_voltage_ref;     // Auxiliary 12V battery reference voltage [V]
    OvmsMetricBool*   ms_v_bat_12v_voltage_alert;   // True = auxiliary battery under voltage alert
    OvmsMetricFloat*  ms_v_bat_temp;                // Battery temperature [°C]

    OvmsMetricFloat*  ms_v_bat_pack_level_min;            // Cell level - weakest cell in pack [%]
    OvmsMetricFloat*  ms_v_bat_pack_level_max;            // Cell level - strongest cell in pack [%]
    OvmsMetricFloat*  ms_v_bat_pack_level_avg;            // Cell level - pack average [%]
    OvmsMetricFloat*  ms_v_bat_pack_level_stddev;         // Cell level - pack standard deviation [%]
                                                          // Note: "level" = voltage levels normalized to percent

    OvmsMetricFloat*  ms_v_bat_pack_vmin;                 // Cell voltage - weakest cell in pack [V]
    OvmsMetricFloat*  ms_v_bat_pack_vmax;                 // Cell voltage - strongest cell in pack [V]
    OvmsMetricFloat*  ms_v_bat_pack_vavg;                 // Cell voltage - pack average [V]
    OvmsMetricFloat*  ms_v_bat_pack_vstddev;              // Cell voltage - current standard deviation [V]
    OvmsMetricFloat*  ms_v_bat_pack_vstddev_max;          // Cell voltage - maximum standard deviation observed [V]

    OvmsMetricFloat*  ms_v_bat_pack_tmin;                 // Cell temperature - coldest cell in pack [°C]
    OvmsMetricFloat*  ms_v_bat_pack_tmax;                 // Cell temperature - warmest cell in pack [°C]
    OvmsMetricFloat*  ms_v_bat_pack_tavg;                 // Cell temperature - pack average [°C]
    OvmsMetricFloat*  ms_v_bat_pack_tstddev;              // Cell temperature - current standard deviation [°C]
    OvmsMetricFloat*  ms_v_bat_pack_tstddev_max;          // Cell temperature - maximum standard deviation observed [°C]

    OvmsMetricVector<float>* ms_v_bat_cell_voltage;       // Cell voltages [V]
    OvmsMetricVector<float>* ms_v_bat_cell_vmin;          // Cell minimum voltages [V]
    OvmsMetricVector<float>* ms_v_bat_cell_vmax;          // Cell maximum voltages [V]
    OvmsMetricVector<float>* ms_v_bat_cell_vdevmax;       // Cell maximum voltage deviation observed [V]
    OvmsMetricVector<short>* ms_v_bat_cell_valert;        // Cell voltage deviation alert level [0=normal, 1=warning, 2=alert]

    OvmsMetricVector<float>* ms_v_bat_cell_temp;          // Cell temperatures [°C]
    OvmsMetricVector<float>* ms_v_bat_cell_tmin;          // Cell minimum temperatures [°C]
    OvmsMetricVector<float>* ms_v_bat_cell_tmax;          // Cell maximum temperatures [°C]
    OvmsMetricVector<float>* ms_v_bat_cell_tdevmax;       // Cell maximum temperature deviation observed [°C]
    OvmsMetricVector<short>* ms_v_bat_cell_talert;        // Cell temperature deviation alert level [0=normal, 1=warning, 2=alert]

    OvmsMetricFloat*  ms_v_charge_voltage;          // Momentary charger supply voltage [V]
    OvmsMetricFloat*  ms_v_charge_current;          // Momentary charger output current [A]
    OvmsMetricFloat*  ms_v_charge_climit;           // Maximum charger output current [A]
    OvmsMetricInt*    ms_v_charge_time;             // Duration of running charge [sec]
    OvmsMetricFloat*  ms_v_charge_kwh;              // Energy sum for running charge [kWh]
    OvmsMetricString* ms_v_charge_mode;             // standard, range, performance, storage
    OvmsMetricBool*   ms_v_charge_timermode;        // True if timer enabled
    OvmsMetricInt*    ms_v_charge_timerstart;       // Time timer is due to start
    OvmsMetricString* ms_v_charge_state;            // charging, topoff, done, prepare, timerwait, heating, stopped
    OvmsMetricString* ms_v_charge_substate;         // scheduledstop, scheduledstart, onrequest, timerwait, powerwait, stopped, interrupted
    OvmsMetricString* ms_v_charge_type;             // undefined, type1, type2, chademo, roadster, teslaus, supercharger, ccs
    OvmsMetricBool*   ms_v_charge_pilot;            // Pilot signal present
    OvmsMetricBool*   ms_v_charge_inprogress;       // True = currently charging
    OvmsMetricFloat*  ms_v_charge_limit_range;      // Sufficient range limit for current charge [km]
    OvmsMetricFloat*  ms_v_charge_limit_soc;        // Sufficient SOC limit for current charge [%]
    OvmsMetricInt*    ms_v_charge_duration_full;    // Estimated time remaing for full charge [min]
    OvmsMetricInt*    ms_v_charge_duration_range;   // … for sufficient range [min]
    OvmsMetricInt*    ms_v_charge_duration_soc;     // … for sufficient SOC [min]
    OvmsMetricFloat*  ms_v_charge_temp;             // Charger temperature [°C]

    OvmsMetricFloat*  ms_v_inv_temp;                // Inverter temperature [°C]

    OvmsMetricInt*    ms_v_mot_rpm;                 // Motor speed (RPM)
    OvmsMetricFloat*  ms_v_mot_temp;                // Motor temperature [°C]

    OvmsMetricBool*   ms_v_door_fl;
    OvmsMetricBool*   ms_v_door_fr;
    OvmsMetricBool*   ms_v_door_rl;
    OvmsMetricBool*   ms_v_door_rr;
    OvmsMetricBool*   ms_v_door_chargeport;
    OvmsMetricBool*   ms_v_door_hood;
    OvmsMetricBool*   ms_v_door_trunk;

    OvmsMetricInt*    ms_v_env_drivemode;           // Active drive profile number [1]
    OvmsMetricInt*    ms_v_env_gear;                // Gear/direction; negative=reverse, 0=neutral [1]
    OvmsMetricFloat*  ms_v_env_throttle;            // Drive pedal state [%]
    OvmsMetricFloat*  ms_v_env_footbrake;           // Brake pedal state [%]
    OvmsMetricBool*   ms_v_env_handbrake;           // Handbrake state
    OvmsMetricBool*   ms_v_env_awake;               // Vehicle/bus awake (switched on)
    OvmsMetricBool*   ms_v_env_charging12v;         // 12V battery charging
    OvmsMetricBool*   ms_v_env_cooling;
    OvmsMetricBool*   ms_v_env_heating;
    OvmsMetricBool*   ms_v_env_hvac;                // Climate control system state
    OvmsMetricBool*   ms_v_env_on;                  // "Ignition" state (drivable)
    OvmsMetricBool*   ms_v_env_locked;              // Vehicle locked
    OvmsMetricBool*   ms_v_env_valet;               // Vehicle in valet mode
    OvmsMetricBool*   ms_v_env_headlights;
    OvmsMetricBool*   ms_v_env_alarm;
    OvmsMetricInt*    ms_v_env_parktime;
    OvmsMetricInt*    ms_v_env_drivetime;
    OvmsMetricBool*   ms_v_env_ctrl_login;          // Module logged in at ECU/controller
    OvmsMetricBool*   ms_v_env_ctrl_config;         // ECU/controller in configuration state
    OvmsMetricFloat*  ms_v_env_temp;                // Ambient temperature [°C]
    OvmsMetricFloat*  ms_v_env_cabintemp;           // Cabin temperature [°C]

    OvmsMetricBool*   ms_v_pos_gpslock;
    OvmsMetricString* ms_v_pos_gpsmode;             // <GPS><GLONASS>; N/A/D/E (None/Autonomous/Differential/Estimated)
    OvmsMetricFloat*  ms_v_pos_gpshdop;             // Horizontal dilution of precision (smaller=better)
    OvmsMetricInt*    ms_v_pos_satcount;
    OvmsMetricFloat*  ms_v_pos_latitude;
    OvmsMetricFloat*  ms_v_pos_longitude;
    OvmsMetricFloat*  ms_v_pos_direction;
    OvmsMetricFloat*  ms_v_pos_altitude;
    OvmsMetricFloat*  ms_v_pos_speed;               // Vehicle speed [kph]
    OvmsMetricFloat*  ms_v_pos_gpsspeed;            // GPS speed over ground [kph]
    OvmsMetricFloat*  ms_v_pos_odometer;
    OvmsMetricFloat*  ms_v_pos_trip;

    OvmsMetricFloat*  ms_v_tpms_fl_t;
    OvmsMetricFloat*  ms_v_tpms_fr_t;
    OvmsMetricFloat*  ms_v_tpms_rr_t;
    OvmsMetricFloat*  ms_v_tpms_rl_t;
    OvmsMetricFloat*  ms_v_tpms_fl_p;
    OvmsMetricFloat*  ms_v_tpms_fr_p;
    OvmsMetricFloat*  ms_v_tpms_rr_p;
    OvmsMetricFloat*  ms_v_tpms_rl_p;
  };

extern MetricsStandard StandardMetrics;
#define StdMetrics StandardMetrics

#endif //#ifndef __METRICS_STANDARD_H__
