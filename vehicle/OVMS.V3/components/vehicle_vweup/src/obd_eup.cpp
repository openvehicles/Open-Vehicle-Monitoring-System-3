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
const OvmsVehicle::poll_pid_t vwup_poll[] = {
  // Note: poller ticker cycles at 3600 seconds = max period
  // { txid, rxid, type, pid, { period_off, period_drive, period_charge } }
  { 0x7E5, 0x7ED, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x028C, { 0, 10, 60 } },
  { 0, 0, 0, 0, { 0, 0, 0 } }
};

VWeUpObd::VWeUpObd()
{
  ESP_LOGI(TAG, "Start VW e-Up OBD vehicle module");
  
  // init configs:
  MyConfig.RegisterParam("vwup", "VW e-Up", true, true);
  ConfigChanged(NULL);
}

VWeUpObd::~VWeUpObd()
{
  ESP_LOGI(TAG, "Stop VW e-Up OBD vehicle module");
}