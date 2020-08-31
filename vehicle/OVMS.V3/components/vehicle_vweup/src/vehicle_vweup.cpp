;
;    0.2.2  Collect VIN only once
;
;    (C) 2020       Chris van der Meijden
;
;    Big thanx to sharkcow and Dimitrie78.
*/
#include "ovms_log.h"
static const char *TAG = "v-vweup";
#define VERSION "0.1.9"
#include <stdio.h>
#include "pcp.h"
#include "vehicle_vweup.h"
#include "metrics_standard.h"
#include "ovms_webserver.h"
#include "ovms_events.h"
#include "ovms_metrics.h"

static const OvmsVehicle::poll_pid_t vwup_polls[] =
{
  { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
  { 0, 0, 0x00, 0x00, { 0, 0, 0 }, 0 }
};

void v_remoteCommandTimer(TimerHandle_t timer)
  {
  OvmsVehicleVWeUP* vwup = (OvmsVehicleVWeUP*) pvTimerGetTimerID(timer);
  vwup->RemoteCommandTimer();
  }
void v_ccDisableTimer(TimerHandle_t timer)
  {
  OvmsVehicleVWeUP* vwup = (OvmsVehicleVWeUP*) pvTimerGetTimerID(timer);
  vwup->CcDisableTimer();
  }
OvmsVehicleVWeUP::OvmsVehicleVWeUP()
  {
  ESP_LOGI(TAG, "Start VW e-Up vehicle module");
  memset (m_vin,0,sizeof(m_vin));
  RegisterCanBus(3,CAN_MODE_ACTIVE,CAN_SPEED_100KBPS);
  MyConfig.RegisterParam("vwup", "VW e-Up", true, true);
  ConfigChanged(NULL);

