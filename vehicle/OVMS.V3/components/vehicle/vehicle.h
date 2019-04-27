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

#ifndef __VEHICLE_H__
#define __VEHICLE_H__

#include <map>
#include <vector>
#include <string>
#include "can.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "metrics_standard.h"

using namespace std;
struct DashboardConfig;

// Polling types supported:
//  (see https://en.wikipedia.org/wiki/OBD-II_PIDs
//   and https://en.wikipedia.org/wiki/Unified_Diagnostic_Services)

#define VEHICLE_POLL_TYPE_NONE          0x00
#define VEHICLE_POLL_TYPE_OBDIICURRENT  0x01 // Mode 01 "current data"
#define VEHICLE_POLL_TYPE_OBDIIFREEZE   0x02 // Mode 02 "freeze frame data"
#define VEHICLE_POLL_TYPE_OBDIIVEHICLE  0x09 // Mode 09 "vehicle information"
#define VEHICLE_POLL_TYPE_OBDIISESSION  0x10 // UDS: Diagnostic Session Control
#define VEHICLE_POLL_TYPE_OBDII_1A  			0x1A // Mode 1A
#define VEHICLE_POLL_TYPE_OBDIIGROUP    0x21 // enhanced data by 8 bit PID
#define VEHICLE_POLL_TYPE_OBDIIEXTENDED 0x22 // enhanced data by 16 bit PID

#define VEHICLE_POLL_NSTATES            4


// Standard MSG protocol commands:

#define CMD_QueryFeatures               1   // ()
#define CMD_SetFeature                  2   // (feature number, value)
#define CMD_QueryParams                 3   // ()
#define CMD_SetParam                    4   // (param number, value)
#define CMD_Reboot                      5   // ()
#define CMD_Alert                       6   // ()
#define CMD_Execute                     7   // (text command with arguments)
#define CMD_SetChargeMode               10  // (mode)
#define CMD_StartCharge                 11  // ()
#define CMD_StopCharge                  12  // ()
#define CMD_SetChargeCurrent            15  // (amps)
#define CMD_SetChargeModeAndCurrent     16  // (mode, amps)
#define CMD_SetChargeTimer              17  // (mode, start time)
#define CMD_WakeupCar                   18  // ()
#define CMD_WakeupTempSystem            19  // ()
#define CMD_Lock                        20  // (pin)
#define CMD_ValetOn                     21  // (pin)
#define CMD_UnLock                      22  // (pin)
#define CMD_ValetOff                    23  // (pin)
#define CMD_Homelink                    24  // (button_nr)
#define CMD_CoolDown                    25  // ()
#define CMD_ClimateControl              26  // (mode)
// 30-39 reserved for server commands
#define CMD_SendSMS                     40  // (phone number, SMS message)
#define CMD_SendUSSD                    41  // (USSD_CODE)
#define CMD_SendRawAT                   49  // (raw AT command)
// 200+ reserved for custom commands


// BMS default deviation thresholds:
#define BMS_DEFTHR_VWARN    0.020   // [V]
#define BMS_DEFTHR_VALERT   0.030   // [V]
#define BMS_DEFTHR_TWARN    2.00    // [°C]
#define BMS_DEFTHR_TALERT   3.00    // [°C]


class OvmsVehicle : public InternalRamAllocated
  {
  public:
    OvmsVehicle();
    virtual ~OvmsVehicle();
    virtual const char* VehicleShortName();

  protected:
    QueueHandle_t m_rxqueue;
    TaskHandle_t m_rxtask;
    bool m_registeredlistener;
    bool m_autonotifications;

  public:
    canbus* m_can1;
    canbus* m_can2;
    canbus* m_can3;

  private:
    void VehicleTicker1(std::string event, void* data);
    void VehicleConfigChanged(std::string event, void* data);
    void PollerSend();
    void PollerReceive(CAN_frame_t* frame);

  protected:
    virtual void IncomingFrameCan1(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan2(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan3(CAN_frame_t* p_frame);
    virtual void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);

  protected:
    int m_minsoc;            // The minimum SOC level before alert
    int m_minsoc_triggered;  // The triggered minimum SOC level to alert at

  protected:
    float m_accel_refspeed;                 // Acceleration calculation: last speed measured (m/s)
    uint32_t m_accel_reftime;               // … timestamp for refspeed (ms)
    float m_accel_smoothing;                // … smoothing factor (samples, 0 = none, default 2.0)
    void CalculateAcceleration();           // Call after ms_v_pos_speed update to derive acceleration

  protected:
    bool m_brakelight_enable;               // Regen brake light enable (default no)
    int m_brakelight_port;                  // … MAX7317 output port number (1, 3…9, default 1 = SW_12V)
    float m_brakelight_on;                  // … activation threshold (deceleration in m/s², default 1.3)
    float m_brakelight_off;                 // … deactivation threshold (deceleration in m/s², default 0.7)
    uint32_t m_brakelight_start;            // … activation start time
    void CheckBrakelight();                 // … check vehicle metrics for regen braking state
    virtual bool SetBrakelight(int on);     // … hardware control method (override for non MAX7317 control)

  protected:
    uint32_t m_ticker;
    int m_12v_ticker;
    int m_chargestate_ticker;
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);
    virtual void Ticker60(uint32_t ticker);
    virtual void Ticker300(uint32_t ticker);
    virtual void Ticker600(uint32_t ticker);
    virtual void Ticker3600(uint32_t ticker);

  protected:
    virtual void NotifyChargeState();
    virtual void NotifyChargeStart();
    virtual void NotifyHeatingStart();
    virtual void NotifyChargeStopped();
    virtual void NotifyChargeDone();
    virtual void NotifyValetEnabled();
    virtual void NotifyValetDisabled();
    virtual void NotifyValetHood();
    virtual void NotifyValetTrunk();
    virtual void NotifyAlarmSounding();
    virtual void NotifyAlarmStopped();
    virtual void Notify12vCritical();
    virtual void Notify12vRecovered();
    virtual void NotifyMinSocCritical();

  protected:
    virtual int GetNotifyChargeStateDelay(const char* state) { return 3; }
    virtual void NotifiedVehicleOn() {}
    virtual void NotifiedVehicleOff() {}
    virtual void NotifiedVehicleAwake() {}
    virtual void NotifiedVehicleAsleep() {}
    virtual void NotifiedVehicleChargeStart() {}
    virtual void NotifiedVehicleChargeStop() {}
    virtual void NotifiedVehicleChargePrepare() {}
    virtual void NotifiedVehicleChargeFinish() {}
    virtual void NotifiedVehicleChargePilotOn() {}
    virtual void NotifiedVehicleChargePilotOff() {}
    virtual void NotifiedVehicleCharge12vStart() {}
    virtual void NotifiedVehicleCharge12vStop() {}
    virtual void NotifiedVehicleLocked() {}
    virtual void NotifiedVehicleUnlocked() {}
    virtual void NotifiedVehicleValetOn() {}
    virtual void NotifiedVehicleValetOff() {}
    virtual void NotifiedVehicleHeadlightsOn() {}
    virtual void NotifiedVehicleHeadlightsOff() {}
    virtual void NotifiedVehicleAlarmOn() {}
    virtual void NotifiedVehicleAlarmOff() {}
    virtual void NotifiedVehicleChargeMode(const char* m) {}
    virtual void NotifiedVehicleChargeState(const char* s) {}

  protected:
    virtual void ConfigChanged(OvmsConfigParam* param);
    virtual void MetricModified(OvmsMetric* metric);
    virtual void CalculateEfficiency();

  public:
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    virtual void GetDashboardConfig(DashboardConfig& cfg);
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
    virtual void Status(int verbosity, OvmsWriter* writer);

  protected:
    void RegisterCanBus(int bus, CAN_mode_t mode, CAN_speed_t speed, dbcfile* dbcfile = NULL);
    bool PinCheck(char* pin);

  public:
    virtual void RxTask();

  public:
    typedef enum
      {
      NotImplemented = 0,
      Success,
      Fail
      } vehicle_command_t;
    typedef enum
      {
      Standard = 0,
      Storage = 1,
      Range = 3,
      Performance = 4
      } vehicle_mode_t;

  public:
    vehicle_mode_t VehicleModeKey(const std::string code);

  public:
    virtual vehicle_command_t CommandSetChargeMode(vehicle_mode_t mode);
    virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    virtual vehicle_command_t CommandStartCharge();
    virtual vehicle_command_t CommandStopCharge();
    virtual vehicle_command_t CommandSetChargeTimer(bool timeron, uint16_t timerstart);
    virtual vehicle_command_t CommandCooldown(bool cooldownon);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandClimateControl(bool enable);
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    virtual vehicle_command_t CommandHomelink(int button, int durationms=1000);

  public:
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t ProcessMsgCommand(std::string &result, int command, const char* args);

  public:
    virtual bool SetFeature(int key, const char* value);
    virtual const std::string GetFeature(int key);

  public:
    typedef struct
      {
      uint32_t txmoduleid;
      uint32_t rxmoduleid;
      uint16_t type;
      uint16_t pid;
      uint16_t polltime[VEHICLE_POLL_NSTATES];
      } poll_pid_t;

  protected:
    uint8_t           m_poll_state;           // Current poll state
    canbus*           m_poll_bus;             // Bus to poll on
    const poll_pid_t* m_poll_plist;           // Head of poll list
    const poll_pid_t* m_poll_plcur;           // Current position in poll list
    uint32_t          m_poll_ticker;          // Polling ticker
    uint32_t          m_poll_moduleid_sent;   // ModuleID last sent
    uint32_t          m_poll_moduleid_low;    // Expected response moduleid low mark
    uint32_t          m_poll_moduleid_high;   // Expected response moduleid high mark
    uint16_t          m_poll_type;            // Expected type
    uint16_t          m_poll_pid;             // Expected PID
    uint16_t          m_poll_ml_remain;       // Bytes remainign for ML poll
    uint16_t          m_poll_ml_offset;       // Offset of ML poll
    uint16_t          m_poll_ml_frame;        // Frame number for ML poll

  protected:
    void PollSetPidList(canbus* bus, const poll_pid_t* plist);
    void PollSetState(uint8_t state);

  // BMS helpers
  protected:
    float* m_bms_voltages;                    // BMS voltages (current value)
    float* m_bms_vmins;                       // BMS minimum voltages seen (since reset)
    float* m_bms_vmaxs;                       // BMS maximum voltages seen (since reset)
    float* m_bms_vdevmaxs;                    // BMS maximum voltage deviations seen (since reset)
    short* m_bms_valerts;                     // BMS voltage deviation alerts (since reset)
    int m_bms_valerts_new;                    // BMS new voltage alerts since last notification
    bool m_bms_has_voltages;                  // True if BMS has a complete set of voltage values
    float* m_bms_temperatures;                // BMS temperatures (celcius current value)
    float* m_bms_tmins;                       // BMS minimum temperatures seen (since reset)
    float* m_bms_tmaxs;                       // BMS maximum temperatures seen (since reset)
    float* m_bms_tdevmaxs;                    // BMS maximum temperature deviations seen (since reset)
    short* m_bms_talerts;                     // BMS temperature deviation alerts (since reset)
    int m_bms_talerts_new;                    // BMS new temperature alerts since last notification
    bool m_bms_has_temperatures;              // True if BMS has a complete set of temperature values
    std::vector<bool> m_bms_bitset_v;         // BMS tracking: true if corresponding voltage set
    std::vector<bool> m_bms_bitset_t;         // BMS tracking: true if corresponding temperature set
    int m_bms_bitset_cv;                      // BMS tracking: count of unique voltage values set
    int m_bms_bitset_ct;                      // BMS tracking: count of unique temperature values set
    int m_bms_readings_v;                     // Number of BMS voltage readings expected
    int m_bms_readingspermodule_v;            // Number of BMS voltage readings per module
    int m_bms_readings_t;                     // Number of BMS temperature readings expected
    int m_bms_readingspermodule_t;            // Number of BMS temperature readings per module
    float m_bms_limit_tmin;                   // Minimum temperature limit (for sanity checking)
    float m_bms_limit_tmax;                   // Maximum temperature limit (for sanity checking)
    float m_bms_limit_vmin;                   // Minimum voltage limit (for sanity checking)
    float m_bms_limit_vmax;                   // Maximum voltage limit (for sanity checking)
    float m_bms_defthr_vwarn;                 // Default voltage deviation warn threshold [V]
    float m_bms_defthr_valert;                // Default voltage deviation alert threshold [V]
    float m_bms_defthr_twarn;                 // Default temperature deviation warn threshold [°C]
    float m_bms_defthr_talert;                // Default temperature deviation alert threshold [°C]

  protected:
    void BmsSetCellArrangementVoltage(int readings, int readingspermodule);
    void BmsSetCellArrangementTemperature(int readings, int readingspermodule);
    void BmsSetCellDefaultThresholdsVoltage(float warn, float alert);
    void BmsSetCellDefaultThresholdsTemperature(float warn, float alert);
    void BmsSetCellLimitsVoltage(float min, float max);
    void BmsSetCellLimitsTemperature(float min, float max);
    void BmsSetCellVoltage(int index, float value);
    void BmsResetCellVoltages();
    void BmsSetCellTemperature(int index, float value);
    void BmsResetCellTemperatures();
    void BmsRestartCellVoltages();
    void BmsRestartCellTemperatures();
    virtual void NotifyBmsAlerts();

  public:
    int BmsGetCellArangementVoltage(int* readings=NULL, int* readingspermodule=NULL);
    int BmsGetCellArangementTemperature(int* readings=NULL, int* readingspermodule=NULL);
    void BmsGetCellDefaultThresholdsVoltage(float* warn, float* alert);
    void BmsGetCellDefaultThresholdsTemperature(float* warn, float* alert);
    void BmsResetCellStats();
    virtual void BmsStatus(int verbosity, OvmsWriter* writer);
    virtual bool FormatBmsAlerts(int verbosity, OvmsWriter* writer, bool show_warnings);
  };

template<typename Type> OvmsVehicle* CreateVehicle()
  {
  return new Type;
  }

class OvmsVehicleFactory
  {
  public:
    OvmsVehicleFactory();
    ~OvmsVehicleFactory();

  public:
    typedef OvmsVehicle* (*FactoryFuncPtr)();
    typedef struct
      {
      FactoryFuncPtr construct;
      const char* name;
      } vehicle_t;
    typedef map<const char*, vehicle_t, CmpStrOp> map_vehicle_t;

    OvmsVehicle *m_currentvehicle;
    std::string m_currentvehicletype;
    map_vehicle_t m_vmap;

  public:
    template<typename Type>
    short RegisterVehicle(const char* VehicleType, const char* VehicleName = "")
      {
      FactoryFuncPtr function = &CreateVehicle<Type>;
        m_vmap.insert(std::make_pair(VehicleType, (vehicle_t){ function, VehicleName }));
      return 0;
      };
    OvmsVehicle* NewVehicle(const char* VehicleType);
    void ClearVehicle();
    void SetVehicle(const char* type);
    void AutoInit();
    OvmsVehicle* ActiveVehicle();
    const char* ActiveVehicleType();
    const char* ActiveVehicleName();
    const char* ActiveVehicleShortName();
  };

extern OvmsVehicleFactory MyVehicleFactory;

#endif //#ifndef __VEHICLE_H__
