#include "ovms_log.h"
static const char *TAG = "v-maxt90";

#include <stdio.h>
#include <string>
#include "esp_timer.h"
#include "vehicle_maxt90.h"
#include "vehicle_obdii.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"

// Seconds since boot. Deliberately not the GNSS-derived wall clock, which
// can be spoofed (it was pushed 15 days forward during Jammertest).
static inline uint32_t now_s()
{
  return (uint32_t)(esp_timer_get_time() / 1000000LL);
}

// A BMS SOC younger than this keeps the coarser VCU SOC (E002) from writing.
static const uint32_t SOC_FALLBACK_S = 180;

// No E015/E018 reply for this long while "charging": the car has gone to
// sleep or the charge is over.
static const uint32_t CHARGE_STALE_S = 120;

// Range on 0x6F0, in km. 
static const unsigned RANGE_MAX_KM = 600;

// Nominal pack capacity in Ah, for v.b.cac. 
static const float CAC_NOMINAL_AH = 245.83f;

// Cabin fan steps on 0x510. 
static const int HVAC_FAN_STEPS = 9;

OvmsVehicleMaxt90::OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Initialising Maxus T90 EV vehicle module");

  // Register CAN1 bus at 500 kbps
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // Custom metrics:
  // Prefix "xmt" = Maxus T90 
  m_hvac_temp_c =
    MyMetrics.InitFloat("xmt.v.hvac.temp", 10, 0.0f, Celcius, false);
  m_pack_capacity_kwh =
    MyMetrics.InitFloat("xmt.b.capacity", 0, 88.5f, kWh, true);

  // Poll list. The three numbers are the interval in seconds for poll state
  // 0, 1 and 2; 0 means "not polled in that state".
  //
  //  State 0: car off. Only what is needed to notice the car waking up,
  //           starting to charge, or the SOC changing.
  //  State 1: ignition on (E015 non-zero).
  //  State 2: car off but charging (E018 active, E015 zero). The T90 is NOT
  //           ignition-on while charging, so before this state existed the
  //           poller sat in state 0 for the whole session and SOC froze.
  //
  // Request rate: about 0.28/s in state 0, 0.7/s in state 1, 0.6/s in state 2.
  static const OvmsPoller::poll_pid_t maxt90_polls[] = {
    // --- VCU 7e3/7eb ---

    // VIN: 17 chars, arrives in three chunks (3 + 7 + 7 bytes)
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF190,
      { 0, 3600, 3600 }, 0, ISOTP_STD },

    // SOC, integer. Fallback only, see D117.
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE002,
      { 60, 10, 60 }, 0, ISOTP_STD },

    // SOH
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE003,
      { 0, 1800, 1800 }, 0, ISOTP_STD },

    // Ignition state: 0 off, 3 on (E015 tracked the ignition in every sweep).
    // Polled every 5 s while the car is off so a wake-up is noticed quickly.
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE015,
      { 5, 10, 10 }, 0, ISOTP_STD },

    // HV active: 2 while charging or driving, 0 when idle or plugged in full
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE018,
      { 30, 10, 10 }, 0, ISOTP_STD },

    // HVAC temp. Ignition on only: with the car off and charging it returns
    // the 0x0200 default and just logged a warning every 30 s.
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE010,
      { 0, 30, 0 }, 0, ISOTP_STD },

    // Ambient temp. Same reason.
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE025,
      { 0, 30, 0 }, 0, ISOTP_STD },

    // --- BMS 748/7c8 ---

    // SOC in 0.1 %. Answers with the car off and charging.
    { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xD117,
      { 60, 30, 15 }, 0, ISOTP_STD },

    // Pack current, 0.1 A with a 32768 offset, negative while charging
    { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB105,
      { 0, 5, 10 }, 0, ISOTP_STD },

    // Pack voltage, 0.1 V (the sum of the 110 cell voltages). Slowly in state
    // 0 too, so the resting voltage is not the last value seen under load
    // (391.8 V charging against 383.9 V at rest).
    { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB11E,
      { 120, 10, 10 }, 0, ISOTP_STD },

    // Cell temperatures max/min/avg, 0.1 C. Six data bytes, so two chunks.
    { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xB110,
      { 0, 30, 30 }, 0, ISOTP_STD },

    POLL_LIST_END
  };

  // Attach the poll list to CAN1 & start in "off" state
  PollSetPidList(m_can1, maxt90_polls);
  PollSetState(0);

  // The charge metrics are persistent, so a value written by an earlier
  // firmware survives the update. The old module left "charging" set and
  // nothing in this one ever cleared it. Reset it; the poll replies set it
  // again within 30 s if the car really is charging.
  StdMetrics.ms_v_charge_inprogress->SetValue(false);
  StdMetrics.ms_v_charge_state->SetValue("");

  ESP_LOGI(TAG, "Maxus T90 EV poller configured on CAN1 @ 500 kbps "
                "(charge-state build)");
}

OvmsVehicleMaxt90::~OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Shutdown Maxus T90 EV vehicle module");
}

//  Multi-frame reply reassembly
//
// The poller delivers one chunk per CAN frame: 3 data bytes from the first
// frame, then 7 from each consecutive frame. A 17 byte VIN therefore arrives
// as 3 + 7 + 7. The earlier module wrote each chunk straight into the VIN
// metric, which is why it only ever held the last seven characters.
//
// Works whether the reply comes in pieces or whole. Returns true once `want`
// bytes are held in m_rxbuf; the caller uses them and clears the buffer.
bool OvmsVehicleMaxt90::Collect(uint16_t pid, const uint8_t* data,
                                uint8_t length, size_t want)
{
  uint32_t now = now_s();
  static int logged = 0;

  // A new PID, or a long gap, means a new reply.
  if (pid != m_rxpid || (now - m_rxtime) > 5) {
    m_rxbuf.clear();
    m_rxpid = pid;
  }
  m_rxtime = now;
  m_rxbuf.append(reinterpret_cast<const char*>(data), length);

  if (logged < 8) {
    logged++;
    ESP_LOGI(TAG, "multi-frame 0x%04x: chunk of %u bytes, %u of %u collected",
             pid, (unsigned)length, (unsigned)m_rxbuf.size(), (unsigned)want);
  }
  return m_rxbuf.size() >= want;
}

//  Charge state and poll state

void OvmsVehicleMaxt90::UpdateChargeState()
{
  // Charging: the car is off, HV is active (E018), and the ignition is off
  // (E015). 
  bool charging = !m_ready && m_e018 > 0 && m_e015 == 0;

  if (charging != m_charging) {
    ESP_LOGI(TAG, "Charging %s (E015=%d E018=%d)",
             charging ? "started" : "stopped", m_e015, m_e018);
  }
  if (charging && !m_charging) {
    // v.c.time and v.c.kwh are persistent and describe the running session.
    // Left alone they resumed from the previous firmware's values
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
    if (want == 0) {
      // The contactors are open. B105 is not polled in this state, so without
      // this the last driving or charging current is reported until the next
      // drive
      StdMetrics.ms_v_bat_current->SetValue(0);
      StdMetrics.ms_v_bat_power->SetValue(0);
    }
    PollSetState(want);
  }
}

void OvmsVehicleMaxt90::Ticker10(uint32_t ticker)
{
  OvmsVehicleOBDII::Ticker10(ticker);

  // A sleeping car does not answer. If we think it is charging but have
  // heard nothing for CHARGE_STALE_S, the charge is over.
  if (m_charging && (now_s() - m_charge_seen) > CHARGE_STALE_S) {
    ESP_LOGI(TAG, "No charge-state reply for %u s: treating the charge as ended",
             (unsigned)(now_s() - m_charge_seen));
    m_e018 = 0;
    UpdateChargeState();
    UpdatePollState();
  }
}

//  Live CAN Frame Handler
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
        StdMetrics.ms_v_env_locked->SetValue(locked);

        ESP_LOGI(TAG, "Lock state changed: %s (CAN 0x281 byte1=0x%02x)",
                 locked ? "LOCKED" : "UNLOCKED", state);

        last_state = state;
      }
      break;
    }

    case 0x375: // Doors & headlights (from the reverse-engineering notes)
    {
      // Broadcast continuously, and this runs in the CAN receive task for
      // every frame. Only act when the bytes we use actually changed.
      static bool seen = false;
      static uint8_t last0 = 0, last1 = 0;
      if (seen && d[0] == last0 && d[1] == last1)
        break;
      seen = true; last0 = d[0]; last1 = d[1];

      // Byte 0: 0x20 front left (driver), 0x40 front right, 0x80 rear right
      // Byte 1: 0x01 rear left
      StdMetrics.ms_v_door_fl->SetValue((d[0] & 0x20) != 0);
      StdMetrics.ms_v_door_fr->SetValue((d[0] & 0x40) != 0);
      StdMetrics.ms_v_door_rr->SetValue((d[0] & 0x80) != 0);
      StdMetrics.ms_v_door_rl->SetValue((d[1] & 0x01) != 0);

      // Low nibble of byte 0: 01 park, 03 low beam, 07 high beam, 13 fog+low
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

      // Byte 0 low nibble: fan step 0..9 (high nibble is the air route)
      StdMetrics.ms_v_env_cabinfan->SetValue(
        ((d[0] & 0x0F) * 100) / HVAC_FAN_STEPS);

      // Byte 1: set point + 40; 0x39 = LOW, 0x49 = HIGH. Ignore anything
      // outside that range rather than publish -40.
      if (d[1] >= 0x30 && d[1] <= 0x50)
        StdMetrics.ms_v_env_cabinsetpoint->SetValue((float)d[1] - 40.0f);

      // Byte 5: 0x80 A/C. 
      // Byte 5: 0x20 on/off
      StdMetrics.ms_v_env_hvac->SetValue((d[5] & 0x20) != 0);
      break;
    }

    case 0x6F0: // Range 
    {
      // Bytes 0-1, little-endian, whole km: 6A 01 -> 0x016A = 362. With the
      // car off the frame reads 00 00 .. 80 ..
      static bool seen = false;
      static uint8_t last0 = 0, last1 = 0;
      if (seen && d[0] == last0 && d[1] == last1)
        break;
      seen = true; last0 = d[0]; last1 = d[1];

      unsigned km = (unsigned)d[0] | ((unsigned)d[1] << 8);
      // Zero means the cluster is asleep, not that the range is zero. Keep
      // the last real value rather than publishing 0 km.
      if (km > 0 && km <= RANGE_MAX_KM) {
        StdMetrics.ms_v_bat_range_est->SetValue((float)km);
        // The car gives one number. The old firmware set both metrics to it,
        // and the ABRP plugin falls back to the ideal range.
        StdMetrics.ms_v_bat_range_ideal->SetValue((float)km);
      }
      break;
    }

    case 0x540: // Odometer
    {
      // 24-bit little-endian in bytes [4..6], resolution 0.1 km
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

//  OBDII Poll Reply Handler
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

        // Filter out bogus default values (0xFFFF, 0x1800 = 61.44%, etc.)
        if (raw == 0xFFFF || raw == 0x1800 || soh <= 50.0f || soh > 150.0f) {
          ESP_LOGW(TAG, "Invalid SOH raw=0x%04x (%.2f %%) ignored", raw, soh);
          break;
        }

        if (StdMetrics.ms_v_bat_soh->AsFloat() != soh) {
          StdMetrics.ms_v_bat_soh->SetValue(soh);
          ESP_LOGD(TAG, "SOH: %.2f %%", soh);
        }
        // Computed, not read: it follows SoH. It sat at 243.621 Ah while SoH
        // moved on, because nothing wrote it after the old firmware.
        StdMetrics.ms_v_bat_cac->SetValue(soh * CAC_NOMINAL_AH / 100.0f);
      }
      break;
    }

    case 0xE015: { // VCU ignition state: 0 off, 3 on
      if (length >= 1) {
        int e015 = data[0];
        bool ready = (e015 != 0);
        bool prev_ready = m_ready;

        // E015 and E018 are read at different times. When the ignition state
        // changes, the E018 we hold describes the previous state: switching
        // off would otherwise pair a fresh E015=0 with a stale E018=2 and
        // report a charge that never happened. Drop it and wait for a fresh
        // reading. (Not on the first read, where nothing is stale.)
        if (m_e015 >= 0 && e015 != m_e015)
          m_e018 = -1;
        m_e015 = e015;
        m_ready = ready;
        StdMetrics.ms_v_env_on->SetValue(ready);
        m_charge_seen = now_s();

        if (ready != prev_ready) {
          ESP_LOGI(TAG, "Ignition %s (E015=%d)", ready ? "on" : "off", e015);
          if (ready)
            m_was_charging = false;   // driving clears "charge done"
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

        // OVMS convention: positive = discharge, so charging is negative
        float volts = StdMetrics.ms_v_bat_voltage->AsFloat(0);
        if (volts > 0) {
          float kw = (current * volts) / 1000.0f;
          StdMetrics.ms_v_bat_power->SetValue(kw);

          // Energy into the pack this session. Samples more than a minute
          // apart are not bridged, so a gap does not invent energy.
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

    case 0xB11E: { // Pack voltage, 0.1 V. A second value follows; ignored.
      if (length >= 2) {
        uint16_t raw = u16be(data);
        float volts = raw * 0.1f;
        if (raw != 0xFFFF && volts > 0 && volts < 1000.0f)
          StdMetrics.ms_v_bat_voltage->SetValue(volts);
      }
      break;
    }

    case 0xB110: { // Cell temperatures max/min/avg, 0.1 C. Two chunks.
      if (Collect(job.pid, data, length, 6)) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(m_rxbuf.data());
        StdMetrics.ms_v_bat_pack_tmax->SetValue(u16be(b)     * 0.1f);
        StdMetrics.ms_v_bat_pack_tmin->SetValue(u16be(b + 2) * 0.1f);
        StdMetrics.ms_v_bat_pack_tavg->SetValue(u16be(b + 4) * 0.1f);
        // v.b.temp is the standard battery temperature, and what the ABRP
        // plugin sends as batt_temp. Nothing else sets it on this vehicle.
        StdMetrics.ms_v_bat_temp->SetValue(u16be(b + 4) * 0.1f);
        m_rxbuf.clear();
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

//   Module Registration

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
