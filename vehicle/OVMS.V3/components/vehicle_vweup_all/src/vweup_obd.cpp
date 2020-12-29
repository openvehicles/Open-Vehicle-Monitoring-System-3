/**
 * Project:      Open Vehicle Monitor System
 * Module:       OBD for VW e-Up
 *
 * (c) 2020  sharkcow <sharkcow@gmx.de>
 * (c) 2019  Anko Hanse <anko_hanse@hotmail.com>
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
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

#include "ovms_log.h"
static const char *TAG = "v-vweup";

#define VERSION "0.1.3"

#include <stdio.h>
#include <string>
#include <iomanip>

#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_utils.h"

#include "vehicle_vweup_all.h"
#include "vweup_obd.h"

const OvmsVehicle::poll_pid_t vweup_polls[] = {
    // Note: poller ticker cycles at 3600 seconds = max period
    // { txid, rxid, type, pid, { VWEUP_OFF, VWEUP_ON, VWEUP_CHARGING }, bus }

    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_U, {0, 1, 5}, 1, ISOTP_STD},
//  Moved to ObdInit:    
//    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_I, {30, 1, 5}, 1}, // 30 in OFF needed: Car gets started with full 12V battery
    // Same tick & order important of above 2: VWUP_BAT_MGMT_I calculates the power

    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_SOC_NORM, {0, 20, 0}, 1, ISOTP_STD},
    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_SOC_ABS, {0, 20, 0}, 1, ISOTP_STD},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_SOC_ABS, {0, 20, 20}, 1, ISOTP_STD},
    {VWUP_CHG_MGMT_TX, VWUP_CHG_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHG_MGMT_SOC_NORM, {0, 0, 20}, 1, ISOTP_STD},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_ENERGY_COUNTERS, {0, 20, 20}, 1, ISOTP_STD},

    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_CELL_MAX, {0, 20, 20}, 1, ISOTP_STD},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_CELL_MIN, {0, 20, 20}, 1, ISOTP_STD},
    // Same tick & order important of above 2: VWUP_BAT_MGMT_CELL_MIN calculates the delta

    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_TEMP, {0, 20, 20}, 1, ISOTP_STD},

    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIISESSION, VWUP_CHG_EXTDIAG, {0, 30, 30}, 1, ISOTP_STD},

    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHG_POWER_EFF, {0, 5, 10}, 1, ISOTP_STD}, // 5 @ VWEUP_ON to detect charging
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHG_POWER_LOSS, {0, 0, 10}, 1, ISOTP_STD},

    {VWUP_MFD_TX, VWUP_MFD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MFD_ODOMETER, {0, 60, 60}, 1, ISOTP_STD},

//    {VWUP_BRK_TX, VWUP_BRK_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BRK_TPMS, {0, 5, 5}, 1},
//    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_TEMP_AMB, {0, 5, 0}, 1},
    {VWUP_MFD_TX, VWUP_MFD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MFD_MAINT_DIST, {0, 60, 60}, 1, ISOTP_STD},
    {VWUP_MFD_TX, VWUP_MFD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MFD_MAINT_TIME, {0, 60, 60}, 1, ISOTP_STD},

    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_TEMP_DCDC, {0, 5, 0}, 1, ISOTP_STD},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_DCDC_U, {0, 5, 10}, 1, ISOTP_STD},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_DCDC_I, {0, 5, 10}, 1, ISOTP_STD}/*,
    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_TEMP_COOL1, {0, 20, 0}, 1},
    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_TEMP_COOL2, {0, 20, 0}, 1},
    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_TEMP_COOL3, {0, 20, 0}, 1},
    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_TEMP_COOL4, {0, 20, 0}, 1},
    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_TEMP_COOL5, {0, 20, 0}, 1},
    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_TEMP_COOL5, {0, 20, 0}, 1},
    {VWUP_BRKBOOST_TX, VWUP_BRKBOOST_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BRKBOOST_TEMP_ECU, {0, 20, 0}, 1},
    {VWUP_BRKBOOST_TX, VWUP_BRKBOOST_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BRKBOOST_TEMP_ACC, {0, 20, 0}, 1},
    {VWUP_STEER_TX, VWUP_STEER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_STEER_TEMP, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_COOL, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_MOT, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_DCDC1, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_DCDC2, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_DCDC3, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_PEU, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_PEV, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_PEW, {0, 20, 0}, 1},
    {VWUP_ELD_TX, VWUP_ELD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_ELD_TEMP_STAT, {0, 20, 0}, 1},
    {VWUP_INF_TX, VWUP_INF_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_INF_TEMP_PCB, {0, 20, 0}, 1},
    {VWUP_INF_TX, VWUP_INF_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_INF_TEMP_AUDIO, {0, 20, 0}, 1},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_TEMP_MAX, {0, 20, 0}, 1},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_TEMP_MIN, {0, 20, 0}, 1},
    {VWUP_BRKSENS_TX, VWUP_BRKSENS_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BRKSENS_TEMP, {0, 20, 0}, 1}
*/
//    {0, 0, 0, 0, {0, 0, 0}, 0},};
    };
    
const OvmsVehicle::poll_pid_t vweup1_polls[] = {
    // specific codes for gen1 model (before year 2020)
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP1_CHG_AC_U, {0, 0, 5}, 1, ISOTP_STD},
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP1_CHG_AC_I, {0, 0, 5}, 1, ISOTP_STD},
    // Same tick & order important of above 2: VWUP_CHG_AC_I calculates the AC power
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP1_CHG_DC_U, {0, 0, 5}, 1, ISOTP_STD},
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP1_CHG_DC_I, {0, 0, 5}, 1, ISOTP_STD},
    // Same tick & order important of above 2: VWUP_CHG_DC_I calculates the DC power
    // Same tick & order important of above 4: VWUP_CHG_DC_I calculates the power loss & efficiency
    {0, 0, 0, 0, {0, 0, 0}, 0, ISOTP_STD}};

const OvmsVehicle::poll_pid_t vweup2_polls[] = {
    // specific codes for gen2 model (from year 2020)
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP2_CHG_AC_U, {0, 0, 5}, 1, ISOTP_STD},
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP2_CHG_AC_I, {0, 0, 5}, 1, ISOTP_STD},
    // Same tick & order important of above 2: VWUP_CHG_AC_I calculates the AC power
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP2_CHG_DC_U, {0, 0, 5}, 1, ISOTP_STD},
    {VWUP_CHG_TX, VWUP_CHG_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP2_CHG_DC_I, {0, 0, 5}, 1, ISOTP_STD},
    // Same tick & order important of above 2: VWUP_CHG_DC_I calculates the DC power
    // Same tick & order important of above 4: VWUP_CHG_DC_I calculates the power loss & efficiency
    {0, 0, 0, 0, {0, 0, 0}, 0, ISOTP_STD}};

const int vweup_polls_len = sizeof(vweup_polls)/sizeof(vweup_polls)[0];
const int vweup1_polls_len = sizeof(vweup1_polls)/sizeof(vweup1_polls)[0];
const int vweup2_polls_len = sizeof(vweup2_polls)/sizeof(vweup2_polls)[0];
//if (vweup1_polls_len > vweup2_polls_len){
OvmsVehicleVWeUp::poll_pid_t vweup_polls_all[1+vweup_polls_len+vweup1_polls_len]; // not good, length should be max of the two possible lists
//}
//else {
//OvmsVehicleVWeUp::poll_pid_t vweup_polls_all[vweup_polls_len+vweup2_polls_len];
//}

void OvmsVehicleVWeUp::OBDInit()
{
    ESP_LOGI(TAG, "Starting connection: OBDII");
    RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
//    ESP_LOGD(TAG,"Starting OBD Polling...");

    // init polls:
    if (vweup_con == 2) // only OBD connected -> get car state by polling OBD
        vweup_polls_all[0] = {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_I, {30, 1, 5}, 1, ISOTP_STD}; // 30 in OFF needed: Car gets started with full 12V battery
    else 
        vweup_polls_all[0] = {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_I, {0, 1, 5}, 1, ISOTP_STD}; 
    if (vweup_modelyear < 2020)
    {
        for(int i=0; i<vweup_polls_len; i++)
            vweup_polls_all[i+1] = vweup_polls[i];   
        for(int i=vweup_polls_len; i<vweup_polls_len+vweup1_polls_len; i++)
            vweup_polls_all[i+1] = vweup1_polls[i-vweup_polls_len];  
    }
    else 
    {
        for(int i=0; i<vweup_polls_len; i++)
            vweup_polls_all[i+1] = vweup_polls[i];   
        for(int i=vweup_polls_len; i<vweup_polls_len+vweup2_polls_len; i++)
            vweup_polls_all[i+1] = vweup2_polls[i-vweup_polls_len];  
    }
    PollSetPidList(m_can1, vweup_polls_all);
    PollSetThrottling(0);
    PollSetResponseSeparationTime(1);
    PollSetState(VWEUP_OFF);

    BatMgmtSoCAbs = MyMetrics.InitFloat("xvu.b.soc.abs", 100, 0, Percentage);
    MotElecSoCAbs = MyMetrics.InitFloat("xvu.m.soc.abs", 100, 0, Percentage);
    MotElecSoCNorm = MyMetrics.InitFloat("xvu.m.soc.norm", 100, 0, Percentage);
    ChgMgmtSoCNorm = MyMetrics.InitFloat("xvu.c.soc.norm", 100, 0, Percentage);
    BatMgmtCellDelta = MyMetrics.InitFloat("xvu.b.cell.delta", SM_STALE_NONE, 0, Volts);

    ChargerPowerEffEcu = MyMetrics.InitFloat("xvu.c.eff.ecu", 100, 0, Percentage);
    ChargerPowerLossEcu = MyMetrics.InitFloat("xvu.c.loss.ecu", SM_STALE_NONE, 0, Watts);
    ChargerPowerEffCalc = MyMetrics.InitFloat("xvu.c.eff.calc", 100, 0, Percentage);
    ChargerPowerLossCalc = MyMetrics.InitFloat("xvu.c.loss.calc", SM_STALE_NONE, 0, Watts);
    ChargerACPower = MyMetrics.InitFloat("xvu.c.ac.p", SM_STALE_NONE, 0, Watts);
    ChargerAC1U = MyMetrics.InitFloat("xvu.c.ac.u1", SM_STALE_NONE, 0, Volts);
    ChargerAC2U = MyMetrics.InitFloat("xvu.c.ac.u2", SM_STALE_NONE, 0, Volts);
    ChargerAC1I = MyMetrics.InitFloat("xvu.c.ac.i1", SM_STALE_NONE, 0, Amps);
    ChargerAC2I = MyMetrics.InitFloat("xvu.c.ac.i2", SM_STALE_NONE, 0, Amps);
    ChargerDC1U = MyMetrics.InitFloat("xvu.c.dc.u1", SM_STALE_NONE, 0, Volts);
    ChargerDC2U = MyMetrics.InitFloat("xvu.c.dc.u2", SM_STALE_NONE, 0, Volts);
    ChargerDC1I = MyMetrics.InitFloat("xvu.c.dc.i1", SM_STALE_NONE, 0, Amps);
    ChargerDC2I = MyMetrics.InitFloat("xvu.c.dc.i2", SM_STALE_NONE, 0, Amps);
    ChargerDCPower = MyMetrics.InitFloat("xvu.c.dc.p", SM_STALE_NONE, 0, Watts);

    TPMSDiffusionFrontLeft = MyMetrics.InitFloat("xvu.v.tp.d.fl", SM_STALE_NONE, 0, Percentage);
    TPMSDiffusionFrontRight = MyMetrics.InitFloat("xvu.v.tp.d.fr", SM_STALE_NONE, 0, Percentage);
    TPMSDiffusionRearLeft = MyMetrics.InitFloat("xvu.v.tp.d.rl", SM_STALE_NONE, 0, Percentage);
    TPMSDiffusionRearRight = MyMetrics.InitFloat("xvu.v.tp.d.rr", SM_STALE_NONE, 0, Percentage);
    TPMSEmergencyFrontLeft = MyMetrics.InitFloat("xvu.v.tp.e.fl", SM_STALE_NONE, 0, Percentage);
    TPMSEmergencyFrontRight = MyMetrics.InitFloat("xvu.v.tp.e.fr", SM_STALE_NONE, 0, Percentage);
    TPMSEmergencyRearLeft = MyMetrics.InitFloat("xvu.v.tp.e.rl", SM_STALE_NONE, 0, Percentage);
    TPMSEmergencyRearRight = MyMetrics.InitFloat("xvu.v.tp.e.rr", SM_STALE_NONE, 0, Percentage);
//    MaintenanceDist = MyMetrics.InitFloat("xvu.v.m.d", SM_STALE_NONE, 0, Kilometers);
//    MaintenanceTime = MyMetrics.InitFloat("xvu.v.m.t", SM_STALE_NONE, 0);

    CoolantTemp1 = MyMetrics.InitFloat("xvu.v.t.c1", SM_STALE_NONE, 0, Degrees);
    CoolantTemp2 = MyMetrics.InitFloat("xvu.v.t.c2", SM_STALE_NONE, 0, Degrees);
    CoolantTemp3 = MyMetrics.InitFloat("xvu.v.t.c3", SM_STALE_NONE, 0, Degrees);
    CoolantTemp4 = MyMetrics.InitFloat("xvu.v.t.c4", SM_STALE_NONE, 0, Degrees);
    CoolantTemp5 = MyMetrics.InitFloat("xvu.v.t.c5", SM_STALE_NONE, 0, Degrees);
    CoolingTempBat = MyMetrics.InitFloat("xvu.v.t.c.b", SM_STALE_NONE, 0, Degrees);
    BrakeboostTempECU = MyMetrics.InitFloat("xvu.v.t.bb.e", SM_STALE_NONE, 0, Degrees);
    BrakeboostTempAccu = MyMetrics.InitFloat("xvu.v.t.bb.a", SM_STALE_NONE, 0, Degrees);
    SteeringTempPA = MyMetrics.InitFloat("xvu.v.t.s", SM_STALE_NONE, 0, Degrees);
    ElectricDriveCoolantTemp = MyMetrics.InitFloat("xvu.v.t.m.c", SM_STALE_NONE, 0, Degrees);
    ElectricDriveTempDCDC = MyMetrics.InitFloat("xvu.v.t.m.dcdc", SM_STALE_NONE, 0, Degrees);
    ElectricDriveTempDCDCPCB = MyMetrics.InitFloat("xvu.v.t.m.pcb", SM_STALE_NONE, 0, Degrees);
    ElectricDriveTempDCDCPEM = MyMetrics.InitFloat("xvu.v.t.m.pem", SM_STALE_NONE, 0, Degrees);
    ElectricDriveTempPhaseU = MyMetrics.InitFloat("xvu.v.t.m.pu", SM_STALE_NONE, 0, Degrees);
    ElectricDriveTempPhaseV = MyMetrics.InitFloat("xvu.v.t.m.pv", SM_STALE_NONE, 0, Degrees);
    ElectricDriveTempPhaseW = MyMetrics.InitFloat("xvu.v.t.m.pw", SM_STALE_NONE, 0, Degrees);
    ElectricDriveTempStator = MyMetrics.InitFloat("xvu.v.t.m.s", SM_STALE_NONE, 0, Degrees);
    InfElecTempPCB = MyMetrics.InitFloat("xvu.v.t.i.pcb", SM_STALE_NONE, 0, Degrees);
    InfElecTempAudio = MyMetrics.InitFloat("xvu.v.t.i.a", SM_STALE_NONE, 0, Degrees);
    BatTempMax = MyMetrics.InitFloat("xvu.v.t.b.mx", SM_STALE_NONE, 0, Degrees);
    BatTempMin = MyMetrics.InitFloat("xvu.v.t.b.mn", SM_STALE_NONE, 0, Degrees);
    BrakesensTemp = MyMetrics.InitFloat("xvu.v.t.b", SM_STALE_NONE, 0, Degrees);

    TimeOffRequested = 0;

    OdoStart = StandardMetrics.ms_v_pos_odometer->AsFloat();
    EnergyRecdStart = StandardMetrics.ms_v_bat_energy_recd_total->AsFloat();
    EnergyUsedStart = StandardMetrics.ms_v_bat_energy_used_total->AsFloat();
}

void OvmsVehicleVWeUp::OBDDeInit()
{
    ESP_LOGI(TAG, "Stopping connection: OBDII");
    PollSetPidList(m_can1, NULL);
}

void OvmsVehicleVWeUp::OBDCheckCarState()
{
    ESP_LOGV(TAG, "CheckCarState(): 12V=%f ChargerEff=%f BatI=%f BatIModified=%u time=%u", StandardMetrics.ms_v_bat_12v_voltage->AsFloat(), ChargerPowerEffEcu->AsFloat(), StandardMetrics.ms_v_bat_current->AsFloat(), StandardMetrics.ms_v_bat_current->LastModified(), monotonictime);

    // 12V Battery: if voltage >= 12.9 it is charging and the car must be on (or charging) for that
    bool voltageSaysOn = StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= 12.9f;

    // HV-Batt current: If there is a current flowing and the value is not older than 2 minutes (120 secs) we are on
    bool currentSaysOn = StandardMetrics.ms_v_bat_current->AsFloat() != 0.0f &&
                         (monotonictime - StandardMetrics.ms_v_bat_current->LastModified()) < 120;

    // Charger ECU: When it reports an efficiency > 0 the car is charging
    bool chargerSaysOn = ChargerPowerEffEcu->AsFloat() > 0.0f;

    if (chargerSaysOn)
    {
        if (!IsCharging())
        {
            ESP_LOGI(TAG, "Setting car state to CHARGING");
            StandardMetrics.ms_v_env_on->SetValue(false);
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            PollSetState(VWUP_CHARGING);
            TimeOffRequested = 0;
        }
        return;
    }
    
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);

    if (voltageSaysOn || currentSaysOn)
    {
        if (!IsOn())
        {
            ESP_LOGI(TAG, "Setting car state to ON");
            StandardMetrics.ms_v_env_on->SetValue(true);
            PollSetState(VWUP_ON);
            TimeOffRequested = 0;
            OdoStart = StandardMetrics.ms_v_pos_odometer->AsFloat();
            EnergyRecdStart = StandardMetrics.ms_v_bat_energy_recd_total->AsFloat();
            EnergyUsedStart = StandardMetrics.ms_v_bat_energy_used_total->AsFloat();
            ESP_LOGI(TAG,"Start Counters: %f, %f, %f",OdoStart,EnergyRecdStart,EnergyUsedStart);
        }
        return;
    }

    if (TimeOffRequested == 0)
    {
        TimeOffRequested = monotonictime;
        if (TimeOffRequested == 0)
        {
            // For the small chance we are requesting exactly at 0 time
            TimeOffRequested--;
        }
        ESP_LOGI(TAG, "Car state to OFF requested. Waiting for possible re-activation ...");
    }

    // When already OFF or I haven't waited for 60 seconds: return
    if (IsOff() || (monotonictime - TimeOffRequested) < 60)
    {
        return;
    }

    // Set car to OFF
    ESP_LOGI(TAG, "Wait is over: Setting car state to OFF");
    StandardMetrics.ms_v_env_on->SetValue(false);
    StandardMetrics.ms_v_charge_voltage->SetValue(0);
    StandardMetrics.ms_v_charge_current->SetValue(0);
    PollSetState(VWUP_OFF);
}

//void OvmsVehicleVweUp::DiagnosticTest()
//{

//}

void OvmsVehicleVWeUp::IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain)
{
    ESP_LOGV(TAG, "IncomingPollReply(type=%u, pid=%X, length=%u, mlremain=%u): called", type, pid, length, mlremain);

    // for (uint8_t i = 0; i < length; i++)
    // {
    //   ESP_LOGV(TAG, "data[%u]=%X", i, data[i]);
    // }

    // If not all data is here: wait for the next call
    if (!PollReply.AddNewData(pid, data, length, mlremain))
    {
        return;
    }

    float value;

    switch (pid)
    {
    case VWUP_BAT_MGMT_U:
        if (PollReply.FromUint16("VWUP_BAT_MGMT_U", value))
        {
            StandardMetrics.ms_v_bat_voltage->SetValue(value / 4.0f);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_U=%f => %f", value, StandardMetrics.ms_v_bat_voltage->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_I:
        if (PollReply.FromUint16("VWUP_BAT_MGMT_I", value))
        {
            // ECU delivers negative current when it goes out of the battery. OVMS wants positive when the battery outputs current.
            StandardMetrics.ms_v_bat_current->SetValue(((value - 2044.0f) / 4.0f) * -1.0f);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_I=%f => %f", value, StandardMetrics.ms_v_bat_current->AsFloat());

            value = StandardMetrics.ms_v_bat_voltage->AsFloat() * StandardMetrics.ms_v_bat_current->AsFloat() / 1000.0f;
            StandardMetrics.ms_v_bat_power->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_POWER=%f => %f", value, StandardMetrics.ms_v_bat_power->AsFloat());
        }
        break;

    case VWUP_MOT_ELEC_SOC_NORM:
        if (PollReply.FromUint16("VWUP_MOT_ELEC_SOC_NORM", value))
        {            
            StandardMetrics.ms_v_bat_soc->SetValue(value / 100.0f);
            MotElecSoCNorm->SetValue(value / 100.0f);
            VALUE_LOG(TAG, "VWUP_MOT_ELEC_SOC_NORM=%f => %f", value, StandardMetrics.ms_v_bat_soc->AsFloat());
        }
        break;

    case VWUP_MOT_ELEC_SOC_ABS:
        if (PollReply.FromUint8("VWUP_MOT_ELEC_SOC_ABS", value))
        {            
            MotElecSoCAbs->SetValue(value / 2.55f);
            VALUE_LOG(TAG, "VWUP_MOT_ELEC_SOC_ABS=%f => %f", value, MotElecSoCAbs->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_SOC_ABS:
        if (PollReply.FromUint8("VWUP_BAT_MGMT_SOC_ABS", value))
        {
            BatMgmtSoCAbs->SetValue(value / 2.5f);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_SOC_ABS=%f => %f", value, BatMgmtSoCAbs->AsFloat());
        }
        break;

    case VWUP_CHG_MGMT_SOC_NORM:
        if (PollReply.FromUint8("VWUP_CHG_MGMT_SOC_NORM", value))
        {
            StandardMetrics.ms_v_bat_soc->SetValue(value / 2.0f);
            ChgMgmtSoCNorm->SetValue(value / 2.0f);
            VALUE_LOG(TAG, "VWUP_CHG_MGMT_SOC_NORM=%f => %f", value, StandardMetrics.ms_v_bat_soc->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_ENERGY_COUNTERS:
        if (PollReply.FromInt32("VWUP_BAT_MGMT_ENERGY_COUNTERS_RECD", value, 8))
        {
            StandardMetrics.ms_v_bat_energy_recd_total->SetValue(value / ((0xFFFFFFFF / 2.0f) / 250200.0f));
            StandardMetrics.ms_v_bat_energy_recd->SetValue(StandardMetrics.ms_v_bat_energy_recd_total->AsFloat()-EnergyRecdStart); // so far we don't know where to get energy recovered on trip directly...
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_ENERGY_COUNTERS_RECD=%f => %f", value, StandardMetrics.ms_v_bat_energy_recd_total->AsFloat());
        }
        if (PollReply.FromInt32("VWUP_BAT_MGMT_ENERGY_COUNTERS_USED", value, 12))
        {
            // Used is negative here, standard metric is positive
            StandardMetrics.ms_v_bat_energy_used_total->SetValue((value * -1.0f) / ((0xFFFFFFFF / 2.0f) / 250200.0f));
            StandardMetrics.ms_v_bat_energy_used->SetValue(StandardMetrics.ms_v_bat_energy_used_total->AsFloat()-EnergyUsedStart); // so far we don't know where to get energy used on trip directly...
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_ENERGY_COUNTERS_USED=%f => %f", value, StandardMetrics.ms_v_bat_energy_used_total->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_CELL_MAX:
        if (PollReply.FromUint16("VWUP_BAT_MGMT_CELL_MAX", value))
        {
            BatMgmtCellMax = value / 4096.0f;
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_CELL_MAX=%f => %f", value, BatMgmtCellMax);
        }
        break;

    case VWUP_BAT_MGMT_CELL_MIN:
        if (PollReply.FromUint16("VWUP_BAT_MGMT_CELL_MIN", value))
        {
            BatMgmtCellMin = value / 4096.0f;
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_CELL_MIN=%f => %f", value, BatMgmtCellMin);

            value = BatMgmtCellMax - BatMgmtCellMin;
            BatMgmtCellDelta->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_CELL_DELTA=%f => %f", value, BatMgmtCellDelta->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_TEMP:
        if (PollReply.FromUint16("VWUP_BAT_MGMT_TEMP", value))
        {
            StandardMetrics.ms_v_bat_temp->SetValue(value / 64.0f);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_TEMP=%f => %f", value, StandardMetrics.ms_v_bat_temp->AsFloat());
        }
        break;

    case VWUP1_CHG_AC_U:
        if (PollReply.FromUint16("VWUP_CHG_AC1_U", value))
        {
            StandardMetrics.ms_v_charge_voltage->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_AC1_U=%f => %f", value, StandardMetrics.ms_v_charge_voltage->AsFloat());
        }
        break;

    case VWUP1_CHG_AC_I:
        if (PollReply.FromUint8("VWUP_CHG_AC1_I", value))
        {
            StandardMetrics.ms_v_charge_current->SetValue(value / 10.0f);
            VALUE_LOG(TAG, "VWUP_CHG_AC1_I=%f => %f", value, StandardMetrics.ms_v_charge_current->AsFloat());

            value = (StandardMetrics.ms_v_charge_voltage->AsFloat() * StandardMetrics.ms_v_charge_current->AsFloat()) / 1000.0f;
            ChargerACPower->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_AC_P=%f => %f", value, ChargerACPower->AsFloat());

            // Standard Charge Power and Charge Efficiency calculation as requested by the standard
            StandardMetrics.ms_v_charge_power->SetValue(value);
            value = value == 0.0f ? 0.0f : ((StandardMetrics.ms_v_bat_power->AsFloat() * -1.0f) / value) * 100.0f;
            StandardMetrics.ms_v_charge_efficiency->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_EFF_STD=%f => %f", value, StandardMetrics.ms_v_charge_efficiency->AsFloat());
        }
        break;

    case VWUP2_CHG_AC_U:
        if (PollReply.FromUint16("VWUP_CHG_AC1_U", value))
        {
            ChargerAC1U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_AC1_U=%f => %f", value, ChargerAC1U->AsFloat());
        }
        if (PollReply.FromUint16("VWUP_CHG_AC2_U", value, 2))
        {
            ChargerAC2U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_AC2_U=%f => %f", value, ChargerAC2U->AsFloat());
            StandardMetrics.ms_v_charge_voltage->SetValue((ChargerAC1U->AsFloat()+ChargerAC2U->AsFloat())/2);
        }
        break;

    case VWUP2_CHG_AC_I:
        if (PollReply.FromUint8("VWUP_CHG_AC1_I", value))
        {
            ChargerAC1I->SetValue(value / 10.0f);
            VALUE_LOG(TAG, "VWUP_CHG_AC1_I=%f => %f", value, ChargerAC1I->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_CHG_AC2_I", value, 1))
        {
            ChargerAC2I->SetValue(value / 10.0f);
            VALUE_LOG(TAG, "VWUP_CHG_AC2_I=%f => %f", value, ChargerAC2I->AsFloat());
            StandardMetrics.ms_v_charge_current->SetValue(ChargerAC1I->AsFloat()+ChargerAC2I->AsFloat());

            value = (ChargerAC1U->AsFloat() * ChargerAC1I->AsFloat() + ChargerAC2U->AsFloat() * ChargerAC2I->AsFloat()) / 1000.0f;
            ChargerACPower->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_AC_P=%f => %f", value, ChargerACPower->AsFloat());

            // Standard Charge Power and Charge Efficiency calculation as requested by the standard
            StandardMetrics.ms_v_charge_power->SetValue(value);
            value = value == 0.0f ? 0.0f : ((StandardMetrics.ms_v_bat_power->AsFloat() * -1.0f) / value) * 100.0f;
            StandardMetrics.ms_v_charge_efficiency->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_EFF_STD=%f => %f", value, StandardMetrics.ms_v_charge_efficiency->AsFloat());
        }
        break;

    case VWUP1_CHG_DC_U:
        if (PollReply.FromUint16("VWUP_CHG_DC_U", value))
        {
            ChargerDC1U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_DC_U=%f => %f", value, ChargerDC1U->AsFloat());
        }
        break;

    case VWUP1_CHG_DC_I:
        if (PollReply.FromUint16("VWUP_CHG_DC_I", value))
        {
            ChargerDC1I->SetValue((value - 510.0f) / 5.0f);
            VALUE_LOG(TAG, "VWUP_CHG_DC_I=%f => %f", value, ChargerDC1I->AsFloat());

            value = (ChargerDC1U->AsFloat() * ChargerDC1I->AsFloat()) / 1000.0f;
            ChargerDCPower->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_DC_P=%f => %f", value, ChargerDCPower->AsFloat());

            value = ChargerACPower->AsFloat() - ChargerDCPower->AsFloat();
            ChargerPowerLossCalc->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_LOSS_CALC=%f => %f", value, ChargerPowerLossCalc->AsFloat());

            value = ChargerACPower->AsFloat() > 0 ? ChargerDCPower->AsFloat() / ChargerACPower->AsFloat() * 100.0f : 0.0f;
            ChargerPowerEffCalc->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_EFF_CALC=%f => %f", value, ChargerPowerEffCalc->AsFloat());
        }
        break;

    case VWUP2_CHG_DC_U:
        if (PollReply.FromUint16("VWUP_CHG_DC1_U", value))
        {
            ChargerDC1U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_DC1_U=%f => %f", value, ChargerDC1U->AsFloat());
        }
        if (PollReply.FromUint16("VWUP_CHG_DC2_U", value, 2))
        {
            ChargerDC2U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_DC2_U=%f => %f", value, ChargerDC2U->AsFloat());
        }
        break;

    case VWUP2_CHG_DC_I:
        if (PollReply.FromUint16("VWUP_CHG_DC1_I", value))
        {
            ChargerDC1I->SetValue((value - 510.0f) / 5.0f);
            VALUE_LOG(TAG, "VWUP_CHG_DC1_I=%f => %f", value, ChargerDC1I->AsFloat());
        }
        if (PollReply.FromUint16("VWUP_CHG_DC2_I", value, 2))
        {
            ChargerDC2I->SetValue((value - 510.0f) / 5.0f);
            VALUE_LOG(TAG, "VWUP_CHG_DC2_I=%f => %f", value, ChargerDC2I->AsFloat());

            value = (ChargerDC1U->AsFloat() * ChargerDC1I->AsFloat() + ChargerDC2U->AsFloat() * ChargerDC2I->AsFloat()) / 1000.0f;
            ChargerDCPower->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_DC_P=%f => %f", value, ChargerDCPower->AsFloat());

            value = ChargerACPower->AsFloat() - ChargerDCPower->AsFloat();
            ChargerPowerLossCalc->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_LOSS_CALC=%f => %f", value, ChargerPowerLossCalc->AsFloat());

            value = ChargerACPower->AsFloat() > 0 ? ChargerDCPower->AsFloat() / ChargerACPower->AsFloat() * 100.0f : 0.0f;
            ChargerPowerEffCalc->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHG_EFF_CALC=%f => %f", value, ChargerPowerEffCalc->AsFloat());
        }
        break;

    case VWUP_CHG_POWER_EFF:
        // Value is offset from 750d%. So a value > 250 would be (more) than 100% efficiency!
        // This means no charging is happening at the moment (standardvalue replied for no charging is 0xFE)
        if (PollReply.FromUint8("VWUP_CHG_POWER_EFF", value))
        {
            ChargerPowerEffEcu->SetValue(value <= 250.0f ? value / 10.0f + 75.0f : 0.0f);
            VALUE_LOG(TAG, "VWUP_CHG_POWER_EFF=%f => %f", value, ChargerPowerEffEcu->AsFloat());
        }
        break;

    case VWUP_CHG_POWER_LOSS:
        if (PollReply.FromUint8("VWUP_CHG_POWER_LOSS", value))
        {
            ChargerPowerLossEcu->SetValue((value * 20.0f) / 1000.0f);
            VALUE_LOG(TAG, "VWUP_CHG_POWER_LOSS=%f => %f", value, ChargerPowerLossEcu->AsFloat());
        }
        break;

    case VWUP_MFD_ODOMETER:
        if (PollReply.FromUint16("VWUP_MFD_ODOMETER", value))
        {
            StandardMetrics.ms_v_pos_odometer->SetValue(value * 10.0f);
            StandardMetrics.ms_v_pos_trip->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat()-OdoStart); // so far we don't know where to get trip distance directly...
            VALUE_LOG(TAG, "VWUP_MFD_ODOMETER=%f => %f", value, StandardMetrics.ms_v_pos_odometer->AsFloat());
        }
        break;

    case VWUP_BRK_TPMS:
        if (PollReply.FromUint8("VWUP_BRK_TPMS", value))
        {
            TPMSDiffusionFrontLeft->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BRK_TPMSD_FL=%f => %f", value, TPMSDiffusionFrontLeft->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_BRK_TPMS", value, 39))
        {
            TPMSDiffusionFrontRight->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BRK_TPMSD_FR=%f => %f", value, TPMSDiffusionFrontRight->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_BRK_TPMS", value, 40))
        {
            TPMSDiffusionRearLeft->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BRK_TPMSD_RL=%f => %f", value, TPMSDiffusionRearLeft->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_BRK_TPMS", value, 41))
        {
            TPMSDiffusionRearRight->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BRK_TPMSD_RR=%f => %f", value, TPMSDiffusionRearRight->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_BRK_TPMS", value, 42))
        {
            TPMSEmergencyFrontLeft->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BRK_TPMSE_FL=%f => %f", value, TPMSEmergencyFrontLeft->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_BRK_TPMS", value, 43))
        {
            TPMSEmergencyFrontRight->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BRK_TPMSE_FR=%f => %f", value, TPMSEmergencyFrontRight->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_BRK_TPMS", value, 44))
        {
            TPMSEmergencyRearLeft->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BRK_TPMSE_RL=%f => %f", value, TPMSEmergencyRearLeft->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_BRK_TPMS", value, 45))
        {
            TPMSEmergencyRearRight->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BRK_TPMSE_RR=%f => %f", value, TPMSEmergencyRearRight->AsFloat());
        }
        break;

     case VWUP_MFD_MAINT_DIST:
        if (PollReply.FromUint16("VWUP_MFD_MAINT_DIST", value))
        {
            StandardMetrics.ms_v_env_service_range->SetValue(value);
            VALUE_LOG(TAG, "VWUP_MFD_MAINT_DIST=%f => %f", value, MaintenanceDist->AsFloat());
        }
        break;
     case VWUP_MFD_MAINT_TIME:
        if (PollReply.FromUint16("VWUP_MFD_MAINT_TIME", value))
        {
            StandardMetrics.ms_v_env_service_days->SetValue(value);
            VALUE_LOG(TAG, "VWUP_MFD_MAINT_TIME=%f => %f", value, MaintenanceTime->AsFloat());
        }
        break;


     case VWUP_MOT_ELEC_TEMP_DCDC:
        if (PollReply.FromUint16("VWUP_MOT_ELEC_TEMP_DCDC", value)){
            StandardMetrics.ms_v_charge_12v_temp->SetValue(value / 10.0f - 273.1f);
            VALUE_LOG(TAG, "VWUP_MOT_ELEC_TEMP_DCDC=%f => %f", value, StandardMetrics.ms_v_charge_12v_temp->AsFloat());
        }
        break;
     case VWUP_ELD_DCDC_U:
        if (PollReply.FromUint16("VWUP_ELD_DCDC_U", value)){
            StandardMetrics.ms_v_charge_12v_voltage->SetValue(value / 512.0f);
            VALUE_LOG(TAG, "VWUP_ELD_DCDC_U=%f => %f", value, StandardMetrics.ms_v_charge_12v_voltage->AsFloat());
        }
        break;
     case VWUP_ELD_DCDC_I:
        if (PollReply.FromUint16("VWUP_ELD_DCDC_I", value)){
            StandardMetrics.ms_v_charge_12v_current->SetValue(value / 16.0f);
            VALUE_LOG(TAG, "VWUP_ELD_DCDC_I=%f => %f", value, StandardMetrics.ms_v_charge_12v_current->AsFloat());
            StandardMetrics.ms_v_charge_12v_power->SetValue(StandardMetrics.ms_v_charge_12v_voltage->AsFloat() * StandardMetrics.ms_v_charge_12v_current->AsFloat());
            VALUE_LOG(TAG, "VWUP_ELD_DCDC_P=%f => %f", StandardMetrics.ms_v_charge_12v_power->AsFloat(), StandardMetrics.ms_v_charge_12v_power->AsFloat());
        }
        break;
   }
}
