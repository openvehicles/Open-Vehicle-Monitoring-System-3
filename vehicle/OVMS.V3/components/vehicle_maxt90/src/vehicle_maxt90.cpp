#include "ovms_log.h"
static const char *TAG = "v-maxt90";

#include <stdio.h>
#include <string>
#include "vehicle_maxt90.h"
#include "vehicle_obdii.h"
#include "metrics_standard.h"

OvmsVehicleMaxt90::OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Initialising Maxus T90 EV vehicle module (derived from OBDII)");

  // Register CAN1 bus at 500 kbps
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // Custom metrics:
  // HVAC/Coolant temp (°C). Use unit enum; 'Other' is safe on all trees.
  m_hvac_temp_c = MyMetrics.InitFloat("x.maxt90.hvac_temp_c", 10, 0.0f, Other, false);

  // Battery capacity (kWh) – static, for dashboards/HA use:
  m_pack_capacity_kwh = MyMetrics.InitFloat("x.maxt90.capacity_kwh", 0, 88.5f, Other, true);

  // Define poll list (VIN, SOC, SOH, READY flag, Plug present, HVAC temp, Ambient temp)
  static const OvmsPoller::poll_pid_t maxt90_polls[] = {
    // VIN – 0x22 F190: one-shot
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF190,
      { 0, 999, 999 }, 0, ISOTP_STD },

    // SOC – 0x22 E002: fast
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE002,
      { 10, 10, 10 }, 0, ISOTP_STD },

    // SOH – 0x22 E003: medium
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE003,
      { 30, 30, 30 }, 0, ISOTP_STD },

    // Ignition/READY bitfield – 0x22 E004: fast (we only need two bits, but sample often)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE004,
      { 10, 10, 10 }, 0, ISOTP_STD },

    // Plug present – 0x22 E009: fast
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE009,
      { 10, 10, 10 }, 0, ISOTP_STD },

    // HVAC/Coolant temp (°C) – 0x22 E010: medium
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE010,
      { 30, 30, 30 }, 0, ISOTP_STD },

    // Ambient temp (°C) – 0x22 E025: medium
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE025,
      { 30, 30, 30 }, 0, ISOTP_STD },

    POLL_LIST_END
  };

  // Attach the poll list to CAN1 & start
  PollSetPidList(m_can1, maxt90_polls);
  PollSetState(0);

  ESP_LOGI(TAG, "Maxus T90 EV poller configured on CAN1 @ 500 kbps");
}

OvmsVehicleMaxt90::~OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Shutdown Maxus T90 EV vehicle module");
}

void OvmsVehicleMaxt90::IncomingPollReply(const OvmsPoller::poll_job_t& job,
                                          uint8_t* data, uint8_t length)
{
  // 'job.pid' is the DID; payload is already assembled for you.

  switch (job.pid)
  {
    case 0xF190: { // VIN (ASCII)
      if (length >= 1) {
        std::string vin(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data) + length);
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
        if (soh > 150.0f) soh = 150.0f; // sanity clamp
        StdMetrics.ms_v_bat_soh->SetValue(soh);
        ESP_LOGD(TAG, "SOH: %.2f %%", soh);
      }
      break;
    }

    case 0xE004: { // READY bitfield (u16): READY when (val & 0x000C) != 0
      if (length >= 2) {
        uint16_t v = u16be(data);
        bool ready = (v & 0x000C) != 0;
        StdMetrics.ms_v_env_on->SetValue(ready);
        ESP_LOGD(TAG, "READY flag: raw=0x%04x ready=%s", v, ready ? "true" : "false");
      }
      break;
    }

    case 0xE009: { // Plug present (u16): plug if low byte == 0x00
      if (length >= 2) {
        uint16_t v = u16be(data);
        bool plug_present = ((v & 0x00FF) == 0x00);
        StdMetrics.ms_v_charge_pilot->SetValue(plug_present);
        ESP_LOGD(TAG, "Plug present: raw=0x%04x plug=%s", v, plug_present ? "true" : "false");
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
      // Not handled yet
      break;
  }
}

// ────────────────────────────────────────────────────────────────
//   Module Registration
// ────────────────────────────────────────────────────────────────

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
