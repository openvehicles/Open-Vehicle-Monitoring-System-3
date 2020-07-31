#include "ovms_log.h"

static const char *TAG = "vwup.obd";

#include <stdio.h>
#include "pcp.h"
#include "obd_eup.h"
#include "metrics_standard.h"
#include "ovms_webserver.h"
#include "ovms_events.h"
#include "ovms_metrics.h"

// Last array (polltime) is for max. 4 different poll-states. I only use State=0 at the moment
const OvmsVehicle::poll_pid_t vwup_polls[] = {
    // Note: poller ticker cycles at 3600 seconds = max period
    // { txid, rxid, type, pid, { period_off, period_drive, period_charge } }
    {VWUP_BAT_MGMT_RX, VWUP_BAT_MGMT_TX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, VWUP_BAT_MGMT_SOC, {10, 10, 10}},
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
  PollSetState(0);
}

VWeUpObd::~VWeUpObd()
{
  ESP_LOGI(TAG, "Stop VW e-Up OBD vehicle module");
}

void VWeUpObd::IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t remain)
{
  ESP_LOGD(TAG, "IncomingPollReply(type=%u, pid=%u, length=%u, remain=%u): called", type, pid, length, remain);

  switch (pid)
  {
  case VWUP_BAT_MGMT_SOC:
    uint8_t value = *data;
    float floatSoC = (float)((short)value) / 2.55f;
    ESP_LOGV(TAG, "IncomingPollReply(): pid=%X value=%u floatSoC=%f", (short)pid, value, floatSoC);
    StandardMetrics.ms_v_bat_soc->SetValue(floatSoC);
    break;
  }
}