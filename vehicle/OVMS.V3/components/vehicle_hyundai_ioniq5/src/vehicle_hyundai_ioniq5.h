/*
;    Project:       Open Vehicle Monitor System
;    Date:          November 2022
;
;    (C) 2022 Michael Geddes
; ----- Kona/Kia Module -----
;    (C) 2018        Geir Øyvind Vælidalo <geir@validalo.net>
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

#ifndef __VEHICLE_HYUNDAI_IONIQ5_H__
#define __VEHICLE_HYUNDAI_IONIQ5_H__

#include "../../vehicle_kiasoulev/src/kia_common.h"
#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif
#include <vector>

// Define this to work out whether a function that is instrumented with
// XARM and XDISARM is leaving unexpectedly (exception).
// This working is predicated on the stack objects being unwound.
// Debug tool: Do NOT leave this defined in production.
// #define DEBUG_FUNC_LEAVE

// Define for Debug of battery monitor internal states.
// Debug tool: Probably don't need it for production
// #define OVMS_DEBUG_BATTERYMON

#ifdef OVMS_DEBUG_BATTERYMON
// Extra verbose debuuging of battery monitor.
// #define OVMS_DEBUG_BATTERYMON_VERBOSE
#endif

// Define when canwrite is available
// #define XIQ_CAN_WRITE

using namespace std;

enum class IqShiftStatus {
  Park,
  Reverse,
  Neutral,
  Drive
};


void xiq_trip_since_parked(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
void xiq_trip_since_charge(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
void xiq_tpms(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
void xiq_aux(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
void xiq_vin(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
void xiq_aux_monitor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);

void xiq_range_stat(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
void xiq_range_reset(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);

//NOTIMPL void CommandOpenTrunk(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
void CommandParkBreakService(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_sjb(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_bcm(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_ign1(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_ign2(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_acc_relay(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_start_relay(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_set_head_light_delay(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_set_one_touch_turn_signal(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_set_auto_door_unlock(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
//NOTIMPL void xiq_set_auto_door_lock(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);

enum class OvmsBatteryState { Unknown, Normal, Charging, Blip, Dip, Low };

// 12V/Aux Battery monitor: The aim is to detect 'dips' (voltage dipping down) and 'blips' (voltage 'blipping' up),
// as well as 'charging' states and 'low voltage'.
// This is used to wake up the 'Ping' cycle and do some polling to detect what's going on.  This prevents us from
// keeping the power-hungry modules from being kept awake and draining the 12V battery (in 3h)
struct OvmsBatteryMon {
  static const uint16_t entry_count = 30;
  static const int32_t entry_mult = 100.0;
  static const int32_t charge_threshold = 1400; // over 14.0v is 'Charging'
  static const int32_t low_threshold = 1150;    // Under 11.5 is 'Low'
  static const int32_t smooth_threshold = 20;   // < 0.2v variation is 'Normal'
  static const int32_t blip_threshold = 30;     // cur is > 0.3v over average is 'Blip'
  static const int32_t dip_threshold = -25;     // cur is < 0.3v over average is 'Dip'
  // Divides the entry_count history elements into 2 parts (as a fraction first_mult / first_divis )
  static const uint16_t first_mult = 2;
  static const uint16_t first_divis = 3;

  int32_t m_voltages[entry_count]; // Voltages times entry_mult


  // First  buffer entry and count
  uint16_t m_first, m_count;
  bool m_dirty;
  bool m_to_notify;

  // Last calculated state
  OvmsBatteryState m_lastState;
  int32_t m_average_last;
  int32_t m_diff_last;

  OvmsBatteryMon();

  OvmsBatteryState calc_state(int32_t &ave_last, int32_t &diff_last);

  // Add a voltage to the circular buffer.
  void add(float voltage);
  // Calculate average of first section (first_mult/first_divis) and last section (the rest)
  bool calc_average( int32_t &average, int32_t &average_first, int32_t &average_last);

  // Calculate state and return true if changed
  bool checkStateChange();

  // Current State
  OvmsBatteryState state();
  std::string to_string();
  uint16_t count()
  {
    return m_count;
  }
  // Last supplied value.
  int32_t last();
  float lastf();
  float average_lastf();
  float diff_lastf();
};

typedef struct {
  int toPercent;
  float maxChargeWatts;
  int tempOptimal;   // Optimal temperature
  float tempDegrade; // Degredation per degree Calc from optimal (ratio)
} charging_step_t;

// Calculates the remaining charge with allowance for temperature.
int CalcRemainingChargeMins(float chargewatts, float availwatts, bool adjustTemp, float tempCelcius, int fromSoc, int toSoc, int batterySizeWh, const charging_step_t charge_steps[]);

class OvmsHyundaiIoniqEv : public KiaVehicle
{
public:
  OvmsHyundaiIoniqEv();
  ~OvmsHyundaiIoniqEv();
  static const char *TAG;
public:
  void IncomingFrameCan1(CAN_frame_t *p_frame) override;
  void Ticker1(uint32_t ticker) override;
  void Ticker10(uint32_t ticker) override;
  void Ticker60(uint32_t ticker) override;
  void Ticker300(uint32_t ticker) override;
  void EventListener(std::string event, void *data);
  void UpdatedAverageTemp(OvmsMetric* metric);
  void IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain);
  void ConfigChanged(OvmsConfigParam *param);
  bool SetFeature(int key, const char *value);
  const std::string GetFeature(int key);
  vehicle_command_t CommandHandler(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  bool Send_SJB_Command( uint8_t b1, uint8_t b2, uint8_t b3);
  bool Send_IGMP_Command( uint8_t b1, uint8_t b2, uint8_t b3);
  bool Send_BCM_Command( uint8_t b1, uint8_t b2, uint8_t b3);
  bool Send_SMK_Command( uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7);
  bool Send_EBP_Command( uint8_t b1, uint8_t b2, uint8_t mode);
  void SendTesterPresent(uint16_t id, uint8_t length);
  bool SetSessionMode(uint16_t id, uint8_t mode);
  void SendCanMessage(uint16_t id, uint8_t count,
    uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
    uint8_t b5, uint8_t b6);
  void SendCanMessageTriple(uint16_t id, uint8_t count,
    uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
    uint8_t b5, uint8_t b6);
  bool SendCanMessage_sync(uint16_t id, uint8_t count,
    uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
    uint8_t b5, uint8_t b6);
  bool SendCommandInSessionMode(uint16_t id, uint8_t count,
    uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
    uint8_t b5, uint8_t b6, uint8_t mode );

  OvmsVehicle::vehicle_command_t CommandLock(const char *pin) override;
  OvmsVehicle::vehicle_command_t CommandUnlock(const char *pin) override;
  OvmsVehicle::vehicle_command_t CommandWakeup() override;

  bool OpenTrunk(const char *password);
  bool ACCRelay(bool, const char *password);
  bool IGN1Relay(bool, const char *password);
  bool IGN2Relay(bool, const char *password);
  bool StartRelay(bool, const char *password);
  void SetHeadLightDelay(bool);
  void SetOneThouchTurnSignal(uint8_t);
  void SetAutoDoorUnlock(uint8_t);
  void SetAutoDoorLock(uint8_t);

  bool IsLHD();
  metric_unit_t GetConsoleUnits();

  bool  kn_emergency_message_sent;

  int m_checklock_retry, m_checklock_start, m_checklock_notify;

protected:
  void HandleCharging();
  void HandleChargeStop();
  void IncomingVMCU_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data);
  void IncomingBMC_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data);
  void IncomingBCM_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data);
  void IncomingIGMP_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data);
  void IncomingOther_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data);
  void IncomingAbsEsp(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain);
  void IncomingCM_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data);
  void RequestNotify(unsigned int which);
  void DoNotify();
  void vehicle_ioniq5_car_on(bool isOn);
  void UpdateMaxRangeAndSOH(void);
  bool SetDoorLock(bool open, const char *password);
  bool LeftIndicator(bool);
  bool RightIndicator(bool);
  bool DriverIndicatorOn()
  {
    if (IsLHD()) {
      return LeftIndicator(true);
    }
    else {
      return RightIndicator(true);
    }
  }
  bool RearDefogger(bool);
  bool FoldMirrors(bool);
  bool BlueChargeLed(bool on, uint8_t mode);
  void SetChargeMetrics(float voltage, float current, float climit, bool ccs);
  void SendTesterPresentMessages();
  void StopTesterPresentMessages();

  // Inline functions to handle the different I5 Poll states.
  inline int PollGetState()
  {
    return m_poll_state;
  }
  inline void PollState_Off()
  {
    PollSetState(0);
  }
  inline bool IsPollState_Off()
  {
    return m_poll_state == 0;
  }

  inline void PollState_Running()
  {
    PollSetState(1);
  }
  inline bool IsPollState_Running()
  {
    return m_poll_state == 1;
  }

  inline void PollState_Charging()
  {
    PollSetState(2);
  }
  inline bool IsPollState_Charging()
  {
    return m_poll_state == 2;
  }

  inline void PollState_Ping()
  {
    PollSetState(3);
  }
  inline bool IsPollState_Ping()
  {
    return m_poll_state == 3;
  }
  inline void PollState_Ping( uint32_t ticks)
  {
    if (hif_keep_awake < ticks) {
      hif_keep_awake = ticks;
    }
    PollSetState(3);
  }
  inline void Poll_CapAwake( uint32_t ticks)
  {
    if (hif_keep_awake > ticks) {
      hif_keep_awake = ticks;
    }
  }

  inline void Set_IGMP_TP_TimeOut(bool on, int16_t seconds)
  {
    if (!on) {
      igmp_tester_present_seconds = 0;
    }
    if (seconds > igmp_tester_present_seconds) {
      igmp_tester_present_seconds = seconds;
    }
  }
  inline void Set_BCM_TP_TimeOut(bool on, int16_t seconds)
  {
    if (!on) {
      bcm_tester_present_seconds = 0;
    }
    else if (seconds > bcm_tester_present_seconds) {
      bcm_tester_present_seconds = seconds;
    }
  }
  void CheckResetDoorCheck();

  void NotifiedOBD2ECUStart() override
  {
    if (m_ecu_lockout == 0)
      ECUStatusChange(StandardMetrics.ms_v_env_on->AsBool() && (StdMetrics.ms_v_env_gear->AsInt() > 0));
  }
  void NotifiedOBD2ECUStop() override
  {
    ECUStatusChange(false);
  }
  void NotifiedVehicleOn() override
  {
    m_ecu_lockout = 20;
  }
  void NotifiedVehicleOff() override
  {
    m_ecu_lockout = 0;
    ECUStatusChange(false);
  }
  void NotifiedVehicleGear( int gear) override
  {
    if (gear <= 0)
      ECUStatusChange(false);
    else if (m_ecu_lockout == 0)
      ECUStatusChange(StandardMetrics.ms_v_env_on->AsBool() && StandardMetrics.ms_m_obd2ecu_on->AsBool());
  }

  int m_ecu_lockout;
  void ECUStatusChange(bool run);
public:
  int RequestVIN();
  bool DriverIndicator(bool on)
  {
    if (IsLHD()) {
      return LeftIndicator(on);
    }
    else {
      return RightIndicator(on);
    }
  }
protected:
  int m_vin_retry;

  OvmsCommand *cmd_hiq;

#define CFG_DEFAULT_MAXRANGE 440
  int hif_maxrange = CFG_DEFAULT_MAXRANGE;        // Configured max range at 20 °C

#define CGF_DEFAULT_BATTERY_CAPACITY 72600
#define HIF_CELL_PAIR_CAPACITY 403

  bool hif_override_capacity = false;
  float hif_battery_capacity = CGF_DEFAULT_BATTERY_CAPACITY;

  unsigned int kn_notifications = 0;

  IqShiftStatus iq_shift_status;
  bool iq_car_on;

  bool iq_lights_on;
  bool iq_lights_park;
  bool iq_lights_low;
  bool iq_lights_high;
  bool iq_lights_dirty;

  uint8_t kn_battery_fan_feedback;

  int16_t igmp_tester_present_seconds;
  int16_t bcm_tester_present_seconds;

  int16_t hif_keep_awake;

  // The last filled voltage cell.
  int32_t iq_last_voltage_cell;

  OvmsBatteryMon hif_aux_battery_mon;

  OvmsMetricBool  *m_b_bms_relay;
  OvmsMetricBool  *m_b_bms_ignition;
  OvmsMetricInt   *m_b_bms_availpower;
  OvmsMetricFloat *m_v_bat_calc_cap;

  OvmsMetricBool   *m_v_env_parklights;
  OvmsMetricFloat   *m_v_charge_current_request;

  /// Accumulated operating time
  OvmsMetricInt *m_v_accum_op_time;

  struct {
    unsigned char ChargingCCS : 1;
    unsigned char ChargingType2 : 1;
    unsigned char : 1;
    unsigned char : 1;
    unsigned char FanStatus : 4;
  } kn_charge_bits;

  RangeCalculator *iq_range_calc;

  // Receive Buffer for OBD ISOTP polls.
  std::string obd_rxbuf;
  uint16_t obd_module;
  uint16_t obd_rxtype;
  uint16_t obd_rxpid;
  uint16_t obd_frame;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  // --------------------------------------------------------------------------
  // Webserver subsystem
  //  - implementation: hif_web.(h,cpp)
  //

public:
  void GetDashboardConfig(DashboardConfig &cfg) override;
  virtual void WebInit();
  static void WebCfgFeatures(PageEntry_t &p, PageContext_t &c);
  static void WebCfgBattery(PageEntry_t &p, PageContext_t &c);
  static void WebBattMon(PageEntry_t &p, PageContext_t &c);

  void RangeCalcReset();
  void RangeCalcStat(OvmsWriter *writer);
#endif //CONFIG_OVMS_COMP_WEBSERVER
public:
  std::string BatteryMonStat()
  {
    return hif_aux_battery_mon.to_string();
  }
};

#ifdef DEBUG_FUNC_LEAVE
// This provides a way of detecting unexpected exceptions etc in the
// Ioniq5 Code.  Most functions in the project are instrumented this way.
// For the moment, we will leave it out of production.
struct OvmsProtect {
  bool m_armed;
  std::string m_name;
  OvmsProtect( const char *name) : m_name(name)
  {
    m_armed = true;
  }
  ~OvmsProtect()
  {
    if (m_armed) {
      ESP_LOGE(OvmsHyundaiIoniqEv::TAG, "%s - Abnormal termination", m_name.c_str());
    }
  }
  void disarm()
  {
    m_armed = false;
  }
};
#define XARM(x) OvmsProtect x_protect(x)
#define XDISARM x_protect.disarm()
#define XARMX(N,X) OvmsProtect x_protect_ ## N(X)
#define XDISARMX(N) x_protect_ ## N.disarm()

#else
// Undef DEBUG_FUNC_LEAVE
#define XARM(x)
#define XDISARM
#define XARMX(n,x)
#define XDISARMX(n)

#endif



// Notifications:
#define SEND_EmergencyAlert           (1<< 5)  // Emergency lights are turned on
#define SEND_EmergencyAlertOff        (1<< 6)  // Emergency lights are turned off

#endif //#ifndef __VEHICLE_HYUNDAI_IONIQ5_H__
