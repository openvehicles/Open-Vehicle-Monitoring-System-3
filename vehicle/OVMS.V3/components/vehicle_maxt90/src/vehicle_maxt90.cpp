#include "ovms_log.h"
static const char *TAG = "v-maxt90";

#include <stdio.h>
#include <string>
#include "vehicle_maxt90.h"
#include "vehicle_obdii.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"

OvmsVehicleMaxt90::OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Initialising Maxus T90 EV vehicle module (derived from OBDII)");

  // Register CAN1 bus at 500 kbps
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // Custom metrics:
  // Prefix "xmt" = Maxus T90 (to match xnl, xmg, etc. in other vehicles)
  // NOTE: adjust Celcius / kWh to match your metric_unit_t enum names
  m_hvac_temp_c =
    MyMetrics.InitFloat("xmt.v.hvac.temp", 10, 0.0f, Celcius, false);
  m_pack_capacity_kwh =
    MyMetrics.InitFloat("xmt.b.capacity", 0, 88.5f, kWh, true);

  // Define poll list:
  //  - State 0: vehicle off
  //  - State 1: vehicle on / driving
  //  - State 2: charging (not used yet, but kept for future)
  //
  // Only READY (0xE004) is polled in state 0 to avoid keeping ECUs awake.
  static const OvmsPoller::poll_pid_t maxt90_polls[] = {
    // VIN (only when on / charge, slow rate)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF190,
      { 0, 3600, 3600 }, 0, ISOTP_STD },

    // SOC
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE002,
      { 0, 10, 10 }, 0, ISOTP_STD },

    // SOH
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE003,
      { 0, 1800, 1800 }, 0, ISOTP_STD },

    // READY flag – polled in all states, faster in "off" to detect wake
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE004,
      { 5, 10, 10 }, 0, ISOTP_STD },

    // Plug present
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE009,
      { 0, 10, 10 }, 0, ISOTP_STD },

    // HVAC temp
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE010,
      { 0, 30, 30 }, 0, ISOTP_STD },

    // Ambient temp
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE025,
      { 0, 30, 30 }, 0, ISOTP_STD },

    POLL_LIST_END
  };

  // Attach the poll list to CAN1 & start in "off" state
  PollSetPidList(m_can1, maxt90_polls);
  PollSetState(0);

  ESP_LOGI(TAG, "Maxus T90 EV poller configured on CAN1 @ 500 kbps");
}

OvmsVehicleMaxt90::~OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Shutdown Maxus T90 EV vehicle module");
}

// ─────────────────────────────────────────────
//  Live CAN Frame Handler (Lock + Odometer)
// ─────────────────────────────────────────────
void OvmsVehicleMaxt90::IncomingFrameCan1(CAN_frame_t* p_frame)
{
  // Let the base class see the frame as well (for diagnostics etc.)
  OvmsVehicleOBDII::IncomingFrameCan1(p_frame);

  if (p_frame->origin != m_can1)
    return;

  const uint8_t* d = p_frame->data.u8;

  switch (p_frame->MsgID)
  {
    case 0x281: // Body Control Module: lock state
    {
      uint8_t state = d[1];
      static uint8_t last_state = 0x00;

      // 0xA9 = locked, 0xA8 = unlocked
      if (state != last_state && (state == 0xA9 || state == 0xA8))
      {
        bool locked = (state == 0xA9);

        // Standard OVMS metric – used by apps / HA:
        StdMetrics.ms_v_env_locked->SetValue(locked);

        ESP_LOGI(TAG, "Lock state changed: %s (CAN 0x281 byte1=0x%02x)",
                 locked ? "LOCKED" : "UNLOCKED", state);

        last_state = state;
      }
      break;
    }

    case 0x540: // Odometer (from your trace)
    {
      // Frame example seen:
      // 540 00 00 00 00 90 f0 02 00
      //
      // Treat bytes [4..6] as 24-bit little-endian, resolution 0.1 km:
      //   raw = d4 + d5*256 + d6*65536
      //   km  = raw / 10
      uint32_t raw =
        (uint32_t)d[4] |
        ((uint32_t)d[5] << 8) |
        ((uint32_t)d[6] << 16);

      float km = raw / 10.0f;

      if (km > 0 && km < 1000000.0f)
      {
        if (StdMetrics.ms_v_pos_odometer->AsFloat() != km)
        {
          StdMetrics.ms_v_pos_odometer->SetValue(km);
          ESP_LOGI(TAG, "Odometer: %.1f km (raw=0x%06x)", km, raw);
        }
      }
      else
      {
        ESP_LOGW(TAG, "Odometer raw=0x%06x (%.1f km) out of range, ignored",
                 raw, km);
      }
      break;
    }

    default:
      break;
  }
}

// ─────────────────────────────────────────────
//  OBDII Poll Reply Handler
// ─────────────────────────────────────────────
void OvmsVehicleMaxt90::IncomingPollReply(const OvmsPoller::poll_job_t& job,
                                          uint8_t* data, uint8_t length)
{
  switch (job.pid)
  {
    case 0xF190: { // VIN (ASCII)
      if (length >= 1) {
        std::string vin(reinterpret_cast<char*>(data),
                        reinterpret_cast<char*>(data) + length);
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
          ESP_LOGW(TAG,
                   "Invalid SOC %.1f ignored (car likely off or poll timeout)",
                   soc);
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
          ESP_LOGI(TAG, "READY flag changed: raw=0x%04x ready=%s",
                   v, ready ? "true" : "false");

          if (!ready && m_poll_state != 0) {
            ESP_LOGI(TAG, "Vehicle OFF detected, setting poll state 0");
            PollSetState(0);
          }
          else if (ready && m_poll_state == 0) {
            ESP_LOGI(TAG, "Vehicle ON detected, setting poll state 1");
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
        ESP_LOGD(TAG, "Plug present: raw=0x%04x plug=%s",
                 v, plug_present ? "true" : "false");
      }
      break;
    }

    case 0xE010: { // HVAC/Coolant temperature (°C)
      if (length >= 2 && m_hvac_temp_c) {
        uint16_t raw = u16be(data);
        float t = raw / 10.0f;

        bool env_on = StdMetrics.ms_v_env_on->AsBool();

        // Ignore the constant bogus 45.8 °C we see when the car is off
        if (!env_on && raw == 458) {
          ESP_LOGW(TAG,
                   "HVAC temp raw=0x%04x (%.1f °C) ignored (car off/default)",
                   raw, t);
          break;
        }

        // Filter out known bogus patterns (default buffer or timeout)
        if (raw == 0x0200 || raw == 0xFFFF || t < -40 || t > 125) {
          ESP_LOGW(TAG, "Invalid HVAC temp raw=0x%04x (%.1f °C) ignored",
                   raw, t);
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

        bool env_on = StdMetrics.ms_v_env_on->AsBool();

        // Ignore the constant bogus 7.5 °C we see when the car is off
        if (!env_on && raw == 75) {
          ESP_LOGW(TAG,
                   "Ambient temp raw=0x%04x (%.1f °C) ignored (car off/default)",
                   raw, ta);
          break;
        }

        // Filter out default/bogus data
        if (raw == 0x0200 || raw == 0xFFFF || ta < -50 || ta > 80) {
          ESP_LOGW(TAG, "Invalid ambient temp raw=0x%04x (%.1f °C) ignored",
                   raw, ta);
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

// ─────────────────────────────────────────────
//   Module Registration
// ─────────────────────────────────────────────

class OvmsVehicleMaxt90Init
{
public:
  OvmsVehicleMaxt90Init();
} MyOvmsVehicleMaxt90Init __attribute__((init_priority(9000)));

OvmsVehicleMaxt90Init::OvmsVehicleMaxt90Init()
{
  ESP_LOGI(TAG, "Registering Vehicle: Maxus T90 EV (9000)");
  // Vehicle type string "MT90" is the type code in OVMS:
  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxt90>(
    "MT90", "Maxus T90 EV");
}
