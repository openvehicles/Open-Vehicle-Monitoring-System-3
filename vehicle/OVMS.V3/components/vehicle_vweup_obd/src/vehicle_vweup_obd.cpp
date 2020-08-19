#include "ovms_log.h"

static const char *TAG = "vwup.obd";

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

// Last array (polltime) is for max. 4 different poll-states. I only use State=0 at the moment
const OvmsVehicle::poll_pid_t vwup_polls[] = {
    // Note: poller ticker cycles at 3600 seconds = max period
    // { txid, rxid, type, pid, { VWUP_OFF, VWUP_ON, VWUP_CHARGING } }
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIISESSION, VWUP_CHARGER_EXTDIAG, {0, 30, 30}},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_POWER_EFF, {0, 5, 10}},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_POWER_LOSS, {0, 0, 10}},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_AC_U, {0, 0, 5}},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_AC_I, {0, 0, 5}},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_DC_U, {0, 0, 5}},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_DC_I, {0, 0, 5}},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_U, {0, 1, 5}},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_I, {0, 1, 5}},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_SOC, {0, 20, 20}},
    {0, 0, 0, 0, {0, 0, 0}}};

OvmsVehicleVWeUpObd::OvmsVehicleVWeUpObd()
{
    ESP_LOGI(TAG, "Start VW e-Up OBD vehicle module");

    // init configs:
    MyConfig.RegisterParam("vwup", "VW e-Up", true, true);
    ConfigChanged(NULL);

    // init polls:
    RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
    PollSetPidList(m_can1, vwup_polls);
    PollSetState(VWUP_OFF);

    ChargerPowerEff = MyMetrics.InitFloat("vwup.chrgr.eff", 100, 0, Percentage);
    ChargerPowerLoss = MyMetrics.InitFloat("vwup.chrgr.loss", SM_STALE_NONE, 0, Watts);
    ChargerAC1U = MyMetrics.InitFloat("vwup.chrgr.ac1.u", SM_STALE_NONE, 0, Volts);
    ChargerAC2U = MyMetrics.InitFloat("vwup.chrgr.ac2.u", SM_STALE_NONE, 0, Volts);
    ChargerAC1I = MyMetrics.InitFloat("vwup.chrgr.ac1.i", SM_STALE_NONE, 0, Amps);
    ChargerAC2I = MyMetrics.InitFloat("vwup.chrgr.ac2.i", SM_STALE_NONE, 0, Amps);
    ChargerDC1U = MyMetrics.InitFloat("vwup.chrgr.dc1.u", SM_STALE_NONE, 0, Volts);
    ChargerDC2U = MyMetrics.InitFloat("vwup.chrgr.dc2.u", SM_STALE_NONE, 0, Volts);
    ChargerDC1I = MyMetrics.InitFloat("vwup.chrgr.dc1.i", SM_STALE_NONE, 0, Amps);
    ChargerDC2I = MyMetrics.InitFloat("vwup.chrgr.dc2.i", SM_STALE_NONE, 0, Amps);
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
    bool voltageSaysOn = StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= 12.9f;

    if (voltageSaysOn && ChargerPowerEff->AsFloat() > 0.0f)
    {
        if (!IsCharging())
        {
            ESP_LOGI(TAG, "Setting car state to CHARGING");
            StandardMetrics.ms_v_env_on->SetValue(false);
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            PollSetState(VWUP_CHARGING);
        }
        return;
    }
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);

    if (voltageSaysOn)
    {
        if (!IsOn())
        {
            ESP_LOGI(TAG, "Setting car state to ON");
            StandardMetrics.ms_v_env_on->SetValue(true);
            PollSetState(VWUP_ON);
        }
        return;
    }

    if (!IsOff())
    {
        ESP_LOGI(TAG, "Setting car state to OFF");
        StandardMetrics.ms_v_env_on->SetValue(false);
        PollSetState(VWUP_OFF);
    }
}

void OvmsVehicleVWeUpObd::IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t remain)
{
    ESP_LOGD(TAG, "IncomingPollReply(type=%u, pid=%X, length=%u, remain=%u): called", type, pid, length, remain);

    // for (uint8_t i = 0; i < length; i++)
    // {
    //   ESP_LOGV(TAG, "data[%u]=%X", i, data[i]);
    // }

    // If not all data is here then wait to the next call
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
            StandardMetrics.ms_v_bat_current->SetValue((value - 2044.0f) / 4.0f);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_I=%f => %f", value, StandardMetrics.ms_v_bat_current->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_SOC:
        if (PollReply.FromUint8("VWUP_BAT_MGMT_SOC", value))
        {
            StandardMetrics.ms_v_bat_soc->SetValue(value / 2.5f);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_SOC=%f => %f", value, StandardMetrics.ms_v_bat_soc->AsFloat());
        }
        break;

    case VWUP_CHARGER_POWER_EFF:
        // Value is offset from 750d%. So a value > 250 would be (more) than 100% efficiency!
        // This means no charging is happening at the moment (standardvalue replied for no charging is 0xFE)
        if (PollReply.FromUint8("VWUP_CHARGER_POWER_EFF", value))
        {
            ChargerPowerEff->SetValue(value <= 250.0f ? value / 10.0f + 75.0f : 0.0f);
            VALUE_LOG(TAG, "VWUP_CHARGER_POWER_EFF=%f => %f", value, ChargerPowerEff->AsFloat());
        }
        break;

    case VWUP_CHARGER_POWER_LOSS:
        if (PollReply.FromUint8("VWUP_CHARGER_POWER_LOSS", value))
        {
            ChargerPowerLoss->SetValue(value * 20.0f);
            VALUE_LOG(TAG, "VWUP_CHARGER_POWER_LOSS=%f => %f", value, ChargerPowerLoss->AsFloat());
        }
        break;

    case VWUP_CHARGER_AC_U:
        if (PollReply.FromUint16("VWUP_CHARGER_AC1_U", value, 1))
        {
            ChargerAC1U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_AC1_U=%f => %f", value, ChargerAC1U->AsFloat());
        }
        if (PollReply.FromUint16("VWUP_CHARGER_AC2_U", value, 3))
        {
            ChargerAC2U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_AC2_U=%f => %f", value, ChargerAC2U->AsFloat());
        }
        break;

    case VWUP_CHARGER_AC_I:
        if (PollReply.FromUint8("VWUP_CHARGER_AC1_I", value))
        {
            ChargerAC1I->SetValue(value / 10.0f);
            VALUE_LOG(TAG, "VWUP_CHARGER_AC1_I=%f => %f", value, ChargerAC1I->AsFloat());
        }
        if (PollReply.FromUint8("VWUP_CHARGER_AC2_I", value, 1))
        {
            ChargerAC2I->SetValue(value / 10.0f);
            VALUE_LOG(TAG, "VWUP_CHARGER_AC2_I=%f => %f", value, ChargerAC2I->AsFloat());
        }
        break;

    case VWUP_CHARGER_DC_U:
        if (PollReply.FromUint16("VWUP_CHARGER_DC1_U", value, 1))
        {
            ChargerDC1U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_DC1_U=%f => %f", value, ChargerDC1U->AsFloat());
        }
        if (PollReply.FromUint16("VWUP_CHARGER_DC2_U", value, 3))
        {
            ChargerDC2U->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_DC2_U=%f => %f", value, ChargerDC2U->AsFloat());
        }
        break;

    case VWUP_CHARGER_DC_I:
        if (PollReply.FromUint16("VWUP_CHARGER_DC1_I", value, 1))
        {
            ChargerDC1I->SetValue((value - 510.0f) / 5.0f);
            VALUE_LOG(TAG, "VWUP_CHARGER_DC1_I=%f => %f", value, ChargerDC1I->AsFloat());
        }
        if (PollReply.FromUint16("VWUP_CHARGER_DC2_I", value, 3))
        {
            ChargerDC2I->SetValue((value - 510.0f) / 5.0f);
            VALUE_LOG(TAG, "VWUP_CHARGER_DC2_I=%f => %f", value, ChargerDC2I->AsFloat());
        }
        break;
    }
}