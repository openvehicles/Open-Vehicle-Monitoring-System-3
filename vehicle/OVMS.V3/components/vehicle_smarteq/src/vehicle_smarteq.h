/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 ; https://github.com/MyLab-odyssey/ED4scan
 */

#ifndef __VEHICLE_SMARTEQ_H__
#define __VEHICLE_SMARTEQ_H__

#define VERSION "2.1.0"
#define PRESET_VERSION 4 // Configuration preset version

#include "ovms_log.h"

#include <deque>
#include <string>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cmath>

#include <stdint.h>
#include <stdio.h>
#include <sdkconfig.h>

#include "can.h"
#include "vehicle.h"
#include "metrics_standard.h"

#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "ovms_notify.h"
#include "ovms_peripherals.h"
#include "ovms_time.h"

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     data[b]
#define CAN_UINT(b)     (uint16_t(((uint16_t)CAN_BYTE(b) << 8) | (uint16_t)CAN_BYTE((b)+1)))
#define CAN_UINT24(b)   (uint32_t( ((uint32_t)CAN_BYTE(b) << 16) | ((uint32_t)CAN_BYTE((b)+1) << 8) | (uint32_t)CAN_BYTE((b)+2) ))
#define CAN_UINT32(b)   (uint32_t( ((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE((b)+1) << 16) | ((uint32_t)CAN_BYTE((b)+2) << 8) | (uint32_t)CAN_BYTE((b)+3) ))
#define CAN_NIBL(b)     (data[b] & 0x0f)
#define CAN_NIBH(b)     (data[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))

// Vehicle specific MSG protocol command IDs:
#define CMD_SetChargeAlerts         204 // (suffsoc)

using namespace std;

typedef std::vector<OvmsPoller::poll_pid_t, ExtRamAllocator<OvmsPoller::poll_pid_t>> poll_vector_t;
typedef std::initializer_list<const OvmsPoller::poll_pid_t> poll_list_t;

class OvmsVehicleSmartEQ : public OvmsVehicle
{
  public:
    OvmsVehicleSmartEQ();
    ~OvmsVehicleSmartEQ();

  private:
    static OvmsVehicleSmartEQ* GetInstance(OvmsWriter* writer=NULL);

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame) override;
    void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
    void IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code) override;
    void HandleCharging();
    void HandleEnergy();
    void HandleTripcounter();
    void HandleChargeport();
    void Handlev2Server();
    void UpdateChargeMetrics();
    int  calcMinutesRemaining(float target, float charge_voltage, float charge_current);
    void HandlePollState();
    void OnlineState();
    void ObdModifyPoll();
    void ResetChargingValues();
    void ResetTripCounters();
    void ResetTotalCounters();
    void Check12vState();
    void DisablePlugin(const char* plugin);
    bool ExecuteCommand(const std::string& command);
    void setTPMSValue();
    void setTPMSValueBoot();
    void NotifyClimate();
    void NotifyClimateTimer();
    void NotifyTripReset();
    void NotifyTripStart();
    void NotifyTripCounters();
    void NotifyTotalCounters();
    void NotifyMaintenance();
    void Notify12Vcharge();
    void NotifySOClimit();
    void DoorLockState();
    void DoorOpenState();
    void WifiRestart();
    void ModemRestart();
    void ModemEventRestart(std::string event, void* data);
    void ReCalcADCfactor(float can12V, OvmsWriter* writer=nullptr);
    void EventListener(std::string event, void* data);

public:
    vehicle_command_t CommandClimateControl(bool enable) override;
    vehicle_command_t CommandHomelink(int button, int durationms=1000) override;
    vehicle_command_t CommandWakeup() override;
    vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer) override;
    vehicle_command_t CommandLock(const char* pin) override;
    vehicle_command_t CommandUnlock(const char* pin) override;
    vehicle_command_t CommandActivateValet(const char* pin) override;
    vehicle_command_t CommandDeactivateValet(const char* pin) override;
    vehicle_command_t CommandCan(uint32_t txid,uint32_t rxid,bool reset=false,bool wakeup=false);
    vehicle_command_t CommandWakeup2();
    vehicle_command_t ProcessMsgCommand(std::string &result, int command, const char* args);
    vehicle_command_t MsgCommandCA(std::string &result, int command, const char* args);
    virtual vehicle_command_t CommandTripStart(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandTripReset(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandMaintenance(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandTripCounters(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandTripTotal(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t Command12Vcharge(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandTPMSset(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandDDT4all(int number, OvmsWriter* writer);
    virtual vehicle_command_t CommandDDT4List(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandSOClimit(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandPreset(int verbosity, OvmsWriter* writer);
    
public:
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgTPMS(PageEntry_t& p, PageContext_t& c);
    static void WebCfgADC(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
#endif
    void ConfigChanged(OvmsConfigParam* param) override;
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);
    uint64_t swap_uint64(uint64_t val);

  public:
    static void xsq_trip_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_trip_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_maintenance(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_trip_counters(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_trip_total(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_tpms_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_ddt4all(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_ddt4list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_calc_adc(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_wakeup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_preset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  private:
    int m_candata_timer;
    bool m_candata_poll;
    bool m_charge_start;
    bool m_charge_finished;
    float m_tpms_pressure[4]; // kPa
    float m_tpms_temperature[4]; // °C
    bool m_tpms_lowbatt[4]; // 0=ok, 1=low
    bool m_tpms_missing_tx[4]; // 0=ok, 1=missing
    bool m_ADCfactor_recalc;       // request recalculation of ADC factor
    int m_ADCfactor_recalc_timer;  // countdown timer for ADC factor recalculation

  public:
    bool UsesTpmsSensorMapping() override { return true; } // using m_tpms_index[]

  protected:
    void Ticker1(uint32_t ticker) override;
    void Ticker60(uint32_t ticker) override;
    void PollerStateTicker(canbus *bus) override;
    void GetDashboardConfig(DashboardConfig& cfg);
    virtual void CalculateEfficiency();
    void vehicle_smart_car_on(bool isOn);

    void PollReply_BMS_BattVolts(const char* data, uint16_t reply_len, uint16_t start);
    void PollReply_BMS_BattTemps(const char* data, uint16_t reply_len);
    void PollReply_BMS_BattState(const char* data, uint16_t reply_len);

    void PollReply_TDB(const char* data, uint16_t reply_len);

    void PollReply_BCM_VIN(const char* data, uint16_t reply_len);
    void PollReply_BCM_VehicleState(const char* data, uint16_t reply_len);
    void PollReply_BCM_DoorUnderhoodOpened(const char* data, uint16_t reply_len);
    void PollReply_BCM_TPMS_InputCapt(const char* data, uint16_t reply_len);
    void PollReply_BCM_TPMS_Status(const char* data, uint16_t reply_len);
    void PollReply_BCM_GenMode(const char* data, uint16_t reply_len);

    void PollReply_EVC_DCDC_ActReq(const char* data, uint16_t reply_len);
    void PollReply_EVC_HV_Energy(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_Load(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_VoltReq(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_Volt(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_Amps(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_Power(const char* data, uint16_t reply_len);
    void PollReply_EVC_USM14VVoltage(const char* data, uint16_t reply_len);
    void PollReply_EVC_14VBatteryVoltage(const char* data, uint16_t reply_len);
    void PollReply_EVC_14VBatteryVoltageReq(const char* data, uint16_t reply_len);
    void PollReply_EVC_CabinBlower(const char* data, uint16_t reply_len);

    void PollReply_OBL_ChargerAC(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph1_RMS_A(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph2_RMS_A(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph3_RMS_A(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph12_RMS_V(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph23_RMS_V(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph31_RMS_V(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Power(const char* data, uint16_t reply_len);    
    void PollReply_OBL_JB2AC_GroundResistance(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_LeakageDiag(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_DCCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_HF10kHz(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_HFCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_LFCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_MaxCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_PhaseFreq(const char* data, uint16_t reply_len);

    void PollReply_obd_trip(const char* data, uint16_t reply_len);
    void PollReply_obd_time(const char* data, uint16_t reply_len);
    void PollReply_obd_start_trip(const char* data, uint16_t reply_len);
    void PollReply_obd_start_time(const char* data, uint16_t reply_len);
    void PollReply_obd_mt_day(const char* data, uint16_t reply_len);
    void PollReply_obd_mt_km(const char* data, uint16_t reply_len);
    void PollReply_obd_mt_level(const char* data, uint16_t reply_len);
    
  protected:
    bool m_enable_write;                    // canwrite
    bool m_enable_LED_state;                // Online LED State
    bool m_enable_lock_state;               // Lock State
    bool m_enable_door_state;               // Door Open State
    bool m_tpms_temp_enable;                // TPMS Temperature Display enabled
    bool m_resettrip;                       // Reset Trip Values when Charging/Driving
    bool m_resettotal;                      // Reset kWh/100km Values when Driving
    bool m_tripnotify;                      // Trip Reset Notification on/off
    bool m_bcvalue;                         // use kWh/100km Value from mt_use_at_reset = true, Calculated = false
    int m_reboot_ticker;
    int m_reboot_time;                      // Restart Network time
    int m_TPMS_FL;                          // TPMS Sensor Front Left
    int m_TPMS_FR;                          // TPMS Sensor Front Right
    int m_TPMS_RL;                          // TPMS Sensor Rear Left
    int m_TPMS_RR;                          // TPMS Sensor Rear Right
    float m_front_pressure;                 // Front Tire Pressure
    float m_rear_pressure;                  // Rear Tire Pressure
    float m_pressure_warning;               // Pressure Warning
    float m_pressure_alert;                 // Pressure Alert
    bool m_tpms_alert_enable;               // TPMS Alert enabled
    bool m_12v_charge;                      //!< 12V charge on/off
    bool m_12v_charge_state;                //!< 12V charge state
    std::string m_hl_canbyte;               //!< canbyte variable for unv
    bool m_extendedStats;                   //!< extended stats for trip and maintenance data
    std::deque<float> m_adc_factor_history; // ring buffer (max 20) for ADC factors

    #define DEFAULT_BATTERY_CAPACITY 16700 // <- net 16700 Wh, gross 17600 Wh
    #define MAX_POLL_DATA_LEN 126
    #define CELLCOUNT 96
    #define SQ_CANDATA_TIMEOUT 15 // seconds until car goes to sleep without CAN activity

  protected:
    std::string   m_rxbuf;

  protected:
    OvmsCommand *cmd_xsq;                               // command for xsq
    OvmsMetricBool          *mt_bus_awake;              // Can Bus active
    OvmsMetricString        *mt_canbyte;                //!< DDT4all canbyte
    OvmsMetricFloat         *mt_adc_factor;             // calculated ADC factor for 12V measurement
    OvmsMetricVector<float> *mt_adc_factor_history;     // last 20 calculated ADC factors for 12V measurement
    OvmsMetricString        *mt_poll_state;             // Poller state
    OvmsMetricString        *mt_reset_time;             // Time since last reset (hh:mm)
    OvmsMetricString        *mt_start_time;             // Time since start (hh:mm)
    OvmsMetricFloat         *mt_start_distance;         // Trip distance since start (km)

    // 0x646 metrics
    OvmsMetricFloat         *mt_reset_consumption;      // Average trip consumption (kWh/100km) reset
    OvmsMetricFloat         *mt_reset_distance;         // Trip distance (km) reset
    OvmsMetricFloat         *mt_reset_energy;           // Trip energy consumption (kWh) reset
    OvmsMetricFloat         *mt_reset_speed;            // Average trip speed (km/h) reset
    // 0x658 metrics
    OvmsMetricString        *mt_bat_serial;             // Battery serial number (hex string)
    // 0x637 metrics
    OvmsMetricFloat         *mt_energy_used;            // Energy used since mission start (kWh)
    OvmsMetricFloat         *mt_energy_recd;            // Energy recovered since mission start (kWh)
    OvmsMetricFloat         *mt_aux_consumption;        // Auxiliary consumption since mission start (kWh)
    OvmsMetricFloat         *mt_total_recovery;         // Total energy recovery (kWh)
    // 0x62d metrics
    OvmsMetricFloat         *mt_worst_consumption;      // Worst average consumption (kWh/100km)
    OvmsMetricFloat         *mt_best_consumption;       // Best average consumption (kWh/100km)
    OvmsMetricFloat         *mt_bcb_power_mains;        // BCB power from mains (W)
    // 0x634 metrics
    OvmsMetricBool          *mt_tcu_refuse_sleep;       // TCU refuse to sleep status
    OvmsMetricString        *mt_tcu_refuse_timestamp;   // TCU refuse to sleep timestamp
    OvmsMetricInt           *mt_charging_timer_value;   // Charging timer value (min)
    OvmsMetricInt           *mt_charging_timer_status;  // Charging timer status
    OvmsMetricInt           *mt_charge_prohibited;      // Charge prohibited status
    OvmsMetricInt           *mt_charge_authorization;   // Charge authorization status
    OvmsMetricInt           *mt_ext_charge_manager;     // External charging manager

    OvmsMetricFloat         *mt_pos_odometer_trip;           // odometer trip in km 
    OvmsMetricFloat         *mt_pos_odometer_start;          // remind odometer start
    OvmsMetricFloat         *mt_pos_odometer_start_total;    // remind odometer start for kWh/100km
    OvmsMetricFloat         *mt_pos_odometer_trip_total;     // counted km for kWh/100km

    OvmsMetricFloat         *mt_evc_hv_energy;          //!< available energy in kWh
    OvmsMetricBool          *mt_evc_LV_DCDC_act_req;    //!< indicates if DC/DC LV system is active, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_DCDC_amps;       //!< current of DC/DC LV system, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_DCDC_load;       //!< load in % of DC/DC LV system, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_DCDC_volt_req;   //!< voltage request in V of DC/DC LV system, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_DCDC_volt;       //!< voltage in V of DC/DC output of LV system, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_DCDC_power;      //!< power in W (x/10) of DC/DC output of LV system, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_USM_volt;        //!< USM 14V voltage (CAN)
    OvmsMetricFloat         *mt_evc_LV_batt_voltage_can; //!< 14V battery voltage (CAN)
    OvmsMetricFloat         *mt_evc_LV_batt_voltage_req; //!< Internal requested 14V

    OvmsMetricVector<float> *mt_bms_temps;              // BMS temperatures
    OvmsMetricFloat         *mt_bms_CV_Range_min;       //!< minimum cell voltage in V, no offset
    OvmsMetricFloat         *mt_bms_CV_Range_max;       //!< maximum cell voltage in V, no offset
    OvmsMetricFloat         *mt_bms_CV_Range_mean;      //!< average cell voltage in V, no offset
    OvmsMetricFloat         *mt_bms_BattLinkVoltage;    //!< Link voltage to drivetrain inverter
    OvmsMetricFloat         *mt_bms_BattContactorVoltage;   //!< voltage at the battery contactor
    OvmsMetricFloat         *mt_bms_BattCV_Sum;         //!< Sum of single cell voltages
    OvmsMetricFloat         *mt_bms_BattPower_power;    //!< calculated power of sample in kW
    OvmsMetricInt           *mt_bms_HVcontactStateCode; //!< contactor state: 0 := OFF, 1 := PRECHARGE, 2 := ON
    OvmsMetricString        *mt_bms_HVcontactStateTXT;  //!< contactor state text
    OvmsMetricFloat         *mt_bms_HV;                 //!< total voltage of HV system in V
    OvmsMetricInt           *mt_bms_EVmode;             //!< Mode the EV is actually in: 0 = none, 1 = slow charge, 2 = fast charge, 3 = normal, 4 = Quick Drop, 5 = Cameleon (Non-Isolated Charging)
    OvmsMetricString        *mt_bms_EVmode_txt;         //!< Mode the EV is actually in text
    OvmsMetricFloat         *mt_bms_12v;                //!< 12V onboard voltage / Clamp 30 Voltage
    OvmsMetricBool          *mt_bms_interlock_hvplug;       //!< HV plug interlock
    OvmsMetricBool          *mt_bms_interlock_service;      //!< Service disconnect interlock
    OvmsMetricInt           *mt_bms_fusi_mode;              //!< FUSI mode code
    OvmsMetricString        *mt_bms_fusi_mode_txt;          //!< FUSI mode text
    OvmsMetricFloat         *mt_bms_mg_rpm;                 //!< MG output revolution (rpm/2)
    OvmsMetricInt           *mt_bms_safety_mode;            //!< Safety mode code
    OvmsMetricString        *mt_bms_safety_mode_txt;        //!< Safety mode text

    OvmsMetricVector<float> *mt_obl_main_amps;          //!< AC current of L1, L2, L3
    OvmsMetricVector<float> *mt_obl_main_volts;         //!< AC voltage of L1, L2, L3
    OvmsMetricVector<float> *mt_obl_main_CHGpower;      //!< Power of rail1, rail2 W (x/2) & max available kw (x/64)    
    OvmsMetricBool          *mt_obl_fastchg;            // 22kw fast charge enabled
    OvmsMetricFloat         *mt_obl_main_freq;          //!< AC input frequency
    OvmsMetricFloat         *mt_obl_main_ground_resistance;           //!< Ground resistance in (Ohm)
    OvmsMetricInt           *mt_obl_main_max_current;                 //!< Charger max current setting (A)
    OvmsMetricString        *mt_obl_main_leakage_diag;                //!< Leakage diagnostic
    OvmsMetricFloat         *mt_obl_main_current_leakage_dc;          //!< DC leakage current (A)
    OvmsMetricFloat         *mt_obl_main_current_leakage_hf_10khz;    //!< HF 10kHz leakage (A)
    OvmsMetricFloat         *mt_obl_main_current_leakage_hf;          //!< HF leakage current (A)
    OvmsMetricFloat         *mt_obl_main_current_leakage_lf;          //!< LF leakage current (A)
    OvmsMetricFloat         *mt_obl_main_current_leakage_ac;          //!< AC leakage current (A)

    OvmsMetricInt           *mt_obd_duration;           //!< obd duration
    OvmsMetricInt           *mt_obd_mt_day_prewarn;     //!< Maintaince pre warning days
    OvmsMetricInt           *mt_obd_mt_day_usual;       //!< Maintaince usual days
    OvmsMetricInt           *mt_obd_mt_km_usual;        //!< Maintaince usual km
    OvmsMetricString        *mt_obd_mt_level;           //!< Maintaince level
 
    OvmsMetricVector<float> *mt_tpms_temp;              // 4 wheel temperatures (°C)
    OvmsMetricVector<float> *mt_tpms_pressure;          // 4 wheel pressures (kPa)
    OvmsMetricVector<short> *mt_tpms_alert;             // 4 wheel alert flags (0=ok, 1=warning, 2=alert)
    OvmsMetricVector<short>  *mt_tpms_low_batt;          // 4 wheel low battery flags (0=ok, 1=low)
    OvmsMetricVector<short>  *mt_tpms_missing_tx;        // 4 wheel missing transmitter flags (0=ok, 1=missing)    
    OvmsMetricFloat         *mt_dummy_pressure;         //!< Dummy pressure for TPMS

    OvmsMetricString        *mt_bcm_vehicle_state;      //!< vehicle state
    OvmsMetricString        *mt_bcm_gen_mode;           //!< Generator mode text
    
  protected:
    bool m_indicator;                       //!< activate indicator e.g. 7 times or whatever
    bool m_ddt4all;                         //!< DDT4ALL mode
    bool m_warning_unlocked;                //!< unlocked warning
    bool m_warning_dooropen;                //!< open doors warning
    bool m_modem_check;                     //!< modem check enabled
    bool m_modem_restart;                   //!< modem restart enabled
    bool m_notifySOClimit;                  //!< notify SOClimit reached one time
    bool m_enable_calcADCfactor;            //!< enable calculation of ADC factor
    int m_adc_samples;                      //!< number of samples for ADC factor calculation
    int m_ddt4all_ticker;                   //!< DDT4ALL active ticker 
    int m_ddt4all_exec;                     //!< DDT4ALL ticker for next execution
    int m_led_state;
    int m_12v_ticker;
    int m_modem_ticker;
    int m_park_timeout_secs;                //!< parking timeout in seconds
    int m_full_km;                          //!< full battery km value for SoC calculation
    int m_cfg_preset_version;               //!< config preset version set in CommandPreset by defined PRESET_VERSION in top of this file
    int m_suffsoc;                          //!< minimum SoC for charging
    int m_suffrange;                        //!< minimum range for charging

  protected:
    poll_vector_t       m_poll_vector;              // List of PIDs to poll
    int                 m_cfg_cell_interval_drv;    // Cell poll interval while driving, default 15 sec.
    int                 m_cfg_cell_interval_chg;    // … while charging, default 60 sec.
};

#endif //#ifndef __VEHICLE_SMARTED_H__
