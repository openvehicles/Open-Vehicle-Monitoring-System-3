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
#include "ovms_mutex.h"
#include "ovms_semaphore.h"

using namespace std;
struct DashboardConfig;


// ISO TP:
//  (see https://en.wikipedia.org/wiki/ISO_15765-2)

#define ISOTP_FT_SINGLE                 0
#define ISOTP_FT_FIRST                  1
#define ISOTP_FT_CONSECUTIVE            2
#define ISOTP_FT_FLOWCTRL               3

// Protocol variant:
#define ISOTP_STD                       0     // standard addressing (11 bit IDs)
#define ISOTP_EXTADR                    1     // extended addressing (19 bit IDs)

// OBD2/UDS Polling types supported:
//  (see https://en.wikipedia.org/wiki/OBD-II_PIDs
//   and https://en.wikipedia.org/wiki/Unified_Diagnostic_Services)
// 
// Depending on the poll type, the response may or may not echo the PID (parameter ID / subtype).
// If no echo is present, the poller will only validate the type.
// 
// Depending on the poll type, you may also need to pass additional arguments. These can be set
// alternatively to the single PID in the poll_pid_t.args field, which extends the pid by a data
// length and up to 6 data bytes. A common example is reading a DTC info, the service has an
// 8 bit PID (subtype) and depending on the subtype 1-4 additional parameter bytes:
// const OvmsVehicle::poll_pid_t twizy_poll_default[] = {
//  { TXID, RXID, VEHICLE_POLL_TYPE_READDTC, {.args={ PID, DATALEN, DATA1, … }}, { 0, 10, 60, 0 }, 0 },
//  …
// See OvmsVehicleRenaultTwizy::ObdRequest() for an example on how to send dynamic requests
// with additional arguments.


#define VEHICLE_POLL_TYPE_NONE          0x00

// OBD (ISO 15031) service identifiers supported:
#define VEHICLE_POLL_TYPE_OBDIICURRENT  0x01 // Mode 01 "current data" (8 bit PID)
#define VEHICLE_POLL_TYPE_OBDIIFREEZE   0x02 // Mode 02 "freeze frame data" (8 bit PID)
#define VEHICLE_POLL_TYPE_READ_ERDTC    0x03 // Mode 03 read emission-related DTC (no PID)
#define VEHICLE_POLL_TYPE_CLEAR_ERDTC   0x04 // Mode 04 clear/reset emission-related DTC (no PID)
#define VEHICLE_POLL_TYPE_READOXSTEST   0x05 // Mode 05 read oxygen sensor monitoring test results (16 bit PID)
#define VEHICLE_POLL_TYPE_READOBMTEST   0x06 // Mode 06 read on-board monitoring test results (8 bit PID)
#define VEHICLE_POLL_TYPE_READ_DCERDTC  0x07 // Mode 07 read driving cycle emission-related DTC (no PID)
#define VEHICLE_POLL_TYPE_REQOBUCTRL    0x08 // Mode 08 request on-board unit control (8 bit PID)
#define VEHICLE_POLL_TYPE_OBDIIVEHICLE  0x09 // Mode 09 "vehicle information" (8 bit PID)
#define VEHICLE_POLL_TYPE_READ_PERMDTC  0x0A // Mode 0A read permanent (cleared) DTC (no PID)

// UDS (ISO 14229) service identifiers supported:
#define VEHICLE_POLL_TYPE_OBDIISESSION  0x10 // UDS: Diagnostic Session Control (8 bit PID)
#define VEHICLE_POLL_TYPE_TESTERPRESENT 0x3E // UDS: TesterPresent (8 bit PID)
#define VEHICLE_POLL_TYPE_SECACCESS     0x27 // UDS: SecurityAccess (8 bit PID)
#define VEHICLE_POLL_TYPE_COMCONTROL    0x28 // UDS: CommunicationControl (8 bit PID)
#define VEHICLE_POLL_TYPE_ECURESET      0x11 // UDS: ECUReset (8 bit PID)
#define VEHICLE_POLL_TYPE_CLEARDTC      0x14 // UDS: ClearDiagnosticInformation (no PID)
#define VEHICLE_POLL_TYPE_READDTC       0x19 // UDS: ReadDTCInformation (8 bit PID)
#define VEHICLE_POLL_TYPE_OBDIIEXTENDED 0x22 // UDS: ReadDataByIdentifier (16 bit PID) (legacy alias for READDATA)
#define VEHICLE_POLL_TYPE_READDATA      0x22 // UDS: ReadDataByIdentifier (16 bit PID)
#define VEHICLE_POLL_TYPE_READMEMORY    0x23 // UDS: ReadMemoryByAddress (no PID)
#define VEHICLE_POLL_TYPE_READSCALING   0x24 // UDS: ReadScalingDataByIdentifier (16 bit PID)
#define VEHICLE_POLL_TYPE_WRITEDATA     0x2E // UDS: WriteDataByIdentifier (16 bit PID)
#define VEHICLE_POLL_TYPE_WRITEMEMORY   0x3D // UDS: WriteMemoryByAddress (8 bit PID)
#define VEHICLE_POLL_TYPE_IOCONTROL     0x2F // UDS: InputOutputControlByIdentifier (16 bit PID)

// Other service identifiers supported:
#define VEHICLE_POLL_TYPE_OBDII_1A      0x1A // Custom: Mode 1A (8 bit PID)
#define VEHICLE_POLL_TYPE_OBDIIGROUP    0x21 // Custom: Read data by 8 bit PID

// Utils:
#define POLL_TYPE_HAS_16BIT_PID(type) \
  ((type) == VEHICLE_POLL_TYPE_READDATA || \
   (type) == VEHICLE_POLL_TYPE_READSCALING || \
   (type) == VEHICLE_POLL_TYPE_WRITEDATA || \
   (type) == VEHICLE_POLL_TYPE_IOCONTROL || \
   (type) == VEHICLE_POLL_TYPE_READOXSTEST)
#define POLL_TYPE_HAS_NO_PID(type) \
  ((type) == VEHICLE_POLL_TYPE_CLEARDTC || \
   (type) == VEHICLE_POLL_TYPE_READMEMORY || \
   (type) == VEHICLE_POLL_TYPE_READ_ERDTC || \
   (type) == VEHICLE_POLL_TYPE_CLEAR_ERDTC || \
   (type) == VEHICLE_POLL_TYPE_READ_DCERDTC || \
   (type) == VEHICLE_POLL_TYPE_READ_PERMDTC)
#define POLL_TYPE_HAS_8BIT_PID(type) \
  (!POLL_TYPE_HAS_NO_PID(type) && !POLL_TYPE_HAS_16BIT_PID(type))

// OBD/UDS Negative Response Code
#define UDS_RESP_TYPE_NRC               0x7F  // see ISO 14229 Annex A.1
#define UDS_RESP_NRC_RCRRP              0x78  // … requestCorrectlyReceived-ResponsePending

// Number of polling states supported
#define VEHICLE_POLL_NSTATES            4

// Macro for poll_pid_t termination
#define POLL_LIST_END                   { 0, 0, 0x00, 0x00, { 0, 0, 0 }, 0, 0 }

// PollSingleRequest specific result codes:
#define POLLSINGLE_OK                   0 
#define POLLSINGLE_TIMEOUT              -1
#define POLLSINGLE_TXFAILURE            -2


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
#define BMS_DEFTHR_VMAXGRAD             0.010   // [V]
#define BMS_DEFTHR_VMAXSDDEV            0.010   // [V]
#define BMS_DEFTHR_VWARN                0.020   // [V]
#define BMS_DEFTHR_VALERT               0.030   // [V]
#define BMS_DEFTHR_TWARN                2.00    // [°C]
#define BMS_DEFTHR_TALERT               3.00    // [°C]


class OvmsVehicle : public InternalRamAllocated
  {
  friend class OvmsVehicleFactory;

  public:
    OvmsVehicle();
    virtual ~OvmsVehicle();
    virtual const char* VehicleShortName();
    virtual const char* VehicleType();

  protected:
    QueueHandle_t m_rxqueue;
    TaskHandle_t m_rxtask;
    bool m_registeredlistener;
    bool m_autonotifications;
    bool m_ready;

  public:
    canbus* m_can1;
    canbus* m_can2;
    canbus* m_can3;
    canbus* m_can4;

  private:
    void VehicleTicker1(std::string event, void* data);
    void VehicleConfigChanged(std::string event, void* data);
    void PollerSend(bool fromTicker);
    void PollerReceive(CAN_frame_t* frame, uint32_t msgid);

  protected:
    virtual void IncomingFrameCan1(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan2(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan3(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan4(CAN_frame_t* p_frame);

  protected:
    virtual void PollerStateTicker();
    virtual void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    virtual void IncomingPollError(canbus* bus, uint16_t type, uint16_t pid, uint16_t code);

  protected:
    int m_minsoc;            // The minimum SOC level before alert
    int m_minsoc_triggered;  // The triggered minimum SOC level to alert at

  protected:
    float m_accel_refspeed;                 // Acceleration calculation: last speed measured (m/s)
    uint32_t m_accel_reftime;               // … timestamp for refspeed (ms)
    float m_accel_smoothing;                // … smoothing factor (samples, 0 = none, default 2.0)
    void CalculateAcceleration();           // Call after ms_v_pos_speed update to derive acceleration

  protected:
    float m_batpwr_smoothing;               // … smoothing factor (samples, 0 = none, default 2.0) …
    float m_batpwr_smoothed;                // … and smoothed value of ms_v_bat_power

  protected:
    bool m_brakelight_enable;               // Regen brake light enable (default no)
    int m_brakelight_port;                  // … MAX7317 output port number (1, 3…9, default 1 = SW_12V)
    float m_brakelight_on;                  // … activation threshold (deceleration in m/s², default 1.3)
    float m_brakelight_off;                 // … deactivation threshold (deceleration in m/s², default 0.7)
    float m_brakelight_basepwr;             // … base power area (+/- from 0 in kW, default 0)
    bool m_brakelight_ignftbrk;             // … ignore foot brake (default no)
    uint32_t m_brakelight_start;            // … activation start time
    virtual void CheckBrakelight();         // … check vehicle metrics for regen braking state (override to customize)
    virtual bool SetBrakelight(int on);     // … hardware control method (override for non MAX7317 control)

  protected:
    uint32_t m_ticker;
    int m_12v_ticker;
    int m_chargestate_ticker;
    int m_vehicleoff_ticker;
    int m_idle_ticker;
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
    virtual void NotifyVehicleIdling();
    virtual void NotifyVehicleOff();

  protected:
    virtual void NotifyGenState();

  protected:
    virtual void NotifyGridLog();
    virtual void NotifyTripLog();

  protected:
    virtual int GetNotifyVehicleStateDelay(const std::string& state) { return 3; }
    virtual int GetNotifyChargeStateDelay(const char* state) { return 3; }

  protected:
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
    virtual void NotifiedVehicleChargeTimermodeOn() {}
    virtual void NotifiedVehicleChargeTimermodeOff() {}
    virtual void NotifiedVehicleAux12vOn() {}
    virtual void NotifiedVehicleAux12vOff() {}
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
    virtual void NotifiedVehicleGenState(const std::string& state) {}

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

#ifdef CONFIG_OVMS_COMP_TPMS
  public:
    virtual bool TPMSRead(std::vector<uint32_t> *tpms);
    virtual bool TPMSWrite(std::vector<uint32_t> &tpms);
#endif // #ifdef CONFIG_OVMS_COMP_TPMS

  public:
    virtual std::vector<std::string> GetTpmsLayout();

  protected:
    uint32_t m_tpms_lastcheck;              // monotonictime of last TPMS alert check
    std::vector<short> m_tpms_laststate;    // last TPMS alert state for change detection

  protected:
    virtual void NotifyTpmsAlerts();

  public:
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t ProcessMsgCommand(std::string &result, int command, const char* args);

  public:
    virtual bool SetFeature(int key, const char* value);
    virtual const std::string GetFeature(int key);

  public:
    typedef struct
      {
      uint32_t txmoduleid;                      // transmission CAN ID (address), 0x7df = OBD2 broadcast
      uint32_t rxmoduleid;                      // expected response CAN ID or 0 for broadcasts
      uint16_t type;                            // UDS poll type / OBD2 "mode", see VEHICLE_POLL_TYPE_…
      union
        {
        uint16_t pid;                           // PID (shortcut for requests w/o payload)
        struct
          {
          uint16_t pid;                         // PID for requests with additional payload
          uint8_t datalen;                      // payload length
          uint8_t data[6];                      // payload data
          } args;
        };
      uint16_t polltime[VEHICLE_POLL_NSTATES];  // poll intervals in seconds for used poll states
      uint8_t  pollbus;                         // 0 = default CAN bus from PollSetPidList(), 1…4 = specific
      uint8_t  protocol;                        // ISOTP_STD / ISOTP_EXTADR
      } poll_pid_t;

  protected:
    OvmsRecMutex      m_poll_mutex;           // Concurrency protection for recursive calls
    uint8_t           m_poll_state;           // Current poll state
    canbus*           m_poll_bus;             // Bus to poll on
    canbus*           m_poll_bus_default;     // Bus default to poll on
    const poll_pid_t* m_poll_plist;           // Head of poll list
    const poll_pid_t* m_poll_plcur;           // Current position in poll list
    uint32_t          m_poll_ticker;          // Polling ticker
    uint8_t           m_poll_protocol;        // ISOTP_STD / ISOTP_EXTADR
    uint32_t          m_poll_moduleid_sent;   // ModuleID last sent
    uint32_t          m_poll_moduleid_low;    // Expected response moduleid low mark
    uint32_t          m_poll_moduleid_high;   // Expected response moduleid high mark
    uint16_t          m_poll_type;            // Expected type
    uint16_t          m_poll_pid;             // Expected PID
    uint16_t          m_poll_ml_remain;       // Bytes remaining for ML poll
    uint16_t          m_poll_ml_offset;       // Offset of ML poll
    uint16_t          m_poll_ml_frame;        // Frame number for ML poll
    uint8_t           m_poll_wait;            // Wait counter for a reply from a sent poll or bytes remaining.
                                              // Gets set = 2 when a poll is sent OR when bytes are remaining after receiving.
                                              // Gets set = 0 when a poll is received.
                                              // Gets decremented with every second/tick in PollerSend().
                                              // PollerSend() aborts when > 0.
                                              // Why set = 2: When a poll gets send just before the next ticker occurs
                                              //              PollerSend() decrements to 1 and doesn't send the next poll.
                                              //              Only when the reply doesn't get in until the next ticker occurs
                                              //              PollserSend() decrements to 0 and abandons the outstanding reply (=timeout)

  private:
    uint8_t           m_poll_sequence_max;    // Polls allowed to be sent in sequence per time tick (second), default 1, 0 = no limit
    uint8_t           m_poll_sequence_cnt;    // Polls already sent in the current time tick (second)
    uint8_t           m_poll_fc_septime;      // Flow control separation time for multi frame responses

  private:
    OvmsRecMutex      m_poll_single_mutex;    // PollSingleRequest() concurrency protection
    std::string*      m_poll_single_rxbuf;    // … response buffer
    int               m_poll_single_rxerr;    // … response error code (NRC) / TX failure code
    OvmsSemaphore     m_poll_single_rxdone;   // … response done (ok/error)

  protected:
    void PollSetPidList(canbus* bus, const poll_pid_t* plist);
    void PollSetState(uint8_t state);
    void PollSetThrottling(uint8_t sequence_max);
    void PollSetResponseSeparationTime(uint8_t septime);
    int PollSingleRequest(canbus* bus, uint32_t txid, uint32_t rxid,
                      std::string request, std::string& response,
                      int timeout_ms=100, uint8_t protocol=ISOTP_STD);
    int PollSingleRequest(canbus* bus, uint32_t txid, uint32_t rxid,
                      uint8_t polltype, uint16_t pid, std::string& response,
                      int timeout_ms=100, uint8_t protocol=ISOTP_STD);

  private:
    CanFrameCallback  m_poll_txcallback;      // Poller CAN TxCallback
    uint32_t          m_poll_txmsgid;         // Poller last TX CAN ID (frame MsgID)

  private:
    void PollerTxCallback(const CAN_frame_t* frame, bool success);
  protected:
    virtual void IncomingPollTxCallback(canbus* bus, uint32_t txid, uint16_t type, uint16_t pid, bool success);


  // BMS helpers
  protected:
    float* m_bms_voltages;                    // BMS voltages (current value)
    float* m_bms_vmins;                       // BMS minimum voltages seen (since reset)
    float* m_bms_vmaxs;                       // BMS maximum voltages seen (since reset)
    float* m_bms_vdevmaxs;                    // BMS maximum voltage deviations seen (since reset)
    short* m_bms_valerts;                     // BMS voltage deviation alerts (since reset)
    int m_bms_valerts_new;                    // BMS new voltage alerts since last notification
    int m_bms_vstddev_cnt;                    // BMS internal stddev counter
    float m_bms_vstddev_avg;                  // BMS internal stddev average
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
    float m_bms_defthr_vmaxgrad;              // Default voltage deviation max valid gradient [V]
    float m_bms_defthr_vmaxsddev;             // Default voltage deviation max valid stddev deviation [V]
    float m_bms_defthr_vwarn;                 // Default voltage deviation warn threshold [V]
    float m_bms_defthr_valert;                // Default voltage deviation alert threshold [V]
    float m_bms_defthr_twarn;                 // Default temperature deviation warn threshold [°C]
    float m_bms_defthr_talert;                // Default temperature deviation alert threshold [°C]
    uint32_t m_bms_vlog_last;                 // Last log time for voltages
    uint32_t m_bms_tlog_last;                 // Last log time for temperatures

  protected:
    void BmsSetCellArrangementVoltage(int readings, int readingspermodule);
    void BmsSetCellArrangementTemperature(int readings, int readingspermodule);
    void BmsSetCellDefaultThresholdsVoltage(float warn, float alert, float maxgrad=-1, float maxsddev=-1);
    void BmsSetCellDefaultThresholdsTemperature(float warn, float alert);
    void BmsSetCellLimitsVoltage(float min, float max);
    void BmsSetCellLimitsTemperature(float min, float max);
    void BmsSetCellVoltage(int index, float value);
    void BmsResetCellVoltages(bool full = false);
    void BmsSetCellTemperature(int index, float value);
    void BmsResetCellTemperatures(bool full = false);
    void BmsRestartCellVoltages();
    void BmsRestartCellTemperatures();
    void BmsTicker();
    virtual void NotifyBmsAlerts();

  public:
    int BmsGetCellArangementVoltage(int* readings=NULL, int* readingspermodule=NULL);
    int BmsGetCellArangementTemperature(int* readings=NULL, int* readingspermodule=NULL);
    void BmsGetCellDefaultThresholdsVoltage(float* warn, float* alert, float* maxgrad=NULL, float* maxsddev=NULL);
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
    typedef CNameMap<vehicle_t> map_vehicle_t;

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

  // Shell commands:
  protected:
    static int vehicle_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete);
    static void vehicle_module(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_wakeup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_homelink(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_climatecontrol(int verbosity, OvmsWriter* writer, bool on);
    static void vehicle_climatecontrol_on(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_climatecontrol_off(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_lock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_unlock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_valet(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_unvalet(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_charge_mode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_charge_current(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_charge_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_charge_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_charge_cooldown(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_stat(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void bms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void bms_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void bms_alerts(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void obdii_request(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  protected:
    static duk_ret_t DukOvmsVehicleType(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleWakeup(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleHomelink(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleClimateControl(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleLock(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleUnlock(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleValet(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleUnvalet(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleSetChargeMode(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleSetChargeCurrent(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleSetChargeTimer(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleStartCharge(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleStopCharge(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleStartCooldown(duk_context *ctx);
    static duk_ret_t DukOvmsVehicleStopCooldown(duk_context *ctx);
#endif // CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  };

extern OvmsVehicleFactory MyVehicleFactory;

#endif //#ifndef __VEHICLE_H__
