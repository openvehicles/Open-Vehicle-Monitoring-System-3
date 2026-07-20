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

// #include <stdio.h>
#include "vehicle_vwegolf.h"
#include "mcp2515.h"

#undef TAG
#define TAG "v-vwegolf"

OvmsVehicleVWeGolf::OvmsVehicleVWeGolf() {
    ESP_LOGI(TAG, "Start vehicle module: VW e-Golf");

    // init configs:
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

    // Hardware acceptance filter on FCAN (CAN2): during OBC charging the bus runs at ~860 fps.
    // Without filtering all frames reach the ISR, starving the Events task → TWDT resets.
    // Pass only the 5 frame IDs we decode; everything else is dropped in hardware.
    //
    // MCP2515 filter table layout:
    //   - RXB0 uses mask[0] applied to filter[0..1]   (2 filter slots)
    //   - RXB1 uses mask[1] applied to filter[2..5]   (4 filter slots)
    //   The chip has no "disable slot" flag, so unused RXB1 slots must be set to a
    //   value that matches one of the wanted IDs (slot 5 mirrors slot 4 below).
    //
    // Filter ID encoding: SetAcceptanceFilter writes the .u32 field as register bytes
    // SIDH/SIDL/EID8/EID0. The standard 11-bit ID maps as SID[10:3] → SIDH (bits 24..31),
    // SID[2:0] → SIDL[7:5] (bits 21..23). Hence the mcp_sid() helper below.
    if (m_can2 != nullptr) {
        auto mcp_sid = [](uint16_t sid) -> uint32_t {
            return (static_cast<uint32_t>(sid >> 3) << 24) |
                   (static_cast<uint32_t>(sid & 0x7) << 21);
        };
        mcp2515_filter_config_t f = {};
        // RXB0: full 11-bit match on the two slots
        f.mask[0].u32   = mcp_sid(0x7FF);
        f.filter[0].u32 = mcp_sid(0x131);   // BMS SoC
        f.filter[1].u32 = mcp_sid(0x187);   // gear selector
        // RXB1: full 11-bit match on the four slots
        f.mask[1].u32   = mcp_sid(0x7FF);
        f.filter[2].u32 = mcp_sid(0x191);   // BMS current/voltage
        f.filter[3].u32 = mcp_sid(0x2AF);   // trip energy
        f.filter[4].u32 = mcp_sid(0x6B4);   // VIN
        f.filter[5].u32 = mcp_sid(0x6B4);   // duplicate — RXB1 has no unused-slot flag
        if (static_cast<mcp2515*>(m_can2)->SetAcceptanceFilter(f) != ESP_OK)
            ESP_LOGE(TAG, "FCAN acceptance filter setup failed — all frames will reach ISR");
    } else {
        ESP_LOGE(TAG, "CAN2 not registered — cannot install FCAN acceptance filter");
    }

    OvmsCommand* cmd_vweg = MyCommandApp.RegisterCommand("xvg", "VW e-Golf controls");
    cmd_vweg->RegisterCommand("offline", "Stop sending OCU keepalive", [this](...) {
        m_is_control_active = false;
        ESP_LOGI(TAG, "OCU keepalive stopped");
    });
    cmd_vweg->RegisterCommand("fold_mirrors", "Fold mirrors in",
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

void OvmsVehicleVWeGolf::IncomingFrameCan2(CAN_frame_t* p_frame) {
    const uint8_t* d = p_frame->data.u8;

    switch (p_frame->MsgID) {
        case 0x131: {
            // State of charge. d[3] * 0.5%. 0xFE is the "not ready" sentinel (127%) — discard it.
            if (d[3] == 0xFE) break;
            float soc = d[3] * 0.5f;
            StandardMetrics.ms_v_bat_soc->SetValue(soc);
            ESP_LOGV(TAG, "0x0131 soc=%.1f%%", soc);
            break;
        }
        case 0x191: {
            // BMS current and voltage; power is derived.
            // d[2]==0xFF is the startup sentinel (all-ones → ~2047A, ~1023V) — discard it.
            if (d[2] == 0xFF) break;
            // Current: 12-bit, factor 1 A, offset -2047 A.
            uint16_t raw_i = ((uint16_t)(d[1] & 0xF0) >> 4) | ((uint16_t)(d[2]) << 4);
            float current = raw_i * 1.0f - 2047.0f;
            StandardMetrics.ms_v_bat_current->SetValue(current);
            // Voltage: 12-bit, factor 0.25 V.
            uint16_t raw_v = (uint16_t)(d[3]) | ((uint16_t)(d[4] & 0x0F) << 8);
            float voltage = raw_v * 0.25f;
            StandardMetrics.ms_v_bat_voltage->SetValue(voltage);
            // Power: negative = charging, positive = driving.
            StandardMetrics.ms_v_bat_power->SetValue(-(voltage * current) / 1000.0f);
            ESP_LOGV(TAG, "0x0191 I=%.1fA V=%.2fV", current, voltage);
            break;
        }
        case 0x2AF: {
            // Trip energy counters. 15-bit, factor 10 Ws → kWh.
            uint16_t raw_recd = (uint16_t)(d[4]) | ((uint16_t)(d[5] & 0x7F) << 8);
            StandardMetrics.ms_v_bat_energy_recd->SetValue(raw_recd * 10.0f / 3600000.0f);
            uint16_t raw_used = (uint16_t)(d[6]) | ((uint16_t)(d[7] & 0x7F) << 8);
            StandardMetrics.ms_v_bat_energy_used->SetValue(raw_used * 10.0f / 3600000.0f);
            break;
        }
        case 0x187: {
            const uint8_t gear_nibble = p_frame->data.u8[2] & 0x0F;
            ESP_LOGV(TAG, "0x187 gear nibble=%d", gear_nibble);
            if (gear_nibble == 2) {
                // Park
                StandardMetrics.ms_v_env_gear->SetValue(0);
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            } else if (gear_nibble == 3) {
                // Reverse
                StandardMetrics.ms_v_env_gear->SetValue(-1);
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            } else if (gear_nibble == 4) {
                // Neutral
                StandardMetrics.ms_v_env_gear->SetValue(0);
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            } else if (gear_nibble == 5) {
                // Drive
                StandardMetrics.ms_v_env_gear->SetValue(1);
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            } else if (gear_nibble == 6) {
                // B mode
                StandardMetrics.ms_v_env_gear->SetValue(1);
                StandardMetrics.ms_v_env_drivemode->SetValue(1);
            }
            break;
        }
        case 0x6B4: {
            // This message contains the VIN in 3 parts, with the first byte identifying the frame.
            // We only set the VIN after all three parts have been received. Once the VIN has been
            // set, we ignore future VIN frames.
            uint8_t frame_idx = p_frame->data.u8[0];
            ESP_LOGV(TAG, "0x6B4 frame_idx=%d parts=0x%02x", frame_idx, m_vin_parts_received);
            if (m_vin_parts_received == 0x07) {
                // We've already received three VIN frames and set the VIN in the metrics.
                break;
            } else if (frame_idx == 0) {
                m_vin_buf[0] = p_frame->data.u8[5];
                m_vin_buf[1] = p_frame->data.u8[6];
                m_vin_buf[2] = p_frame->data.u8[7];
                m_vin_parts_received |= 0x01;
            } else if (frame_idx == 1) {
                memcpy(&m_vin_buf[3], &p_frame->data.u8[1], 7);
                m_vin_parts_received |= 0x02;
            } else if (frame_idx == 2) {
                memcpy(&m_vin_buf[10], &p_frame->data.u8[1], 7);
                m_vin_parts_received |= 0x04;
            }

            if (m_vin_parts_received == 0x07) {
                // Set the VIN now that we've received all three parts.
                m_vin_buf[17] = '\0';
                StandardMetrics.ms_v_vin->SetValue(m_vin_buf);
            }
            break;
        }
    }
}

void OvmsVehicleVWeGolf::IncomingFrameCan3(CAN_frame_t* p_frame) {
    m_last_message_received = 0;
    uint8_t* d = p_frame->data.u8;

    uint8_t tmp_u8 = 0;
    uint16_t tmp_u16 = 0;
    uint32_t tmp_u32 = 0;
    // int8_t tmp_i8 = 0;
    // int16_t tmp_i16 = 0;
    // int32_t tmp_i32 = 0;
    float tmp_f32 = 0.0F;

    // //TODO Debug only doesn't work ECU timeout
    // vTaskDelay(pdMS_TO_TICKS(500)); //500ms wait.. hopefully my log isn't get messed up
    // //TODO end doesn't work ECU timeout

    switch (p_frame->MsgID) {
        // TODO: Need to move to verify
        case 0xFD:  // Vehicle speed from ESP module. 16-bit LE in d[4:5], factor 0.01 km/h.
        {
            tmp_u16 = ((uint16_t)(d[4]) >> 0) | ((uint16_t)(d[5]) << 8);
            tmp_f32 = ((float)tmp_u16) * 0.01F;
            StandardMetrics.ms_v_pos_speed->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x00FD speed=%.2f km/h", tmp_f32);
            break;
        }
        case 0x131:  // State of charge. d[3] * 0.5%. 0xFE = "not ready" sentinel (127%).
        {
            // BMS-native frame on FCAN (CAN2) but also bridged by J533 onto KCAN.
            // Decoded here as well so SoC continues to update on bus paths where
            // the FCAN-side decode is unavailable (observed on CCS DC sessions).
            if (d[3] == 0xFE) break;
            tmp_f32 = ((float)d[3]) * 0.5F;
            StandardMetrics.ms_v_bat_soc->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x0131 soc=%.1f%%", tmp_f32);
            break;
        }
        // case 0x3D6: //Ladezustand
        //   {
        //     ESP_LOGD(TAG, "chargeState: %x", ((d[6] & 0x30)>>4));
        //     if(((d[6] & 0x30)>>4) == 0x0)
        //     {
        //       StdMetrics.ms_v_charge_state->SetValue("stopped");
        //       ESP_LOGV(TAG, "charging stopped");
        //     }
        //     if(((d[6] & 0x30)>>4) == 0x1)
        //     {
        //       StdMetrics.ms_v_charge_state->SetValue("charging");
        //       ESP_LOGV(TAG, "charging");
        //     }
        //     if(((d[6] & 0x30)>>4) == 0x2)
        //     {
        //       StdMetrics.ms_v_charge_state->SetValue("init");
        //       ESP_LOGV(TAG, "charging init");
        //     }
        //     if(((d[6] & 0x30)>>4) == 0x3)
        //     {
        //       StdMetrics.ms_v_charge_state->SetValue("error");
        //       ESP_LOGV(TAG, "charging error");
        //     }

        //   break;
        //   }
        // Working, but sign bit missing
        case 0x486:  // GPS position. Lat: bits 0-26 (factor 1e-6°), Lon: bits 27-54.
        {
            // Sign bits: bit 55 (d[6] MSB) = Southern hemisphere, bit 56 (d[7] bit 0) = Western.
            // Confirmed consistent with known N/E location. S/W hemisphere still needs a capture.
            // Sentinel frames (all 0xFF) decode to lat=134°/lon=268° — filter by range.
            tmp_u32 = ((uint32_t)(d[0])) | ((uint32_t)(d[1]) << 8) |
                      ((uint32_t)(d[2]) << 16) | ((uint32_t)(d[3] & 0x7) << 24);
            float lat = ((float)tmp_u32) * 0.000001F;
            if ((d[6] >> 7) & 1) lat = -lat;  // Southern hemisphere

            tmp_u32 = ((uint32_t)(d[3] & 0xf8) >> 3) | ((uint32_t)(d[4]) << 5) |
                      ((uint32_t)(d[5]) << 13) | ((uint32_t)(d[6] & 0x7f) << 21);
            float lon = ((float)tmp_u32) * 0.000001F;
            if ((d[7] >> 0) & 1) lon = -lon;  // Western hemisphere

            bool valid = (lat > -91.0F && lat < 91.0F && lon > -181.0F && lon < 181.0F);
            StandardMetrics.ms_v_pos_gpslock->SetValue(valid);
            if (valid) {
                StandardMetrics.ms_v_pos_latitude->SetValue(lat);
                StandardMetrics.ms_v_pos_longitude->SetValue(lon);
            }
            ESP_LOGV(TAG, "0x0486 lat=%.6f lon=%.6f valid=%d", lat, lon, valid);
            break;
        }
        case 0x583:  // ZV_02: central locking and door open states.
        {
            // d[2] bit 1: locked externally. d[3] bits 4:0: trunk, rr, rl, fr, fl (1=open).
            StdMetrics.ms_v_env_locked->SetValue((d[2] & 0x2) >> 1);
            StdMetrics.ms_v_door_fl->SetValue((d[3] & 0x1) >> 0);
            StdMetrics.ms_v_door_fr->SetValue((d[3] & 0x2) >> 1);
            StdMetrics.ms_v_door_rl->SetValue((d[3] & 0x4) >> 2);
            StdMetrics.ms_v_door_rr->SetValue((d[3] & 0x8) >> 3);
            StdMetrics.ms_v_door_trunk->SetValue((d[3] & 0x10) >> 4);
            ESP_LOGV(TAG, "0x0583 locked=%u fl=%u fr=%u rl=%u rr=%u trunk=%u",
                     (d[2] & 0x2) >> 1, d[3] & 0x1, (d[3] & 0x2) >> 1,
                     (d[3] & 0x4) >> 2, (d[3] & 0x8) >> 3, (d[3] & 0x10) >> 4);
            break;
        }
        case 0x594:  // HV charge management
        {
            // 0x594 => AC/DC charging, is climate timer active or not, plug connected (secured or
            // not), programmed cabin temp, charging active, time until HV battery full in 5min
            // steps
            tmp_u16 = ((uint16_t)(d[1] & 0xf0) >> 4) | ((uint16_t)(d[2] & 0x1f) << 4) |
                      0;  // Faktor 5 Offset 0, Minimum 0, Maximum 2545 [5min] Initial 2550
            tmp_u16 = (uint16_t)tmp_u16;
            tmp_u16 = (((int16_t)tmp_u16) * 5);
            StdMetrics.ms_v_charge_duration_full->SetValue(
                tmp_u16,
                Minutes);  // working          // Estimated time remaing for full charge [min]

            tmp_u8 = ((uint8_t)(d[2] & 0x60) >> 5) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            if (tmp_u8 == 0x1) {
                StdMetrics.ms_v_charge_timermode->SetValue(true);  // True if timer enabled
            } else {
                StdMetrics.ms_v_charge_timermode->SetValue(false);  // false if timer disabled
            }

            {
                bool was_charging = StdMetrics.ms_v_charge_inprogress->AsBool();
                bool is_charging = (d[3] & 0x20) != 0;  // bit 5 of d[3]
                StdMetrics.ms_v_charge_inprogress->SetValue(is_charging);
                StdMetrics.ms_v_charge_state->SetValue(is_charging ? "charging" : "stopped");
                if (is_charging) {
                    StdMetrics.ms_v_charge_voltage->SetValue(
                        StandardMetrics.ms_v_bat_voltage->AsFloat());
                }
                if (is_charging != was_charging) {
                    if (is_charging) NotifyChargeStart();
                    else NotifyChargeStopped();
                }
            }

            tmp_u16 = ((uint16_t)(d[3] & 0xc0) >> 6) | ((uint16_t)(d[4] & 0x7f) << 2) |
                      0;  // Faktor 50 Offset 0, Minimum 0, Maximum 25450 [W] Initial 25500
            tmp_u16 = (uint16_t)tmp_u16;
            tmp_u16 = (((int16_t)tmp_u16) * 50);  // maximum charging power

            tmp_u8 = ((uint8_t)(d[4] & 0x80) >> 7) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            if (tmp_u8 == 0x1) {
                // parking climate control timer set
            } else {
                // parking climate control timer not set
            }

            tmp_u8 = ((uint8_t)(d[5] & 0x1) << 0) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            if (tmp_u8 == 0x1) {
                // error charging plug
            } else {
                // no error charging plug
            }

            tmp_u8 = ((uint8_t)(d[5] & 0x2) >> 1) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            if (tmp_u8 == 0x1) {
                // error charging plug lock
            } else {
                // no error charging plug lock
            }

            tmp_u8 = ((uint8_t)(d[5] & 0xc) >> 2) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            // Charge port open = cable physically present (ChargeType != 0).
            // The framework's status display gates on ms_v_door_chargeport — without it,
            // the "Not charging" fallback always shows regardless of charge_inprogress.
            switch (tmp_u8) {
                case 0x0: {
                    // No connector — do not overwrite last known type with "undefined".
                    // NOTE: CCS DC charging also reads 0 here; the CCS indicator is
                    // elsewhere in the frame and not yet identified.
                    StdMetrics.ms_v_door_chargeport->SetValue(false);
                    break;
                }
                case 0x1: {
                    StdMetrics.ms_v_charge_type->SetValue("type2");
                    StdMetrics.ms_v_door_chargeport->SetValue(true);
                    break;
                }
                case 0x2: {
                    StdMetrics.ms_v_charge_type->SetValue("ccs");
                    StdMetrics.ms_v_door_chargeport->SetValue(true);
                    break;
                }
                case 0x3: {
                    // Cable connected, charge complete or not needed (e.g. 100% SoC).
                    StdMetrics.ms_v_door_chargeport->SetValue(true);
                    break;
                }
                default:
                    break;
            }

            tmp_u8 = ((uint8_t)(d[5] & 0x10) >> 4) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            if (tmp_u8 == 0x1) {
                // vehicle connected to power grid
            } else {
                // vehicle not connected to power grid
            }

            tmp_u8 = ((uint8_t)(d[5] & 0x60) >> 5) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            switch (tmp_u8) {
                case 0x0: {
                    // no HV request
                    break;
                }
                case 0x1: {
                    // charging request
                    break;
                }
                case 0x2: {
                    // conditioning request
                    break;
                }
                case 0x3: {
                    // climatisation request
                    break;
                }
            }

            // tmp_u8 =
            // ((uint8_t) (d[5] & 0x80) >> 7) |
            // 0; // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            // tmp_u8 = (uint8_t) tmp_u8;
            // if(tmp_u8 == 0x1)
            // {
            //   // no conditioning cabin request
            // }
            // else
            // {
            //   //conditioning cabin request
            // }

            tmp_u8 = ((uint8_t)(d[6] & 0xc) >> 2) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            switch (tmp_u8) {
                case 0x0: {
                    // no conditioning request
                    break;
                }
                case 0x1: {
                    // instant conditioning request
                    break;
                }
                case 0x2: {
                    // timed conditioning request
                    break;
                }
                case 0x3: {
                    // error conditioning request
                    break;
                }
            }

            tmp_u8 = ((uint8_t)(d[6] & 0x30) >> 4) |
                     0;  // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;
            switch (tmp_u8) {
                case 0x0: {
                    // plug status init
                    break;
                }
                case 0x1: {
                    // no plug detected
                    break;
                }
                case 0x2: {
                    // plug detected but not locked
                    break;
                }
                case 0x3: {
                    // eplug detected and locked
                    break;
                }
            }
            // StdMetrics.ms_v_charge_state;                  // charging, topoff, done, prepare,
            // timerwait, heating, stopped StdMetrics.ms_v_charge_substate;               //
            // scheduledstop, scheduledstart, onrequest, timerwait, powerwait, stopped, interrupted

            tmp_u8 = ((uint8_t)(d[7] & 0x1F) << 0) |
                     0;  // Faktor 0.5 Offset 15.5, Minimum 15.5, Maximum 29.5 [°C] Initial 30.5
            tmp_u8 = (uint8_t)tmp_u8;
            tmp_f32 = ((float)tmp_u8) * 0.5F + 15.5F;
            StandardMetrics.ms_v_env_cabinsetpoint->SetValue(
                tmp_f32);  // working            // Cabin setpoint temperature [°C]

            tmp_u8 = ((uint8_t)(d[7] & 0x20) >> 5) |
                     0;                /// Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;  // 0x0 no rear window heating 0x1 rear window heating
            // if(tmp_u8 == 0x1)
            // {
            //   //
            // }
            // else
            // {
            //   //
            // }

            tmp_u8 = ((uint8_t)(d[7] & 0x40) >> 6) |
                     0;                /// Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;  // 0x0 no hv bat conditioning 0x1 hv bat conditioning
            // if(tmp_u8 == 0x1)
            // {
            //   //
            // }
            // else
            // {
            //   //
            // }

            tmp_u8 = ((uint8_t)(d[7] & 0x80) >> 7) |
                     0;                /// Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            tmp_u8 = (uint8_t)tmp_u8;  // 0x0 no front window heating 0x1 front window heating
            // if(tmp_u8 == 0x1)
            // {
            //   //
            // }
            // else
            // {
            //   //
            // }

            ESP_LOGV(TAG, "0x0594 charging=%d timer=%d type=%s setpoint=%.1f°C",
                     StdMetrics.ms_v_charge_inprogress->AsBool(),
                     StdMetrics.ms_v_charge_timermode->AsBool(),
                     StdMetrics.ms_v_charge_type->AsString().c_str(),
                     StdMetrics.ms_v_env_cabinsetpoint->AsFloat());

            break;
        }
        case 0x59E:  // BMS battery pack temperature. Factor 0.5°C, offset -40°C.
        {
            // 0xFE/0xFF are startup sentinels (decode to 87/87.5°C). Discard them.
            if (d[2] >= 0xFE) break;
            tmp_f32 = ((float)d[2]) * 0.5F - 40.0F;
            StandardMetrics.ms_v_bat_temp->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x059E bat_temp=%.1f°C", tmp_f32);
            break;
        }
        case 0x5CA:  // HV battery energy content. 11-bit, factor 50 Wh → kWh.
        {
            // Near-max raw value (upper 7 bits of d[2] all set) is a startup sentinel
            // that decodes to ~102 kWh — well above the physical 35.8 kWh capacity.
            if ((d[2] & 0x7F) == 0x7F) break;
            tmp_u16 = ((uint16_t)(d[1] & 0xf0) >> 4) | ((uint16_t)(d[2] & 0x7f) << 4);
            tmp_f32 = ((float)tmp_u16) * 50.0F / 1000.0F;
            StandardMetrics.ms_v_bat_capacity->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x05CA bat_capacity=%.1f kWh", tmp_f32);
            break;
        }
        case 0x5EA:  // Clima ECU status: cabin temperature and remote mode.
        {
            // Cabin temperature: 10-bit, factor 0.1°C, offset -40°C.
            // Near-max raw value is a startup sentinel decoding to ~62°C. Discard it.
            tmp_u16 = ((uint16_t)(d[6] & 0xfc) >> 2) | ((uint16_t)(d[7] & 0xf) << 6);
            if (tmp_u16 >= 0x3FE) break;
            tmp_f32 = ((float)tmp_u16) * 0.1F - 40.0F;

            // remote_mode: 0=idle, 2=running, 3=just activated. HVAC on when != 0.
            tmp_u8 = ((uint8_t)(d[3] & 0xc0) >> 6) | ((uint8_t)(d[4] & 0x1) << 2);
            StandardMetrics.ms_v_env_hvac->SetValue(tmp_u8 != 0);
            StandardMetrics.ms_v_env_cabintemp->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x05EA clima_cabin=%.1f°C remote_mode=%u", tmp_f32, tmp_u8);
            break;
        }
        case 0x5F5:  // Range estimates from the instrument cluster.
        {
            // Estimated range (matches instrument cluster display): 11-bit, factor 1 km.
            tmp_u16 = ((uint16_t)(d[3] & 0xe0) >> 5) | ((uint16_t)(d[4]) << 3);
            StandardMetrics.ms_v_bat_range_est->SetValue((float)tmp_u16);

            // Ideal range (BMS model, typically lower than estimated): 11-bit, factor 1 km.
            tmp_u16 = ((uint16_t)(d[0])) | ((uint16_t)(d[1] & 0x7) << 8);
            StdMetrics.ms_v_bat_range_ideal->SetValue((float)tmp_u16);
            ESP_LOGV(TAG, "0x05F5 range_est=%u range_ideal=%u km",
                     StandardMetrics.ms_v_bat_range_est->AsInt(),
                     StdMetrics.ms_v_bat_range_ideal->AsInt());
            break;
        }
        case 0x65A:  // BCM_01: bonnet/hood open indicator (MHWIVSchalter, d[4] bit 0).
        {
            StdMetrics.ms_v_door_hood->SetValue(d[4] & 0x1);
            ESP_LOGV(TAG, "0x065A hood=%u", d[4] & 0x1);
            break;
        }
        case 0x66E:  // InnenTemp: cabin interior temperature sensor.
        {
            // 0xFE is the ECU's "not ready" sentinel (decodes to 77°C). Discard it.
            if (d[4] == 0xFE) break;
            tmp_f32 = ((float)d[4]) * 0.5F - 50.0F;
            StandardMetrics.ms_v_env_cabintemp->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x066E cabin_temp=%.1f°C", tmp_f32);
            break;
        }
        case 0x6B0:  // FS temperature sensor (windshield/front area). Not yet mapped to a metric.
        {
            tmp_f32 = ((float)d[4]) * 0.5F - 40.0F;
            ESP_LOGV(TAG, "0x06B0 fs_temp=%.1f°C", tmp_f32);
            break;
        }
        case 0x6B5:  // Ambient temperature: solar sensor and outside air.
        {
            tmp_u16 = ((uint16_t)(d[6])) | ((uint16_t)(d[7] & 0x7) << 8);
            ESP_LOGV(TAG, "0x06B5 solar_sensor=%.1f°C", ((float)tmp_u16) * 0.1F - 40.0F);
            tmp_u16 = ((uint16_t)(d[2])) | ((uint16_t)(d[3] & 0x3) << 8);
            ESP_LOGV(TAG, "0x06B5 air_sensor=%.1f°C", ((float)tmp_u16) * 0.1F - 40.0F);
            break;
        }
        case 0x6B7:  // AussenTemp gefiltert Kilometerstand
        {
            tmp_u32 =
                ((uint32_t)(d[0] & 0xff) << 0) | ((uint32_t)(d[1] & 0xff) << 8) |
                ((uint32_t)(d[2] & 0xf) << 16) |
                0;  // odometer Faktor 1 Offset 0, Minimum 0, Maximum 1045873 [km] Initial 1045874
            tmp_u32 = (uint32_t)tmp_u32;
            // tmp_f32 = ((float)tmp_u32)*1.0F;
            StandardMetrics.ms_v_pos_odometer->SetValue(tmp_u32);  // working
            ESP_LOGV(TAG, "0x06B7 odo=%u km", tmp_u32);

            // Park time: 17-bit field at bit offset 20, factor 1 s.
            // d[2] bits [7:4] → result bits [3:0], d[3] → [11:4], d[4] bits [4:0] → [16:12].
            tmp_u32 =
                ((uint32_t)(d[2] & 0xf0) >> 4) | ((uint32_t)(d[3]) << 4) |
                ((uint32_t)(d[4] & 0x1f) << 12);
            StandardMetrics.ms_v_env_parktime->SetValue(tmp_u32);
            ESP_LOGV(TAG, "0x06B7 parktime=%u", tmp_u32);

            tmp_u8 = ((uint8_t)(d[7] & 0xff) << 0) |
                     0;  // outerTemp Faktor 0.5 Offset -50, Minimum -50, Maximum 75 [°C] Initial 77
            tmp_u8 = (uint8_t)tmp_u8;
            tmp_f32 = ((float)tmp_u8) * 0.5F - 50.0F;
            StandardMetrics.ms_v_env_temp->SetValue(tmp_f32);  // working
            ESP_LOGV(TAG, "0x06B7 outside=%.1f°C", tmp_f32);
            break;
        }
        case 0x3C0:  // clamp status received
        {
            // the following are from d[2]
            // KL_S Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            // KL_15 Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            // KL_X Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            // KL_50 Startanforderung Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            // Remotestart Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            // KL_Infotainment Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            // Remotestart_KL15_Anf Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            // Remotestart_Motor_Start Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
            break;
        }
        default: {
            // only for debug log ALL Incoming frames As i didn't know what frames are coming in
            // after wakeup had to log all only all unknown ESP_LOGI(TAG, "T26: timestamp: %.24i
            // 3R29 %12x %02x %02x %02x %02x %02x %02x %02x %02x",
            // StandardMetrics.ms_m_monotonic->AsInt(), p_frame->MsgID, d[0], d[1], d[2], d[3],
            // d[4], d[5], d[6], d[7]);
            break;
        }
    }
}

// ms_v_env_awake;                     // Vehicle is fully awake (switched on by the user)
// ms_v_env_on;                        // Vehicle is in "ignition" state (drivable)

// 0x12dd546f BCM_04... ansteuerung LED Ladeanzeige mit Dimmung und Farbe?

// 0x5B0 => TimeDate

void OvmsVehicleVWeGolf::Ticker1(uint32_t ticker) {
    // 10 seconds after last received message we assume that the car is sleeping
    m_is_car_online = m_last_message_received < 10;

    if (m_last_message_received < 254) m_last_message_received++;
    ESP_LOGV(TAG, "0x5A7 last_msg=%u", m_last_message_received);

    if (m_is_control_active &&
        m_is_car_online)  // after wakeup the other ECUs waiting for the car to be online before we
                          // send some Heartbeat messages otherwise we have some serious txerrors
                          // just before the car ist active
    {
        SendOcuHeartbeat();  // working
        ESP_LOGV(TAG, "Heartbeat sending triggered");
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandLock(const char* pin) {
    if (!PinCheck(pin)) {
        ESP_LOGW(TAG, "PinCheck failed in CommandLock");
        return Fail;
    }
    m_lock_requested = true;
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandUnlock(const char* pin) {
    if (!PinCheck(pin)) {
        ESP_LOGW(TAG, "PinCheck failed in CommandUnlock");
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
    m_panic_mode_requested = true;
    return Success;
}

void OvmsVehicleVWeGolf::Ticker10(uint32_t ticker) {
    // working
    m_climate_control_temp = MyConfig.GetParamValueInt("xvg", "cc_temp", 21);
    m_climate_control_on_battery = (MyConfig.GetParamValueBool("xvg", "cc_onbat", false) ? 1 : 0);

    ESP_LOGV(TAG,
             "Trigger10 cc_temp: %u °C, cc_onbat: %u, control_mirror %u, control_horn: %u, "
             "control_indicator: %u, control_panicMode: %u, control_unlock %u, control_lock %u",
             m_climate_control_temp, m_climate_control_on_battery, m_mirror_fold_in_requested,
             m_horn_requested, m_indicators_requested, m_panic_mode_requested, m_unlock_requested,
             m_lock_requested);
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandWakeup() {
    ESP_LOGV(TAG, "Wakeup triggered");

    // Info: eGolf300 after sending only the heartbeat message ID:0x5A7 or the first message here
    // ID:0x17330301 one ECU with ID:0x5F5 is answering on the bus perhaps ID:0x66E and ID:0x6B5 too
    // Info: perhaps this could be used for another method to wakeup the car comf CAN perhaps
    // changing the settings is possible without weaking up everything

    if (!m_is_car_online) {
        ESP_LOGI(TAG, "Car is sleeping we are trying to wake it up");
        // Wake up the Bus //CLI: can can3 tx extended 0x17330301 0x40 0x00 0x01 0x1F 0x00 0x00 0x00
        // 0x00
        canbus* comfBus;
        comfBus = m_can3;
        uint8_t length = 8;
        uint8_t data[length];
        data[0] = 0x40;
        data[1] = 0x00;
        data[2] = 0x01;
        data[3] = 0x1F;
        data[4] = 0x00;
        data[5] = 0x00;
        data[6] = 0x00;
        data[7] = 0x00;
        comfBus->WriteExtended(0x17330301, length, data);
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGV(TAG, "First message send ID: data 0->7");
        ESP_LOGV(TAG,
                 "First message send ID:0x17330301 data %02x %02x %02x %02x %02x %02x %02x %02x",
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

        vTaskDelay(pdMS_TO_TICKS(100));
        length = 8;
        data[0] = 0x67;  // source node identifier identity of the transmitter of the message
        data[1] = 0x10;  // could be anything
        data[2] = 0x41;  // 0-5 => State,  6 => eCall Car Wakeup
        data[3] = 0x84;  // 0-8 => eCall Wakeup
        data[4] = 0x14;
        data[5] = 0x00;
        data[6] = 0x00;
        data[7] = 0x00;
        comfBus->WriteExtended(0x1B000067, length, data);
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGV(TAG, "second message send ID: data 0->7");
        ESP_LOGV(TAG,
                 "second message send ID:0x1B000067 data %02x %02x %02x %02x %02x %02x %02x %02x",
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

        m_is_control_active = true;
    } else {
        ESP_LOGI(TAG, "Wakeup not necessary car was online before");
    }
    return Success;
}

void OvmsVehicleVWeGolf::SendOcuHeartbeat() {
    uint8_t tmp_u8 = 0;

    canbus* comfBus;
    comfBus = m_can3;
    uint8_t length = 8;
    uint8_t data[length];
    length = 8;
    data[0] = 0x00;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;

    // Spiegelanklappen
    if (m_mirror_fold_in_requested) {
        tmp_u8 = 1;
        data[5] = (((uint8_t)tmp_u8) << 7) & 0x80;
        m_mirror_fold_in_requested = false;
        ESP_LOGI(TAG, "Mirror fold in");
    }

    // Hupen
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

    // Warnblinken
    if (m_indicators_requested) {
        tmp_u8 = 1;
        data[6] = (((uint8_t)tmp_u8) << 3) & 0x8;
        m_indicators_requested = false;
        ESP_LOGI(TAG, "Hazard lights");
    }

    // Panicalarm
    if (m_panic_mode_requested) {
        tmp_u8 = 1;
        data[6] = (((uint8_t)tmp_u8) << 4) & 0x10;
        m_panic_mode_requested = false;
        ESP_LOGI(TAG, "PanicAlarm!");
    }

    comfBus->WriteStandard(0x5A7, length, data);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGV(TAG, "Heartbeat send ID: data 0->7");
    ESP_LOGI(TAG, "FHeartbeat send ID:0x5A7 data %02x %02x %02x %02x %02x %02x %02x %02x", data[0],
             data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}
