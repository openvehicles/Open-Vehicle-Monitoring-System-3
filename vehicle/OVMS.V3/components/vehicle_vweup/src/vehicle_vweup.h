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

#include "ovms_webserver.h"

#include "ovms_mutex.h"
#include "ovms_semaphore.h"

#include "poll_reply_helper.h"

#define DEFAULT_MODEL_YEAR 2020

using namespace std;

typedef enum {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  AUTO_DISABLE_CLIMATE_CONTROL
} RemoteCommand;

// Value update & conversion debug logging:
#define VALUE_LOG(...)    ESP_LOGD(__VA_ARGS__)
// …disable:
//#define VALUE_LOG(...)

// Car (poll) states
#define VWEUP_OFF         0
#define VWEUP_ON          1
#define VWEUP_CHARGING    2

// Connections available (vweup_con):
#define CON_NONE          0
#define CON_T26           1
#define CON_OBD           2
#define CON_BOTH          3

typedef std::vector<OvmsVehicle::poll_pid_t, ExtRamAllocator<OvmsVehicle::poll_pid_t>> poll_vector_t;
typedef std::initializer_list<const OvmsVehicle::poll_pid_t> poll_list_t;

typedef enum {
  OBDS_Init = 0,
  OBDS_DeInit,
  OBDS_Config,
  OBDS_Run,
} obd_state_t;

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
  void ConfigChanged(OvmsConfigParam *param);
  bool SetFeature(int key, const char *value);
  const std::string GetFeature(int key);

protected:
  void Ticker1(uint32_t ticker);

public:
  vehicle_command_t CommandHomelink(int button, int durationms = 1000);
  vehicle_command_t CommandClimateControl(bool enable);
  vehicle_command_t CommandLock(const char *pin);
  vehicle_command_t CommandUnlock(const char *pin);
  vehicle_command_t CommandStartCharge();
  vehicle_command_t CommandStopCharge();
  vehicle_command_t CommandActivateValet(const char *pin);
  vehicle_command_t CommandDeactivateValet(const char *pin);
  vehicle_command_t CommandWakeup();

protected:
  int GetNotifyChargeStateDelay(const char *state);

protected:
  void ResetTripCounters();
  void ResetChargeCounters();

public:
  bool IsOff() {
    return m_poll_state == VWEUP_OFF;
  }
  bool IsOn() {
    return m_poll_state == VWEUP_ON;
  }
  bool IsCharging() {
    return m_poll_state == VWEUP_CHARGING;
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

private:
  float m_odo_start;
  float m_energy_recd_start;
  float m_energy_used_start;
  float m_energy_charged_start;
  float m_coulomb_recd_start;
  float m_coulomb_used_start;


  // --------------------------------------------------------------------------
  // Web UI Subsystem
  //  - implementation: vweup_web.(h,cpp)
  //

protected:
  void WebInit();
  void WebDeInit();

  static void WebCfgFeatures(PageEntry_t &p, PageContext_t &c);
  static void WebCfgClimate(PageEntry_t &p, PageContext_t &c);
  static void WebDispChgMetrics(PageEntry_t &p, PageContext_t &c);


  // --------------------------------------------------------------------------
  // T26 Connection Subsystem
  //  - implementation: vweup_t26.(h,cpp)
  //

protected:
  void T26Init();

protected:
  void IncomingFrameCan3(CAN_frame_t *p_frame);

public:
  void SendOcuHeartbeat();
  void CCCountdown();
  void CCOn();
  void CCOnP();
  void CCOff();
  static void ccCountdown(TimerHandle_t timer);
  static void sendOcuHeartbeat(TimerHandle_t timer);

private:
  void SendCommand(RemoteCommand);
  OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);
  void vehicle_vweup_car_on(bool turnOn);

public:
  char m_vin[18];
  bool vin_part1;
  bool vin_part2;
  bool vin_part3;
  int vweup_remote_climate_ticker;
  int vweup_cc_temp_int;
  bool ocu_awake;
  bool ocu_working;
  bool ocu_what;
  bool ocu_wait;
  bool vweup_cc_on;
  bool vweup_cc_turning_on;
  bool signal_ok;
  int cc_count;
  int cd_count;
  int fas_counter_on;
  int fas_counter_off;
  bool dev_mode;

private:
  RemoteCommand vweup_remote_command; // command to send, see RemoteCommandTimer()
  TimerHandle_t m_sendOcuHeartbeat;
  TimerHandle_t m_ccCountdown;


  // --------------------------------------------------------------------------
  // OBD2 subsystem
  //  - implementation: vweup_obd.(h,cpp)
  //

protected:
  void OBDInit();
  void OBDDeInit();
  void OBDCheckCarState();

protected:
  void IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain);

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
  OvmsMetricInt *ServiceDays;                     // Days until next scheduled maintenance/service

  OvmsMetricFloat *BatTempMax;
  OvmsMetricFloat *BatTempMin;

protected:
  obd_state_t         m_obd_state;                // OBD subsystem state
  poll_vector_t       m_poll_vector;              // List of PIDs to poll

  int                 m_cfg_cell_interval_drv;    // Cell poll interval while driving, default 15 sec.
  int                 m_cfg_cell_interval_chg;    // … while charging, default 60 sec.
  uint16_t            m_cell_last_vi;             // Index of last cell voltage read
  uint16_t            m_cell_last_ti;             // … temperature

  float               m_range_est_factor;         // For range calculation during charge

private:
  PollReplyHelper     PollReply;

  float               BatMgmtCellMax;             // Maximum cell voltage
  float               BatMgmtCellMin;             // Minimum cell voltage

  uint32_t            TimeOffRequested;           // For Off-Timeout: Monotonictime when the poll should
                                                  //   have gone to VWEUP_OFF, 0 means no Off requested

};

#endif //#ifndef __VEHICLE_EUP_H__
