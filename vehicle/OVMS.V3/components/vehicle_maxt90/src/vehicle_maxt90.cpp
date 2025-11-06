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
  m_hvac_temp_c = MyMetrics.InitFloat("x.maxt90.hvac_temp_c", 10, 0.0f, Other, false);
  m_pack_capacity_kwh = MyMetrics.InitFloat("x.maxt90.capacity_kwh", 0, 88.5f, Other, true);

  // Define poll list (VIN, SOC, SOH, READY flag, Plug present, HVAC temp, Ambient temp)
  static const OvmsPoller::poll_pid_t maxt90_polls[] = {
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF190, { 0, 999, 999 }, 0, ISOTP_STD }, // VIN
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE002, { 10, 10, 10 }, 0, ISOTP_STD }, // SOC
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE003, { 30, 30, 30 }, 0, ISOTP_STD }, // SOH
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE004, { 10, 10, 10 }, 0, ISOTP_STD }, // READY
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE009, { 10, 10, 10 }, 0, ISOTP_STD }, // Plug
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE010, { 30, 30, 30 }, 0, ISOTP_STD }, // HVAC temp
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE025, { 30, 30, 30 }, 0, ISOTP_STD }, // Ambient temp
    POLL_LIST_END
  };

  // Attach the poll list to CAN1 & start idle
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

    case 0xE002: { // SOC (%)
      if (length >= 1) {
        float soc = data[0];
        if (soc > 0 && soc <= 100) {
          if (StdMetrics.ms_v_bat_soc->AsFloat() != soc) {
            StdMetrics.ms_v_bat_soc->SetValue(soc);
            ESP_LOGD(TAG, "SOC: %.0f %%", soc);
          }
        } else {
          ESP_LOGW(TAG, "Invalid SOC %.1f ignored (car likely off or poll timeout)", soc);
        }
      }
      break;
    }

    case 0xE003: { // SOH (%)
      if (length >= 2) {
        uint16_t raw = u16be(data);
        float soh = raw / 100.0f;

        // Filter out bogus default values (0xFFFF, 0x1800 = 61.44%, etc.)
        if (raw == 0xFFFF || raw == 0x1800 || soh <= 50.0f || soh > 150.0f) {
          ESP_LOGW(TAG, "Invalid SOH raw=0x%04x (%.2f %%) ignored", raw, soh);
          break;
        }

        if (StdMetrics.ms_v_bat_soh->AsFloat() != soh) {
          StdMetrics.ms_v_bat_soh->SetValue(soh);
          ESP_LOGD(TAG, "SOH: %.2f %%", soh);
        }
      }
      break;
    }

    case 0xE004: { // READY bitfield
      if (length >= 2) {
        uint16_t v = u16be(data);
        bool ready = (v & 0x000C) != 0;
        bool prev_ready = StdMetrics.ms_v_env_on->AsBool();
        StdMetrics.ms_v_env_on->SetValue(ready);

        if (ready != prev_ready) {
          ESP_LOGI(TAG, "READY flag changed: raw=0x%04x ready=%s", v, ready ? "true" : "false");

          if (!ready && m_poll_state != 0) {
            ESP_LOGI(TAG, "Vehicle OFF detected, suspending polling");
            PollSetState(0);
          }
          else if (ready && m_poll_state == 0) {
            ESP_LOGI(TAG, "Vehicle ON detected, resuming polling");
            PollSetState(1);
          }
        }
      }
      break;
    }

    case 0xE009: { // Plug present (u16)
      if (length >= 2) {
        uint16_t v = u16be(data);
        bool plug_present = ((v & 0x00FF) == 0x00);
        StdMetrics.ms_v_charge_pilot->SetValue(plug_present);
        ESP_LOGD(TAG, "Plug present: raw=0x%04x plug=%s", v, plug_present ? "true" : "false");
      }
      break;
    }

    case 0xE010: { // HVAC/Coolant temperature (°C)
      if (length >= 2 && m_hvac_temp_c) {
        uint16_t raw = u16be(data);
        float t = raw / 10.0f;

        // Filter out known bogus patterns (default buffer or timeout)
        if (raw == 0x0200 || raw == 0xFFFF || t < -40 || t > 125) {
          ESP_LOGW(TAG, "Invalid HVAC temp raw=0x%04x (%.1f °C) ignored", raw, t);
          break;
        }

        m_hvac_temp_c->SetValue(t);
        ESP_LOGD(TAG, "HVAC/Coolant temp: %.1f °C", t);
      }
      break;
    }

    case 0xE025: { // Ambient temperature (°C)
      if (length >= 2) {
        uint16_t raw = u16be(data);
        float ta = raw / 10.0f;

        // Filter out default/bogus data
        if (raw == 0x0200 || raw == 0xFFFF || ta < -50 || ta > 80) {
          ESP_LOGW(TAG, "Invalid ambient temp raw=0x%04x (%.1f °C) ignored", raw, ta);
          break;
        }

        StdMetrics.ms_v_env_temp->SetValue(ta);
        ESP_LOGD(TAG, "Ambient temp: %.1f °C", ta);
      }
      break;
    }

    default:
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
