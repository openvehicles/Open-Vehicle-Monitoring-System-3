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
/*
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
#define VWEUP_OFF 0
#define VWEUP_ON 1
#define VWEUP_CHARGING 2

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
    bool vweup_enable_obd;
    bool vweup_enable_t26;
    bool vweup_enable_write;
    int vweup_modelyear;
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
    void SendCommand(RemoteCommand);
    OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);

    RemoteCommand vweup_remote_command; // command to send, see RemoteCommandTimer()

    void vehicle_vweup_car_on(bool isOn);
    TimerHandle_t m_sendOcuHeartbeat;
    TimerHandle_t m_ccCountdown;

  public:
//    void WebInit();
//    void WebDeInit();
    void SendOcuHeartbeat();
    void CCCountdown();
    void CCOn();
    void CCOnP();
    void CCOff();
    static void ccCountdown(TimerHandle_t timer);
    static void sendOcuHeartbeat(TimerHandle_t timer);

    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgClimate(PageEntry_t& p, PageContext_t& c);
    static void WebDispChgMetrics(PageEntry_t& p, PageContext_t& c);
    static void WebDispTempMetrics(PageEntry_t& p, PageContext_t& c);
    virtual vehicle_command_t CommandWakeup();

    OvmsMetricFloat *MotElecSoCAbs;           // Absolute SoC of main battery from motor electrics ECU
    OvmsMetricFloat *MotElecSoCNorm;           // Normalized SoC of main battery from motor electrics ECU
    OvmsMetricFloat *BatMgmtSoCAbs;           // Absolute SoC of main battery from battery management ECU
    OvmsMetricFloat *ChgMgmtSoCNorm;           // Normalized SoC of main battery from charge management ECU
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
    OvmsMetricFloat *CoolantTemp1; //
    OvmsMetricFloat *CoolantTemp2; //
    OvmsMetricFloat *CoolantTemp3; //
    OvmsMetricFloat *CoolantTemp4; //
    OvmsMetricFloat *CoolantTemp5; //
    OvmsMetricFloat *CoolingTempBat; //
    OvmsMetricFloat *BrakeboostTempECU;
    OvmsMetricFloat *BrakeboostTempAccu;
    OvmsMetricFloat *SteeringTempPA;
    OvmsMetricFloat *ElectricDriveCoolantTemp;
    OvmsMetricFloat *ElectricDriveTempDCDC;
    OvmsMetricFloat *ElectricDriveTempDCDCPCB;
    OvmsMetricFloat *ElectricDriveTempDCDCPEM;
    OvmsMetricFloat *ElectricDriveTempPhaseU;
    OvmsMetricFloat *ElectricDriveTempPhaseV;
    OvmsMetricFloat *ElectricDriveTempPhaseW;
    OvmsMetricFloat *ElectricDriveTempStator;
    OvmsMetricFloat *InfElecTempPCB;
    OvmsMetricFloat *InfElecTempAudio;
    OvmsMetricFloat *BatTempMax;
    OvmsMetricFloat *BatTempMin;
    OvmsMetricFloat *BrakesensTemp;

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

    uint32_t TimeOffRequested; // For Off-Timeout: Monotonictime when the poll should have gone to VWEUP_OFF
                               //                  0 means no Off requested so far

    bool IsOff() { return m_poll_state == VWEUP_OFF; }
    bool IsOn() { return m_poll_state == VWEUP_ON; }
    bool IsCharging() { return m_poll_state == VWEUP_CHARGING; }

  };

#endif //#ifndef __VEHICLE_EUP_H__
*/