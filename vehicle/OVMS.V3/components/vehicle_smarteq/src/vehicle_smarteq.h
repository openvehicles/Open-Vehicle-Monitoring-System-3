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
 */

#ifndef __VEHICLE_SMARTEQ_H__
#define __VEHICLE_SMARTEQ_H__

#include <atomic>
#include <stdint.h>
#include "can.h"
#include "vehicle.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "freertos/timers.h"
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
    void TimeCheckTask();
    void Check12vState();
    void TimeBasedClimateData();
    void CheckV2State();
    void DisablePlugin(const char* plugin);
    void ModemNetworkType();
    bool ExecuteCommand(const std::string& command);
    void setTPMSValue(int index, int indexcar);
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
    void WifiRestart();
    void ModemRestart();
    void ModemEventRestart(std::string event, void* data);

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
    virtual vehicle_command_t CommandSetClimate(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandTripCounters(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandTripTotal(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandClimate(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t Command12Vcharge(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandTPMSset(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandDDT4all(int number, OvmsWriter* writer);
    virtual vehicle_command_t CommandDDT4List(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandSOClimit(int verbosity, OvmsWriter* writer);

public:
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgClimate(PageEntry_t& p, PageContext_t& c);
    static void WebCfgTPMS(PageEntry_t& p, PageContext_t& c);
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
    static void xsq_climate(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_trip_counters(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_trip_total(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_tpms_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_ddt4all(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xsq_ddt4list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  private:
    unsigned int m_candata_timer;
    unsigned int m_candata_poll;
    bool m_charge_start;
    bool m_charge_finished;
    float m_tpms_pressure[4]; // kPa
    int m_tpms_index[4];

  protected:
    void Ticker1(uint32_t ticker) override;
    void PollerStateTicker(canbus *bus) override;
    void GetDashboardConfig(DashboardConfig& cfg);
    virtual void CalculateEfficiency();
    void vehicle_smart_car_on(bool isOn);
    
    void PollReply_BMS_BattVolts(const char* data, uint16_t reply_len, uint16_t start);
    void PollReply_BMS_BattTemps(const char* data, uint16_t reply_len);
    void PollReply_BMS_BattState(const char* data, uint16_t reply_len);
    void PollReply_HVAC(const char* data, uint16_t reply_len);
    void PollReply_TDB(const char* data, uint16_t reply_len);
    void PollReply_VIN(const char* data, uint16_t reply_len);
    void PollReply_EVC_HV_Energy(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_State(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_Load(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_Volt(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_Amps(const char* data, uint16_t reply_len);
    void PollReply_EVC_DCDC_Power(const char* data, uint16_t reply_len);
    void PollReply_EVC_ext_power(const char* data, uint16_t reply_len);
    void PollReply_EVC_plug_present(const char* data, uint16_t reply_len);
    void PollReply_OBL_ChargerAC(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph1_RMS_A(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph2_RMS_A(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph3_RMS_A(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph12_RMS_V(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph23_RMS_V(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Ph31_RMS_V(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Power(const char* data, uint16_t reply_len);
    void PollReply_obd_trip(const char* data, uint16_t reply_len);
    void PollReply_obd_time(const char* data, uint16_t reply_len);
    void PollReply_obd_start_trip(const char* data, uint16_t reply_len);
    void PollReply_obd_start_time(const char* data, uint16_t reply_len);
    void PollReply_obd_mt_day(const char* data, uint16_t reply_len);
    void PollReply_obd_mt_km(const char* data, uint16_t reply_len);
    void PollReply_obd_mt_level(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_GroundResistance(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_LeakageDiag(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_DCCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_HF10kHz(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_HFCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_LFCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_MaxCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_CurrentSum(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_VoltageSum(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_HVNetCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_HVVoltageSum(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_RawDCCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_RawHF10kHz(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_RawHFCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_RawLFCurrent(const char* data, uint16_t reply_len);
    void PollReply_OBL_JB2AC_Frequency(const char* data, uint16_t reply_len);

  protected:
    bool m_enable_write;                    // canwrite
    bool m_enable_LED_state;                // Online LED State
    bool m_enable_lock_state;               // Lock State
    bool m_ios_tpms_fix;                    // IOS TPMS Display Fix
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
    bool m_climate_system;                  //!< climate system on/off
    std::string m_hl_canbyte;               //!< canbyte variable for unv
    std::string m_network_type;             //!< Network type from xsq.modem.net.type
    std::string m_network_type_ls;          //!< Network type last state reminder
    bool m_extendedStats;                   //!< extended stats for trip and maintenance data

    #define DEFAULT_BATTERY_CAPACITY 17600
    #define MAX_POLL_DATA_LEN 126
    #define CELLCOUNT 96
    #define SQ_CANDATA_TIMEOUT 10

  protected:
    std::string   m_rxbuf;

  protected:
    OvmsCommand *cmd_xsq;                               // command for xsq
    OvmsMetricVector<float> *mt_bms_temps;              // BMS temperatures
    OvmsMetricBool          *mt_bus_awake;              // Can Bus active
    OvmsMetricFloat         *mt_use_at_reset;           // kWh use at reset in Display
    OvmsMetricFloat         *mt_use_at_start;           // kWh use at start in Display
    OvmsMetricFloat         *mt_pos_odo_trip;           // odometer trip in km 
    OvmsMetricFloat         *mt_pos_odometer_start;     // remind odometer start
    OvmsMetricFloat         *mt_pos_odometer_start_total;     // remind odometer start for kWh/100km
    OvmsMetricFloat         *mt_pos_odometer_trip_total;// counted km for kWh/100km
    OvmsMetricBool          *mt_obl_fastchg;            // ODOmeter at Start
    OvmsMetricFloat         *mt_evc_hv_energy;          //!< available energy in kWh
    OvmsMetricFloat         *mt_evc_LV_DCDC_amps;       //!< current of DC/DC LV system, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_DCDC_load;       //!< load in % of DC/DC LV system, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_DCDC_volt;       //!< voltage in V of DC/DC output of LV system, not 12V battery!
    OvmsMetricFloat         *mt_evc_LV_DCDC_power;      //!< power in W (x/10) of DC/DC output of LV system, not 12V battery!
    OvmsMetricInt           *mt_evc_LV_DCDC_state;      //!< DC/DC state
    OvmsMetricBool          *mt_evc_ext_power;          //!< indicates ext power supply
    OvmsMetricBool          *mt_evc_plug_present;       //!< charging plug present
    OvmsMetricFloat         *mt_bms_CV_Range_min;       //!< minimum cell voltage in V, no offset
    OvmsMetricFloat         *mt_bms_CV_Range_max;       //!< maximum cell voltage in V, no offset
    OvmsMetricFloat         *mt_bms_CV_Range_mean;      //!< average cell voltage in V, no offset
    OvmsMetricFloat         *mt_bms_BattLinkVoltage;    //!< Link voltage to drivetrain inverter
    OvmsMetricFloat         *mt_bms_BattCV_Sum;         //!< Sum of single cell voltages
    OvmsMetricFloat         *mt_bms_BattPower_voltage;  //!< voltage value sample (raw), (x/64) for actual value
    OvmsMetricFloat         *mt_bms_BattPower_current;  //!< current value sample (raw), (x/32) for actual value
    OvmsMetricFloat         *mt_bms_BattPower_power;    //!< calculated power of sample in kW
    OvmsMetricInt           *mt_bms_HVcontactState;     //!< contactor state: 0 := OFF, 1 := PRECHARGE, 2 := ON
    OvmsMetricFloat         *mt_bms_HV;                 //!< total voltage of HV system in V
    OvmsMetricInt           *mt_bms_EVmode;             //!< Mode the EV is actually in: 0 = none, 1 = slow charge, 2 = fast charge, 3 = normal, 4 = fast balance
    OvmsMetricFloat         *mt_bms_LV;                 //!< 12V onboard voltage / LV system
    OvmsMetricFloat         *mt_bms_Amps;               //!< battery current in ampere (x/32) reported by by BMS
    OvmsMetricFloat         *mt_bms_Amps2;              //!< battery current in ampere read by live data on CAN or from BMS
    OvmsMetricFloat         *mt_bms_Power;              //!< power as product of voltage and amps in kW
    OvmsMetricVector<float> *mt_obl_main_amps;          //!< AC current of L1, L2, L3
    OvmsMetricVector<float> *mt_obl_main_volts;         //!< AC voltage of L1, L2, L3
    OvmsMetricVector<float> *mt_obl_main_CHGpower;      //!< Power of rail1, rail2 W (x/2) & max available kw (x/64)
    OvmsMetricFloat         *mt_obl_main_freq;          //!< AC input frequency
    OvmsMetricFloat         *mt_obl_main_ground_resistance;           //!< Ground resistance in (Ohm)
    OvmsMetricInt           *mt_obl_main_max_current;                 //!< Charger max current setting (A)
    OvmsMetricString        *mt_obl_main_leakage_diag;                //!< Leakage diagnostic
    OvmsMetricFloat         *mt_obl_main_current_leakage_dc;          //!< DC leakage current (A)
    OvmsMetricFloat         *mt_obl_main_current_leakage_hf_10khz;    //!< HF 10kHz leakage (A)
    OvmsMetricFloat         *mt_obl_main_current_leakage_hf;          //!< HF leakage current (A)
    OvmsMetricFloat         *mt_obl_main_current_leakage_lf;          //!< LF leakage current (A)
    OvmsMetricFloat         *mt_obl_main_hv_net_amps;                 //!< Net current (A)
    OvmsMetricFloat         *mt_obl_main_hv_net_volts;                //!< Net voltage (V)
    OvmsMetricFloat         *mt_obl_main_amps_sum;                    //!< Current sum (A)
    OvmsMetricFloat         *mt_obl_main_volts_sum;                   //!< Voltage sum (V)
    OvmsMetricFloat         *mt_obl_main_hv_volts_sum;                //!< HV Voltage sum (V)
    OvmsMetricFloat         *mt_obl_main_current_leakage_ac;          //!< AC leakage current (A)
    OvmsMetricFloat         *mt_obl_main_current_leakage_dc_raw;      //!< DC leakage current raw value
    OvmsMetricFloat         *mt_obl_main_current_leakage_hf_10khz_raw;//!< HF 10kHz leakage raw value
    OvmsMetricFloat         *mt_obl_main_current_leakage_hf_raw;     //!< HF leakage current raw value
    OvmsMetricFloat         *mt_obl_main_current_leakage_lf_raw;     //!< LF leakage current raw value
    OvmsMetricInt           *mt_obd_duration;           //!< obd duration
    OvmsMetricFloat         *mt_obd_trip_km;            //!< obd trip data km
    OvmsMetricFloat         *mt_obd_start_trip_km;      //!< obd trip data km start
    OvmsMetricString        *mt_obd_trip_time;          //!< obd trip data HH:mm
    OvmsMetricString        *mt_obd_start_trip_time;    //!< obd trip data HH:mm start
    OvmsMetricInt           *mt_obd_mt_day_prewarn;     //!< Maintaince pre warning days
    OvmsMetricInt           *mt_obd_mt_day_usual;       //!< Maintaince usual days
    OvmsMetricInt           *mt_obd_mt_km_usual;        //!< Maintaince usual km
    OvmsMetricString        *mt_obd_mt_level;           //!< Maintaince level
    OvmsMetricBool          *mt_climate_on;             //!< climate at time on/off
    OvmsMetricBool          *mt_climate_weekly;         //!< climate weekly auto on/off at day start/end
    OvmsMetricString        *mt_climate_time;           //!< climate time
    OvmsMetricInt           *mt_climate_h;              //!< climate time hour
    OvmsMetricInt           *mt_climate_m;              //!< climate time minute
    OvmsMetricInt           *mt_climate_ds;             //!< climate day start
    OvmsMetricInt           *mt_climate_de;             //!< climate day end
    OvmsMetricInt           *mt_climate_1to3;           //!< climate one to three (homelink 0-2) times in following time
    OvmsMetricString        *mt_climate_data;           //!< climate data from app/website
    OvmsMetricString        *mt_canbyte;                //!< DDT4all canbyte
    OvmsMetricFloat         *mt_dummy_pressure;         //!< Dummy pressure for TPMS

  protected:
    bool m_climate_start;
    bool m_climate_start_day;
    bool m_climate_init;                    //!< climate init after boot
    bool m_v2_restart;
    bool m_v2_check;
    bool m_indicator;                       //!< activate indicator e.g. 7 times or whtever
    bool m_ddt4all;                         //!< DDT4ALL mode
    bool m_warning_unlocked;                //!< unlocked warning
    bool m_modem_check;                     //!< modem check enabled
    bool m_modem_restart;                   //!< modem restart enabled
    bool m_notifySOClimit;                  //!< notify SOClimit reached one time
    int m_ddt4all_ticker;                   //!< DDT4ALL active ticker 
    int m_ddt4all_exec;                     //!< DDT4ALL ticker for next execution
    int m_led_state;
    int m_climate_ticker;
    int m_12v_ticker;
    int m_v2_ticker;
    int m_modem_ticker;
    int m_park_timeout_secs;                //!< parking timeout in seconds
  
  protected:
    poll_vector_t       m_poll_vector;              // List of PIDs to poll
    int                 m_cfg_cell_interval_drv;    // Cell poll interval while driving, default 15 sec.
    int                 m_cfg_cell_interval_chg;    // … while charging, default 60 sec.
};

#endif //#ifndef __VEHICLE_SMARTED_H__
