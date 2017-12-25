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

#include "vehicle.h"

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

void xks_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_tpms(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xks_cells(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
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

class OvmsVehicleKiaSoulEv : public OvmsVehicle
  {
  public:
    OvmsVehicleKiaSoulEv();
    ~OvmsVehicleKiaSoulEv();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    void Ticker1(uint32_t ticker);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void ConfigChanged(OvmsConfigParam* param);
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);
    vehicle_command_t CommandHandler(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    bool Send_SJB_Command( uint8_t b1, uint8_t b2, uint8_t b3);
    bool Send_BCM_Command( uint8_t b1, uint8_t b2, uint8_t b3);
    bool Send_SMK_Command( uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7);
    bool Send_EBP_Command( uint8_t b1, uint8_t b2);

    virtual OvmsVehicle::vehicle_command_t CommandLock(const char* pin);
    virtual OvmsVehicle::vehicle_command_t CommandUnlock(const char* pin);

    bool OpenTrunk(const char* password);
    bool OpenChargePort(const char* password);
    bool ACCRelay(bool,const char *password);
    bool IGN1Relay(bool,const char *password);
    bool IGN2Relay(bool,const char *password);
    bool StartRelay(bool,const char *password);

    char m_vin[18];

    uint32_t ks_tpms_id[4];
    uint8_t ks_battery_cell_voltage[101];
    OvmsMetricInt* 		m_b_cell_volt_max_no;		//Max cell voltage no           02 21 01 -> 23 7
    OvmsMetricInt* 		m_b_cell_volt_min_no; 	//Min cell voltage no           02 21 01 -> 24 2
    OvmsMetricFloat*	m_b_cell_volt_max;     // Battery cell maximum voltage
    OvmsMetricFloat*	m_b_cell_volt_min;     // Battery cell minimum voltage
    OvmsMetricInt* 		m_b_cell_det_max_no; 		//02 21 05 -> 24 3
    OvmsMetricInt*		m_b_cell_det_min_no; 		//02 21 05 -> 24 6
    OvmsMetricFloat*	m_b_cell_det_max;      // Battery cell maximum detoriation
    OvmsMetricFloat*	m_b_cell_det_min;      // Battery cell minimum detoriation
    OvmsMetricInt* 		m_b_min_temperature; 			//02 21 05 -> 21 7
    OvmsMetricInt*		m_b_inlet_temperature; 		//02 21 05 -> 21 6
    OvmsMetricInt*		m_b_max_temperature; 			//02 21 05 -> 22 1
    OvmsMetricInt*		m_b_heat_1_temperature; 	//02 21 05 -> 23 6
    OvmsMetricInt*		m_b_heat_2_temperature; 	//02 21 05 -> 23 7

    OvmsMetricBool*		m_v_env_lowbeam;
    OvmsMetricBool*		m_v_env_highbeam;

    OvmsMetricFloat* m_obc_pilot_duty;

    OvmsMetricFloat* m_ldc_out_voltage;
    OvmsMetricFloat* m_ldc_out_current;
    OvmsMetricFloat* m_ldc_in_voltage;
    OvmsMetricFloat* m_ldc_temperature;

  protected:
    void IncomingTPMS(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingOBC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingVMCU(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingBMC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingLDC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void RequestNotify(unsigned int which);
    void DoNotify();
    void vehicle_kiasoulev_car_on(bool isOn);
    void UpdateMaxRangeAndSOH(void);
    uint16_t calcMinutesRemaining(float target);
    void SendCanMessage(uint16_t id, uint8_t count,
						uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
						uint8_t b5, uint8_t b6);
    bool SendCanMessage_sync(uint16_t id, uint8_t count,
    					uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
						uint8_t b5, uint8_t b6);
    bool SendCommandInSessionMode(uint16_t id, uint8_t count,
    					uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
						uint8_t b5, uint8_t b6 );
    void SendTesterPresent(uint16_t id, uint8_t length);
    bool SetSessionMode(uint16_t id, uint8_t mode);
    bool SetDoorLock(bool open, const char* password);
    bool LeftIndicator(bool);
    bool RightIndicator(bool);
    bool RearDefogger(bool);
    bool IsPasswordOk(const char *password);
    void SetChargeMetrics(float voltage, float current, float climit, bool chademo);

    OvmsCommand *cmd_xks;

    // Kia Soul EV specific metrics
    OvmsMetricString* m_version;
    OvmsMetricFloat*  m_c_power;            				// Available charge power
    OvmsMetricFloat*  m_c_speed;									// km/h

		#define CFG_DEFAULT_MAXRANGE 160
    int ks_maxrange = CFG_DEFAULT_MAXRANGE;        // Configured max range at 20 °C

		#define CGF_DEFAULT_BATTERY_CAPACITY 27000
    float ks_battery_capacity = CGF_DEFAULT_BATTERY_CAPACITY; //TODO Detect battery capacity from VIN or number of batterycells

    unsigned int ks_notifications = 0;

    float ks_obc_volt;
    KsShiftBits ks_shift_bits;

    float ks_trip_start_odo;
    float ks_last_soc;
    float ks_last_ideal_range;
    uint8_t ks_bms_soc;
    float ks_start_cdc; 					// Used to calculate trip power use (Cumulated discharge)
    float ks_start_cc;  					// Used to calculate trip recuperation (Cumulated charge)
    float ks_cum_charge_start; 		// Used to calculate charged power.

    INT ks_battery_current; 			// Temporary storage for Battery current: 0x7ec 02 21 01 -> 21 7+22 1

    bool ks_openChargePort;				// Tells the Ticker1 to open charge port
    bool ks_key_fob_open_charge_port;	// Enable/disable open charge port using trunk-button on key fob

    uint32_t ks_battery_cum_charge_current; 		//Cumulated charge current    02 21 01 -> 24 6+7 & 25 1+2
    uint32_t ks_battery_cum_discharge_current;	//Cumulated discharge current 02 21 01 -> 25 3-6
    uint32_t ks_battery_cum_charge; 						//Cumulated charge power      02 21 01 -> 25 7 + 26 1-3
    uint32_t ks_battery_cum_discharge; 				//Cumulated discharge power   02 21 01 -> 26 4-7
    uint32_t ks_battery_cum_op_time; 					//Cumulated operating time    02 21 01 -> 27 1-4

    int8_t ks_battery_module_temp[8];

    uint8_t ks_heatsink_temperature; //TODO Remove?
    uint8_t ks_battery_fan_feedback;

    bool ks_ldc_enabled;

    bool ks_aux_bat_ok;

    struct {
      unsigned char ChargingChademo : 1;
      unsigned char ChargingJ1772 : 1;
      unsigned char : 1;
      unsigned char : 1;
      unsigned char FanStatus : 4;
    } ks_charge_bits;

    struct {
      uint8_t byte[8];
      uint8_t status;
      uint16_t id;
    } ks_send_can;

    const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
  };

#define SQR(n) ((n)*(n))
#define ABS(n) (((n) < 0) ? -(n) : (n))
#define MIN(n,m) ((n) < (m) ? (n) : (m))
#define MAX(n,m) ((n) > (m) ? (n) : (m))
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))

#define XSTR(x)   STR(x)
#define STR(x)    #x

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     data[b]
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b+1) << 16)  | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (can_databuffer[b] & 0x0f)
#define CAN_NIBH(b)     (can_databuffer[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))

#define TO_CELCIUS(n)	((float)n-40)
#define TO_PSI(n)		((float)n/4.0)

#define BAT_SOC			StdMetrics.ms_v_bat_soc->AsFloat(100)
#define BAT_SOH			StdMetrics.ms_v_bat_soh->AsFloat(100)
#define LIMIT_SOC		StdMetrics.ms_v_charge_limit_soc->AsFloat(0)
#define LIMIT_RANGE		StdMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers)
#define EST_RANGE		StdMetrics.ms_v_bat_range_est->AsFloat(100, Kilometers)
#define IDEAL_RANGE		StdMetrics.ms_v_bat_range_ideal->AsFloat(100, Kilometers)
#define FULL_RANGE		StdMetrics.ms_v_bat_range_full->AsFloat(160, Kilometers)
#define POS_ODO			StdMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers)
#define CHARGE_CURRENT	StdMetrics.ms_v_charge_current->AsFloat(0, Amps)
#define CHARGE_VOLTAGE	StdMetrics.ms_v_charge_voltage->AsFloat(0, Volts)
#define SET_CHARGE_STATE(n)		StdMetrics.ms_v_charge_state->SetValue(n)
#define CUM_CHARGE		((float)ks_battery_cum_charge/10.0)
#define CUM_DISCHARGE	((float)ks_battery_cum_discharge/10.0)
#define SET_TPMS_ID(n, v)	if (v > 0) ks_tpms_id[n] = v;

#define VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID 0x2F	// InputOutputControlByCommonID
#define VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_LOC_ID	0x30 	// InputOutputControlByLocalID
#define VEHICLE_POLL_TYPE_OBDII_TESTER_PRESENT	0x3E 	// TesterPresent

#define DEFAULT_SESSION 0x01
#define PROGRAMMING_SESSION 0x02
#define EXTENDED_DIAGNOSTIC_SESSION 0x03
#define SAFETY_SYSTEM_DIAGNOSTIC_SESSION 0x04

// ECUs
#define SMART_JUNCTION_BOX 0x771
#define BODY_CONTROL_MODULE  0x7A0
#define SMART_KEY_UNIT 0x7A5
#define ABS_EBP_UNIT 0x7A5

// Notifications:
#define SEND_AuxBattery_Low           (1<< 0)  // text alert: AUX battery problem
//#define SEND_PowerNotify            (1<< 1)  // text alert: power usage summary
//#define SEND_DataUpdate             (1<< 2)  // regular data update (per minute)
//#define SEND_StreamUpdate           (1<< 3)  // stream data update (per second)
//#define SEND_BatteryStats           (1<< 4)  // separate battery stats (large)
//#define SEND_CodeAlert              (1<< 5)  // text alert: fault code (SEVCON/inputs/...)
//#define SEND_PowerLog               (1<< 6)  // RT-PWR-Log history entry
//#define SEND_ResetResult            (1<< 7)  // text alert: RESET OK/FAIL
#define SEND_ChargeState            (1<< 8)  // text alert: STAT command

#endif //#ifndef __VEHICLE_KIASOULEV_H__
