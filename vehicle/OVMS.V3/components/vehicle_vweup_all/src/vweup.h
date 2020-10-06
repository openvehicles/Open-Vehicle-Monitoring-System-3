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

#include "vehicle.h"
#include "ovms_webserver.h"
#include "poll_reply_helper.h"

#define DEFAULT_MODEL_YEAR 2020

using namespace std;

typedef enum
  {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  AUTO_DISABLE_CLIMATE_CONTROL
  } RemoteCommand;

// So I can easily swap between logging all Values as Info or as Debug
// #define VALUE_LOG(t, d, v1, v2) (ESP_LOGI(t, d, v1, v2))
#define VALUE_LOG(t, d, v1, v2) (ESP_LOGD(t, d, v1, v2))

// Car (poll) states
#define VWUP_OFF 0
#define VWUP_ON 1
#define VWUP_CHARGING 2
/*
// ECUs TX/RX
#define VWUP_MOT_ELEC_TX 0x7E0  //ECU 01 motor elecronics
#define VWUP_MOT_ELEC_RX 0x7E8
#define VWUP_BAT_MGMT_TX 0x7E5  //ECU 8C hybrid battery management
#define VWUP_BAT_MGMT_RX 0x7ED
#define VWUP_ELD_TX 0x7E6  //ECU 51 electric drive
#define VWUP_ELD_RX 0x7EE
#define VWUP_CHG_TX 0x744 //ECU C6 high voltage charger
#define VWUP_CHG_RX 0x7AE
#define VWUP_MFD_TX 0x714 //ECU 17 multi-function display
#define VWUP_MFD_RX 0x77E
#define VWUP_BRK_TX 0x713 //ECU 03 brake electronics
#define VWUP_BRK_RX 0x77D


// PIDs of ECUs
#define VWUP_MOT_ELEC_SOC_NORM 0x1164
#define VWUP_BAT_MGMT_U 0x1E3B
#define VWUP_BAT_MGMT_I 0x1E3D
#define VWUP_BAT_MGMT_SOC 0x028C
#define VWUP_BAT_MGMT_ENERGY_COUNTERS 0x1E32
#define VWUP_BAT_MGMT_CELL_MAX 0x1E33
#define VWUP_BAT_MGMT_CELL_MIN 0x1E34
#define VWUP_BAT_MGMT_TEMP 0x2A0B
#define VWUP_CHG_EXTDIAG 0x03
#define VWUP_CHG_POWER_EFF 0x15D6
#define VWUP_CHG_POWER_LOSS 0x15E1
#define VWUP1_CHG_AC_U 0x1DA9
#define VWUP1_CHG_AC_I 0x1DA8
#define VWUP1_CHG_DC_U 0x1DA7
#define VWUP1_CHG_DC_I 0x4237
#define VWUP2_CHG_AC_U 0x41FC
#define VWUP2_CHG_AC_I 0x41FB
#define VWUP2_CHG_DC_U 0x41F8
#define VWUP2_CHG_DC_I 0x41F9
#define VWUP_MFD_ODOMETER 0x2203
#define VWUP_MFD_MAINT_DIST 0x2260
#define VWUP_MFD_MAINT_TIME 0x2261
#define VWUP_BRK_TPMS 0x1821
#define VWUP_MOT_TEMP_AMB 0xF446
#define VWUP_MOT_TEMP_DCDC 0x116F
#define VWUP_MOT_TEMP_PEM 0x1116
#define VWUP_ELD_TEMP_PEM 0x3EB5
#define VWUP_ELD_TEMP_MOT 0x3E94
//#define VWUP__TEMP_CABIN 0x
*/
class OvmsVehicleVWeUpAll : public OvmsVehicle
  {
  public:
    OvmsVehicleVWeUpAll();
    ~OvmsVehicleVWeUpAll();

  public:
    void ConfigChanged(OvmsConfigParam* param);
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);

  public:
    void IncomingFrameCan3(CAN_frame_t* p_frame);

  protected:
    virtual void Ticker1(uint32_t ticker);
    char m_vin[18];

  public:
    vehicle_command_t CommandHomelink(int button, int durationms=1000);
    vehicle_command_t CommandClimateControl(bool enable);
    vehicle_command_t CommandLock(const char* pin);
    vehicle_command_t CommandUnlock(const char* pin);
    vehicle_command_t CommandStartCharge();
    vehicle_command_t CommandStopCharge();
    vehicle_command_t CommandActivateValet(const char* pin);
    vehicle_command_t CommandDeactivateValet(const char* pin);
    void RemoteCommandTimer();
    void CcDisableTimer();
    bool vin_part1;
    bool vin_part2;
    bool vin_part3;
    bool vwup_enable_obd;
    bool vwup_enable_t26;
    bool vwup_enable_write;
    int vwup_modelyear;
    int vwup_remote_climate_ticker;
    int vwup_cc_temp_int;
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
    void SendCommand(RemoteCommand);
    OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);

    RemoteCommand vwup_remote_command; // command to send, see RemoteCommandTimer()

    void vehicle_vweup_car_on(bool isOn);
    TimerHandle_t m_sendOcuHeartbeat;
    TimerHandle_t m_ccCountdown;

  public:
    void WebInit();
    void WebDeInit();
    void SendOcuHeartbeat();
    void CCCountdown();
    void CCOn();
    void CCOnP();
    void CCOff();
    static void ccCountdown(TimerHandle_t timer);
    static void sendOcuHeartbeat(TimerHandle_t timer);

    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgClimate(PageEntry_t& p, PageContext_t& c);
    static void WebDispStdMetrics(PageEntry_t& p, PageContext_t& c);
    virtual vehicle_command_t CommandWakeup();

    OvmsMetricFloat *BatMgmtSoC;           // Absolute SoC of main battery
    OvmsMetricFloat *BatMgmtCellDelta;     // Highest voltage - lowest voltage of all cells [V]

    OvmsMetricFloat *ChargerACPower;       // AC Power [kW]
    OvmsMetricFloat *ChargerDCPower;       // DC Power [kW]
    OvmsMetricFloat *ChargerPowerEffEcu;   // Efficiency of the Charger [%] (from ECU)
    OvmsMetricFloat *ChargerPowerLossEcu;  // Power loss of Charger [kW] (from ECU)
    OvmsMetricFloat *ChargerPowerEffCalc;  // Efficiency of the Charger [%] (calculated from U and I)
    OvmsMetricFloat *ChargerPowerLossCalc; // Power loss of Charger [kW] (calculated from U and I)

    OvmsMetricFloat *TPMSDiffusionFrontLeft; // TPMS Indicator for Pressure Diffusion Front Left Tyre
    OvmsMetricFloat *TPMSDiffusionFrontRight; // TPMS Indicator for Pressure Diffusion Front Right Tyre
    OvmsMetricFloat *TPMSDiffusionRearLeft; // TPMS Indicator for Pressure Diffusion Rear Left Tyre
    OvmsMetricFloat *TPMSDiffusionRearRight; // TPMS Indicator for Pressure Diffusion Rear Right Tyre
    OvmsMetricFloat *TPMSEmergencyFrontLeft; // TPMS Indicator for Tyre Emergency Front Left Tyre
    OvmsMetricFloat *TPMSEmergencyFrontRight; // TPMS Indicator for Tyre Emergency Front Right Tyre
    OvmsMetricFloat *TPMSEmergencyRearLeft; // TPMS Indicator for Tyre Emergency Rear Left Tyre
    OvmsMetricFloat *TPMSEmergencyRearRight; // TPMS Indicator for Tyre Emergency Rear Right Tyre
    OvmsMetricFloat *MaintenanceDist; // Distance to next maintenance
    OvmsMetricFloat *MaintenanceTime; // Days to next maintenance

    //OBD
    void ObdInit();
    void IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain);

//  protected:
//    virtual void Ticker1(uint32_t ticker);

  private:
    PollReplyHelperAll PollReply;

    float BatMgmtCellMax; // Maximum cell voltage
    float BatMgmtCellMin; // Minimum cell voltage

    float ChargerAC1U; // AC Voltage Phase 1
    float ChargerAC2U; // AC Voltage Phase 2
    float ChargerAC1I; // AC Current Phase 1
    float ChargerAC2I; // AC Current Phase 2
    float ChargerDC1U; // DC Voltage 1
    float ChargerDC2U; // DC Voltage 2
    float ChargerDC1I; // DC Current 1
    float ChargerDC2I; // DC Current 2

    void CheckCarState();

    uint32_t TimeOffRequested; // For Off-Timeout: Monotonictime when the poll should have gone to VWUP_OFF
                               //                  0 means no Off requested so far

    bool IsOff() { return m_poll_state == VWUP_OFF; }
    bool IsOn() { return m_poll_state == VWUP_ON; }
    bool IsCharging() { return m_poll_state == VWUP_CHARGING; }

  };

#endif //#ifndef __VEHICLE_EUP_H__
