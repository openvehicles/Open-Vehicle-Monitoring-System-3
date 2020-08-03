#include "ovms_log.h"

static const char *TAG = "vwup.obd";

#include <stdio.h>
#include "pcp.h"
#include "obd_eup.h"
#include "metrics_standard.h"
#include "ovms_webserver.h"
#include "ovms_events.h"
#include "ovms_metrics.h"
#include "bytes_to_float_converter.h"

// Last array (polltime) is for max. 4 different poll-states. I only use State=0 at the moment
const OvmsVehicle::poll_pid_t vwup_polls[] = {
    // Note: poller ticker cycles at 3600 seconds = max period
    // { txid, rxid, type, pid, { VWUP_OFF, VWUP_ON, VWUP_CHARGING } }
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_U, {30, VWUP_OFF_TICKER_THRESHOLD, VWUP_OFF_TICKER_THRESHOLD}}, // Used to find out if car is on
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_I, {0, 0, 0}},
    {VWUP_BAT_MGMT_TX, VWUP_BAT_MGMT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_SOC, {0, 0, 0}},
    {0, 0, 0, 0, {0, 0, 0}}};

VWeUpObd::VWeUpObd()
{
  ESP_LOGI(TAG, "Start VW e-Up OBD vehicle module");

  // init configs:
  MyConfig.RegisterParam("vwup", "VW e-Up", true, true);
  ConfigChanged(NULL);

  // init polls:
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  PollSetPidList(m_can1, vwup_polls);
  PollSetState(VWUP_OFF);
}

VWeUpObd::~VWeUpObd()
{
  ESP_LOGI(TAG, "Stop VW e-Up OBD vehicle module");
}

void VWeUpObd::IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t remain)
{
  ESP_LOGD(TAG, "IncomingPollReply(type=%u, pid=%X, length=%u, remain=%u): called", type, pid, length, remain);

  if (IsOff())
  {
    ESP_LOGI(TAG, "Setting car state to ON");
    PollSetState(VWUP_ON);
  }
  CarOffTicker = 0;

  // for (uint8_t i = 0; i < length; i++)
  // {
  //   ESP_LOGV(TAG, "data[%u]=%X", i, data[i]);
  // }

  BytesToFloatConverter value(data, length);

  switch (pid)
  {
  case VWUP_BAT_MGMT_U:
    if (length < 2)
    {
      ESP_LOGW(TAG, "data for VWUP_BAT_MGMT_U has insufficient length");
    }
    else
    {
      StandardMetrics.ms_v_bat_voltage->SetValue(value.FromUint16() / 4.0f);
      ESP_LOGV(TAG, "StandardMetrics.ms_v_bat_voltage=%f", StandardMetrics.ms_v_bat_voltage->AsFloat());
    }
    break;

  case VWUP_BAT_MGMT_I:
    if (length < 2)
    {
      ESP_LOGW(TAG, "data for VWUP_BAT_MGMT_I has insufficient length");
    }
    else
    {
      StandardMetrics.ms_v_bat_current->SetValue(value.FromSint12() / 4.0f);
      ESP_LOGV(TAG, "StandardMetrics.ms_v_bat_current=%f", StandardMetrics.ms_v_bat_current->AsFloat());
    }
    break;

  case VWUP_BAT_MGMT_SOC:
    if (length < 1)
    {
      ESP_LOGW(TAG, "data for VWUP_BAT_MGMT_SOC has insufficient length");
    }
    else
    {
      StandardMetrics.ms_v_bat_soc->SetValue(value.FromUint8() / 2.55f);
      ESP_LOGV(TAG, "StandardMetrics.ms_v_bat_soc=%f", StandardMetrics.ms_v_bat_soc->AsFloat());
    }
    break;
  }
}

void VWeUpObd::Ticker1(uint32_t ticker)
{
  if (IsOff())
  {
    return;
  }

  if (CarOffTicker > VWUP_OFF_TICKER_THRESHOLD * 4) // 4 trys
  {
    ESP_LOGI(TAG, "Setting car state to OFF");
    PollSetState(VWUP_OFF);
  }
  else
  {
    CarOffTicker++;
    ESP_LOGV(TAG, "CarOffTicker increased to %u", CarOffTicker);
  }
}

void VWeUpObd::Ticker10(uint32_t ticker)
{
  // if (!IsOn()) {
  //   ESP_LOGI(TAG, "Setting car state to ON");
  //   PollSetState(VWUP_ON);
  // } else {
  //   ESP_LOGI(TAG, "Setting car state to CHARGING");
  //   PollSetState(VWUP_CHARGING);
  // }
}