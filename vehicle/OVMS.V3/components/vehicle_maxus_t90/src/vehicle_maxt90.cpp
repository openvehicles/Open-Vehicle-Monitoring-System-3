#include "ovms_log.h"
static const char *TAG = "v-maxt90";

#include <stdio.h>
#include <string>
#include "esp_timer.h"
#include "vehicle_maxt90.h"
#include "vehicle_obdii.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"

// Seconds since boot (immune to GNSS clock changes)
static inline uint32_t now_s()
{
  return (uint32_t)(esp_timer_get_time() / 1000000LL);
}

static const uint32_t SOC_FALLBACK_S = 180;
static const uint32_t CHARGE_STALE_S = 120;
static const int HVAC_FAN_STEPS = 9;

OvmsVehicleMaxt90::OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Initialising Maxus T90 EV vehicle module (derived from OBDII)");

  // Register CAN1 bus at 500 kbps
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // Custom metrics:
  // Prefix "xmt" = Maxus T90 (to match xnl, xmg, etc. in other vehicles)
  m_hvac_temp_c =
    MyMetrics.InitFloat("xmt.v.hvac.temp", 10, 0.0f, Celcius, false);
  m_pack_capacity_kwh =
    MyMetrics.InitFloat("xmt.b.capacity", 0, 88.5f, kWh, true);

  // Poll list:
  //  - State 0: car off (polls E015 every 5s, D117 every 60s)
  //  - State 1: ignition on (E015 non-zero)
  //  - State 2: car off but charging (E018 active, E015 zero)
  static const OvmsPoller::poll_pid_t maxt90_polls[] = {
    // --- VCU 7e3/7eb ---
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF190, { 0, 3600, 3600 }, 0, ISOTP_STD }, // VIN (17 chars)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE002, { 60, 10, 60 }, 0, ISOTP_STD },   // SOC (VCU integer)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE003, { 0, 1800, 1800 }, 0, ISOTP_STD }, // SOH
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE015, { 5, 10, 10 }, 0, ISOTP_STD },    // Ignition state
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE018, { 30, 10, 10 }, 0, ISOTP_STD },   // HV active state
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE010, { 0, 30, 0 }, 0, ISOTP_STD },     // HVAC temp
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE025, { 0, 30, 0 }, 0, ISOTP_STD },     // Ambient temp

    // --- BMS 748/7c8 ---
    { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xD117, { 60, 30, 15 }, 0, ISOTP_STD },  // SOC (BMS 0.1%)
    { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB105, { 0, 5, 10 }, 0, ISOTP_STD },    // Pack current
    { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB11E, { 0, 10, 10 }, 0, ISOTP_STD },   // Pack voltage
    { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB110, { 0, 30, 30 }, 0, ISOTP_STD },   // Cell temps max/min/avg

    POLL_LIST_END
  };

  // Attach the poll list to CAN1 & start in "off" state
  PollSetPidList(m_can1, maxt90_polls);
  PollSetState(0);

  StdMetrics.ms_v_charge_inprogress->SetValue(false);
  StdMetrics.ms_v_charge_state->SetValue("");

  ESP_LOGI(TAG, "Maxus T90 EV poller configured on CAN1 @ 500 kbps");
}

OvmsVehicleMaxt90::~OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Shutdown Maxus T90 EV vehicle module");
}

bool OvmsVehicleMaxt90::Collect(uint16_t pid, const uint8_t* data, uint8_t length, size_t want)
{
  uint32_t now = now_s();

  if (pid != m_rxpid || (now - m_rxtime) > 5) {
    m_rxbuf.clear();
    m_rxpid = pid;
  }
  m_rxtime = now;
  m_rxbuf.append(reinterpret_cast<const char*>(data), length);

  return m_rxbuf.size() >= want;
}

void OvmsVehicleMaxt90::UpdateChargeState()
{
  bool charging = !m_ready && m_e018 > 0 && m_e015 == 0;

  if (charging != m_charging) {
    ESP_LOGI(TAG, "Charging %s (E015=%d E018=%d)",
             charging ? "started" : "stopped", m_e015, m_e018);
  }
  if (charging && !m_charging) {
    StdMetrics.ms_v_charge_time->SetValue(0);
    StdMetrics.ms_v_charge_kwh->SetValue(0);
    m_pow_have = false;
  }
  m_charging = charging;
  StdMetrics.ms_v_charge_inprogress->SetValue(charging);

  if (charging) {
    m_was_charging = true;
    StdMetrics.ms_v_charge_state->SetValue("charging");
  } else if (m_was_charging && !m_ready) {
    StdMetrics.ms_v_charge_state->SetValue("done");
  } else {
    StdMetrics.ms_v_charge_state->SetValue("");
  }
}

void OvmsVehicleMaxt90::UpdatePollState()
{
  int want = m_ready ? 1 : (m_charging ? 2 : 0);
  if (want != (int)m_poll_state) {
    ESP_LOGI(TAG, "Poll state %d -> %d (ready=%d charging=%d)",
             (int)m_poll_state, want, (int)m_ready, (int)m_charging);
    PollSetState(want);
  }
}

void OvmsVehicleMaxt90::Ticker10(uint32_t ticker)
{
  OvmsVehicleOBDII::Ticker10(ticker);

  if (m_charging && (now_s() - m_charge_seen) > CHARGE_STALE_S) {
    ESP_LOGI(TAG, "No charge-state reply for %u s: treating charge as ended",
             (unsigned)(now_s() - m_charge_seen));
    m_e018 = 0;
    UpdateChargeState();
    UpdatePollState();
  }
}

// ─────────────────────────────────────────────
//  Live CAN Frame Handler
// ─────────────────────────────────────────────
void OvmsVehicleMaxt90::IncomingFrameCan1(CAN_frame_t* p_frame)
{
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

      if (state != last_state && (state == 0xA9 || state == 0xA8))
      {
        bool locked = (state == 0xA9);
        StdMetrics.ms_v_env_locked->SetValue(locked);

        ESP_LOGI(TAG, "Lock state changed: %s (CAN 0x281 byte1=0x%02x)",
                 locked ? "LOCKED" : "UNLOCKED", state);

        last_state = state;
      }
      break;
    }

    case 0x375: // Doors & headlights
    {
      static bool seen = false;
      static uint8_t last0 = 0, last1 = 0;
      if (seen && d[0] == last0 && d[1] == last1)
        break;
      seen = true; last0 = d[0]; last1 = d[1];

      StdMetrics.ms_v_door_fl->SetValue((d[0] & 0x20) != 0);
      StdMetrics.ms_v_door_fr->SetValue((d[0] & 0x40) != 0);
      StdMetrics.ms_v_door_rr->SetValue((d[0] & 0x80) != 0);
      StdMetrics.ms_v_door_rl->SetValue((d[1] & 0x01) != 0);

      StdMetrics.ms_v_env_headlights->SetValue((d[0] & 0x0F) >= 0x03);
      break;
    }

    case 0x510: // HVAC status
    {
      static bool seen = false;
      static uint8_t last0 = 0, last1 = 0, last5 = 0;
      if (seen && d[0] == last0 && d[1] == last1 && d[5] == last5)
        break;
      seen = true; last0 = d[0]; last1 = d[1]; last5 = d[5];

      StdMetrics.ms_v_env_cabinfan->SetValue(
        ((d[0] & 0x0F) * 100) / HVAC_FAN_STEPS);

      if (d[1] >= 0x30 && d[1] <= 0x50)
        StdMetrics.ms_v_env_cabinsetpoint->SetValue((float)d[1] - 40.0f);

      StdMetrics.ms_v_env_hvac->SetValue((d[5] & 0x80) != 0);
      break;
    }

    case 0x540: // Odometer
    {
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
    case 0xF190: { // VIN (ASCII, 17 chars, three chunks)
      if (Collect(job.pid, data, length, 17)) {
        std::string vin = m_rxbuf.substr(0, 17);
        m_rxbuf.clear();
        StdMetrics.ms_v_vin->SetValue(vin);
        ESP_LOGI(TAG, "VIN: %s", vin.c_str());
      }
      break;
    }

    case 0xE002: { // VCU SOC, integer %. Only used when the BMS is silent.
      if (length >= 1) {
        float soc = data[0];
        if (soc > 0 && soc <= 100) {
          bool bms_fresh = m_bms_soc_seen &&
                           (now_s() - m_bms_soc_time) < SOC_FALLBACK_S;
          if (!bms_fresh && StdMetrics.ms_v_bat_soc->AsFloat() != soc) {
            StdMetrics.ms_v_bat_soc->SetValue(soc);
            ESP_LOGD(TAG, "SOC (VCU): %.0f %%", soc);
          }
        } else {
          ESP_LOGW(TAG,
                   "Invalid SOC %.1f ignored (car likely off or poll timeout)",
                   soc);
        }
      }
      break;
    }

    case 0xD117: { // BMS SOC, 0.1 %
      if (length >= 2) {
        float soc = u16be(data) * 0.1f;
        if (soc > 0 && soc <= 100) {
          m_bms_soc_time = now_s();
          m_bms_soc_seen = true;
          if (StdMetrics.ms_v_bat_soc->AsFloat() != soc) {
            StdMetrics.ms_v_bat_soc->SetValue(soc);
            ESP_LOGD(TAG, "SOC (BMS): %.1f %%", soc);
          }
        } else {
          ESP_LOGW(TAG, "Invalid BMS SOC %.1f ignored", soc);
        }
      }
      break;
    }

    case 0xE003: { // SOH (%)
      if (length >= 2) {
        uint16_t raw = u16be(data);
        float soh = raw / 100.0f;

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

    case 0xE015: { // VCU ignition state: 0 off, 3 on
      if (length >= 1) {
        int e015 = data[0];
        bool ready = (e015 != 0);
        bool prev_ready = m_ready;

        if (m_e015 >= 0 && e015 != m_e015)
          m_e018 = -1;
        m_e015 = e015;
        m_ready = ready;
        StdMetrics.ms_v_env_on->SetValue(ready);
        m_charge_seen = now_s();

        if (ready != prev_ready) {
          ESP_LOGI(TAG, "Ignition %s (E015=%d)", ready ? "on" : "off", e015);
          if (ready)
            m_was_charging = false;
        }
        UpdateChargeState();
        UpdatePollState();
      }
      break;
    }

    case 0xE018: { // VCU HV active: 2 charging or driving, 0 otherwise
      if (length >= 1) {
        m_e018 = data[0];
        m_charge_seen = now_s();
        UpdateChargeState();
        UpdatePollState();
      }
      break;
    }

    case 0xB105: { // Pack current: 0.1 A, offset 32768, negative = charging
      if (length >= 2) {
        uint16_t raw = u16be(data);
        if (raw == 0xFFFF) break;
        float current = ((int)raw - 32768) * 0.1f;
        StdMetrics.ms_v_bat_current->SetValue(current);

        float volts = StdMetrics.ms_v_bat_voltage->AsFloat(0);
        if (volts > 0) {
          float kw = (current * volts) / 1000.0f;
          StdMetrics.ms_v_bat_power->SetValue(kw);

          uint32_t now = now_s();
          if (m_charging && kw < 0 && m_pow_have &&
              (now - m_pow_time) <= 60) {
            float kwh = StdMetrics.ms_v_charge_kwh->AsFloat(0) +
                        (-kw) * (float)(now - m_pow_time) / 3600.0f;
            StdMetrics.ms_v_charge_kwh->SetValue(kwh);
          }
          m_pow_time = now;
          m_pow_have = true;
        }
      }
      break;
    }

    case 0xB11E: { // Pack voltage, 0.1 V
      if (length >= 2) {
        uint16_t raw = u16be(data);
        float volts = raw * 0.1f;
        if (raw != 0xFFFF && volts > 0 && volts < 1000.0f)
          StdMetrics.ms_v_bat_voltage->SetValue(volts);
      }
      break;
    }

    case 0xB110: { // Cell temperatures max/min/avg, 0.1 C
      if (Collect(job.pid, data, length, 6)) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(m_rxbuf.data());
        StdMetrics.ms_v_bat_pack_tmax->SetValue(u16be(b)     * 0.1f);
        StdMetrics.ms_v_bat_pack_tmin->SetValue(u16be(b + 2) * 0.1f);
        StdMetrics.ms_v_bat_pack_tavg->SetValue(u16be(b + 4) * 0.1f);
        m_rxbuf.clear();
      }
      break;
    }

    case 0xE010: { // HVAC/Coolant temperature (°C)
      if (length >= 2 && m_hvac_temp_c) {
        uint16_t raw = u16be(data);
        float t = raw / 10.0f;
        bool env_on = StdMetrics.ms_v_env_on->AsBool();

        if (!env_on && raw == 458) {
          ESP_LOGW(TAG,
                   "HVAC temp raw=0x%04x (%.1f °C) ignored (car off/default)",
                   raw, t);
          break;
        }

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

        if (!env_on && raw == 75) {
          ESP_LOGW(TAG,
                   "Ambient temp raw=0x%04x (%.1f °C) ignored (car off/default)",
                   raw, ta);
          break;
        }

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
//  Module Registration
// ─────────────────────────────────────────────

class OvmsVehicleMaxt90Init
{
public:
  OvmsVehicleMaxt90Init();
} MyOvmsVehicleMaxt90Init __attribute__((init_priority(9000)));

OvmsVehicleMaxt90Init::OvmsVehicleMaxt90Init()
{
  ESP_LOGI(TAG, "Registering Vehicle: Maxus T90 EV (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxt90>(
    "MT90", "Maxus T90 EV");
}
