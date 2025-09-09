/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th September 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Tom Parker
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

#ifndef __VEHICLE_NISSANLEAF_H__
#define __VEHICLE_NISSANLEAF_H__

#include <iostream>
#include <algorithm>
#include "freertos/timers.h"
#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif
#include "ovms_semaphore.h"
#include "ovms_mutex.h"
#include "ovms_peripherals.h"
#include "nl_types.h"

#define DEFAULT_MODEL_YEAR 2012
#define DEFAULT_PIN_EV 1
#define DEFAULT_CABINTEMP_OFFSET .0
#define DEFAULT_SUFF_RANGE_CALC "ideal"
#define DEFAULT_RANGEDROP 0
#define DEFAULT_SOCDROP 0
#define DEFAULT_AUTOCHARGE_ENABLED true
#define DEFAULT_SPEED_DIVISOR 98.0
#define GEN_1_NEW_CAR_GIDS 281
#define GEN_1_NEW_CAR_AH 66
#define GEN_1_KM_PER_KWH 7.1
#define GEN_1_WH_PER_GID 80
#define GEN_1_30_NEW_CAR_GIDS 356
#define GEN_1_30_NEW_CAR_AH 79
#define GEN_2_40_NEW_CAR_GIDS 502
#define GEN_2_40_NEW_CAR_AH 115
#define GEN_2_62_NEW_CAR_GIDS 775
#define GEN_2_62_NEW_CAR_AH 176
#define CMD_QueryChargeAlerts 203 // ()
#define CMD_SetChargeAlerts 204 // (range, soc)
#define REMOTE_COMMAND_REPEAT_COUNT 24 // number of times to send the remote command after the first time
#define ACTIVATION_REQUEST_TIME 10 // tenths of a second to hold activation request signal

using namespace std;

typedef enum
  {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  START_CHARGING,
  STOP_CHARGING,
  AUTO_DISABLE_CLIMATE_CONTROL,
  UNLOCK_DOORS,
  LOCK_DOORS
  } RemoteCommand;

typedef enum
  {
  CAN_READ_ONLY,
  INVALID_SYNTAX,
  COMMAND_RESULT_OK
  } CommandResult;

typedef enum
  {
  CHARGER_STATUS_IDLE,
  CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT,
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_QUICK_CHARGING,
  CHARGER_STATUS_FINISHED,
  CHARGER_STATUS_INTERRUPTED,
  CHARGER_STATUS_V2X
  } ChargerStatus;

typedef enum
  {
  NORMAL,
  CAPACITY_DROP,
  LBC_MALFUNCTION,
  HIGH_TEMP,
  LOW_TEMP
  } PowerLimitStates;

class OvmsVehicleNissanLeaf : public OvmsVehicle
  {
  public:
    OvmsVehicleNissanLeaf();
    ~OvmsVehicleNissanLeaf();

  public:
    static OvmsVehicleNissanLeaf* GetInstance(OvmsWriter* writer=NULL);
    void ConfigChanged(OvmsConfigParam* param) override;
    bool SetFeature(int key, const char* value);
    vehicle_command_t ProcessMsgCommand(std::string &result, int command, const char* args);
    vehicle_command_t MsgCommandCA(std::string &result, int command, const char* args);
    const std::string GetFeature(int key);

  public:
    bool ObdRequest(uint16_t txid, uint16_t rxid, uint32_t request, string& response, int timeout_ms /*=3000*/, uint8_t bus);
    void IncomingFrameCan1(CAN_frame_t* p_frame) override;
    void IncomingFrameCan2(CAN_frame_t* p_frame) override;
    void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
    vehicle_command_t CommandHomelink(int button, int durationms=1000) override;
    vehicle_command_t CommandClimateControl(bool enable) override;
    vehicle_command_t CommandLock(const char* pin) override;
    vehicle_command_t CommandUnlock(const char* pin) override;
    vehicle_command_t CommandWakeup() override;
    void RemoteCommandTimer();
    void CcDisableTimer();
    void MITMDisableTimer();
    void CommandWakeupTCU(); // New wakeup command

  // --------------------------------------------------------------------------
  // Webserver subsystem
  //  - implementation: nl_web.(h,cpp)
  //

  public:
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
#endif
    static void shell_obd_request(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  public:
    void GetDashboardConfig(DashboardConfig& cfg);

  private:
    void CommandInit(); // initialise shell commands specific to Leaf
    void vehicle_nissanleaf_car_on(bool isOn);
    void vehicle_nissanleaf_charger_status(ChargerStatus status);
    void SendCanMessage(uint16_t id, uint8_t length, uint8_t *data);
    void Ticker1(uint32_t ticker) override;
    void Ticker10(uint32_t ticker) override;
    void HandleEnergy();
    void HandleCharging();
    void HandleChargeEstimation();
    void HandleExporting();
    void HandleRange();
    int  calcMinutesRemaining(float target, float charge_power_w);
    void SendCommand(RemoteCommand);
    OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);
    OvmsVehicle::vehicle_command_t CommandStartCharge() override;
    OvmsVehicle::vehicle_command_t CommandStopCharge() override;
    virtual int GetNotifyChargeStateDelay(const char* state);
    RemoteCommand nl_remote_command; // command to send, see RemoteCommandTimer()
    uint8_t nl_remote_command_ticker; // number remaining remote command frames to send
    void PollReply_Battery(const uint8_t *reply_data, uint16_t reply_len);
    void PollReply_QC(const uint8_t *reply_data, uint16_t reply_len);
    void PollReply_L0L1L2(const uint8_t *reply_data, uint16_t reply_len);
    void PollReply_VIN(const uint8_t *reply_data, uint16_t reply_len);
    void PollReply_BMS_Volt(const uint8_t *reply_data, uint16_t reply_len);
    void PollReply_BMS_Shunt(const uint8_t *reply_data, uint16_t reply_len);
    void PollReply_BMS_Temp(const uint8_t *reply_data, uint16_t reply_len);
    void PollReply_BMS_SOH(const uint8_t *reply_data, uint16_t reply_len);
    void ResetTripCounters();
    void UpdateTripCounters();

    TimerHandle_t m_remoteCommandTimer;
    TimerHandle_t m_ccDisableTimer;
    TimerHandle_t m_MITMstop;
    metric_unit_t m_odometer_units = Other;
    OvmsMetricInt *m_gids;
    OvmsMetricInt *m_max_gids;
    OvmsMetricFloat *m_hx;
    OvmsMetricFloat *m_soc_new_car;
    OvmsMetricFloat *m_soc_instrument;
    OvmsMetricInt *m_range_instrument;
    OvmsMetricVector<int> *m_bms_thermistor;
    OvmsMetricVector<int> *m_bms_temp_int;
    OvmsMetricBitset<96> *m_bms_balancing;
    /// @brief State of health - calculated
    /// @note ah / new car ah * 100
    OvmsMetricFloat *m_soh_new_car;
    /// @brief State of health - read from BMS
    OvmsMetricFloat *m_soh_instrument;
    OvmsMetricFloat *m_battery_energy_capacity;
    OvmsMetricFloat *m_battery_energy_available;
    OvmsMetricInt *m_battery_type;
    OvmsMetricBool *m_battery_heaterpresent;
    OvmsMetricBool *m_battery_heatrequested;
    OvmsMetricBool *m_battery_heatergranted;
    OvmsMetricFloat *m_battery_out_power_limit;
    OvmsMetricFloat *m_battery_in_power_limit;
    OvmsMetricFloat *m_battery_chargerate_max;
    OvmsMetricString *m_charge_limit;
    OvmsMetricVector<int> *m_charge_duration;
    OvmsMetricVector<string> *m_charge_duration_label;
    OvmsMetricInt *m_charge_minutes_3kW_remaining;
    OvmsMetricInt *m_remaining_chargebars;
    OvmsMetricInt *m_capacitybars;
    OvmsMetricInt *m_quick_charge;
    OvmsMetricString *m_charge_state_previous;
    OvmsMetricString *m_charge_user_notified;           // For sending autocharge notifications only after charge status has changed
    OvmsMetricString *m_charge_event_reason;
    OvmsMetricFloat *m_soc_nominal;
    OvmsMetricInt *m_charge_count_qc;
    OvmsMetricInt *m_charge_count_l0l1l2;
    OvmsMetricBool *m_climate_fan_only;
    OvmsMetricBool *m_climate_remoteheat;
    OvmsMetricBool *m_climate_remotecool;
    OvmsMetricBool *m_climate_rqinprogress;
    OvmsMetricString *m_climate_vent;
    OvmsMetricString *m_climate_intake;
    OvmsMetricInt *m_climate_fan_speed;
    OvmsMetricInt *m_climate_fan_speed_limit;
    OvmsMetricFloat *m_climate_setpoint;
    OvmsMetricBool *m_climate_auto;
    OvmsMetricFloat  *mt_pos_odometer_start;  // ODOmeter at Start to count trips

    // Relay statuses
    OvmsMetricFloat *m_qc_relay_status;
    OvmsMetricFloat *m_ac_relay_status;
    OvmsMetricInt *m_charge_mode;

    int    cfg_ev_request_port = DEFAULT_PIN_EV;        // EGPIO port number for EV SYSTEM ACTIVATION REQUEST
    int    cfg_allowed_rangedrop;                       // Allowed drop of range after charging
    int    cfg_allowed_socdrop;                         // Allowed drop of SOC after charging
    bool   cfg_enable_write;                            // Enable/disable can write (polling and commands
    bool   cfg_enable_autocharge;                       // Enable/disable automatic charge control based on SOC or range
    bool   cfg_ze1;                                     // Enable/disable ZE1 specific features
    bool   cfg_soh_newcar;                              // True if SOH is calculated from new car max ah, false if from BMS
    string cfg_limit_range_calc;                        // What range calc to use for charge to range feature
    float  cfg_speed_divisor;                           // Divisor used for dividing raw speed value received from can1 0x284 msg


    int         m_MITM = 0;
    double      m_trip_odo;                             // trip distance estimated from speed
    uint32_t    m_trip_last_upd_time;                  // timestamp as of last trip counter update
    float       m_trip_last_upd_speed;                 // speed as of last trip counter update
    float       m_cum_energy_used_wh;                   // Cumulated energy (in wh) used within 1 second ticker interval
    float       m_cum_energy_recd_wh;                   // Cumulated energy (in wh) recovered  within 1 second ticker interval
    float       m_cum_energy_charge_wh;                 // Cumulated energy (in wh) charged within 10 second ticker interval
    float       m_cum_energy_gen_wh;                    // Cumulated energy (in wh) exported within 10 second ticker interval
    bool        m_ZE0_charger;                          // True if 2011-2012 ZE0 LEAF with 0x380 message (Gen 1)
    bool        m_AZE0_charger;                         // True if 2013+ AZE0 LEAF with 0x390 message (Gen 2)
    bool        m_climate_really_off;                   // Needed for AZE0 to shown correct hvac status while charging
    bool        m_kWh_capacity_read;                    // m_battery_energy_capacity Ah*V fallback inhibitor

    OvmsPoller::poll_pid_t* obdii_polls;

  protected:
    OvmsCommand*        cmd_xnl;
    string              nl_obd_rxbuf;
    OvmsMutex           nl_obd_request;
    OvmsSemaphore       nl_obd_rxwait;
  };

#endif //#ifndef __VEHICLE_NISSANLEAF_H__
