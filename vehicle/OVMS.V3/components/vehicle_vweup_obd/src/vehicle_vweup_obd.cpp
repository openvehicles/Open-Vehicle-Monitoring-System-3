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

#include "ovms_log.h"

static const char *TAG = "v-vweup-obd";

#include <stdio.h>
#include "pcp.h"
#include "vehicle_vweup_obd.h"
#include "metrics_standard.h"
#include "ovms_webserver.h"
#include "ovms_events.h"
#include "ovms_metrics.h"

class OvmsVehicleVWeUpObdInit
{
public:
    OvmsVehicleVWeUpObdInit()
    {
        ESP_LOGI(TAG, "Registering Vehicle: VW e-Up OBD2 (9000)");
        MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeUpObd>("VWUP.OBD", "VW e-Up (OBD2)");
    }
} OvmsVehicleVWeUpObdInit __attribute__((init_priority(9000)));

const OvmsVehicle::poll_pid_t vwup_polls[] = {
    // Note: poller ticker cycles at 3600 seconds = max period
    // { txid, rxid, type, pid, { VWUP_OFF, VWUP_ON, VWUP_CHARGING }, bus }

    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_U, {0, 1, 5}, 1},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_I, {30, 1, 5}, 1}, // 30 in OFF needed: Car gets started with full 12V battery
    // Same tick & order important of above 2: VWUP_BAT_MGMT_I calculates the power

    {VWUP_MOT_ELEC_TX, VWUP_MOT_ELEC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MOT_ELEC_SOC_NORM, {0, 20, 20}, 1},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_SOC, {0, 20, 20}, 1},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_ENERGY_COUNTERS, {0, 20, 20}, 1},

    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_CELL_MAX, {0, 20, 20}, 1},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_CELL_MIN, {0, 20, 20}, 1},
    // Same tick & order important of above 2: VWUP_BAT_MGMT_CELL_MIN calculates the delta

    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_TEMP, {0, 20, 20}, 1},

    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIISESSION, VWUP_CHARGER_EXTDIAG, {0, 30, 30}, 1},

    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_AC_U, {0, 0, 5}, 1},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_AC_I, {0, 0, 5}, 1},
    // Same tick & order important of above 2: VWUP_CHARGER_AC_I calculates the AC power
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_DC_U, {0, 0, 5}, 1},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_DC_I, {0, 0, 5}, 1},
    // Same tick & order important of above 2: VWUP_CHARGER_DC_I calculates the DC power
    // Same tick & order important of above 4: VWUP_CHARGER_DC_I calculates the power loss & efficiency

    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_POWER_EFF, {0, 5, 10}, 1}, // 5 @ VWUP_ON to detect charging
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_POWER_LOSS, {0, 0, 10}, 1},

    {VWUP_MFD_TX, VWUP_MFD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MFD_ODOMETER, {0, 60, 60}, 1},
    {0, 0, 0, 0, {0, 0, 0}, 0}};

OvmsVehicleVWeUpObd::OvmsVehicleVWeUpObd()
{
    ESP_LOGI(TAG, "Start VW e-Up OBD vehicle module");

    // init configs:
    MyConfig.RegisterParam("vwup", "VW e-Up", true, true);
    ConfigChanged(NULL);

    // init polls:
    RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
    PollSetPidList(m_can1, vwup_polls);
    PollSetThrottling(0);
    PollSetResponseSeparationTime(1);
    PollSetState(VWUP_OFF);

    BatMgmtSoC = MyMetrics.InitFloat("xuo.b.soc", 100, 0, Percentage);
    BatMgmtCellDelta = MyMetrics.InitFloat("xuo.b.cell.delta", SM_STALE_NONE, 0, Volts);

    ChargerPowerEffEcu = MyMetrics.InitFloat("xuo.c.eff.ecu", 100, 0, Percentage);
    ChargerPowerLossEcu = MyMetrics.InitFloat("xuo.c.loss.ecu", SM_STALE_NONE, 0, Watts);
    ChargerPowerEffCalc = MyMetrics.InitFloat("xuo.c.eff.calc", 100, 0, Percentage);
    ChargerPowerLossCalc = MyMetrics.InitFloat("xuo.c.loss.calc", SM_STALE_NONE, 0, Watts);
    ChargerACPower = MyMetrics.InitFloat("xuo.c.ac.p", SM_STALE_NONE, 0, Watts);
    ChargerDCPower = MyMetrics.InitFloat("xuo.c.dc.p", SM_STALE_NONE, 0, Watts);

    TimeOffRequested = 0;
}

OvmsVehicleVWeUpObd::~OvmsVehicleVWeUpObd()
{
    ESP_LOGI(TAG, "Stop VW e-Up OBD vehicle module");
}

void OvmsVehicleVWeUpObd::Ticker1(uint32_t ticker)
{
    CheckCarState();
}

void OvmsVehicleVWeUpObd::CheckCarState()
{
    ESP_LOGV(TAG, "CheckCarState(): 12V=%f ChargerEff=%f BatI=%f BatIModified=%u time=%u", StandardMetrics.ms_v_bat_12v_voltage->AsFloat(), ChargerPowerEffEcu->AsFloat(), StandardMetrics.ms_v_bat_current->AsFloat(), StandardMetrics.ms_v_bat_current->LastModified(), monotonictime);

    // 12V Battery: if voltage >= 12.9 it is charging and the car must be on (or charging) for that
    bool voltageSaysOn = StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= 12.9f;

    // HV-Batt current: If there is a current flowing and the value is not older than 2 minutes (120 secs) we are on
    bool currentSaysOn = StandardMetrics.ms_v_bat_current->AsFloat() != 0.0f &&
                         (monotonictime - StandardMetrics.ms_v_bat_current->LastModified()) < 120;

    // Charger ECU: When it reports an efficiency > 0 the car is charing
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
    PollSetState(VWUP_OFF);
}

void OvmsVehicleVWeUpObd::IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t remain)
{
    ESP_LOGV(TAG, "IncomingPollReply(type=%u, pid=%X, length=%u, remain=%u): called", type, pid, length, remain);

    // for (uint8_t i = 0; i < length; i++)
    // {
    //   ESP_LOGV(TAG, "data[%u]=%X", i, data[i]);
    // }

    // If not all data is here: wait for the next call
    if (!PollReply.AddNewData(pid, data, length, remain))
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
            VALUE_LOG(TAG, "VWUP_MOT_ELEC_SOC_NORM=%f => %f", value, StandardMetrics.ms_v_bat_soc->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_SOC:
        if (PollReply.FromUint8("VWUP_BAT_MGMT_SOC", value))
        {
            BatMgmtSoC->SetValue(value / 2.5f);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_SOC=%f => %f", value, BatMgmtSoC->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_ENERGY_COUNTERS:
        if (PollReply.FromInt32("VWUP_BAT_MGMT_ENERGY_COUNTERS_RECD", value, 8))
        {
            StandardMetrics.ms_v_bat_energy_recd_total->SetValue(value / ((0xFFFFFFFF / 2.0f) / 250200.0f));
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_ENERGY_COUNTERS_RECD=%f => %f", value, StandardMetrics.ms_v_bat_energy_recd_total->AsFloat());
        }
        if (PollReply.FromInt32("VWUP_BAT_MGMT_ENERGY_COUNTERS_USED", value, 12))
        {
            // Used is negative here, standard metric is positive
            StandardMetrics.ms_v_bat_energy_used_total->SetValue((value * -1.0f) / ((0xFFFFFFFF / 2.0f) / 250200.0f));
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

    case VWUP_CHARGER_AC_U:
        if (PollReply.FromUint16("VWUP_CHARGER_AC1_U", value))
        {
            ChargerAC1U = value;
            VALUE_LOG(TAG, "VWUP_CHARGER_AC1_U=%f => %f", value, ChargerAC1U);
        }
        if (PollReply.FromUint16("VWUP_CHARGER_AC2_U", value, 2))
        {
            ChargerAC2U = value;
            VALUE_LOG(TAG, "VWUP_CHARGER_AC2_U=%f => %f", value, ChargerAC2U);
        }
        break;

    case VWUP_CHARGER_AC_I:
        if (PollReply.FromUint8("VWUP_CHARGER_AC1_I", value))
        {
            ChargerAC1I = value / 10.0f;
            VALUE_LOG(TAG, "VWUP_CHARGER_AC1_I=%f => %f", value, ChargerAC1I);
        }
        if (PollReply.FromUint8("VWUP_CHARGER_AC2_I", value, 1))
        {
            ChargerAC2I = value / 10.0f;
            VALUE_LOG(TAG, "VWUP_CHARGER_AC2_I=%f => %f", value, ChargerAC2I);

            value = (ChargerAC1U * ChargerAC1I + ChargerAC2U * ChargerAC2I) / 1000.0f;
            ChargerACPower->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_AC_P=%f => %f", value, ChargerACPower->AsFloat());

            // Standard Charge Power and Charge Efficiency calculation as requested by the standard
            StandardMetrics.ms_v_charge_power->SetValue(value);
            value = value == 0.0f ? 0.0f : ((StandardMetrics.ms_v_bat_power->AsFloat() * -1.0f) / value) * 100.0f;
            StandardMetrics.ms_v_charge_efficiency->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_EFF_STD=%f => %f", value, StandardMetrics.ms_v_charge_efficiency->AsFloat());
        }
        break;

    case VWUP_CHARGER_DC_U:
        if (PollReply.FromUint16("VWUP_CHARGER_DC1_U", value))
        {
            ChargerDC1U = value;
            VALUE_LOG(TAG, "VWUP_CHARGER_DC1_U=%f => %f", value, ChargerDC1U);
        }
        if (PollReply.FromUint16("VWUP_CHARGER_DC2_U", value, 2))
        {
            ChargerDC2U = value;
            VALUE_LOG(TAG, "VWUP_CHARGER_DC2_U=%f => %f", value, ChargerDC2U);
        }
        break;

    case VWUP_CHARGER_DC_I:
        if (PollReply.FromUint16("VWUP_CHARGER_DC1_I", value))
        {
            ChargerDC1I = (value - 510.0f) / 5.0f;
            VALUE_LOG(TAG, "VWUP_CHARGER_DC1_I=%f => %f", value, ChargerDC1I);
        }
        if (PollReply.FromUint16("VWUP_CHARGER_DC2_I", value, 2))
        {
            ChargerDC2I = (value - 510.0f) / 5.0f;
            VALUE_LOG(TAG, "VWUP_CHARGER_DC2_I=%f => %f", value, ChargerDC2I);

            value = (ChargerDC1U * ChargerDC1I + ChargerDC2U * ChargerDC2I) / 1000.0f;
            ChargerDCPower->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_DC_P=%f => %f", value, ChargerDCPower->AsFloat());

            value = ChargerACPower->AsFloat() - ChargerDCPower->AsFloat();
            ChargerPowerLossCalc->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_LOSS_CALC=%f => %f", value, ChargerPowerLossCalc->AsFloat());

            value = ChargerACPower->AsFloat() > 0 ? ChargerDCPower->AsFloat() / ChargerACPower->AsFloat() * 100.0f : 0.0f;
            ChargerPowerEffCalc->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_EFF_CALC=%f => %f", value, ChargerPowerEffCalc->AsFloat());
        }
        break;

    case VWUP_CHARGER_POWER_EFF:
        // Value is offset from 750d%. So a value > 250 would be (more) than 100% efficiency!
        // This means no charging is happening at the moment (standardvalue replied for no charging is 0xFE)
        if (PollReply.FromUint8("VWUP_CHARGER_POWER_EFF", value))
        {
            ChargerPowerEffEcu->SetValue(value <= 250.0f ? value / 10.0f + 75.0f : 0.0f);
            VALUE_LOG(TAG, "VWUP_CHARGER_POWER_EFF=%f => %f", value, ChargerPowerEffEcu->AsFloat());
        }
        break;

    case VWUP_CHARGER_POWER_LOSS:
        if (PollReply.FromUint8("VWUP_CHARGER_POWER_LOSS", value))
        {
            ChargerPowerLossEcu->SetValue((value * 20.0f) / 1000.0f);
            VALUE_LOG(TAG, "VWUP_CHARGER_POWER_LOSS=%f => %f", value, ChargerPowerLossEcu->AsFloat());
        }
        break;

    case VWUP_MFD_ODOMETER:
        if (PollReply.FromUint16("VWUP_MFD_ODOMETER", value))
        {
            StandardMetrics.ms_v_pos_odometer->SetValue(value * 10.0f);
            VALUE_LOG(TAG, "VWUP_MFD_ODOMETER=%f => %f", value, StandardMetrics.ms_v_pos_odometer->AsFloat());
        }
        break;
    }
}