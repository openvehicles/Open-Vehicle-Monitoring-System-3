#include "ovms_log.h"
static const char *TAG = "v-maxt90";

#include <stdio.h>
#include <string>
#include "vehicle_maxt90.h"
#include "vehicle_obdii.h"
#include "metrics_standard.h"

//
// ────────────────────────────────────────────────────────────────
//   Class Definition
// ────────────────────────────────────────────────────────────────
//

OvmsVehicleMaxt90::OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Initialising Maxus T90 EV vehicle module (derived from OBDII)");

  // Register CAN1 bus at 500 kbps
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // Create custom metrics
  // Poll E010 suggests a slow-moving HVAC/coolant temp in °C (scale /10)
  m_hvac_temp_c = MyMetrics.InitFloat("x.maxt90.hvac_temp_c", 10, 0, "°C");

  //
  // Define our T90 poll list (VIN, SOC, SOH, HVAC temp, Ambient temp)
  //
  static const OvmsPoller::poll_pid_t maxt90_polls[] = {
    // VIN – Service 0x22, PID 0xF190 (one-shot on connect)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF190,
      { 0, 999, 999 }, 0, ISOTP_STD },

    // SOC – 0x22 E002 (fast)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE002,
      { 10, 10, 10 }, 0, ISOTP_STD },

    // SOH – 0x22 E003 (medium)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE003,
      { 30, 30, 30 }, 0, ISOTP_STD },

    // HVAC/Coolant temperature (°C) – 0x22 E010 (medium)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE010,
      { 30, 30, 30 }, 0, ISOTP_STD },

    // Ambient temperature (°C) – 0x22 E025 (medium)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE025,
      { 30, 30, 30 }, 0, ISOTP_STD },

    POLL_LIST_END
  };

  // Attach the poll list to CAN1
  PollSetPidList(m_can1, maxt90_polls);
  // Start in poll state 0 (default OBDII behaviour)
  PollSetState(0);

  ESP_LOGI(TAG, "Maxus T90 EV poller configured on CAN1 @ 500 kbps");
}

OvmsVehicleMaxt90::~OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Shutdown Maxus T90 EV vehicle module");
}

void OvmsVehicleMaxt90::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid,
                                          uint8_t* data, uint8_t length, uint16_t mlremain)
{
  // Let the base class assemble multi-frame payloads and such if needed:
  OvmsVehicleOBDII::IncomingPollReply(bus, type, pid, data, length, mlremain);

  if (mlremain) {
    // More data to come; wait for the final chunk before decoding.
    return;
  }

  switch (pid)
  {
    case 0xF190: { // VIN (ASCII)
      if (length >= 1) {
        std::string vin;
        vin.reserve(length);
        for (uint8_t i = 0; i < length; i++)
          vin.push_back(static_cast<char>(data[i]));
        StdMetrics.ms_v_vin->SetValue(vin);
        ESP_LOGD(TAG, "VIN: %s", vin.c_str());
      }
      break;
    }

    case 0xE002: { // SOC (%): dat[0]
      if (length >= 1) {
        float soc = data[0];
        StdMetrics.ms_v_bat_soc->SetValue(soc);
        ESP_LOGD(TAG, "SOC: %.0f %%", soc);
      }
      break;
    }

    case 0xE003: { // SOH (%): (dat[0]*256 + dat[1]) / 100
      if (length >= 2) {
        float soh = (static_cast<uint16_t>(data[0]) << 8 | data[1]) / 100.0f;
        // Some OEMs report > 100 early; clamp sensibly:
        if (soh > 150.0f) soh = 150.0f;
        StdMetrics.ms_v_bat_soh->SetValue(soh);
        ESP_LOGD(TAG, "SOH: %.2f %%", soh);
      }
      break;
    }

    case 0xE010: { // HVAC/Coolant temperature (°C): u16 / 10
      if (length >= 2 && m_hvac_temp_c) {
        float t = u16be(data) / 10.0f;
        m_hvac_temp_c->SetValue(t);
        ESP_LOGD(TAG, "HVAC/Coolant temp: %.1f °C", t);
      }
      break;
    }

    case 0xE025: { // Ambient temperature (°C): u16 / 10
      if (length >= 2) {
        float ta = u16be(data) / 10.0f;
        StdMetrics.ms_v_env_temp->SetValue(ta);
        ESP_LOGD(TAG, "Ambient temp: %.1f °C", ta);
      }
      break;
    }

    default:
      // Not handled (yet) — keep exploring…
      break;
  }
}

//
// ────────────────────────────────────────────────────────────────
//   Module Registration
// ────────────────────────────────────────────────────────────────
//

class OvmsVehicleMaxt90Init
{
public:
  OvmsVehicleMaxt90Init();
} MyOvmsVehicleMaxt90Init __attribute__((init_priority(9000)));

OvmsVehicleMaxt90Init::OvmsVehicleMaxt90Init()
{
  ESP_LOGI(TAG, "Registering Vehicle: Maxus T90 EV (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxt90>("MAXT90", "Maxus T90 EV");
}
