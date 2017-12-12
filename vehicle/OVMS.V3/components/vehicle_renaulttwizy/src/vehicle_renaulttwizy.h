/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: Main class
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

#ifndef __VEHICLE_RENAULTTWIZY_H__
#define __VEHICLE_RENAULTTWIZY_H__

#include "can.h"
#include "vehicle.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"

#include "rt_types.h"
#include "rt_battmon.h"
#include "rt_pwrmon.h"

using namespace std;

class OvmsVehicleRenaultTwizy : public OvmsVehicle
{
  
  public:
    OvmsVehicleRenaultTwizy();
    ~OvmsVehicleRenaultTwizy();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);

  public:
    void Ticker1(uint32_t ticker);
    void ConfigChanged(OvmsConfigParam* param);
    void EventListener(string event, void* data);
  
  private:
    void UpdateMaxRange();
    void UpdateChargeTimes();
    int ChargeTime(int dstsoc);
    
  
  // --------------------------------------------------------------------------
  // Twizy main commands
  // 
    
  protected:
    OvmsCommand *cmd_xrt;
    OvmsCommand *cmd_power;
    OvmsCommand *cmd_batt;
    OvmsCommand *cmd_gps;
    
  public:
    vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);
    vehicle_command_t CommandPower(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    vehicle_command_t CommandBatt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  
  // --------------------------------------------------------------------------
  // Twizy main metrics
  // 
  
  protected:
    static size_t m_modifier;
    OvmsMetricString *m_version;
    
  
  // --------------------------------------------------------------------------
  // Twizy state variables
  // 
  
  protected:
  
    char twizy_vin[8] = "";                     // last 7 digits of full VIN
    
    // Car + charge status from CAN:
    #define CAN_STATUS_FOOTBRAKE    0x01        //  bit 0 = 0x01: 1 = Footbrake
    #define CAN_STATUS_MODE_D       0x02        //  bit 1 = 0x02: 1 = Forward mode "D"
    #define CAN_STATUS_MODE_R       0x04        //  bit 2 = 0x04: 1 = Reverse mode "R"
    #define CAN_STATUS_MODE         0x06        //  mask
    #define CAN_STATUS_GO           0x08        //  bit 3 = 0x08: 1 = "GO" = Motor ON (Ignition)
    #define CAN_STATUS_KEYON        0x10        //  bit 4 = 0x10: 1 = Car awake (key turned)
    #define CAN_STATUS_CHARGING     0x20        //  bit 5 = 0x20: 1 = Charging
    #define CAN_STATUS_OFFLINE      0x40        //  bit 6 = 0x40: 1 = Switch-ON/-OFF phase / 0 = normal operation
    #define CAN_STATUS_ONLINE       0x80        //  bit 7 = 0x80: 1 = CAN-Bus online (test flag to detect offline)
    unsigned char twizy_status = CAN_STATUS_OFFLINE;
    
    
    struct {
      
      // Status flags:
      
      unsigned CarAwake:1;          // Twizy switched on
      unsigned CarON:1;             // Twizy in GO mode
      unsigned CarLocked:1;         // Speed limit mode active
      unsigned ValetMode:1;         // Conditional speed limit mode active
      
      unsigned PilotSignal:1;       // Power cable connected
      unsigned ChargePort:1;        // Charge cycle detection
      unsigned Charging:1;          // Main charge in progress
      unsigned Charging12V:1;       // DC-DC converter active
      
      unsigned CtrlLoggedIn:1;      // Logged into SEVCON
      unsigned CtrlCfgMode:1;       // SEVCON in configuration mode
      
      // Configuration flags:
      
      unsigned EnableWrite:1;       // CAN write enabled
      unsigned DisableReset:1;      // SEVCON reset on error disabled
      unsigned DisableKickdown:1;   // SEVCON automatic kickdown disabled
      unsigned DisableAutoPower:1;  // SEVCON automatic power level adjustment disabled
      unsigned EnableInputs:1;      // SimpleConsole inputs enabled
      
    } twizy_flags;
    
    
    int twizy_tmotor = 0;                           // motor temperature [°C]
    int twizy_soh = 0;                              // BMS SOH [%]
    
    unsigned int twizy_last_soc = 0;                // sufficient charge SOC threshold helper
    unsigned int twizy_last_idealrange = 0;         // sufficient charge range threshold helper
    
    unsigned int twizy_soc = 5000;                  // detailed SOC (1/100 %)
    unsigned int twizy_soc_min = 10000;             // min SOC reached during last discharge
    unsigned int twizy_soc_min_range = 100;         // twizy_range at min SOC
    unsigned int twizy_soc_max = 0;                 // max SOC reached during last charge
    unsigned int twizy_soc_tripstart = 0;           // SOC at last power on
    unsigned int twizy_soc_tripend = 0;             // SOC at last power off
    
    int cfg_suffsoc = 0;                            // Configured sufficient SOC
    int cfg_suffrange = 0;                          // Configured sufficient range
    
    #define CFG_DEFAULT_MAXRANGE 80
    int cfg_maxrange = CFG_DEFAULT_MAXRANGE;        // Configured max range at 20 °C
    float twizy_maxrange = 0;                       // Max range at current temperature
    
    unsigned int twizy_range_est = 50;              // Twizy estimated range in km
    unsigned int twizy_range_ideal = 50;            // calculated ideal range in km
    
    unsigned int twizy_speed = 0;                   // current speed in 1/100 km/h
    unsigned long twizy_odometer = 0;               // current odometer in 1/100 km = 10 m
    unsigned long twizy_odometer_tripstart = 0;     // odometer at last power on
    unsigned int twizy_dist = 0;                    // cyclic distance counter in 1/10 m = 10 cm
    
    unsigned char twizy_lock_speed = 0;             // if Lock mode: fix speed to this (kph)
    unsigned long twizy_valet_odo = 0;              // if Valet mode: reduce speed if odometer > this
    
    
    // NOTE: the power values are derived by...
    //    twizy_power = ( twizy_current * twizy_batt[0].volt_act + 128 ) / 256
    // The twizy_current measurement has a high time resolution of 1/100 s,
    // but the battery voltage has a low time resolution of just 1 s, so the
    // power values need to be seen as an estimation.
    
    signed int twizy_power = 0;                     // momentary battery power in 256/40 W = 6.4 W, negative=charging
    signed int twizy_power_min = 32767;             // min "power" in current GPS log section
    signed int twizy_power_max = -32768;            // max "power" in current GPS log section
    
    // power is collected in 6.4 W & 1/100 s resolution
    // to convert power sums to Wh:
    //  Wh = power_sum * 6.4 * 1/100 * 1/3600
    //  Wh = power_sum / 56250
    #define WH_DIV (56250L)
    #define WH_RND (28125L)
    
    
    UINT8 twizy_autorecup_checkpoint = 0;   // change detection for autorecup function
    UINT twizy_autorecup_level = 1000;      // autorecup: current recup level (per mille)
    
    UINT8 twizy_autodrive_checkpoint = 0;   // change detection for autopower function
    UINT twizy_autodrive_level = 1000;      // autopower: current drive level (per mille)
    
    
    signed int twizy_current = 0;           // momentary battery current in 1/4 A, negative=charging
    signed int twizy_current_min = 32767;   // min battery current in GPS log section
    signed int twizy_current_max = -32768;  // max battery current in GPS log section
    
    UINT32 twizy_charge_use = 0;            // coulomb count: batt current used (discharged) 1/400 As
    UINT32 twizy_charge_rec = 0;            // coulomb count: batt current recovered (charged) 1/400 As
    
    // cAh = [1/400 As] / 400 / 3600 * 100
    #define CAH_DIV (14400L)
    #define CAH_RND (7200L)
    #define AH_DIV (CAH_DIV * 100L)
    
    int cfg_chargemode = 0;
    #define TWIZY_CHARGEMODE_DEFAULT    0   // notify on limits (FW: "standard")
    #define TWIZY_CHARGEMODE_AUTOSTOP   1   // stop on limits (FW: "storage")
    
    int cfg_chargelevel = 0;
    
    int twizy_chargestate = 4;              // 1=charging, 2=top off, 4=done, 21=stopped charging
    bool twizy_chg_stop_request = false;
    UINT8 twizy_chg_power_request = 0;      // BMS to CHG power level request (0..7)
    
    // battery capacity helpers:
    UINT32 twizy_cc_charge = 0;             // coulomb count: charge sum in CC phase 1/400 As
    UINT8 twizy_cc_power_level = 0;         // end of CC phase detection
    UINT twizy_cc_soc = 0;                  // SOC at end of CC phase
    
    #define CFG_DEFAULT_CAPACITY 108
    float cfg_bat_cap_actual_prc = 100;     // actual capacity in percent of…
    float cfg_bat_cap_nominal_ah = CFG_DEFAULT_CAPACITY; // …usable capacity of fresh battery in Ah
    
    int twizy_chargeduration = 0;           // charge duration in seconds
    
    UINT8 twizy_fan_timer = 0;              // countdown timer for RB1 charger fan
    #define TWIZY_FAN_THRESHOLD   45    // temperature in °C
    #define TWIZY_FAN_OVERSHOOT   5     // hold time in minutes after switch-off
    
    int cfg_gpslog_interval = 5;        // GPS-Log interval while driving [seconds]
    uint32_t twizy_last_gpslog = 0;     // Time of last GPS-Log update
    
    // Notifications:
    #define SEND_BatteryAlert           (1<< 0)  // alert: battery problem
    #define SEND_PowerNotify            (1<< 1)  // info: power usage summary
    #define SEND_PowerStats             (1<< 2)  // data: RT-PWR-Stats history entry
    #define SEND_GPSLog                 (1<< 3)  // data: RT-GPS-Log history entry
    #define SEND_BatteryStats           (1<< 4)  // data: separate battery stats (large)
    #define SEND_CodeAlert              (1<< 5)  // alert: fault code (SEVCON/inputs/...)
    #define SEND_TripLog                (1<< 6)  // data: RT-PWR-Log history entry
    #define SEND_ResetResult            (1<< 7)  // info/alert: RESET OK/FAIL
    #define SEND_ChargeState            (1<< 8)  // info: STAT command
    #define SEND_ChargeAlert            (1<< 9)  // alert: STAT command (charge interrupt)
    #define SEND_SDOLog                 (1<<10)  // data: RT-ENG-SDO history entry
    
  protected:
    unsigned int twizy_notifications = 0;
    void RequestNotify(unsigned int which);
    void DoNotify();
    
    void SendGPSLog();
    //void SendPWRLog();
    //void SendSDOLog();
    
    
    // --------------------------------------------------------------------------
    // Twizy power statistics subsystem
    //  - implementation: rt_pwrmon.(h,cpp)
    // 
    
  public:
    void PowerInit();
    void PowerUpdate();
    void PowerReset();
    bool PowerIsModified();
    
  private:
    void PowerCollectData();
    
  protected:
    speedpwr twizy_speedpwr[3];                 // speed power usage statistics
    UINT8 twizy_speed_state = 0;                    // speed state, one of:
    #define CAN_SPEED_CONST         0           // constant speed
    #define CAN_SPEED_ACCEL         1           // accelerating
    #define CAN_SPEED_DECEL         2           // decelerating
    
    // Speed resolution is ~11 (=0.11 kph) & ~100 ms
    #define CAN_SPEED_THRESHOLD     10
    
    // Speed change threshold for accel/decel detection:
    //  ...applied to twizy_accel_avg (4 samples):
    #define CAN_ACCEL_THRESHOLD     7
    
    signed int twizy_accel_avg = 0;                 // running average of speed delta
    
    signed int twizy_accel_min = 0;                 // min speed delta of trip
    signed int twizy_accel_max = 0;                 // max speed delta of trip
    
    unsigned int twizy_speed_distref = 0;           // distance reference for speed section
    
    
    levelpwr twizy_levelpwr[2];                 // level power usage statistics
    #define CAN_LEVEL_UP            0           // uphill
    #define CAN_LEVEL_DOWN          1           // downhill
    
    unsigned long twizy_level_odo = 0;              // level section odometer reference
    signed int twizy_level_alt = 0;                 // level section altitude reference
    unsigned long twizy_level_use = 0;              // level section use collector
    unsigned long twizy_level_rec = 0;              // level section rec collector
    #define CAN_LEVEL_MINSECTLEN    100         // min section length (in m)
    #define CAN_LEVEL_THRESHOLD     1           // level change threshold (grade percent)
    
    
    // --------------------------------------------------------------------------
    // Twizy battery monitoring subsystem
    //  - implementation: rt_battmon.(h,cpp)
    // 
    
  public:
    void BatteryInit();
    void BatteryUpdate();
    void BatteryReset();
    void FormatPackData(int verbosity, OvmsWriter* writer, int pack);
    void FormatCellData(int verbosity, OvmsWriter* writer, int cell);
    void BatterySendDataUpdate();
    void FormatBatteryStatus(int verbosity, OvmsWriter* writer, int pack);
    void FormatBatteryVolts(int verbosity, OvmsWriter* writer, bool show_deviations);
    void FormatBatteryTemps(int verbosity, OvmsWriter* writer, bool show_deviations);
  
  private:
    void BatteryCheckDeviations();
  
  protected:
    #define BATT_PACKS      1
    #define BATT_CELLS      16                  // 14 on LiPo pack, 16 on LiFe pack
    #define BATT_CMODS      8                   // 7 on LiPo pack, 8 on LiFe pack
    
    UINT8 batt_pack_count = 1;
    UINT8 batt_cmod_count = 7;
    UINT8 batt_cell_count = 14;
    
    OvmsMetricFloat *m_batt_soc_min;
    OvmsMetricFloat *m_batt_soc_max;
    
    OvmsMetricInt *m_batt_pack_count;
    OvmsMetricInt *m_batt_cmod_count;
    OvmsMetricInt *m_batt_cell_count;
    
    battery_pack twizy_batt[BATT_PACKS];
    battery_cmod twizy_cmod[BATT_CMODS];
    battery_cell twizy_cell[BATT_CELLS];
    
    
    // --------------------------------------------------------------------------
    // Twizy SEVCON subsystem
    //  - implementation: rt_sevcon.(h,cpp)
    // 
    
    signed char twizy_button_cnt = 0;           // will count key presses (errors) in STOP mode (msg 081)
    
    
    
};

#endif // __VEHICLE_RENAULTTWIZY_H__
