/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd July 2025
;
;    (C) 2025       Carsten Schmiemann
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

#ifndef __VEHICLE_NIU_GTEVO__
#define __VEHICLE_NIU_GTEVO__

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

// CAN buffer access macros
#define CAN_BYTE(b) data[b]
#define CAN_BIT(byte_index, bit_pos) ((CAN_BYTE(byte_index) >> (bit_pos)) & 0x01)

using namespace std;

#define TAG (OvmsVehicleNiuGTEVO::s_tag)

class OvmsVehicleNiuGTEVO : public OvmsVehicle
{
public:
  static const char *s_tag;

public:
  OvmsVehicleNiuGTEVO();
  ~OvmsVehicleNiuGTEVO();

  void CalculateEfficiency();

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  static void WebCfgCommon(PageEntry_t &p, PageContext_t &c);
#endif
  // Define OVMS vehicle and GT EVO specific functions
  void ConfigChanged(OvmsConfigParam *param) override;
  void EvoWakeup();
  void EvoShutdown();
  void IncomingFrameCan1(CAN_frame_t *p_frame) override;
  void SyncMetrics();

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  void WebInit();
  void WebDeInit();
#endif
  // Define GT EVO integration state machine vars
  bool BatteryAconnected = false;
  bool BatteryBconnected = false;
  bool ignitionDetected = false;
  bool chargerDetected = false;
  bool chargerEmulation = false;

  // Define charger emulation frames
  uint8_t charger_f1[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
  uint8_t charger_f2[8] = {0x02, 0x00, 0xF5, 0x0F, 0x00, 0x00, 0x1F, 0x00};
  uint8_t charger_f3[8] = {0x77, 0x00, 0x00, 0x00, 0xA3, 0x02, 0x00, 0x00};
  uint8_t charger_f4[8] = {0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00};
  uint8_t charger_f5[8] = {0x00, 0x00, 0x50, 0x00, 0xCD, 0x01, 0x00, 0x00};

protected:
  // Define internally used settings and CAN bus related vars
  int m_range_ideal = 0;
  int m_soh_user = 0;
  float m_battery_ah_user = 0;
  float m_battery_capacity = 0;
  int can_message_counter = 0;
  bool m_first_boot = false;
  uint32_t can_lastActivityTime = 0;
  uint32_t ignition_lastActivityTime = 0;
  uint32_t charger_lastActivityTime = 0;
  uint32_t batteryA_lastActivityTime = 0;
  uint32_t batteryB_lastActivityTime = 0;

  // Define and init battery arrays
  float batteryA_voltage = 0, batteryA_current = 0, batteryA_power = 0, batteryA_soc = 0, batteryA_soc_cycles = 0;
  float batteryB_voltage = 0, batteryB_current = 0, batteryB_power = 0, batteryB_soc = 0, batteryB_soc_cycles = 0;
  float batteryA_cell_voltage[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  float batteryB_cell_voltage[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  float batteryA_cell_temperature[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  float batteryB_cell_temperature[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  char batteryA_firmware_version[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  char batteryA_hardware_version[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  char batteryB_firmware_version[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  char batteryB_hardware_version[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

  // Define raw CAN values
  float can_vcu_speed = 0;
  float can_vcu_odometer = 0;
  float can_dcdc_12v_voltage = 0;
  float can_dcdc_12v_current = 0;
  float can_dcdc_12v_temperature = 0;
  float can_foc_temperature = 0;
  float can_foc_motor_temperature = 0;
  int can_foc_gear = 0;
  bool can_lcu_drl = false;
  bool can_lcu_lowbeam = false;
  bool can_lcu_autolight = false;
  bool can_vcu_sw_sidestand = false;
  bool can_vcu_sw_ready = false;
  bool can_vcu_sw_seat = false;

  // Define energy and time calculation as well as Ticker functions
  int calcMinutesRemaining(float charge_voltage, float charge_current);
  void ChargeStatistics();
  void EnergyStatistics();
  void Ticker10(uint32_t ticker) override;
  void Ticker1(uint32_t ticker) override;

  // Define NIU GT EVO Ph2 specific metrics
  OvmsMetricBool *mt_bus_awake;             // CAN bus awake status
  OvmsMetricBool *mt_bus_charger_emulation; // Charger emulation status
  OvmsMetricFloat *mt_pos_odometer_start;   // ODOmeter at trip start
  OvmsMetricBool *mt_batA_detected;         // Battery A connected and detected by CAN
  OvmsMetricBool *mt_batB_detected;         // Battery B connected and detected by CAN
  OvmsMetricFloat *mt_batA_voltage;         // Battery A voltage
  OvmsMetricFloat *mt_batB_voltage;         // Battery B voltage
  OvmsMetricFloat *mt_batA_current;         // Battery A current
  OvmsMetricFloat *mt_batB_current;         // Battery B current
  OvmsMetricFloat *mt_batA_cycles;          // Battery A soc cycles
  OvmsMetricFloat *mt_batB_cycles;          // Battery B soc cycles
  OvmsMetricString *mt_batA_firmware;       // Firmware and hardware versions
  OvmsMetricString *mt_batA_hardware;       // Firmware and hardware versions
  OvmsMetricString *mt_batB_firmware;       // Firmware and hardware versions
  OvmsMetricString *mt_batB_hardware;       // Firmware and hardware versions
  OvmsMetricFloat *mt_batA_soc;             // Battery A individual soc
  OvmsMetricFloat *mt_batB_soc;             // Battery B individual soc

public:
  vehicle_command_t CommandWakeup();
  vehicle_command_t CommandHomelink(int button, int durationms = 1000);
  // Shell commands:
  static void CommandDebug(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandTripReset(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandChargerEnable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);
  static void CommandChargerDisable(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv);

private:
  // Define helper vars for 25km rolling consumption calculation
  std::deque<std::pair<float, float>> consumptionHistory; // distance, consumption queue as data source for rolling consumption calc
  float totalConsumption = 0.0;                           // total aggregated battery consumption
  float totalDistance = 0.0;                              // total distance driven within rolling cons
  const int m_final_charge_phase_minutes = 60;            // Estimated minutes for the TAPER to 100% CV phase.
  const int m_final_charge_phase_soc = 85.0f;             // SOC where taper phase (CV) begins, it is not exactly every time on 85
};

#endif // #ifndef __VEHICLE_NIU_GTEVO__
