/*
;    Project:       Open Vehicle Monitor System
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris van der Meijden
;    (C) 2020       Soko
;    (C) 2020       sharkcow <sharkcow@gmx.de>
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

#ifndef __VEHICLE_EUP_H__
#define __VEHICLE_EUP_H__

#include <atomic>

#include "can.h"
#include "vehicle.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

#include "ovms_mutex.h"
#include "ovms_semaphore.h"

#include "poll_reply_helper.h"
#include "vweup_utils.h"

#define DEFAULT_MODEL_YEAR 2020
#define CC_TEMP_MIN 15
#define CC_TEMP_MAX 30

using namespace std;

// VW e-Up specific MSG protocol commands:
#define CMD_SetChargeAlerts         204 // (suffsoc, current limit, charge mode)


typedef enum {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  AUTO_DISABLE_CLIMATE_CONTROL
} RemoteCommand;

// profile0 communication state (charge & AC control)
#define PROFILE0_IDLE 0
#define PROFILE0_INIT 1
#define PROFILE0_WAKE 2
#define PROFILE0_REQUEST 3
#define PROFILE0_READ 4
#define PROFILE0_WRITE 5
#define PROFILE0_WRITEREAD 6
#define PROFILE0_SWITCH 7

// keys that control behavior of profile0 state machine
#define P0_KEY_NOP 0
#define P0_KEY_READ 1
#define P0_KEY_SWITCH 20
#define P0_KEY_CLIMIT 21
#define P0_KEY_CC_TEMP 22

// Value update & conversion debug logging:
#define VALUE_LOG(...)    ESP_LOGV(__VA_ARGS__)
// …disable:
//#define VALUE_LOG(...)

// Car (poll) states
#define VWEUP_OFF         0           // All systems sleeping
#define VWEUP_AWAKE       1           // Base systems online
#define VWEUP_CHARGING    2           // Base systems online & car is charging the main battery
#define VWEUP_ON          3           // All systems online & car is drivable

// Connections available (vweup_con):
#define CON_NONE          0
#define CON_T26           1
#define CON_OBD           2
#define CON_BOTH          3

typedef std::vector<OvmsPoller::poll_pid_t, ExtRamAllocator<OvmsPoller::poll_pid_t>> poll_vector_t;
typedef std::initializer_list<const OvmsPoller::poll_pid_t> poll_list_t;

typedef enum {
  OBDS_Init = 0,
  OBDS_DeInit,
  OBDS_Config,
  OBDS_Run,
  OBDS_Pause,
} obd_state_t;

typedef enum {
  CHGTYPE_None = 0,
  CHGTYPE_AC,
  CHGTYPE_DC,
} chg_type_t;

typedef enum {
  UP_None = 0,
  UP_Charging,
  UP_Driving,
} use_phase_t;

class OvmsVehicleVWeUp : public OvmsVehicle
{
  // --------------------------------------------------------------------------
  // General Framework & Connection Manager
  //

public:
  OvmsVehicleVWeUp();
  ~OvmsVehicleVWeUp();
  static OvmsVehicleVWeUp *GetInstance(OvmsWriter *writer = NULL);

public:
  void ConfigChanged(OvmsConfigParam *param) override;
  void MetricModified(OvmsMetric* metric);
  bool SetFeature(int key, const char *value);
  const std::string GetFeature(int key);
  vehicle_command_t ProcessMsgCommand(std::string &result, int command, const char* args); 
  vehicle_command_t MsgCommandCA(std::string &result, int command, const char* args);

protected:
  void Ticker1(uint32_t ticker) override;
  void Ticker10(uint32_t ticker) override;
  void Ticker60(uint32_t ticker) override;

public:
  vehicle_command_t CommandHomelink(int button, int durationms = 1000) override;
  vehicle_command_t CommandClimateControl(bool enable) override;
  vehicle_command_t CommandLock(const char *pin) override;
  vehicle_command_t CommandUnlock(const char *pin) override;
  vehicle_command_t CommandStartCharge() override;
  vehicle_command_t CommandStopCharge() override;
  vehicle_command_t CommandSetChargeCurrent(uint16_t limit) override;
  vehicle_command_t CommandActivateValet(const char *pin) override;
  vehicle_command_t CommandDeactivateValet(const char *pin) override;
  vehicle_command_t CommandWakeup() override;

protected:
  int GetNotifyChargeStateDelay(const char *state);
  void NotifiedVehicleChargeState(const char* s);
  int CalcChargeTime(float capacity, float max_pwr, int from_soc, int to_soc);
  void UpdateChargeTimes();

protected:
  void ResetTripCounters();
  void UpdateTripOdo();
  void ResetChargeCounters();
  void SetChargeType(chg_type_t chgtype);
  void SetChargeState(bool charging);
  void SetUsePhase(use_phase_t usephase);
  void SetSOH(float soh_new);

public:
  bool IsOff() {
    return m_poll_state == VWEUP_OFF;
  }
  bool IsAwake() {
    return m_poll_state == VWEUP_AWAKE;
  }
  bool IsCharging() {
    return m_poll_state == VWEUP_CHARGING;
  }
  bool IsOn() {
    return m_poll_state == VWEUP_ON;
  }

  bool HasT26() {
    return (vweup_con & CON_T26) != 0;
  }
  bool HasNoT26() {
    return (vweup_con & CON_T26) == 0;
  }
  bool HasOBD() {
    return (vweup_con & CON_OBD) != 0;
  }
  bool HasNoOBD() {
    return (vweup_con & CON_OBD) == 0;
  }
  bool IsOBDReady() {
    return (m_obd_state == OBDS_Run);
  }
  bool IsT26Ready() {
    return (m_can3 != NULL);
  }

  bool IsChargeModeAC() {
    return m_chg_type == CHGTYPE_AC;
  }
  bool IsChargeModeDC() {
    return m_chg_type == CHGTYPE_DC;
  }

protected:
  static size_t m_modifier;
  OvmsMetricString *m_version;
  OvmsCommand *cmd_xvu;

public:
  bool vweup_enable_obd;
  bool vweup_enable_t26;
  bool vweup_enable_write;
  int vweup_con;                // 0: none, 1: only T26, 2: only OBD2; 3: both
  int vweup_modelyear;
  uint8_t climit_max;
  
private:
  use_phase_t m_use_phase;
  double m_odo_trip;
  uint32_t m_tripfrac_reftime;
  float m_tripfrac_refspeed;
  float m_soc_norm_start;
  float m_soc_abs_start;
  float m_energy_recd_start;
  float m_energy_used_start;
  float m_energy_charged_start;
  float m_coulomb_recd_start;
  float m_coulomb_used_start;
  float m_coulomb_charged_start;
  float m_charge_kwh_grid_start;
  double m_charge_kwh_grid;
  int m_chargestart_ticker;
  int m_chargestop_ticker;
  float m_chargestate_lastsoc;
  int m_timermode_ticker;
  bool m_timermode_new;

  // --------------------------------------------------------------------------
  // Web UI Subsystem
  //  - implementation: vweup_web.(h,cpp)
  //

#ifdef CONFIG_OVMS_COMP_WEBSERVER
protected:
  void WebInit();
  void WebDeInit();

  static void WebCfgFeatures(PageEntry_t &p, PageContext_t &c);
  static void WebCfgClimate(PageEntry_t &p, PageContext_t &c);
  static void WebDispChgMetrics(PageEntry_t &p, PageContext_t &c);
  static void WebDispBattHealth(PageEntry_t &p, PageContext_t &c);
#endif

public:
  void GetDashboardConfig(DashboardConfig& cfg);


  // --------------------------------------------------------------------------
  // T26 Connection Subsystem
  //  - implementation: vweup_t26.(h,cpp)
  //

protected:
  void T26Init();
  void T26Ticker1(uint32_t ticker);

protected:
  void IncomingFrameCan3(CAN_frame_t *p_frame) override;

public:
  void SendOcuHeartbeat();
  static void sendOcuHeartbeat(TimerHandle_t timer);
  void CCTempSet(int temperature);
  static void Profile0RetryTimer(TimerHandle_t timer);
  void Profile0RetryCallBack();
  void WakeupT26();
  void WakeupT26Stage2();
  void WakeupT26Stage3();
  void StartStopChargeT26(bool start);
  void StartStopChargeT26Workaround(bool start);
  void SetChargeCurrent(int limit);
  void RequestProfile0();
  void ReadProfile0(uint8_t *data);
  void WriteProfile0();
  void ActivateProfile0();
  SemaphoreHandle_t xWakeSemaphore;
  SemaphoreHandle_t xChargeSemaphore;
  SemaphoreHandle_t xCurrentSemaphore;

private:
  void SendCommand(RemoteCommand);
  OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);
  void vehicle_vweup_car_on(bool turnOn);

public:
  char m_vin[18];
  bool vin_part1;
  bool vin_part2;
  bool vin_part3;
  bool ocu_awake;
  bool ocu_working;
  bool ocu_what;
  bool ocu_wait;
  bool vweup_cc_on;
  bool signal_ok;
  bool t26_12v_boost;
  bool t26_car_on;
  bool t26_ring_awake;
  int t26_12v_boost_cnt;
  int t26_12v_boost_last_cnt;
  int t26_12v_wait_off;
  int cd_count;
  int fas_counter_on;
  int fas_counter_off;

  bool profile0_recv;
  uint8_t profile0_cntr[3];
  int profile0_idx;
  int profile0_chan;
  int profile0_mode;
  int profile0_charge_current;
  int profile0_cc_temp;
  bool profile0_cc_onbat;
  int profile0_charge_current_old;
  int profile0_cc_temp_old;
  static const int profile0_len = 48;
  uint8_t profile0[profile0_len];
  int charge_current;
  int profile0_state;
  int profile0_delay;
  int profile0_retries;
  int profile0_key;
  int profile0_val;
  bool profile0_activate;
  bool ocu_response;
  bool fakestop;
  bool xvu_update;
  bool chg_autostop;
  bool chg_workaround;
  bool t26_init;
  bool wakeup_success;
  bool charge_timeout;
  uint8_t lever;
  uint8_t lever0_cnt;
  bool p_problem;
  bool chargestartstop;
  
private:
  RemoteCommand vweup_remote_command; // command to send, see RemoteCommandTimer()
  TimerHandle_t m_sendOcuHeartbeat = 0;
  TimerHandle_t profile0_timer = 0;


  // --------------------------------------------------------------------------
  // OBD2 subsystem
  //  - implementation: vweup_obd.(h,cpp)
  //

protected:
  void OBDInit();
  void OBDDeInit();

protected:
  bool OBDSetState(obd_state_t state);
  static const char *GetOBDStateName(obd_state_t state) {
    const char *statename[] = { "INIT", "DEINIT", "CONFIG", "RUN", "PAUSE" };
    return statename[state];
  }
  void PollSetState(uint8_t state);
  static const char *GetPollStateName(uint8_t state) {
    const char *statename[4] = { "OFF", "AWAKE", "CHARGING", "ON" };
    return statename[state];
  }
  void PollerStateTicker(canbus *bus) override;

  void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;

protected:
  void UpdateChargePower(float power_kw);
  void UpdateChargeCap(bool charging);

public:
  static void ShellPollControl(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
  static void CommandReadProfile0(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
  static void CommandResetProfile0(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

protected:
  OvmsMetricFloat *MotElecSoCAbs;                 // Absolute SoC of main battery from motor electrics ECU
  OvmsMetricFloat *MotElecSoCNorm;                // Normalized SoC of main battery from motor electrics ECU
  OvmsMetricFloat *BatMgmtSoCAbs;                 // Absolute SoC of main battery from battery management ECU
  OvmsMetricFloat *ChgMgmtSoCNorm;                // Normalized SoC of main battery from charge management ECU
  OvmsMetricFloat *BatMgmtCellDelta;              // Highest voltage - lowest voltage of all cells [V]

  OvmsMetricFloat *ChargerACPower;                // AC Power [kW]
  OvmsMetricFloat *ChargerAC1U;                   // AC Voltage Phase 1 [V]
  OvmsMetricFloat *ChargerAC2U;                   // AC Voltage Phase 2 [V]
  OvmsMetricFloat *ChargerAC1I;                   // AC Current Phase 1 [A]
  OvmsMetricFloat *ChargerAC2I;                   // AC Current Phase 2 [A]
  OvmsMetricFloat *ChargerDC1U;                   // DC Voltage 1 [V]
  OvmsMetricFloat *ChargerDC2U;                   // DC Voltage 2 [V]
  OvmsMetricFloat *ChargerDC1I;                   // DC Current 1 [A]
  OvmsMetricFloat *ChargerDC2I;                   // DC Current 2 [A]
  OvmsMetricFloat *ChargerDCPower;                // DC Power [kW]
  OvmsMetricFloat *ChargerPowerEffEcu;            // Efficiency of the Charger [%] (from ECU)
  OvmsMetricFloat *ChargerPowerLossEcu;           // Power loss of Charger [kW] (from ECU)
  OvmsMetricFloat *ChargerPowerEffCalc;           // Efficiency of the Charger [%] (calculated from U and I)
  OvmsMetricFloat *ChargerPowerLossCalc;          // Power loss of Charger [kW] (calculated from U and I)

  OvmsMetricFloat     *m_chg_ccs_voltage;         // CCS charger supplied voltage [V]
  OvmsMetricFloat     *m_chg_ccs_current;         // CCS Charger supplied current [A]
  OvmsMetricFloat     *m_chg_ccs_power;           // CCS Charger supplied power [kW]

  OvmsMetricInt *ServiceDays;                     // Days until next scheduled maintenance/service
  OvmsMetricVector<float> *TPMSDiffusion;         // TPMS Indicator for Pressure Diffusion
  OvmsMetricVector<float> *TPMSEmergency;         // TPMS Indicator for Tyre Emergency

  OvmsMetricFloat *BatTempMax;
  OvmsMetricFloat *BatTempMin;

  OvmsMetricInt       *m_lv_pwrstate;             // Low voltage (12V) systems power state (0x1DEC[0]: 0-15)
  OvmsMetricInt       *m_lv_autochg;              // Low voltage (12V) auto charge mode (0x1DED[0]: 0/1)
  OvmsMetricInt       *m_hv_chgmode;              // High voltage charge mode (0x1DD6[0]: 0/1/4)

  OvmsMetricInt       *m_chg_timer_socmin;        // Scheduled minimum SOC
  OvmsMetricInt       *m_chg_timer_socmax;        // Scheduled maximum SOC
  OvmsMetricBool      *m_chg_timer_def;           // true = Scheduled charging is default

  OvmsMetricFloat     *m_bat_soh_vw = 0;          // Battery SOH from ECU 8C PID 74 CB via OBD [%]
  OvmsMetricFloat     *m_bat_soh_range = 0;       // Battery SOH based on MFD range estimation [%]
  OvmsMetricFloat     *m_bat_soh_charge = 0;      // Battery SOH based on coulomb charge count [%]
  OvmsMetricVector<float> *m_bat_cell_soh;        // Battery cell SOH from ECU 8C PID 74 CB via OBD [%]
  std::vector<OvmsMetricVector<float>*> m_bat_cmod_hist; // Battery cell module SOH history from ECU 8C PID 74 CC via OBD [%]

  OvmsMetricFloat     *m_bat_energy_range;        // Battery energy available from MFD range estimation [kWh]
  OvmsMetricFloat     *m_bat_cap_kwh_range;       // Battery usable capacity derived from MFD range estimation [kWh]

  OvmsMetricFloat     *m_bat_cap_ah_abs;          // Battery capacity based on coulomb charge count [Ah]
  OvmsMetricFloat     *m_bat_cap_ah_norm;         // … using normalized SOC
  OvmsMetricFloat     *m_bat_cap_kwh_abs;         // … based on energy charge count [kWh]
  OvmsMetricFloat     *m_bat_cap_kwh_norm;        // … using normalized SOC

  SmoothExp<float>    m_smooth_cap_ah_abs;        // … and smoothing for these
  SmoothExp<float>    m_smooth_cap_ah_norm;
  SmoothExp<float>    m_smooth_cap_kwh_abs;
  SmoothExp<float>    m_smooth_cap_kwh_norm;

protected:
  obd_state_t         m_obd_state;                // OBD subsystem state
  poll_vector_t       m_poll_vector;              // List of PIDs to poll

  int                 m_cfg_cell_interval_drv;    // Cell poll interval while driving, default 15 sec.
  int                 m_cfg_cell_interval_chg;    // … while charging, default 60 sec.
  int                 m_cfg_cell_interval_awk;    // … while awake, default 60 sec.
  uint16_t            m_cell_last_vi;             // Index of last cell voltage read
  uint16_t            m_cell_last_ti;             // … temperature

  float               m_range_est_factor;         // For range calculation during charge
  float               m_bat_cap_range_hist[3];    // Range capacity maximum detection for SOH calculation

  chg_type_t          m_chg_type;                 // CHGTYPE_None / _AC / _DC
  int                 m_cfg_dc_interval;          // Interval for DC fast charge test/log PIDs

  int                 m_chg_ctp_car;              // Charge time prediction by car

private:
  PollReplyHelper     PollReply;

  float               BatMgmtCellMax;             // Maximum cell voltage
  float               BatMgmtCellMin;             // Minimum cell voltage

};

#endif //#ifndef __VEHICLE_EUP_H__
