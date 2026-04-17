/*
;    Project:       Open Vehicle Monitor System
;	 Subproject:    Integrate VW e-Golf
;
;    Changes:
;    February 7 2026: Initial Implementation
;
;    (C) 2026  Erick Fuentes <fuentes.erick@gmail.com>
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "vehicle_vwegolf.h"

#undef TAG
#define TAG "v-vwegolf"

// ---------------------------------------------------------------------------
// Module registration
// ---------------------------------------------------------------------------

OvmsVehicleVWeGolf::OvmsVehicleVWeGolf() {
    ESP_LOGI(TAG, "Start vehicle module: VW e-Golf");

    MyConfig.RegisterParam("xvg", "VW e-Golf", true, true);

    // KCAN (CAN3) carries comfort, body, and clima frames via the J533 gateway.
    // FCAN (CAN2) is the powertrain bus (BMS, motor controller, VIN).
    // CAN1 (OBD) is diagnostic-only and inaccessible while the car is asleep.
    //
    // FCAN is listen-only: we read gear and VIN but never transmit on this bus.
    // Active mode would require the ESP32 CAN controller to ACK every received
    // frame; its ACK timing on a bus already managed by native ECUs produces
    // spurious ECC TX-direction errors (ecc != 0 → CAN_logerror every ~200 ms)
    // even though rxerr/txerr stay at zero. Listen-only eliminates this entirely.
    RegisterCanBus(2, CAN_MODE_LISTEN, CAN_SPEED_500KBPS);  // FCAN — powertrain (read-only)
    RegisterCanBus(3, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);  // KCAN — comfort / clima

    // 0x66E broadcasts 0xFE as a "no data yet" sentinel before the interior sensor
    // is ready. That decodes to 77°C and would persist across reboots via the metric
    // persistence feature. Clear it so the metric stays undefined until real data arrives.
    StandardMetrics.ms_v_env_cabintemp->Clear();

    OvmsCommand* cmd_xvg = MyCommandApp.RegisterCommand("xvg", "VW e-Golf controls");

    // Diagnostic: manually stop OCU keepalive and leave the NM ring.
    cmd_xvg->RegisterCommand("offline", "Stop sending OCU keepalive (diagnostic)", [this](...) {
        m_ocu_active = false;
        ESP_LOGI(TAG, "OCU keepalive stopped");
    });

    cmd_xvg->RegisterCommand("fold_mirrors", "Fold mirrors in",
                             [this](...) { CommandMirrorFoldIn(); });
}

OvmsVehicleVWeGolf::~OvmsVehicleVWeGolf() {
    MyCommandApp.UnregisterCommand("xvg");
    ESP_LOGI(TAG, "Stop vehicle module: VW e-Golf");
}

class OvmsVehicleVWeGolfInit {
 public:
    OvmsVehicleVWeGolfInit();
} MyOvmsVehicleVWeGolfInit __attribute__((init_priority(9000)));

OvmsVehicleVWeGolfInit::OvmsVehicleVWeGolfInit() {
    ESP_LOGI(TAG, "Registering Vehicle: VW e-Golf (9000)");
    MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeGolf>("VWEG", "VW e-Golf");
}

// ---------------------------------------------------------------------------
// FCAN (CAN2) — powertrain bus: gear selector, VIN
// ---------------------------------------------------------------------------

void OvmsVehicleVWeGolf::IncomingFrameCan2(CAN_frame_t* p_frame) {
    // FCAN-native frames (gear, VIN) arrive here; everything else comes via can3.
    // Forward to IncomingFrameCan3 so shared decoders fire regardless of which bus
    // the frame arrived on.
    IncomingFrameCan3(p_frame);

    uint8_t* d = p_frame->data.u8;

    switch (p_frame->MsgID) {
        case 0x187: {
            // Gear selector position. Nibble at d[2][3:0]: 2=P, 3=R, 4=N, 5=D, 6=B.
            // OVMS convention: negative=reverse, 0=neutral/park, positive=forward.
            // drivemode 1 signals B-mode (increased regen).
            const uint8_t gear = d[2] & 0x0F;
            if (gear == 2) {
                StandardMetrics.ms_v_env_gear->SetValue(0);
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            } else if (gear == 3) {
                StandardMetrics.ms_v_env_gear->SetValue(-1);
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            } else if (gear == 4) {
                StandardMetrics.ms_v_env_gear->SetValue(0);
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            } else if (gear == 5) {
                StandardMetrics.ms_v_env_gear->SetValue(1);
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            } else if (gear == 6) {
                StandardMetrics.ms_v_env_gear->SetValue(1);
                StandardMetrics.ms_v_env_drivemode->SetValue(1);
            }
            ESP_LOGV(TAG, "0x187 gear nibble=%u", gear);
            break;
        }
        case 0x6B4: {
            // VIN is broadcast in three frames distinguished by data[0] (0, 1, 2).
            // We assemble all three before committing — a partial VIN is meaningless.
            // Once all three are received the VIN is immutable; further frames are ignored.
            uint8_t frame_idx = d[0];
            if (m_vin_parts_received == 0x07) break;

            if (frame_idx == 0) {
                m_vin_buf[0] = d[5];
                m_vin_buf[1] = d[6];
                m_vin_buf[2] = d[7];
                m_vin_parts_received |= 0x01;
            } else if (frame_idx == 1) {
                memcpy(&m_vin_buf[3], &d[1], 7);
                m_vin_parts_received |= 0x02;
            } else if (frame_idx == 2) {
                memcpy(&m_vin_buf[10], &d[1], 7);
                m_vin_parts_received |= 0x04;
            }

            if (m_vin_parts_received == 0x07) {
                m_vin_buf[17] = '\0';
                StandardMetrics.ms_v_vin->SetValue(m_vin_buf);
                ESP_LOGI(TAG, "VIN: %s", m_vin_buf);
            }
            break;
        }
    }
    // J533 bridges KCAN traffic onto CAN2; forward every frame so the KCAN
    // decoder in IncomingFrameCan3 can process it regardless of which bus it arrives on.
    IncomingFrameCan3(p_frame);
}

// ---------------------------------------------------------------------------
// KCAN (CAN3) — convenience bus: body, BMS, clima, GPS, charging
// ---------------------------------------------------------------------------

void OvmsVehicleVWeGolf::IncomingFrameCan3(CAN_frame_t* p_frame) {
    // Only genuine KCAN frames (origin==can3) reset the idle counter and trigger the
    // OCU keepalive. IncomingFrameCan2 forwards FCAN frames here for shared decode, but
    // those must not be treated as KCAN activity — during charging, FCAN is flooded with
    // OBC data while KCAN is completely silent, and counting FCAN frames as KCAN-alive
    // causes the heartbeat to hammer a silent bus until the TWAI controller hits bus-off.
    if (p_frame->origin == m_can3) {
        m_bus_idle_ticks = 0;

        // Send OCU keepalive at ~5Hz while active. VW OSEK NM requires keepalives at
        // ~200ms intervals; Ticker1 alone (1Hz) is too slow for the ECU to stay in network.
        // SendOcuHeartbeat self-throttles (180ms min) against TX queue overflow on bus bursts.
        if (m_ocu_active) {
            SendOcuHeartbeat();
        }
    }

    uint8_t* d = p_frame->data.u8;
    float f = 0.0f;
    uint16_t u16;
    uint32_t u32;

    switch (p_frame->MsgID) {
        case 0x00FD: {
            // Vehicle speed from ESP (electronic stability program) module.
            // 16-bit little-endian in d[4:5], factor 0.01 km/h.
            f = ((uint16_t)(d[4]) | ((uint16_t)(d[5]) << 8)) * 0.01f;
            StandardMetrics.ms_v_pos_speed->SetValue(f);
            ESP_LOGV(TAG, "0x00FD speed=%.2f km/h", f);
            break;
        }
        case 0x0131: {
            // State of charge. Single byte in d[3], factor 0.5 %.
            // 0xFE is the ECU's "not ready" sentinel (decodes to 127.0%) — discard it
            // to avoid persisting a nonsense value across reboots via metric persistence.
            if (d[3] == 0xFE) break;
            f = d[3] * 0.5f;
            StandardMetrics.ms_v_bat_soc->SetValue(f);
            ESP_LOGV(TAG, "0x0131 soc=%.1f%%", f);
            break;
        }
        case 0x0191: {
            // BMS current and voltage; power is derived.
            // d[2]==0xFF is the startup sentinel (all-ones current field → ~2047A, ~1023V).
            if (d[2] == 0xFF) break;
            // Current: 12-bit, factor 1 A, offset -2047 A (raw=2047 → 0 A).
            // NOTE: during AC charging this field reads 0A — 0x0191 appears to carry
            // inverter/motor current only. Charging current comes from a different frame
            // not yet identified. TODO: capture charging session and identify the frame.
            u16 = ((uint16_t)(d[1] & 0xF0) >> 4) | ((uint16_t)(d[2]) << 4);
            float current = u16 * 1.0f - 2047.0f;
            StandardMetrics.ms_v_bat_current->SetValue(current);

            // Voltage: 12-bit, factor 0.25 V.
            u16 = (uint16_t)(d[3]) | ((uint16_t)(d[4] & 0x0F) << 8);
            float voltage = u16 * 0.25f;
            StandardMetrics.ms_v_bat_voltage->SetValue(voltage);

            // Negated: OVMS convention is charge=negative, drive=positive.
            StandardMetrics.ms_v_bat_power->SetValue(-(voltage * current) / 1000.0f);
            ESP_LOGV(TAG, "0x0191 raw=%02x%02x%02x%02x%02x I=%.1fA V=%.2fV",
                     d[1], d[2], d[3], d[4], d[5], current, voltage);
            break;
        }
        case 0x02AF: {
            // Trip energy: regeneration recovered and total consumed.
            // Both are 15-bit, factor 10 Ws; convert to kWh.
            u16 = (uint16_t)(d[4]) | ((uint16_t)(d[5] & 0x7F) << 8);
            StandardMetrics.ms_v_bat_energy_recd->SetValue(u16 * 10.0f / 3600000.0f);

            u16 = (uint16_t)(d[6]) | ((uint16_t)(d[7] & 0x7F) << 8);
            StandardMetrics.ms_v_bat_energy_used->SetValue(u16 * 10.0f / 3600000.0f);
            break;
        }
        case 0x0486: {
            // GPS position. Latitude: bits 0-26 (27 bits), longitude: bits 27-54 (28 bits),
            // factor 1e-6 degrees. Sign bits inferred from bit layout (55 bits used out of
            // 64): bit 55 (d[6] MSB) = lat sign, bit 56 (d[7] bit 0) = lon sign.
            // Confirmed consistent with known N/E location (Norway). Needs a S/W hemisphere
            // capture to verify. Only write metrics when values are within valid range —
            // prevents sentinel frames (all 0xFF → 134°/268°) from overwriting good position.
            u32 = (uint32_t)(d[0]) | ((uint32_t)(d[1]) << 8) | ((uint32_t)(d[2]) << 16) |
                  ((uint32_t)(d[3] & 0x07) << 24);
            float lat = u32 * 0.000001f;
            if ((d[6] >> 7) & 1) lat = -lat;  // Southern hemisphere

            u32 = ((uint32_t)(d[3] & 0xF8) >> 3) | ((uint32_t)(d[4]) << 5) |
                  ((uint32_t)(d[5]) << 13) | ((uint32_t)(d[6] & 0x7F) << 21);
            float lon = u32 * 0.000001f;
            if ((d[7] >> 0) & 1) lon = -lon;  // Western hemisphere

            bool valid = (lat > -91.0f && lat < 91.0f && lon > -181.0f && lon < 181.0f);
            StandardMetrics.ms_v_pos_gpslock->SetValue(valid);
            if (valid) {
                StandardMetrics.ms_v_pos_latitude->SetValue(lat);
                StandardMetrics.ms_v_pos_longitude->SetValue(lon);
            }
            ESP_LOGV(TAG, "0x0486 lat=%.6f lon=%.6f valid=%d", lat, lon, valid);
            break;
        }
        case 0x0583: {
            // ZV_02: central locking and door open states.
            // d[2] bit 1: locked externally. d[3] bits 4:0: trunk, rr, rl, fr, fl (1=open).
            StandardMetrics.ms_v_env_locked->SetValue((d[2] & 0x02) >> 1);
            StandardMetrics.ms_v_door_fl->SetValue((d[3] & 0x01) >> 0);
            StandardMetrics.ms_v_door_fr->SetValue((d[3] & 0x02) >> 1);
            StandardMetrics.ms_v_door_rl->SetValue((d[3] & 0x04) >> 2);
            StandardMetrics.ms_v_door_rr->SetValue((d[3] & 0x08) >> 3);
            StandardMetrics.ms_v_door_trunk->SetValue((d[3] & 0x10) >> 4);
            ESP_LOGV(TAG, "0x0583 locked=%u fl=%u fr=%u rl=%u rr=%u trunk=%u", (d[2] & 0x02) >> 1,
                     d[3] & 0x01, (d[3] & 0x02) >> 1, (d[3] & 0x04) >> 2, (d[3] & 0x08) >> 3,
                     (d[3] & 0x10) >> 4);
            break;
        }
        case 0x0594: {
            // HV charge management: AC/DC type, timer, plug, cabin setpoint.

            // Time to full charge: 9-bit, factor 5 min.
            u16 = ((uint16_t)(d[1] & 0xF0) >> 4) | ((uint16_t)(d[2] & 0x1F) << 4);
            StandardMetrics.ms_v_charge_duration_full->SetValue(u16 * 5, Minutes);

            // Charge timer enabled when bits [6:5] of d[2] == 0x01.
            StandardMetrics.ms_v_charge_timermode->SetValue(((d[2] & 0x60) >> 5) == 0x01);

            // Charging in progress: bit 5 of d[3].
            {
                bool was_charging = StandardMetrics.ms_v_charge_inprogress->AsBool();
                bool is_charging = (d[3] & 0x20) != 0;
                StandardMetrics.ms_v_charge_inprogress->SetValue(is_charging);
                StandardMetrics.ms_v_charge_state->SetValue(is_charging ? "charging" : "stopped");
                // Mirror HV bus voltage; charge current is not yet decoded.
                if (is_charging) {
                    StandardMetrics.ms_v_charge_voltage->SetValue(
                        StandardMetrics.ms_v_bat_voltage->AsFloat());
                }
                if (is_charging != was_charging) {
                    if (is_charging) NotifyChargeStart();
                    else NotifyChargeStopped();
                }
            }

            // Charge type from bits [3:2] of d[5]: 0=none, 1=AC Type2, 3=cable connected
            // (charge not needed — e.g. 100% SOC). CCS DC keeps KCAN silent so case 2
            // is not observable. Do not update in the default case — leave last value.
            // Charge port open = cable physically present (ChargeType != 0).
            // The framework's status display gates on ms_v_door_chargeport — without it,
            // the "Not charging" fallback always shows regardless of charge_inprogress.
            {
                uint8_t charge_type = (d[5] & 0x0C) >> 2;
                switch (charge_type) {
                    case 0:
                        StandardMetrics.ms_v_door_chargeport->SetValue(false);
                        break;
                    case 1:
                        StandardMetrics.ms_v_charge_type->SetValue("type2");
                        StandardMetrics.ms_v_door_chargeport->SetValue(true);
                        break;
                    case 3:
                        // Cable connected, charge complete or not needed.
                        StandardMetrics.ms_v_door_chargeport->SetValue(true);
                        break;
                    default:
                        break;
                }
            }

            // Cabin temperature setpoint: 5-bit, factor 0.5°C, offset +15.5°C.
            f = (d[7] & 0x1F) * 0.5f + 15.5f;
            StandardMetrics.ms_v_env_cabinsetpoint->SetValue(f);

            ESP_LOGV(TAG, "0x0594 charging=%d timer=%d type=%s d[3]=%02x d[5]=%02x setpoint=%.1f°C",
                     StandardMetrics.ms_v_charge_inprogress->AsBool(),
                     StandardMetrics.ms_v_charge_timermode->AsBool(),
                     StandardMetrics.ms_v_charge_type->AsString().c_str(), d[3], d[5], f);
            break;
        }
        case 0x059E: {
            // BMS battery pack temperature. Factor 0.5°C, offset -40°C.
            // d[2]==0xFE is the startup sentinel (decodes to 87°C).
            if (d[2] == 0xFE) break;
            f = d[2] * 0.5f - 40.0f;
            StandardMetrics.ms_v_bat_temp->SetValue(f);
            ESP_LOGV(TAG, "0x059E bat_temp=%.1f°C", f);
            break;
        }
        case 0x05CA: {
            // HV battery energy content. 11-bit, factor 50 Wh → kWh.
            // d[2]==0xFF is the startup sentinel (near-max field → ~102 kWh; real battery ≤40 kWh).
            if (d[2] == 0xFF) break;
            u16 = ((uint16_t)(d[1] & 0xF0) >> 4) | ((uint16_t)(d[2] & 0x7F) << 4);
            f = u16 * 50.0f / 1000.0f;
            StandardMetrics.ms_v_bat_capacity->SetValue(f);
            ESP_LOGV(TAG, "0x05CA bat_capacity=%.1f kWh", f);
            break;
        }
        case 0x05EA: {
            // Clima ECU status: cabin temperature and remote_mode.
            // remote_mode: 0=idle, 2=running (steady-state), 3=just activated (transient).
            // Value 1 observed in on-battery clima capture (car sleeping, no plug); meaning TBD —
            // could be "requested/pending" before the ECU commits to running. Needs further RE.
            // HVAC on whenever remote_mode != 0.
            u16 = ((uint16_t)(d[6] & 0xFC) >> 2) | ((uint16_t)(d[7] & 0x0F) << 6);
            uint8_t remote_mode = ((d[3] & 0xC0) >> 6) | ((d[4] & 0x01) << 2);
            // d[3][5:3] and d[0][3:1] carry additional status nibbles seen in captures;
            // meaning not yet confirmed — decode when RE is complete.
            StandardMetrics.ms_v_env_hvac->SetValue(remote_mode != 0);
            // Raw value ≥ 1020 decodes to ≥ 62.0°C — the ECU broadcasts 0x3FE or 0x3FF
            // as "not available" when sleeping or uninitialized. Discard; valid cabin
            // temperature is well below this range.
            if (u16 < 1020) {
                f = u16 * 0.1f - 40.0f;
                StandardMetrics.ms_v_env_cabintemp->SetValue(f);
            }
            ESP_LOGV(TAG, "0x05EA clima_cabin=%.1f°C remote_mode=%u", f, remote_mode);
            break;
        }
        case 0x17332510: {
            // BAP status/ACK from clima ECU (node 0x25), extended 29-bit frame.
            // Pattern `49 58 XX` (DLC=3): ECU pushes its port 0x18 (start/stop trigger) state.
            // opcode=4 (0x49 bits[6:4]), node=0x25, port=0x18 (0x58 & 0x3F).
            // XX=0x01 = clima running; XX=0x00 = clima stopped.
            // This is the authoritative real-time source for ms_v_env_hvac:
            //   - Start: arrives ~4–5 s after command (ECU waits for blower to spin up)
            //   - Stop:  arrives ~0.3 s after command
            // 0x05EA remote_mode remains as a secondary/fallback path (e.g. after OVMS reboot
            // while clima is already running).
            if (p_frame->FIR.B.DLC == 3 && d[0] == 0x49 && d[1] == 0x58) {
                bool hvac_on = (d[2] == 0x01);
                StandardMetrics.ms_v_env_hvac->SetValue(hvac_on);
                ESP_LOGI(TAG, "0x17332510 BAP port 0x18: hvac=%s", hvac_on ? "on" : "off");
            }
            break;
        }
        case 0x05F5: {
            // Range estimates from the instrument cluster.
            // Estimated range (displayed on dash): 11-bit, factor 1 km.
            u16 = ((uint16_t)(d[3] & 0xE0) >> 5) | ((uint16_t)(d[4]) << 3);
            StandardMetrics.ms_v_bat_range_est->SetValue((float)u16);

            // Ideal range (BMS model lower bound, typically lower than estimated).
            u16 = (uint16_t)(d[0]) | ((uint16_t)(d[1] & 0x07) << 8);
            StandardMetrics.ms_v_bat_range_ideal->SetValue((float)u16);

            ESP_LOGV(TAG, "0x05F5 range_est=%u range_ideal=%u km",
                     StandardMetrics.ms_v_bat_range_est->AsInt(),
                     StandardMetrics.ms_v_bat_range_ideal->AsInt());
            break;
        }
        case 0x065A: {
            // BCM_01: hood open indicator (d[4] bit 0).
            StandardMetrics.ms_v_door_hood->SetValue(d[4] & 0x01);
            ESP_LOGV(TAG, "0x065A hood=%u", d[4] & 0x01);
            break;
        }
        case 0x066E: {
            // InnenTemp: cabin interior temperature sensor.
            // 0xFE is the ECU's "not ready" sentinel — it decodes to 77°C and must be
            // discarded to avoid storing a nonsense value in the metric.
            if (d[4] == 0xFE) break;
            f = d[4] * 0.5f - 50.0f;
            StandardMetrics.ms_v_env_cabintemp->SetValue(f);
            ESP_LOGV(TAG, "0x066E cabin_temp=%.1f°C", f);
            break;
        }
        case 0x06B0: {
            // FS temperature sensor (windshield/front area).
            // Logged for reference; not yet mapped to a standard metric.
            f = d[4] * 0.5f - 40.0f;
            ESP_LOGV(TAG, "0x06B0 fs_temp=%.1f°C", f);
            break;
        }
        case 0x06B5: {
            // Ambient temperature from two sensors: solar sensor and outside air.
            // d[6]==0xFE is the startup sentinel (both sensors report max-range garbage).
            if (d[6] == 0xFE) break;
            f = ((uint16_t)(d[6]) | ((uint16_t)(d[7] & 0x07) << 8)) * 0.1f - 40.0f;
            ESP_LOGV(TAG, "0x06B5 solar_sensor=%.1f°C", f);
            f = ((uint16_t)(d[2]) | ((uint16_t)(d[3] & 0x03) << 8)) * 0.1f - 40.0f;
            ESP_LOGV(TAG, "0x06B5 air_sensor=%.1f°C", f);
            break;
        }
        case 0x06B7: {
            // Odometer, parking time, and filtered outside temperature.

            // Odometer: 20-bit, factor 1 km.
            uint32_t odo = (uint32_t)(d[0]) | ((uint32_t)(d[1]) << 8) | ((uint32_t)(d[2] & 0x0F) << 16);
            StandardMetrics.ms_v_pos_odometer->SetValue(odo);

            // Park time: bit-field across d[2:4]. Exact encoding not fully confirmed.
            u32 = ((uint32_t)(d[2] & 0xF0) << 4) | ((uint32_t)(d[3]) << 4) |
                  ((uint32_t)(d[4] & 0x1F) << 12);
            StandardMetrics.ms_v_env_parktime->SetValue(u32);

            // Outside temperature (filtered): factor 0.5°C, offset -50°C.
            f = d[7] * 0.5f - 50.0f;
            StandardMetrics.ms_v_env_temp->SetValue(f);

            ESP_LOGV(TAG, "0x06B7 odo=%u km outside=%.1f°C", odo, f);
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Periodic tickers
// ---------------------------------------------------------------------------

void OvmsVehicleVWeGolf::Ticker1(uint32_t ticker) {
    OvmsVehicle::Ticker1(ticker);

    // Count consecutive seconds of KCAN silence. IncomingFrameCan3 resets this to 0
    // whenever a frame arrives, so it measures how long since the last activity.
    if (m_bus_idle_ticks < 254) m_bus_idle_ticks++;

    bool bus_alive = m_bus_idle_ticks < VWEGOLF_BUS_TIMEOUT_SECS;
    bool just_went_idle = (m_bus_idle_ticks == VWEGOLF_BUS_TIMEOUT_SECS);
    ESP_LOGV(TAG, "Ticker1: bus_idle=%u alive=%d ocu=%d", m_bus_idle_ticks, bus_alive,
             m_ocu_active);

    // On idle: clear OCU node presence so no further heartbeats queue on a dead bus.
    // Do NOT clear metrics — they are owned by their decoders and stale-expire naturally.
    // (Clearing charge_inprogress here would falsely show "not charging" during CCS DC,
    // which keeps KCAN silent while actively charging.)
    if (just_went_idle) {
        m_ocu_active = false;
        ESP_LOGI(TAG, "KCAN idle: OCU node presence cleared");
    }

    // Guard: only heartbeat when we've joined the bus and the bus has live traffic.
    if (m_ocu_active && bus_alive) {
        SendOcuHeartbeat();
    }
}

void OvmsVehicleVWeGolf::Ticker10(uint32_t ticker) {
    OvmsVehicle::Ticker10(ticker);

    // Re-read config periodically so changes take effect without a restart.
    m_climate_temp = (uint8_t)MyConfig.GetParamValueInt("xvg", "cc_temp", 21);
    m_climate_on_battery = MyConfig.GetParamValueBool("xvg", "cc_onbat", false);
    ESP_LOGV(TAG, "Ticker10: cc_temp=%u°C cc_onbat=%d", m_climate_temp, m_climate_on_battery);
}

// ---------------------------------------------------------------------------
// OCU keepalive
// ---------------------------------------------------------------------------

void OvmsVehicleVWeGolf::SendOcuHeartbeat() {
    // 0x5A7 is the OCU (On-board Control Unit) presence frame. The gateway and comfort
    // ECUs expect this every second from any active controller node on KCAN. OVMS must
    // keep sending it for as long as it wants to remain an active bus participant.
    //
    // One-shot actions (horn, lock, mirror fold etc.) are encoded into this frame on the
    // tick they are requested, then the flags are cleared so subsequent ticks send idle.
    uint8_t data[8] = {};  // idle state: all bytes zero

    if (m_mirror_fold_requested) {
        data[5] |= (1 << 7);  // d[5] bit 7: mirror fold toggle
        m_mirror_fold_requested = false;
        ESP_LOGI(TAG, "0x5A7 action: mirror fold");
    }
    if (m_horn_requested) {
        data[6] |= (1 << 0);  // d[6] bit 0: horn
        m_horn_requested = false;
        ESP_LOGI(TAG, "0x5A7 action: horn");
    }
    if (m_lock_requested) {
        // TODO: lock may require vehicle-specific authentication bytes alongside
        //       this bit — not yet confirmed from captures.
        data[6] |= (1 << 1);  // d[6] bit 1: lock
        m_lock_requested = false;
        ESP_LOGI(TAG, "0x5A7 action: lock");
    }
    if (m_unlock_requested) {
        // TODO: unlock likely has the same authentication requirement as lock.
        data[6] |= (1 << 2);  // d[6] bit 2: unlock
        m_unlock_requested = false;
        ESP_LOGI(TAG, "0x5A7 action: unlock");
    }
    if (m_indicators_requested) {
        data[6] |= (1 << 3);  // d[6] bit 3: hazard lights flash
        m_indicators_requested = false;
        ESP_LOGI(TAG, "0x5A7 action: indicators");
    }
    if (m_panic_requested) {
        data[6] |= (1 << 4);  // d[6] bit 4: panic alarm
        m_panic_requested = false;
        ESP_LOGI(TAG, "0x5A7 action: panic");
    }

    esp_err_t ok = m_can3->WriteStandard(0x5A7, 8, data);
    if (ok != ESP_OK) ESP_LOGW(TAG, "0x5A7 heartbeat TX dropped (err=%d)", ok);
    ESP_LOGV(TAG, "0x5A7: %02x %02x %02x %02x %02x %02x %02x %02x", data[0], data[1], data[2],
             data[3], data[4], data[5], data[6], data[7]);
}

// ---------------------------------------------------------------------------
// Vehicle commands
// ---------------------------------------------------------------------------

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandWakeup() {
    if (m_bus_idle_ticks < VWEGOLF_BUS_TIMEOUT_SECS) {
        // Bus already has live traffic — no wake-up needed.
        ESP_LOGI(TAG, "Wakeup: KCAN already active");
        return Success;
    }

    ESP_LOGI(TAG, "Wakeup: asserting dominant bits on KCAN");

    // Reset the KCAN controller to clear any stuck frame from the TWAI HW TX FIFO.
    // A stale heartbeat left from the prior session occupies the FIFO slot; on a sleeping
    // bus it can never drain (no ACK), so the wake frame queues behind it and never
    // produces dominant bits. Stop/Start preserves mode and speed.
    m_can3->Reset();
    vTaskDelay(pdMS_TO_TICKS(5));

    // Extended frames to assert dominant bits and trigger the NM wake cascade.
    // TX_Fail on Frame 1 is expected — nodes are still asleep and cannot ACK.
    uint8_t data[8] = {0x40, 0x00, 0x01, 0x1F, 0x00, 0x00, 0x00, 0x00};
    m_can3->WriteExtended(0x17330301, 8, data, pdMS_TO_TICKS(50));
    vTaskDelay(pdMS_TO_TICKS(50));

    // Frame 2: OSEK NM presence frame for node 0x67, observed during wake in captures.
    // TODO: determine whether this second frame is strictly necessary.
    data[0] = 0x67;
    data[1] = 0x10;
    data[2] = 0x41;
    data[3] = 0x84;
    data[4] = 0x14;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    m_can3->WriteExtended(0x1B000067, 8, data, pdMS_TO_TICKS(50));
    vTaskDelay(pdMS_TO_TICKS(100));

    m_ocu_active = true;
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandLock(const char* pin) {
    if (!PinCheck(pin)) {
        ESP_LOGW(TAG, "CommandLock: PIN check failed");
        return Fail;
    }
    m_lock_requested = true;
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandUnlock(const char* pin) {
    if (!PinCheck(pin)) {
        ESP_LOGW(TAG, "CommandUnlock: PIN check failed");
        return Fail;
    }
    m_unlock_requested = true;
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandMirrorFoldIn() {
    m_mirror_fold_requested = true;
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandHorn() {
    m_horn_requested = true;
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandIndicators() {
    m_indicators_requested = true;
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandPanic() {
    m_panic_requested = true;
    return Success;
}

void OvmsVehicleVWeGolf::WakeKcanBus() {
    ESP_LOGI(TAG, "WakeKcanBus: asserting dominant bits on KCAN");

    // Reset the KCAN controller to clear any stuck frame from the TWAI HW TX FIFO.
    // A stale heartbeat left from the prior session occupies the FIFO slot; on a sleeping
    // bus it can never drain (no ACK), so the wake frame queues behind it and never
    // produces dominant bits. Stop/Start preserves mode and speed.
    m_can3->Reset();
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t data[8] = {0x40, 0x00, 0x01, 0x1F, 0x00, 0x00, 0x00, 0x00};
    m_can3->WriteExtended(0x17330301, 8, data, pdMS_TO_TICKS(50));
    vTaskDelay(pdMS_TO_TICKS(50));

    data[0] = 0x67;
    data[1] = 0x10;
    data[2] = 0x41;
    data[3] = 0x84;
    data[4] = 0x14;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    m_can3->WriteExtended(0x1B000067, 8, data, pdMS_TO_TICKS(50));
    vTaskDelay(pdMS_TO_TICKS(100));

    m_ocu_active = true;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandWakeup() {
    if (m_bus_idle_ticks < VWEGOLF_BUS_TIMEOUT_SECS) {
        ESP_LOGI(TAG, "Wakeup: KCAN already active");
        return Success;
    }
    WakeKcanBus();
    return Success;
}

void OvmsVehicleVWeGolf::SendOcuHeartbeat() {
    // Self-throttle: minimum 180 ms between sends regardless of caller (Ticker1,
    // incoming-frame hook, or any future call site). Guards against TX queue overflow
    // during NM wake bursts and ensures Ticker1 can't double-fire on top of the
    // incoming-frame path. CommandClimateControl bumps this tick to suppress heartbeats
    // while a multi-frame BAP burst is in flight.
    uint32_t now = xTaskGetTickCount();
    if ((now - m_last_heartbeat_tick) * portTICK_PERIOD_MS < 180) {
        return;
    }
    m_last_heartbeat_tick = now;

    uint8_t tmp_u8 = 0;

    uint8_t data[8] = {0};

    // Mirror fold
    if (m_mirror_fold_in_requested) {
        tmp_u8 = 1;
        data[5] = (((uint8_t)tmp_u8) << 7) & 0x80;
        m_mirror_fold_in_requested = false;
        ESP_LOGI(TAG, "Mirror fold in");
    }

    // Horn
    if (m_horn_requested) {
        tmp_u8 = 1;
        data[6] = (((uint8_t)tmp_u8) >> 0) & 0x1;
        m_horn_requested = false;
        ESP_LOGI(TAG, "Horn");
    }

    // Door Lock //TODO there must be some vehicle specific identification send together with this
    // signal so not working OOB
    if (m_lock_requested >= 1) {
        tmp_u8 = 1;
        data[6] = (((uint8_t)tmp_u8) << 1) & 0x2;
        m_lock_requested = false;
        ESP_LOGI(TAG, "DoorLock");
    }

    // Door Unlock //TODO there must be some vehicle specific identification send together with this
    // signal so not working OOB
    if (m_unlock_requested) {
        tmp_u8 = 1;
        data[6] = (((uint8_t)tmp_u8) << 2) & 0x4;
        m_unlock_requested = false;
        ESP_LOGI(TAG, "DoorUnlock");
    }

    // Hazard lights
    if (m_indicators_requested) {
        tmp_u8 = 1;
        data[6] = (((uint8_t)tmp_u8) << 3) & 0x8;
        m_indicators_requested = false;
        ESP_LOGI(TAG, "Hazard lights");
    }

    // Panic alarm
    if (m_panic_mode_requested) {
        tmp_u8 = 1;
        data[6] = (((uint8_t)tmp_u8) << 4) & 0x10;
        m_panic_mode_requested = false;
        ESP_LOGI(TAG, "PanicAlarm!");
    }

    m_can3->WriteStandard(0x5A7, 8, data);
    ESP_LOGV(TAG, "Heartbeat 0x5A7: %02x %02x %02x %02x %02x %02x %02x %02x", data[0], data[1],
             data[2], data[3], data[4], data[5], data[6], data[7]);
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandClimateControl(bool enable) {
    ESP_LOGI(TAG, "Climate control: %s", enable ? "start" : "stop");

    // Abort if bus-off: the TWAI HW cannot TX at all and all Write calls silently
    // overflow the queue. CommandWakeup's Reset() should prevent this, but guard anyway.
    if (m_can3->GetErrorState() == CAN_errorstate_busoff) {
        ESP_LOGW(TAG, "Climate control: KCAN controller in bus-off — aborting");
        return Fail;
    }

    // Wake the bus if sleeping; wait 1 s for the NM-join flood (~300 ms burst) to settle
    // before sending BAP. If already active, skip wakeup and send immediately.
    if (m_bus_idle_ticks >= VWEGOLF_BUS_TIMEOUT_SECS) {
        ESP_LOGI(TAG, "Climate control: KCAN idle — waking bus and joining NM ring");
        CommandWakeup();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Rolling counter: included in the command and echoed back (| 0x80) in the ECU's ACK,
    // so we can match the ACK to this specific command invocation.
    m_bap_counter = (m_bap_counter == 0xFF) ? 0x01 : m_bap_counter + 1;

    // Suppress the OCU heartbeat for the duration of the 3-frame sequence. When the bus
    // wakes from the ping, IncomingFrameCan3 fires and the heartbeat rate-limit check can
    // pass (>180ms since last TX). A heartbeat queuing between Frame 1 and Frame 2 blocks
    // the continuation frame and the ECU discards the incomplete multi-frame message.
    m_last_heartbeat_tick = xTaskGetTickCount();

    uint8_t data[8];

    // Frame 1: BAP multi-frame start → port 0x19 (clima parameters), 8-byte payload.
    // 0x80 = multi-frame start, channel 0.  0x08 = total payload length.
    // 0x29 0x59 = BAP header: opcode=2 (Status push), node=0x25, port=0x19.
    data[0] = 0x80;
    data[1] = 0x08;           // 8-byte payload follows across this frame + continuation
    data[2] = 0x29;           // BAP opcode=2, node=0x25 [5:2]
    data[3] = 0x59;           // BAP node=0x25 [1:0], port=0x19
    data[4] = m_bap_counter;  // rolling counter; ACK will echo this | 0x80
    data[5] = 0x06;           // duration: 0x06 = 15 min (confirmed from capture: port 51 BCD
                              // countdown, 30s per unit, 0x06 * 150s = 15 min). Use 0x04 for 10 min.
    data[6] = 0x00;           // unknown
    data[7] = 0x01;           // unknown (mode/profile flag)
    // Accept both ESP_OK (frame in TWAI HW FIFO, physical TX imminent) and ESP_QUEUED
    // (frame placed in the OVMS FreeRTOS SW queue behind a concurrently-transmitting frame).
    // The SW queue is FIFO — if Frame 1 is queued, Frames 2 and 3 will follow it in order,
    // so the ECU always sees a complete multi-frame sequence. Only bail on ESP_FAIL, which
    // means the SW queue itself overflowed (bus stuck or configuration error).
    esp_err_t ok1 = m_can3->WriteExtended(0x17332501, 8, data, pdMS_TO_TICKS(200));
    if (ok1 == ESP_FAIL) {
        ESP_LOGW(TAG, "BAP clima %s frame 1 TX queue overflow (ok=%d) counter=0x%02X",
                 enable ? "start" : "stop", ok1, m_bap_counter);
        m_ocu_active = false;
        return Fail;
    }

    // Frame 2: BAP continuation — remaining payload bytes including target temperature.
    // Temperature encoding: raw = (celsius - 10) * 10  (e.g. 21°C → 0x6E)
    uint8_t climate_temp = (uint8_t)MyConfig.GetParamValueInt("xvg", "cc-temp", 21);
    uint8_t temp_raw = (uint8_t)((climate_temp - 10) * 10);
    data[0] = 0xC0;  // multi-frame continuation, channel 0
    data[1] = 0x06;  // unknown
    data[2] = 0x00;  // unknown
    data[3] = temp_raw;  // target temperature: (celsius - 10) * 10
    data[4] = 0x00;  // padding / unknown
    esp_err_t ok2 = m_can3->WriteExtended(0x17332501, 5, data, pdMS_TO_TICKS(200));
    if (ok2 == ESP_FAIL) {
        ESP_LOGW(TAG, "BAP clima %s frame 2 TX queue overflow (ok=%d) counter=0x%02X",
                 enable ? "start" : "stop", ok2, m_bap_counter);
        m_ocu_active = false;
        return Fail;
    }

    // Frame 3: BAP single-frame trigger → port 0x18 (immediate start/stop).
    // 0x29 0x58 = opcode=2, node=0x25, port=0x18. d[3] = 0x01 start / 0x00 stop.
    data[0] = 0x29;
    data[1] = 0x58;
    data[2] = 0x00;
    data[3] = enable ? 0x01 : 0x00;
    esp_err_t ok3 = m_can3->WriteExtended(0x17332501, 4, data, pdMS_TO_TICKS(200));
    if (ok3 == ESP_FAIL) {
        ESP_LOGW(TAG, "BAP clima %s frame 3 TX queue overflow (ok=%d) counter=0x%02X",
                 enable ? "start" : "stop", ok3, m_bap_counter);
        m_ocu_active = false;
        return Fail;
    }

    ESP_LOGI(TAG, "BAP clima %s sent: counter=0x%02X temp=%u°C", enable ? "start" : "stop",
             m_bap_counter, climate_temp);

    // Optimistic update: give the app immediate feedback. 0x05EA will overwrite this
    // with ground truth once the ECU responds.
    StandardMetrics.ms_v_env_hvac->SetValue(enable);
    m_ocu_active = enable;  // stay in NM ring while clima runs; leave after stop
    return Success;
}
