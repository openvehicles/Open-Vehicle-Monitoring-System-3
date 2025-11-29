/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th Apr 2022
;
;    (C) 2022       Carsten Schmiemann
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

#ifndef __VEHICLE_RENAULTZOE_PH2_H__
#define __VEHICLE_RENAULTZOE_PH2_H__

#include <atomic>

#include "can.h"
#include "vehicle.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "freertos/timers.h"

#include <deque>

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b) data[b]
#define CAN_UINT(b) (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b + 1))
#define CAN_UINT24(b) (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b + 1) << 8) | CAN_BYTE(b + 2))
#define CAN_UINT32(b) (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b + 1) << 16) | ((UINT)CAN_BYTE(b + 2) << 8) | CAN_BYTE(b + 3))
#define CAN_NIBL(b) (data[b] & 0x0f)
#define CAN_NIBH(b) (data[b] >> 4)
#define CAN_NIB(n) (((n) & 1) ? CAN_NIBL((n) >> 1) : CAN_NIBH((n) >> 1))
#define CAN_INT(b) ((int16_t)(((CAN_BYTE(b) << 8) | CAN_BYTE(b + 1))))
#define CAN_BIT(byte_index, bit_pos) ((CAN_BYTE(byte_index) >> (bit_pos)) & 0x01)

// Define Pollstates
#define POLLSTATE_OFF PollSetState(0);
#define POLLSTATE_ON PollSetState(1);
#define POLLSTATE_DRIVING PollSetState(2);
#define POLLSTATE_CHARGING PollSetState(3);

// Define V-CAN IDs - the name is the sending ECU - IDs in ascending order
#define ATCU_A5_ID 0x0A4
#define ECM_A6_ID 0x3C0
#define HEVC_A2_ID 0x3C3
#define BCM_A4_ID 0x418
#define HEVC_A1_ID 0x437
#define HEVC_R17_ID 0x480
#define BCM_A11_ID 0x491
#define HVAC_A2_ID 0x43E // scaling needs to be checked
#define BCM_A3_ID 0x46C
#define HFM_A1_ID 0x46F
#define USM_A2_ID 0x47C
#define BCM_A7_ID 0x47D
#define METER_A3_ID 0x47F
#define METER_A5_ID 0x5B9
#define BCM_A2_ID 0x5BA
#define HVAC_R3_ID 0x5C2
#define HEVC_A101_ID 0x5C5
#define HEVC_A23_ID 0x625
#define HEVC_A22_ID 0x6C9
#define HEVC_R11_ID 0x6E9
#define HEVC_R12_ID 0x6F2

// Define ISO-TP diagnostic request/response IDs
#define METER_DDT_REQ_ID 0x743
#define METER_DDT_RESP_ID 0x763
#define HVAC_DDT_REQ_ID 0x744
#define HVAC_DDT_RESP_ID 0x764
#define BCM_DDT_REQ_ID 0x745
#define BCM_DDT_RESP_ID 0x765

using namespace std;

#define TAG (OvmsVehicleRenaultZoePh2::s_tag)

class OvmsVehicleRenaultZoePh2 : public OvmsVehicle
{
public:
  static const char *s_tag;

public:
  OvmsVehicleRenaultZoePh2();
  ~OvmsVehicleRenaultZoePh2();

  void CalculateEfficiency();

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  static void WebCfgCommon(PageEntry_t &p, PageContext_t &c);
#endif
  // Define OVMS vehicle and zoe specific functions
  void ConfigChanged(OvmsConfigParam *param) override;
  void ZoeWakeUp();
  void ZoeShutdown();
  void IncomingFrameCan1(CAN_frame_t *p_frame) override;
  void SyncVCANtoMetrics();
  void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t *data, uint8_t length) override;
  void TxCallback(const CAN_frame_t* frame, bool success);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  void WebInit();
  void WebDeInit();
#endif
  // Define Zoe integration state machine vars
  bool CarIsCharging = false;
  bool CarPluggedIn = false;
  bool CarLastCharging = false;
  bool CarIsDriving = false;
  bool CarIsDrivingInit = false;
  float ACInputPowerFactor = 0.0;
  float Bat_cell_capacity_Ah = 0.0;
  bool LongPreCond = false;
  int LongPreCondCounter = 0;

  // V-CAN metrics
  float vcan_dcdc_current = 0;
  float vcan_dcdc_load = 0;
  float vcan_dcdc_voltage = 0;
  bool vcan_door_engineHood = false;
  bool vcan_door_fl = false;
  bool vcan_door_fr = false;
  bool vcan_door_rl = false;
  bool vcan_door_rr = false;
  bool vcan_door_trunk = false;
  float vcan_hvac_blower = 0;
  float vcan_hvac_cabin_setpoint = 0;
  float vcan_hvac_compressor_power = 0;
  float vcan_hvac_ptc_heater_active = 0;
  float vcan_hvBat_available_energy = 0;
  float vcan_hvBat_ChargeTimeRemaining = 0;
  float vcan_hvBat_estimatedRange = 0;
  float vcan_hvBat_max_chg_power = 0;
  float vcan_hvBat_SoC = 0;
  float vcan_hvBat_voltage = 0;
  bool vcan_light_brake = false;
  bool vcan_light_fogFront = false;
  bool vcan_light_fogRear = false;
  bool vcan_light_highbeam = false;
  bool vcan_light_lowbeam = false;
  float vcan_lastTrip_AuxCons = 0;
  float vcan_lastTrip_AvgCons = 0;
  float vcan_lastTrip_Cons = 0;
  float vcan_lastTrip_Distance = 0;
  float vcan_lastTrip_Recv = 0;
  float vcan_mains_available_chg_power = 0;
  int vcan_tyrePressure_FL = 0;
  int vcan_tyrePressure_FR = 0;
  int vcan_tyrePressure_RL = 0;
  int vcan_tyrePressure_RR = 0;
  int vcan_tyreState_FL = 0;
  int vcan_tyreState_FR = 0;
  int vcan_tyreState_RL = 0;
  int vcan_tyreState_RR = 0;
  bool vcan_vehicle_ChgPlugState = false;
  int vcan_vehicle_ChargeState = false;
  bool vcan_vehicle_LockState = false;
  float vcan_vehicle_ext_temp = 0;
  int vcan_vehicle_gear = 0;
  bool vcan_vehicle_ignition = false;
  float vcan_vehicle_speed = 0;

protected:
  // Define internally used settings and CAN bus related vars
  string zoe_obd_rxbuf;
  int m_range_ideal;
  int m_battery_capacity;
  bool m_UseCarTrip = false;
  bool m_UseBMScalculation = false;
  bool m_vcan_enabled = false;
  char zoe_vin[18] = "";
  int vcan_message_counter = 0;
  bool vcan_poller_state = false;
  uint32_t vcan_lastActivityTime = 0;

  // 12V auto-recharge settings and state
  bool m_auto_12v_recharge_enabled = false;
  float m_auto_12v_threshold = 12.4;
  bool m_dcdc_manual_enabled = false;
  bool m_dcdc_active = false;
  uint32_t m_dcdc_last_send_time = 0;
  uint32_t m_dcdc_start_time = 0;

  // Coming home function settings
  bool m_coming_home_enabled = false;
  bool m_last_locked_state = false;
  bool m_last_headlight_state = false;

  // Remote climate trigger (via key fob remote lighting button)
  bool m_remote_climate_enabled = false;
  uint32_t m_remote_climate_last_trigger_time = 0;  // Timestamp of last trigger to prevent multiple activations
  uint8_t m_remote_climate_last_button_state = 0;   // Last button state for edge detection

  // TX callback tracking for command verification
  std::atomic<uint32_t> m_tx_success_id;   // ID of last successfully transmitted frame
  std::atomic<bool> m_tx_success_flag;     // Flag indicating successful transmission

  // Define poll response functions by ECU
  void IncomingINV(uint16_t type, uint16_t pid, const char *data, uint16_t len);
  void IncomingEVC(uint16_t type, uint16_t pid, const char *data, uint16_t len);
  void IncomingBCM(uint16_t type, uint16_t pid, const char *data, uint16_t len);
  void IncomingLBC(uint16_t type, uint16_t pid, const char *data, uint16_t len);
  void IncomingHVAC(uint16_t type, uint16_t pid, const char *data, uint16_t len);
  void IncomingUCM(uint16_t type, uint16_t pid, const char *data, uint16_t len);

  // Define energy and time calculation as well as Ticker functions
  int calcMinutesRemaining(float charge_voltage, float charge_current);
  void ChargeStatistics();
  void EnergyStatisticsOVMS();
  void EnergyStatisticsBMS();
  void Ticker10(uint32_t ticker) override; // Handle charge, energy statistics
  void Ticker1(uint32_t ticker) override;  // Handle trip counter

  // Define Renault ZOE Ph2 specific metrics
  OvmsMetricBool *mt_bus_awake;                       // CAN bus awake status
  OvmsMetricBool *mt_poller_inhibit;                  // Inhibit ISO-TP poller for vehicle maintenance
  OvmsMetricFloat *mt_pos_odometer_start;             // ODOmeter at trip start
  OvmsMetricFloat *mt_bat_used_start;                 // Used battery kWh at trip start
  OvmsMetricFloat *mt_bat_recd_start;                 // Recd battery kWh at trip start
  OvmsMetricFloat *mt_bat_chg_start;                  // Charge battery kWh at charge start
  OvmsMetricFloat *mt_bat_max_charge_power;           // Battery max allowed charge, recd power
  OvmsMetricFloat *mt_bat_cycles;                     // Battery full charge cycles
  OvmsMetricFloat *mt_main_power_available;           // Mains power available
  OvmsMetricString *mt_main_phases;                   // Mains phases used
  OvmsMetricFloat *mt_main_phases_num;                // Mains phases used
  OvmsMetricString *mt_inv_status;                    // Inverter status string
  OvmsMetricFloat *mt_inv_hv_voltage;                 // Inverter battery voltage sense, used for motor power calc
  OvmsMetricFloat *mt_inv_hv_current;                 // Inverter battery current sense, used for motor power calc
  OvmsMetricFloat *mt_mot_temp_stator1;               // Temp of motor stator 1
  OvmsMetricFloat *mt_mot_temp_stator2;               // Temp of motor stator 2
  OvmsMetricFloat *mt_hvac_compressor_speed;          // Compressor speed
  OvmsMetricFloat *mt_hvac_compressor_speed_req;      // Compressor speed request
  OvmsMetricFloat *mt_hvac_compressor_pressure;       // Compressor high-side pressure in BAR
  OvmsMetricFloat *mt_hvac_compressor_power;          // Compressor power usage
  OvmsMetricFloat *mt_hvac_compressor_discharge_temp; // Compressor discharge temperature
  OvmsMetricFloat *mt_hvac_compressor_suction_temp;   // Compressor suction temperature
  OvmsMetricString *mt_hvac_compressor_mode;          // Compressor mode
  OvmsMetricFloat *mt_hvac_cooling_evap_setpoint;     // Cooling mode - evap temp setpoint
  OvmsMetricFloat *mt_hvac_cooling_evap_temp;         // Cooling mode - evap temp
  OvmsMetricFloat *mt_hvac_heating_cond_setpoint;     // Heating mode - cond temp setpoint
  OvmsMetricFloat *mt_hvac_heating_cond_temp;         // Heating mode - cond temp (before PTC)
  OvmsMetricFloat *mt_hvac_heating_temp;              // Heating mode - temp after PTC (Air outlet)
  OvmsMetricFloat *mt_hvac_heating_ptc_req;           // Heating mode - request PTCs (0-5) (compensation of cond_temp to temp)
  OvmsMetricFloat *mt_12v_dcdc_load;                  // DC-DC converter load in percent
  OvmsMetricFloat *mt_bat_cons_aux;                   // Aux power consumption trip (Compressor consumption)

public:
  // Renault ZOE commands
  vehicle_command_t CommandClimateControl(bool enable);
  vehicle_command_t CommandWakeup();
  vehicle_command_t CommandLock(const char *pin);
  vehicle_command_t CommandUnlock(const char *pin);
  vehicle_command_t CommandUnlockTrunk(const char *pin);
  vehicle_command_t CommandHomelink(int button, int durationms = 1000);
  vehicle_command_t CommandDDT(uint32_t txid, uint32_t rxid, bool ext_frame, string request);

  // Shell commands:
  static void CommandPollerStartIdle(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandPollerStop(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandLighting(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandUnlockTrunkShell(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandDebug(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandPollerInhibit(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandPollerResume(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandDdtHvacEnableCompressor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandDdtHvacDisableCompressor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandDdtClusterResetService(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandDdtHvacEnablePTC(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandDdtHvacDisablePTC(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandDcdcEnable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandDcdcDisable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);

  // DCDC control functions
  void SendDCDCMessage();
  void EnableDCDC(bool manual = true);
  void DisableDCDC();
  void CheckAutoRecharge();

  // Coming home function
  void TriggerComingHomeLighting();

private:
  // Define helper vars for 100km rolling consumption calculation
  std::deque<std::pair<float, float>> consumptionHistory; // distance, consumption queue as data source for rolling consumption calc
  float totalConsumption = 0.0;                           // total aggregated battery consumption
  float totalDistance = 0.0;                              // total distance driven withng rolling cons. calc needed if less than 200km
};

#endif // #ifndef __VEHICLE_RENAULTZOE_PH2_H__
