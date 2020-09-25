/**
 * Project:      Open Vehicle Monitor System
 * Module:       VW e-Up via OBD Port
 * 
 * (c) 2020 Soko
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __VEHICLE_VWEUP_OBD_H__
#define __VEHICLE_VWEUP_OBD_H__

#include "vehicle.h"
#include "poll_reply_helper.h"

// So I can easily swap between logging all Values as Info or as Debug
// #define VALUE_LOG(t, d, v1, v2) (ESP_LOGI(t, d, v1, v2))
#define VALUE_LOG(t, d, v1, v2) (ESP_LOGD(t, d, v1, v2))

// Car (poll) states
#define VWUP_OFF 0
#define VWUP_ON 1
#define VWUP_CHARGING 2

// ECUs TX/RX
#define VWUP_MOT_ELEC_TX 0x7E0
#define VWUP_MOT_ELEC_RX 0x7E8
#define VWUP_BAT_MGMT_TX 0x7E5
#define VWUP_BAT_MGMT_RX 0x7ED
#define VWUP_CHARGER_TX 0x744
#define VWUP_CHARGER_RX 0x7AE
#define VWUP_MFD_TX 0x714
#define VWUP_MFD_RX 0x77E

// PIDs of ECUs
#define VWUP_MOT_ELEC_SOC_NORM 0x1164
#define VWUP_BAT_MGMT_U 0x1E3B
#define VWUP_BAT_MGMT_I 0x1E3D
#define VWUP_BAT_MGMT_SOC 0x028C
#define VWUP_BAT_MGMT_ENERGY_COUNTERS 0x1E32
#define VWUP_BAT_MGMT_CELL_MAX 0x1E33
#define VWUP_BAT_MGMT_CELL_MIN 0x1E34
#define VWUP_BAT_MGMT_TEMP 0x2A0B
#define VWUP_CHARGER_EXTDIAG 0x03
#define VWUP_CHARGER_POWER_EFF 0x15D6
#define VWUP_CHARGER_POWER_LOSS 0x15E1
#define VWUP_CHARGER_AC_U 0x41FC
#define VWUP_CHARGER_AC_I 0x41FB
#define VWUP_CHARGER_DC_U 0x41F8
#define VWUP_CHARGER_DC_I 0x41F9
#define VWUP_MFD_ODOMETER 0x2203

class OvmsVehicleVWeUpObd : public OvmsVehicle
{
public:
    OvmsVehicleVWeUpObd();
    ~OvmsVehicleVWeUpObd();
    
    OvmsMetricFloat *BatMgmtSoC;           // Absolute SoC of main battery
    OvmsMetricFloat *BatMgmtCellDelta;     // Highest voltage - lowest voltage of all cells [V]

    OvmsMetricFloat *ChargerACPower;       // AC Power [kW]
    OvmsMetricFloat *ChargerDCPower;       // DC Power [kW]
    OvmsMetricFloat *ChargerPowerEffEcu;   // Efficiency of the Charger [%] (from ECU)
    OvmsMetricFloat *ChargerPowerLossEcu;  // Power loss of Charger [kW] (from ECU)
    OvmsMetricFloat *ChargerPowerEffCalc;  // Efficiency of the Charger [%] (calculated from U and I)
    OvmsMetricFloat *ChargerPowerLossCalc; // Power loss of Charger [kW] (calculated from U and I)

    void IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain);

protected:
    virtual void Ticker1(uint32_t ticker);

private:
    PollReplyHelper PollReply;

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

#endif //#ifndef __VEHICLE_VWEUP_OBD_H__