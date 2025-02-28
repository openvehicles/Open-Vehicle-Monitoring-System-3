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
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b+1) << 16)  | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (data[b] & 0x0f)
#define CAN_NIBH(b)     (data[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))

using namespace std;

typedef std::vector<OvmsPoller::poll_pid_t, ExtRamAllocator<OvmsPoller::poll_pid_t>> poll_vector_t;
typedef std::initializer_list<const OvmsPoller::poll_pid_t> poll_list_t;

class OvmsVehicleSmartEQ : public OvmsVehicle
{
  public:
    OvmsVehicleSmartEQ();
    ~OvmsVehicleSmartEQ();

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
    void GPSOnOff();
    void TimeBasedClimateData();
    void TimeBasedClimateDataApp();
    void CheckV2State();
    void DisablePlugin(const char* plugin);
    void ModemNetworkType();
    bool ExecuteCommand(const std::string& command);
    void ResetOldValues();

public:
    vehicle_command_t CommandClimateControl(bool enable) override;
    vehicle_command_t CommandHomelink(int button, int durationms=1000) override;
    vehicle_command_t CommandWakeup() override;
    vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer) override;
    vehicle_command_t CommandLock(const char* pin) override;
    vehicle_command_t CommandUnlock(const char* pin) override;
    vehicle_command_t CommandActivateValet(const char* pin) override;
    vehicle_command_t CommandDeactivateValet(const char* pin) override;
    vehicle_command_t CommandCan(uint32_t txid,uint32_t rxid,bool enable=false);
    vehicle_command_t CommandWakeup2();

public:
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgClimate(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
#endif
    void ConfigChanged(OvmsConfigParam* param) override;
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);
    uint64_t swap_uint64(uint64_t val);

  private:
    unsigned int m_candata_timer;
    unsigned int m_candata_poll;
    bool m_charge_start;
    bool m_charge_finished;

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
    void PollReply_ocs_trip(const char* data, uint16_t reply_len);
    void PollReply_ocs_time(const char* data, uint16_t reply_len);
    void PollReply_ocs_mt_day(const char* data, uint16_t reply_len);
    void PollReply_ocs_mt_km(const char* data, uint16_t reply_len);
    void PollReply_ocs_mt_level(const char* data, uint16_t reply_len);

  protected:
    bool m_enable_write;                    // canwrite
    bool m_enable_LED_state;                // Online LED State
    bool m_ios_tpms_fix;                    // IOS TPMS Display Fix
    bool m_resettrip;                       // Reset Trip Values when Charging/Driving
    bool m_resettotal;                      // Reset kWh/100km Values when Driving
    bool m_bcvalue;                         // use kWh/100km Value from mt_use_at_reset = true, Calculated = false
    int m_reboot_ticker;
    int m_reboot_time;                      // Restart Network time
    int m_TPMS_FL;                          // TPMS Sensor Front Left
    int m_TPMS_FR;                          // TPMS Sensor Front Right
    int m_TPMS_RL;                          // TPMS Sensor Rear Left
    int m_TPMS_RR;                          // TPMS Sensor Rear Right
    bool m_12v_charge;                      //!< 12V charge on/off
    bool m_booster_system;                  //!< booster system on/off
    bool m_gps_onoff;                       //!< GPS on/off at parking activated
    bool m_gps_off;                         //!< GPS off while parking > 10 minutes
    int m_gps_reactmin;                     //!< GPS reactivate all x minutes after parking
    std::string m_hl_canbyte;                 //!< homelink canbyte
    std::string m_network_type;               //!< Network type from xsq.modem.net.type
    std::string m_network_type_ls;            //!< Network type last state reminder

    #define DEFAULT_BATTERY_CAPACITY 17600
    #define MAX_POLL_DATA_LEN 126
    #define CELLCOUNT 96
    #define SQ_CANDATA_TIMEOUT 10

  protected:
    std::string   m_rxbuf;

  protected:
    OvmsMetricVector<float> *mt_bms_temps;              // BMS temperatures
    OvmsMetricBool          *mt_bus_awake;              // Can Bus active
    OvmsMetricFloat         *mt_use_at_reset;           // kWh use at reset in Display
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
    OvmsMetricInt           *mt_ocs_duration;           //!< OCS duration
    OvmsMetricFloat         *mt_ocs_trip_km;            //!< OCS trip data km
    OvmsMetricString        *mt_ocs_trip_time;          //!< OCS trip data HH:mm
    OvmsMetricInt           *mt_ocs_mt_day_prewarn;     //!< Maintaince pre warning days
    OvmsMetricInt           *mt_ocs_mt_day_usual;       //!< Maintaince usual days
    OvmsMetricInt           *mt_ocs_mt_km_usual;        //!< Maintaince usual km
    OvmsMetricString        *mt_ocs_mt_level;           //!< Maintaince level
    OvmsMetricBool          *mt_booster_on;             //!< booster at time on/off
    OvmsMetricBool          *mt_booster_weekly;         //!< booster weekly auto on/off at day start/end
    OvmsMetricString        *mt_booster_time;           //!< booster time
    OvmsMetricInt           *mt_booster_h;              //!< booster time hour
    OvmsMetricInt           *mt_booster_m;              //!< booster time minute
    OvmsMetricInt           *mt_booster_ds;             //!< booster day start
    OvmsMetricInt           *mt_booster_de;             //!< booster day end
    OvmsMetricInt           *mt_booster_1to3;           //!< booster one to three (homelink 0-2) times in following time
    OvmsMetricString        *mt_booster_data;           //!< booster data from app/website

  protected:
    bool m_booster_start;
    bool m_booster_start_day;
    bool m_booster_init;                    //!< booster init after boot
    bool m_v2_restart;
    bool m_v2_check;
    bool m_indicator;                       //!< activate indicator e.g. 7 times or whtever
    bool m_ddt4all;                           //!< DDT4ALL mode
    int m_led_state;
    int m_booster_ticker;
    int m_gps_ticker;
    int m_12v_ticker;
    int m_v2_ticker;
  
  protected:
    poll_vector_t       m_poll_vector;              // List of PIDs to poll
    int                 m_cfg_cell_interval_drv;    // Cell poll interval while driving, default 15 sec.
    int                 m_cfg_cell_interval_chg;    // … while charging, default 60 sec.
};

#endif //#ifndef __VEHICLE_SMARTED_H__
