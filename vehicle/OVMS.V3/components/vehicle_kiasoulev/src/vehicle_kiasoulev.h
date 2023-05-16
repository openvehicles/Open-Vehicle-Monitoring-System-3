/*
;    Project:       Open Vehicle Monitor System
;    Date:          22th October 2017
;
;    Changes:
;    1.0  Initial stub
;
;    (C) 2017        Geir Øyvind Vælidalo <geir@validalo.net>
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

#ifndef __VEHICLE_KIASOULEV_H__
#define __VEHICLE_KIASOULEV_H__

#include "kia_common.h"
#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;

typedef union {
  struct { //TODO Is this the correct order, or should it be swapped?
    unsigned char Park : 1;
    unsigned char Reverse : 1;
    unsigned char Neutral : 1;
    unsigned char Drive : 1;
    unsigned char Break : 1;
    unsigned char ECOOff : 1;
    unsigned char : 1;
    unsigned char : 1;
  };
  unsigned char value;
} KsShiftBits;

void xks_trip_since_parked(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_trip_since_charge(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_tpms(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_aux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_vin(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void CommandOpenTrunk(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void CommandOpenChargePort(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void CommandParkBreakService(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_sjb(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_bcm(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_ign1(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_ign2(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_acc_relay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_start_relay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_set_head_light_delay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_set_one_touch_turn_signal(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_set_auto_door_unlock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_set_auto_door_lock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

class OvmsVehicleKiaSoulEv : public KiaVehicle
  {
  public:
    OvmsVehicleKiaSoulEv();
    ~OvmsVehicleKiaSoulEv();

  public:
    void IncomingFrameCan1(const CAN_frame_t* p_frame) override;
    void IncomingFrameCan2(const CAN_frame_t* p_frame) override;
    void Ticker1(uint32_t ticker);
    void Ticker10(uint32_t ticker);
    void Ticker300(uint32_t ticker);
    void EventListener(std::string event, void* data);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlremain) override;
    void ConfigChanged(OvmsConfigParam* param);
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);
    vehicle_command_t CommandHandler(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    bool Send_SJB_Command( uint8_t b1, uint8_t b2, uint8_t b3);
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

    virtual OvmsVehicle::vehicle_command_t CommandLock(const char* pin);
    virtual OvmsVehicle::vehicle_command_t CommandUnlock(const char* pin);

    bool OpenTrunk(const char* password);
    bool OpenChargePort(const char* password);
    bool ACCRelay(bool,const char *password);
    bool IGN1Relay(bool,const char *password);
    bool IGN2Relay(bool,const char *password);
    bool StartRelay(bool,const char *password);
    void SetHeadLightDelay(bool);
    void SetOneThouchTurnSignal(uint8_t);
    void SetAutoDoorUnlock(uint8_t);
    void SetAutoDoorLock(uint8_t);
    int8_t GetDoorLockStatus();

    char m_street[128]; //Current street
    int m_street_pos;

    OvmsMetricFloat* m_v_env_climate_temp;
    OvmsMetricBool*  m_v_env_climate_driver_only;
    OvmsMetricBool*  m_v_env_climate_resirc;
    OvmsMetricBool*  m_v_env_climate_auto;
    OvmsMetricBool*  m_v_env_climate_ac;
    OvmsMetricInt*	  m_v_env_climate_fan_speed;
    OvmsMetricInt*	  m_v_env_climate_mode;

    OvmsMetricInt*	  m_v_pos_dist_to_dest;
    OvmsMetricInt*	  m_v_pos_arrival_hour;
    OvmsMetricInt*	  m_v_pos_arrival_minute;
    OvmsMetricString*	  m_v_pos_street;

    bool  ks_emergency_message_sent;

  protected:
    void HandleCharging();
    void HandleChargeStop();
    void IncomingTPMS(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingOBC(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingVMCU(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingBMC(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingLDC(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingSJB(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlremain);
    void RequestNotify(unsigned int which);
    void DoNotify();
    void vehicle_kiasoulev_car_on(bool isOn);
    void UpdateMaxRangeAndSOH(void);
    uint16_t calcMinutesRemaining(float target);
    bool SetDoorLock(bool open, const char* password);
    bool LeftIndicator(bool);
    bool RightIndicator(bool);
    bool RearDefogger(bool);
    bool BlueChargeLed(bool on, uint8_t mode);
    void SetChargeMetrics(float voltage, float current, float climit, bool chademo);
    void SendTesterPresentMessages();
    void StopTesterPresentMessages();

    OvmsCommand *cmd_xks;

		#define CFG_DEFAULT_MAXRANGE 160
    int ks_maxrange = CFG_DEFAULT_MAXRANGE;        // Configured max range at 20 °C

		#define CGF_DEFAULT_BATTERY_CAPACITY 27000
    float ks_battery_capacity = CGF_DEFAULT_BATTERY_CAPACITY; //TODO Detect battery capacity from VIN or number of batterycells

    unsigned int ks_notifications = 0;

    KsShiftBits ks_shift_bits;

    int16_t ks_battery_current; 			// Temporary storage for Battery current: 0x7ec 02 21 01 -> 21 7+22 1

    bool ks_openChargePort;				// Tells the Ticker1 to open charge port
    bool ks_key_fob_open_charge_port;	// Enable/disable open charge port using trunk-button on key fob

    bool ks_charge_timer_off; //True if the charge timer button is on

    uint8_t ks_heatsink_temperature; //TODO Remove?
    uint8_t ks_battery_fan_feedback;

    int16_t sjb_tester_present_seconds;
    int16_t smk_tester_present_seconds;

    uint32_t ks_clock;
    int16_t ks_utc_diff;

    bool ks_ldc_enabled;

    struct {
      unsigned char ChargingChademo : 1;
      unsigned char ChargingJ1772 : 1;
      unsigned char : 1;
      unsigned char : 1;
      unsigned char FanStatus : 4;
    } ks_charge_bits;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //  - implementation: ks_web.(h,cpp)
    //

    public:
      virtual void WebInit();
      static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
      static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
      static void WebBattMon(PageEntry_t& p, PageContext_t& c);

    public:
      void GetDashboardConfig(DashboardConfig& cfg);
#endif //CONFIG_OVMS_COMP_WEBSERVER
  };

#define SET_SJB_TP_TIMEOUT(n)	sjb_tester_present_seconds = on ? MAX(sjb_tester_present_seconds, n) : 0;
#define SET_SMK_TP_TIMEOUT(n)	smk_tester_present_seconds = on ? MAX(smk_tester_present_seconds, n) : 0;

// Notifications:
//#define SEND_AuxBattery_Low           (1<< 0)  // text alert: AUX battery problem
//#define SEND_PowerNotify            (1<< 1)  // text alert: power usage summary
//#define SEND_DataUpdate             (1<< 2)  // regular data update (per minute)
//#define SEND_StreamUpdate           (1<< 3)  // stream data update (per second)
//#define SEND_BatteryStats           (1<< 4)  // separate battery stats (large)
#define SEND_EmergencyAlert           (1<< 5)  // Emergency lights are turned on
#define SEND_EmergencyAlertOff        (1<< 6)  // Emergency lights are turned off
//#define SEND_ResetResult            (1<< 7)  // text alert: RESET OK/FAIL
//#define SEND_ChargeState            (1<< 8)  // text alert: STAT command

#endif //#ifndef __VEHICLE_KIASOULEV_H__
