#include "ovms_log.h"
static const char *TAG = "v-maxus-t90";

#include "ovms_metric.h"
#include "vehicle.h"
#include "vehicle/vehicle.h"
#include "vehicle_maxus_t90.h"

static const OvmsVehicle::poll_pid_t t90_polls[] = {
  // bus, type,                   req,   resp,  pid,    len, slow, normal, fast (seconds)
  { 1,   VEHICLE_POLL_TYPE_OBDIIEXT, 0x7e3, 0x7eb, 0xe002, 1,   30,   10,     5 }, // SoC
  POLL_LIST_END
};

OvmsVehicleMaxusT90::OvmsVehicleMaxusT90()
{
  ESP_LOGI(TAG, "Maxus T90 EV vehicle initialising");
  RegisterPollVector(t90_polls);
  StandardMetrics.ms_v_type->SetValue("T90EV");
  StandardMetrics.ms_v_bat_soc->SetValue(0);
}

OvmsVehicleMaxusT90::~OvmsVehicleMaxusT90()
{
  ESP_LOGI(TAG, "Maxus T90 EV vehicle shutdown");
}

const char* OvmsVehicleMaxusT90::VehicleShortName()
{
  return "Maxus T90 EV";
}

void OvmsVehicleMaxusT90::IncomingPollReply(CAN_frame_t* frame, uint16_t pid,
    uint8_t* data, uint8_t length, uint16_t mlremain)
{
  OvmsVehicle::IncomingPollReply(frame, pid, data, length, mlremain);

  if (pid == 0xe002 && length >= 1) {
    // Try data[0] first. If your RX frames look like "e0 02 64", change to data[2].
    uint8_t soc = data[0];
    StandardMetrics.ms_v_bat_soc->SetValue((float)soc);
  }
}

// Register with vehicle factory
class OvmsVehicleMaxusT90Init {
public:
  OvmsVehicleMaxusT90Init();
} MyOvmsVehicleMaxusT90Init __attribute__ ((init_priority (9000)));

OvmsVehicleMaxusT90Init::OvmsVehicleMaxusT90Init()
{
  ESP_LOGI(TAG, "Registering vehicle: Maxus T90 EV");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxusT90>("T90EV","Maxus T90 EV");
}
