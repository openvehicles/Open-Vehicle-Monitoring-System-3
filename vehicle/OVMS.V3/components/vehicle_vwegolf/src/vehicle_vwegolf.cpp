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
#include "mcp2515.h"

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

    // During charging the OBC floods FCAN at ~860 frames/s which overflows the MCP2515
    // RX buffers causing a continuous ISR storm that starves the Events task → TWDT.
    // Hardware acceptance filter passes only the 5 FCAN IDs we decode; everything else
    // is dropped in hardware before reaching the RX buffers.
    //
    // .u32 encoding: SetAcceptanceFilter sends bytes u8[3..0] in order as MCP2515
    // registers SIDH, SIDL, EID8, EID0.  .b.sid stores the SID in bits [10:0] (LSB),
    // which lands in EID8/EID0 — wrong.  Use u32 = (SID>>3)<<24 | (SID&7)<<21 instead.
    {
        auto mcp_sid = [](uint16_t sid) -> uint32_t {
            return (static_cast<uint32_t>(sid >> 3) << 24) |
                   (static_cast<uint32_t>(sid & 0x7) << 21);
        };
        // MCP2515 has 2 RX buffers: RXB0 uses mask[0] + filter[0..1] (2 slots);
        // RXB1 uses mask[1] + filter[2..5] (4 slots). We need 5 unique IDs total;
        // filter[5] duplicates filter[4] (0x6B4) so the unused slot does not
        // accidentally accept id 0 with the all-don't-care default.
        mcp2515_filter_config_t f = {};
        f.mask[0].u32   = mcp_sid(0x7FF);
        f.mask[1].u32   = mcp_sid(0x7FF);
        f.filter[0].u32 = mcp_sid(0x131);   // SoC (BMS)              -> RXB0
        f.filter[1].u32 = mcp_sid(0x187);   // gear selector          -> RXB0
        f.filter[2].u32 = mcp_sid(0x191);   // BMS current/voltage    -> RXB1
        f.filter[3].u32 = mcp_sid(0x2AF);   // trip energy            -> RXB1
        f.filter[4].u32 = mcp_sid(0x6B4);   // VIN                    -> RXB1
        f.filter[5].u32 = mcp_sid(0x6B4);   // (duplicate of filter[4] to fill unused slot)
        if (static_cast<mcp2515*>(m_can2)->SetAcceptanceFilter(f) != ESP_OK)
            ESP_LOGE(TAG, "FCAN acceptance filter setup failed — all frames will reach ISR");
    }

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

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
#endif
}

OvmsVehicleVWeGolf::~OvmsVehicleVWeGolf() {
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebDeInit();
#endif
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
// FCAN (CAN2) — powertrain bus: BMS, gear selector, VIN
// ---------------------------------------------------------------------------

void OvmsVehicleVWeGolf::IncomingFrameCan2(CAN_frame_t* p_frame) {
    uint8_t* d = p_frame->data.u8;
    float f;
    uint16_t u16;

    switch (p_frame->MsgID) {
        case 0x0131: {
            // State of charge. Single byte in d[3], factor 0.5 %.
            // 0xFE is the ECU's "not ready" sentinel (decodes to 127.0%) — discard it.
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
}

// ---------------------------------------------------------------------------
// KCAN (CAN3) — convenience bus: body, BMS, clima, GPS, charging
// ---------------------------------------------------------------------------

void OvmsVehicleVWeGolf::IncomingFrameCan3(CAN_frame_t* p_frame) {
    m_bus_idle_ticks = 0;

    // Send OCU keepalive at ~5Hz while active. VW OSEK NM requires keepalives at
    // ~200ms intervals; Ticker1 alone (1Hz) is too slow for the ECU to stay in network.
    // SendOcuHeartbeat self-throttles (180ms min) against TX queue overflow on bus bursts.
    if (m_ocu_active) {
        SendOcuHeartbeat();
    }

    uint8_t* d = p_frame->data.u8;

    // Track OEM OCU activity: any non-zero 0x5A7 means the car's OCU is still active.
    // Reset idle counter so we don't try to wake while it's conflicting with our heartbeat.
    if (p_frame->MsgID == 0x5A7) {
        if (d[0] | d[1] | d[2] | d[3] | d[4] | d[5] | d[6] | d[7]) {
            m_oem_ocu_idle_ticks = 0;
        }
    }
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
            // remote_mode: 2=running (steady-state), 3=just activated (transient).
            // Value 1 observed while car sleeping on 0x05EA — treat as off until RE confirms,
            // else the metric pins true from idle broadcasts.
            u16 = ((uint16_t)(d[6] & 0xFC) >> 2) | ((uint16_t)(d[7] & 0x0F) << 6);
            uint8_t remote_mode = ((d[3] & 0xC0) >> 6) | ((d[4] & 0x01) << 2);
            StandardMetrics.ms_v_env_hvac->SetValue(remote_mode == 2 || remote_mode == 3);
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
                // Arm grace: transaction is done, keep the ring warm briefly then stand down.
                m_ocu_grace_secs = 0;
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
    if (m_oem_ocu_idle_ticks < 254) m_oem_ocu_idle_ticks++;

    bool bus_alive = m_bus_idle_ticks < VWEGOLF_BUS_TIMEOUT_SECS;
    bool just_went_idle = (m_bus_idle_ticks == VWEGOLF_BUS_TIMEOUT_SECS);
    ESP_LOGV(TAG, "Ticker1: bus_idle=%u alive=%d ocu=%d", m_bus_idle_ticks, bus_alive,
             m_ocu_active);

    // Clear OCU node presence on either condition:
    //   1. Bus went idle (no frames for VWEGOLF_BUS_TIMEOUT_SECS) — no one to hear us.
    //   2. OEM OCU is present — it owns 0x5A7, our TX would collide on arbitration.
    // Without (2), m_ocu_active stays set across an entire drive cycle whenever the
    // bus never idles, and the next RX frame triggers a heartbeat that collides with
    // the OEM OCU every time. Do NOT clear metrics — decoders own them; stale-expire
    // handles freshness. (Clearing charge_inprogress here would falsely show "not
    // charging" during CCS DC, which keeps KCAN silent while actively charging.)
    bool oem_ocu_present = m_oem_ocu_idle_ticks < VWEGOLF_BUS_TIMEOUT_SECS;

    if (m_ocu_active) {
        if (m_ocu_session_secs < 255) m_ocu_session_secs++;
        if (m_ocu_grace_secs < 255) m_ocu_grace_secs++;
    }
    bool grace_expired = m_ocu_grace_secs < 255 && m_ocu_grace_secs >= VWEGOLF_OCU_ACK_GRACE_SECS;
    bool cap_expired = m_ocu_session_secs >= VWEGOLF_OCU_SESSION_CAP_SECS;

    if (m_ocu_active && (just_went_idle || oem_ocu_present || grace_expired || cap_expired)) {
        m_ocu_active = false;
        const char* reason = just_went_idle    ? "KCAN idle"
                             : oem_ocu_present ? "OEM OCU active"
                             : grace_expired   ? "ACK grace expired"
                                               : "session cap";
        ESP_LOGI(TAG, "OCU presence cleared: %s", reason);
    }

    // Guard: only heartbeat when we've joined the bus and the bus has live traffic.
    if (m_ocu_active && bus_alive) {
        SendNmAlive();
        SendOcuHeartbeat();
    }

    // Fire deferred clima BAP burst once the wake-settle window has elapsed.
    // CommandClimateControl returns immediately after WakeKcanBus so the dispatch task
    // isn't blocked for 1 s; we pick up here on the next Ticker1 once the NM-join flood
    // has subsided. Bus must still be alive — otherwise wake didn't take and the burst
    // would just queue against a dead controller.
    if (m_clima_pending) {
        uint32_t now = xTaskGetTickCount();
        uint32_t elapsed_ms = (now - m_clima_pending_tick) * portTICK_PERIOD_MS;
        if (!bus_alive) {
            ESP_LOGW(TAG, "Deferred clima: bus went idle before settle — aborting");
            m_clima_pending = false;
            StandardMetrics.ms_v_env_hvac->SetValue(!m_clima_pending_enable);
        } else if (elapsed_ms >= VWEGOLF_CLIMA_SETTLE_MS) {
            bool enable = m_clima_pending_enable;
            m_clima_pending = false;
            if (SendClimaBapBurst(enable) != Success) {
                StandardMetrics.ms_v_env_hvac->SetValue(!enable);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Vehicle commands
// ---------------------------------------------------------------------------

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
    m_mirror_fold_in_requested = true;
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
    m_ocu_session_secs = 0;
    m_ocu_grace_secs = 255;
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
    // Hard gate: a BAP multi-frame burst is in flight on KCAN. A 0x5A7 queued between
    // the start/continuation/trigger frames blocks the continuation and the ECU drops
    // the message. Worst-case burst is 3×200 ms = 600 ms, well past the 180 ms throttle.
    if (m_bap_burst_active) {
        return;
    }

    // Hard gate: OEM OCU owns 0x5A7. If it's alive, our TX collides on arbitration
    // every frame (identical ID) — TEC climbs on no-ACK until bus-off. Stand down
    // until the OEM OCU has been silent for >= VWEGOLF_BUS_TIMEOUT_SECS.
    if (m_oem_ocu_idle_ticks < VWEGOLF_BUS_TIMEOUT_SECS) {
        return;
    }

    // Self-throttle: minimum 180 ms between sends regardless of caller (Ticker1,
    // incoming-frame hook, or any future call site). Guards against TX queue overflow
    // during NM wake bursts and ensures Ticker1 can't double-fire on top of the
    // incoming-frame path.
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
    if (m_panic_requested) {
        tmp_u8 = 1;
        data[6] = (((uint8_t)tmp_u8) << 4) & 0x10;
        m_panic_requested = false;
        ESP_LOGI(TAG, "PanicAlarm!");
    }

    m_can3->WriteStandard(0x5A7, 8, data);
    ESP_LOGV(TAG, "Heartbeat 0x5A7: %02x %02x %02x %02x %02x %02x %02x %02x", data[0], data[1],
             data[2], data[3], data[4], data[5], data[6], data[7]);
}

void OvmsVehicleVWeGolf::SendNmAlive() {
    // Ring drops silent nodes after a few cadences; a one-shot alive on wake survives
    // long enough for warm-bus commands but not a cold BAP burst. OEM 0x67 cadence
    // ~1.3 s (kcan-can3-clima_schedule.crtd) — Ticker1's 1 Hz tick matches.
    // Burst gate: a 0x1B frame between BAP frames blocks the continuation.
    if (m_bap_burst_active) {
        return;
    }
    uint8_t data[8] = {0x67, 0x10, 0x41, 0x84, 0x14, 0x00, 0x00, 0x00};
    m_can3->WriteExtended(0x1B000067, 8, data);
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::SendClimaBapBurst(bool enable) {
    // Rolling counter: echoed back (| 0x80) in the ECU's ACK to match the command.
    m_bap_counter = (m_bap_counter == 0xFF) ? 0x01 : m_bap_counter + 1;

    // Suppress heartbeats for the full 3-frame burst. Set before frame 1, cleared on
    // every exit path below — a 0x5A7 between frames blocks the continuation.
    m_bap_burst_active = true;

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
        ESP_LOGW(TAG, "BAP clima frame 1 TX queue overflow");
        m_bap_burst_active = false;
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
        ESP_LOGW(TAG, "BAP clima frame 2 TX queue overflow");
        m_bap_burst_active = false;
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
        ESP_LOGW(TAG, "BAP clima frame 3 TX queue overflow");
        m_bap_burst_active = false;
        m_ocu_active = false;
        return Fail;
    }

    m_bap_burst_active = false;

    ESP_LOGI(TAG, "BAP clima %s sent: counter=0x%02X temp=%u°C", enable ? "start" : "stop",
             m_bap_counter, climate_temp);

    // Optimistic update: give the app immediate feedback. 0x05EA will overwrite this
    // with ground truth once the ECU responds.
    StandardMetrics.ms_v_env_hvac->SetValue(enable);

    // Stay in the NM ring after both start and stop. On stop the ECU broadcasts a
    // 0x05→0x00 status transition on BAP port 0x12 a few hundred ms later; dropping
    // out immediately would miss the ACK. Natural bus-idle timeout in Ticker1 clears
    // m_ocu_active once KCAN goes quiet.
    m_ocu_active = true;
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandClimateControl(bool enable) {
    ESP_LOGI(TAG, "Climate control: %s", enable ? "start" : "stop");

    if (m_can3->GetErrorState() == CAN_errorstate_busoff) {
        ESP_LOGW(TAG, "Climate control: KCAN controller in bus-off — aborting");
        return Fail;
    }

    // Wake the bus if it has been quiet long enough that ECUs are likely sleeping.
    // Two conditions must both be true:
    //   1. No KCAN frame for >= CLIMA_WAKE_SECS (bus is going/gone to sleep).
    //   2. No non-zero OEM 0x5A7 for >= CLIMA_WAKE_SECS (OEM OCU is off — safe to
    //      send our heartbeat without causing an arbitration-loss → bus-off cycle).
    // Defer the BAP burst to Ticker1 so the dispatch task isn't blocked for 1 s while
    // the NM-join flood subsides. Ticker1 fires once VWEGOLF_CLIMA_SETTLE_MS elapses.
    if (m_bus_idle_ticks >= VWEGOLF_CLIMA_WAKE_SECS &&
        m_oem_ocu_idle_ticks >= VWEGOLF_CLIMA_WAKE_SECS) {
        ESP_LOGI(TAG, "Climate control: KCAN quiet %u s, OEM OCU quiet %u s — waking bus",
                 m_bus_idle_ticks, m_oem_ocu_idle_ticks);
        WakeKcanBus();
        m_clima_pending = true;
        m_clima_pending_enable = enable;
        m_clima_pending_tick = xTaskGetTickCount();
        return Success;
    }

    return SendClimaBapBurst(enable);
}
