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

    // KCAN is the convenience bus carrying comfort, body, and clima frames.
    // FCAN is the powertrain bus with BMS, motor controller, and VIN.
    // The OBD port (CAN1) carries diagnostic-only traffic that is inaccessible
    // while the car is asleep, so we skip it here.
    RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);  // FCAN — powertrain
    RegisterCanBus(3, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);  // KCAN — comfort / clima

    // 0x66E broadcasts 0xFE as a "no data yet" sentinel before the interior sensor
    // is ready. That decodes to 77°C and would persist across reboots via the metric
    // persistence feature. Clear it so the metric stays undefined until real data arrives.
    StandardMetrics.ms_v_env_cabintemp->Clear();

    OvmsCommand* cmd_xvg = MyCommandApp.RegisterCommand("xvg", "VW e-Golf controls");

    // Diagnostic: stop sending OCU keepalive. Useful when the OVMS is done with a
    // remote session and should stop asserting node presence on KCAN.
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
    // J533 bridges FCAN frames onto KCAN, so the bulk of traffic (SOC, speed,
    // charging, clima, etc.) arrives on can3 via IncomingFrameCan3. The frames
    // that reach IncomingFrameCan2 are FCAN-native (gear, VIN) plus occasional
    // bridged frames during bus startup. Forward everything so KCAN decoders
    // also fire for any frame that arrives here.
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
    // Any frame on KCAN resets the idle counter. Ticker1 uses this to decide
    // whether the bus is alive and whether OCU keepalive should be sent.
    m_bus_idle_ticks = 0;

    // Send OCU keepalive at ~5Hz while active. VW OSEK NM requires keepalives at
    // ~200ms intervals; Ticker1 alone (1Hz) is too slow for the ECU to stay in network.
    if (m_ocu_active && ++m_ocu_heartbeat_counter >= 75) {
        m_ocu_heartbeat_counter = 0;
        SendOcuHeartbeat();
    }

    uint8_t* d = p_frame->data.u8;
    float f;
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

            // Power is negated so charge shows negative and drive shows positive,
            // matching OVMS convention.
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
                // Mirror HV bus voltage to charge-side metric; they are the same physical
                // bus during AC charging. Charge current is not yet decoded — see TODO below.
                if (is_charging) {
                    StandardMetrics.ms_v_charge_voltage->SetValue(
                        StandardMetrics.ms_v_bat_voltage->AsFloat());
                }
                if (is_charging != was_charging) {
                    if (is_charging) NotifyChargeStart();
                    else NotifyChargeStopped();
                }
            }

            // Charge type from bits [3:2] of d[5]: 1=AC (Type 2).
            // NOTE: during CCS DC charging these bits read 0 — the CCS indicator is
            // elsewhere in the frame. Log d[3] and d[5] raw to identify it.
            // Do not set the metric in the default case — leave last known value intact.
            switch ((d[5] & 0x0C) >> 2) {
                case 1:
                    StandardMetrics.ms_v_charge_type->SetValue("type2");
                    break;
                default:
                    break;
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
            f = d[2] * 0.5f - 40.0f;
            StandardMetrics.ms_v_bat_temp->SetValue(f);
            ESP_LOGV(TAG, "0x059E bat_temp=%.1f°C", f);
            break;
        }
        case 0x05CA: {
            // HV battery energy content. 11-bit, factor 50 Wh → kWh.
            u16 = ((uint16_t)(d[1] & 0xF0) >> 4) | ((uint16_t)(d[2] & 0x7F) << 4);
            f = u16 * 50.0f / 1000.0f;
            StandardMetrics.ms_v_bat_capacity->SetValue(f);
            ESP_LOGV(TAG, "0x05CA bat_capacity=%.1f kWh", f);
            break;
        }
        case 0x05EA: {
            // Clima ECU status: cabin temperature, remote mode, and operational status.
            // remote_mode values observed from captures:
            //   0 = idle (no remote session)
            //   2 = clima running via remote (steady-state during the run)
            //   3 = clima just activated via remote command (transient, first few seconds only)
            // Earlier captures only saw 0 and 3; the 15-min full-cycle capture confirmed that
            // remote_mode stays at 2 for the entire run after the initial activation.
            // HVAC is on whenever remote_mode != 0.
            u16 = ((uint16_t)(d[6] & 0xFC) >> 2) | ((uint16_t)(d[7] & 0x0F) << 6);
            f = u16 * 0.1f - 40.0f;
            uint8_t remote_mode = ((d[3] & 0xC0) >> 6) | ((d[4] & 0x01) << 2);
            uint8_t status_02 = (d[3] & 0x38) >> 3;
            uint8_t status_03 = (d[0] & 0x0E) >> 1;
            StandardMetrics.ms_v_env_hvac->SetValue(remote_mode != 0);
            StandardMetrics.ms_v_env_cabintemp->SetValue(f);
            ESP_LOGV(TAG, "0x05EA clima_cabin=%.1f°C remote_mode=%u status02=%u status03=%u", f,
                     remote_mode, status_02, status_03);
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
            // BCM_01: bonnet/hood open indicator (MHWIVSchalter, d[4] bit 0).
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
            f = ((uint16_t)(d[6]) | ((uint16_t)(d[7] & 0x07) << 8)) * 0.1f - 40.0f;
            ESP_LOGV(TAG, "0x06B5 solar_sensor=%.1f°C", f);
            f = ((uint16_t)(d[2]) | ((uint16_t)(d[3] & 0x03) << 8)) * 0.1f - 40.0f;
            ESP_LOGV(TAG, "0x06B5 air_sensor=%.1f°C", f);
            break;
        }
        case 0x06B7: {
            // Odometer, parking time, and filtered outside temperature.

            // Odometer: 20-bit, factor 1 km.
            u32 = (uint32_t)(d[0]) | ((uint32_t)(d[1]) << 8) | ((uint32_t)(d[2] & 0x0F) << 16);
            StandardMetrics.ms_v_pos_odometer->SetValue(u32);

            // Park time: bit-field across d[2:4]. Exact encoding not fully confirmed.
            u32 = ((uint32_t)(d[2] & 0xF0) << 4) | ((uint32_t)(d[3]) << 4) |
                  ((uint32_t)(d[4] & 0x1F) << 12);
            StandardMetrics.ms_v_env_parktime->SetValue(u32);

            // Outside temperature (filtered): factor 0.5°C, offset -50°C.
            f = d[7] * 0.5f - 50.0f;
            StandardMetrics.ms_v_env_temp->SetValue(f);

            ESP_LOGV(TAG, "0x06B7 odo=%u km outside=%.1f°C", u32, f);
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
    // Count consecutive seconds of KCAN silence. IncomingFrameCan3 resets this to 0
    // whenever a frame arrives, so it measures how long since the last activity.
    if (m_bus_idle_ticks < 254) m_bus_idle_ticks++;

    bool bus_alive = m_bus_idle_ticks < VWEGOLF_BUS_TIMEOUT_SECS;
    bool just_went_idle = (m_bus_idle_ticks == VWEGOLF_BUS_TIMEOUT_SECS);
    ESP_LOGV(TAG, "Ticker1: bus_idle=%u alive=%d ocu=%d", m_bus_idle_ticks, bus_alive,
             m_ocu_active);

    // When the bus goes idle the clima ECU has stopped broadcasting 0x05EA.
    // Clear hvac so the metric doesn't stay stuck on after a remote session ends.
    if (just_went_idle) {
        StandardMetrics.ms_v_env_hvac->SetValue(false);
    }

    // Only send the OCU keepalive when we have deliberately joined the bus AND the bus
    // has active traffic. When the bus is sleeping there is nobody to ACK frames —
    // the ESP32 CAN controller accumulates TX errors that can corrupt the error counter.
    if (m_ocu_active && bus_alive) {
        SendOcuHeartbeat();
    }
}

void OvmsVehicleVWeGolf::Ticker10(uint32_t ticker) {
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

    // These extended frames put dominant bits on the bus to wake the KCAN transceivers.
    // TX_Fail on the first frame is expected — nodes are still asleep and cannot ACK.
    // Frame 1 triggers responses from 0x5F5 and nearby ECUs in captured traces.
    uint8_t data[8] = {0x40, 0x00, 0x01, 0x1F, 0x00, 0x00, 0x00, 0x00};
    m_can3->WriteExtended(0x17330301, 8, data);
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
    m_can3->WriteExtended(0x1B000067, 8, data);
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

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandClimateControl(bool enable) {
    ESP_LOGI(TAG, "Climate control: %s", enable ? "start" : "stop");

    if (m_bus_idle_ticks >= VWEGOLF_BUS_TIMEOUT_SECS) {
        // The bus is sleeping. Use the NM wake sequence (same as CommandWakeup) to bring
        // the bus and clima ECU up before sending the BAP command.
        //
        // Timing evidence from kcan-can3-clima_on_off.crtd (confirmed working sequence):
        //   +0ms   NM Frame 1 (17330301) sent — bus flooded within 10ms
        //   +153ms NM Frame 2 (1B000067) sent
        //   +453ms Clima ECU (17332510) first broadcasts status — ECU is BAP-ready
        //
        // A 200ms wait was previously used after a BAP Get ping. That is too short:
        // the clima ECU needs ~450ms from the first dominant bit before it is ready.
        // Using the full NM sequence with a 500ms settle matches the confirmed timing.
        //
        // TX_Fail on Frame 1 is expected (nodes still asleep) — the physical dominant
        // bits are sufficient to start the NM wake cascade.
        CommandWakeup();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    m_ocu_active = true;

    // Rolling counter: included in the command and echoed back (| 0x80) in the ECU's ACK,
    // so we can match the ACK to this specific command invocation.
    m_bap_counter = (m_bap_counter == 0xFF) ? 0x01 : m_bap_counter + 1;

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
    // Use a 20ms queue wait on all three frames. The TWAI TX queue can be briefly
    // full from OCU heartbeat frames fired during the 500ms settle — with wait=0
    // (default) the frames drop silently with no TX or TX_Fail event logged.
    esp_err_t ok1 = m_can3->WriteExtended(0x17332501, 8, data, pdMS_TO_TICKS(20));

    // Frame 2: BAP multi-frame continuation — remaining 4 bytes of the port 0x19 payload.
    // Full 8-byte payload: [counter] 06 00 01  06 00 20 00
    // Byte index 6 (0x20) is suspected target temperature. Encoding not yet confirmed:
    // port 0x16 schedule uses (celsius + 35); port 0x19 may differ. Use thomasakarlsen
    // default (0x20) until captured with two distinct target temperatures.
    // TODO: replace 0x20 with encoded m_climate_temp once encoding is confirmed.
    data[0] = 0xC0;  // multi-frame continuation, channel 0
    data[1] = 0x06;  // unknown
    data[2] = 0x00;  // unknown
    data[3] = 0x20;  // suspected target temperature (TODO: confirm and use m_climate_temp)
    data[4] = 0x00;  // padding / unknown
    esp_err_t ok2 = m_can3->WriteExtended(0x17332501, 5, data, pdMS_TO_TICKS(20));

    // Frame 3: BAP single-frame trigger → port 0x18 (immediate start/stop).
    // 0x29 0x58 = opcode=2, node=0x25, port=0x18. Payload byte 1 is the on/off flag.
    data[0] = 0x29;
    data[1] = 0x58;
    data[2] = 0x00;
    data[3] = enable ? 0x01 : 0x00;
    esp_err_t ok3 = m_can3->WriteExtended(0x17332501, 4, data, pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "BAP clima %s: frame TX ok1=%d ok2=%d ok3=%d counter=0x%02X",
             enable ? "start" : "stop", ok1, ok2, ok3, m_bap_counter);

    // Do NOT set ms_v_env_hvac here. If TX fails silently (CAN error after wake ping,
    // bus-off recovery, etc.) and we pre-set the metric, the app's next toggle press
    // will send the opposite command. The metric is updated from the ECU's 0x17332510
    // ACK once that decode is implemented, and from 0x05EA once confirmed.

    ESP_LOGI(TAG, "Climate %s sequence sent (counter=0x%02x)", enable ? "start" : "stop",
             m_bap_counter);
    return Success;
}
