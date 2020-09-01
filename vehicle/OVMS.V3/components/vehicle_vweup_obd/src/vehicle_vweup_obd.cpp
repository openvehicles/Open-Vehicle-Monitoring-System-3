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
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_U, {0, 1, 5}},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_I, {0, 1, 5}},
    // Same tick & order important of above 2: VWUP_BAT_MGMT_I calculates the power
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_SOC, {0, 20, 20}},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_ENERGY_COUNTERS, {0, 20, 20}},

    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_CELL_MAX, {0, 20, 20}},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_CELL_MIN, {0, 20, 20}},
    // Same tick & order important of above 2: VWUP_BAT_MGMT_CELL_MIN calculates the delta

    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIISESSION, VWUP_CHARGER_EXTDIAG, {0, 30, 30}},

    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_AC_U, {0, 0, 5}},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_AC_I, {0, 0, 5}},    
    // Same tick & order important of above 2: VWUP_CHARGER_AC_I calculates the AC power
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_DC_U, {0, 0, 5}},
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_DC_I, {0, 0, 5}},
    // Same tick & order important of above 2: VWUP_CHARGER_DC_I calculates the DC power
    // Same tick & order important of above 4: VWUP_CHARGER_DC_I calculates the power loss & efficiency

    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_POWER_EFF, {0, 5, 10}}, // 5 @ VWUP_ON to detect charging
    {VWUP_CHARGER_TX, VWUP_CHARGER_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_CHARGER_POWER_LOSS, {0, 0, 10}},

    {VWUP_MFD_TX, VWUP_MFD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_MFD_ODOMETER, {0, 60, 60}},
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
    PollSetThrottling(0);
    PollSetResponseSeparationTime(1);
    PollSetState(VWUP_OFF);

    BatMgmtEnergyUsed = MyMetrics.InitFloat("vwup.batmgmt.enrg.used", SM_STALE_NONE, 0, kWh);
    BatMgmtEnergyCharged = MyMetrics.InitFloat("vwup.batmgmt.enrg.chrgd", SM_STALE_NONE, 0, kWh);
    BatMgmtCellDelta = MyMetrics.InitFloat("vwup.batmgmt.cell.delta", SM_STALE_NONE, 0, Volts);

    ChargerPowerEff = MyMetrics.InitFloat("vwup.chrgr.eff.ecu", 100, 0, Percentage);
    ChargerPowerLoss = MyMetrics.InitFloat("vwup.chrgr.loss.ecu", SM_STALE_NONE, 0, Watts);
    ChargerPowerEffCalc = MyMetrics.InitFloat("vwup.chrgr.eff.calc", 100, 0, Percentage);
    ChargerPowerLossCalc = MyMetrics.InitFloat("vwup.chrgr.loss.calc", SM_STALE_NONE, 0, Watts);
    ChargerACP = MyMetrics.InitFloat("vwup.chrgr.ac.p", SM_STALE_NONE, 0, Watts);
    ChargerDCP = MyMetrics.InitFloat("vwup.chrgr.dc.p", SM_STALE_NONE, 0, Watts);
}

OvmsVehicleVWeUpObd::~OvmsVehicleVWeUpObd()
{
    ESP_LOGI(TAG, "Stop VW e-Up OBD vehicle module");
}

void OvmsVehicleVWeUpObd::Ticker1(uint32_t ticker)
{
    CheckCarState();
}

void OvmsVehicleVWeUpObd::Ticker10(uint32_t ticker)
{
    // Option: with mongoose ala canlog_tcpclient.cpp

//    if (ticker < 30) return;

// WORKS BUT FREEZES OVMS SOMEHOW
    // const char *payload = "Message from OVMS";

    // char rx_buffer[128];
    // char addr_str[128];
    // int addr_family;
    // int ip_protocol;

    
    // ESP_LOGW(TAG, "Trying to send TCP package...");


    //     struct sockaddr_in destAddr;
    //     destAddr.sin_addr.s_addr = inet_addr("192.168.254.6");
    //     destAddr.sin_family = AF_INET;
    //     destAddr.sin_port = htons(54321);
    //     addr_family = AF_INET;
    //     ip_protocol = IPPROTO_IP;
    //     inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

    //     int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
    //     if (sock < 0) {
    //         ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    //         return;
    //     }
    //     ESP_LOGI(TAG, "Socket created");

    //     int err = connect(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
    //     if (err != 0) {
    //         ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
    //         return;
    //     }
    //     ESP_LOGI(TAG, "Successfully connected");

    //     while (1) {
    //         int err = send(sock, payload, strlen(payload), 0);
    //         if (err < 0) {
    //             ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
    //             break;
    //         }

    //         // int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
    //         // // Error occured during receiving
    //         // if (len < 0) {
    //         //     ESP_LOGE(TAG, "recv failed: errno %d", errno);
    //         //     break;
    //         // }
    //         // // Data received
    //         // else {
    //         //     rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
    //         //     ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
    //         //     ESP_LOGI(TAG, "%s", rx_buffer);
    //         // }

    //         //vTaskDelay(2000 / portTICK_PERIOD_MS);
    //         break;
    //     }

    //     //if (sock != -1) {
    //         ESP_LOGE(TAG, "Shutting down socket and restarting...");
    //         shutdown(sock, 0);
    //         close(sock);
    //     //}    
    // ESP_LOGW(TAG, "TCP ended...");

//https://github.com/nkolban/esp32-snippets/blob/master/sockets/client/socketClient.c
    // ESP_LOGD(tag, "start");
	// int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// ESP_LOGD(tag, "socket: rc: %d", sock);
	// struct sockaddr_in serverAddress;
	// serverAddress.sin_family = AF_INET;
	// inet_pton(AF_INET, "192.168.1.200", &serverAddress.sin_addr.s_addr);
	// serverAddress.sin_port = htons(9999);

	// int rc = connect(sock, (struct sockaddr *)&serverAddress, sizeof(struct sockaddr_in));
	// ESP_LOGD(tag, "connect rc: %d", rc);

	// char *data = "Hello world";
	// rc = send(sock, data, strlen(data), 0);
	// ESP_LOGD(tag, "send: rc: %d", rc);

	// rc = close(sock);
	// ESP_LOGD(tag, "close: rc: %d", rc);

	// vTaskDelete(NULL);
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
            PollSetState(VWUP_CHARGING);
        }
        return;
    }

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

            value = StandardMetrics.ms_v_bat_voltage->AsFloat() * StandardMetrics.ms_v_bat_current->AsFloat() / 1000.0f;
            StandardMetrics.ms_v_bat_power->SetValue(value);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_POWER=%f => %f", value, StandardMetrics.ms_v_bat_power->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_SOC:
        if (PollReply.FromUint8("VWUP_BAT_MGMT_SOC", value))
        {
            StandardMetrics.ms_v_bat_soc->SetValue(value / 2.5f);
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_SOC=%f => %f", value, StandardMetrics.ms_v_bat_soc->AsFloat());
        }
        break;

    case VWUP_BAT_MGMT_ENERGY_COUNTERS:
        if (PollReply.FromInt32("VWUP_BAT_MGMT_ENERGY_COUNTERS_CHARGED", value, 8))
        {
            BatMgmtEnergyCharged->SetValue(value / ((0xFFFFFFFF / 2.0f) / 250200.0f));
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_ENERGY_COUNTERS_CHARGED=%f => %f", value, BatMgmtEnergyCharged->AsFloat());
        }
        if (PollReply.FromInt32("VWUP_BAT_MGMT_ENERGY_COUNTERS_USED", value, 12))
        {
            BatMgmtEnergyUsed->SetValue(value / ((0xFFFFFFFF / 2.0f) / 250200.0f));
            VALUE_LOG(TAG, "VWUP_BAT_MGMT_ENERGY_COUNTERS_USED=%f => %f", value, BatMgmtEnergyUsed->AsFloat());
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

            value = ChargerAC1U * ChargerAC1I + ChargerAC2U * ChargerAC2I;
            ChargerACP->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_AC_P=%f => %f", value, ChargerACP->AsFloat());
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

            value = ChargerDC1U * ChargerDC1I + ChargerDC2U * ChargerDC2I;
            ChargerDCP->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_DC_P=%f => %f", value, ChargerDCP->AsFloat());
            
            value = ChargerACP->AsFloat() - ChargerDCP->AsFloat();
            ChargerPowerLossCalc->SetValue(value);            
            VALUE_LOG(TAG, "VWUP_CHARGER_LOSS_CALC=%f => %f", value, ChargerPowerLossCalc->AsFloat());

            value = ChargerACP->AsFloat() > 0 ? ChargerDCP->AsFloat() / ChargerACP->AsFloat() * 100.0f : 0.0f;
            ChargerPowerEffCalc->SetValue(value);
            VALUE_LOG(TAG, "VWUP_CHARGER_EFF_CALC=%f => %f", value, ChargerPowerEffCalc->AsFloat());
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

    case VWUP_MFD_ODOMETER:
        if (PollReply.FromUint16("VWUP_MFD_ODOMETER", value))
        {
            StandardMetrics.ms_v_pos_odometer->SetValue(value * 10.0f);
            VALUE_LOG(TAG, "VWUP_MFD_ODOMETER=%f => %f", value, StandardMetrics.ms_v_pos_odometer->AsFloat());
        }
        break;
    }
}