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

    // 0x66E (InnenTemp) broadcasts 0xFE as a sentinel before valid data is available,
    // which decodes to 77°C and persists across reboots. Clear it so it shows as
    // undefined rather than a wrong value.
    StandardMetrics.ms_v_env_cabintemp->Clear();

    OvmsCommand* cmd_vweg = MyCommandApp.RegisterCommand("xvg", "VW-eGolf framework");
    cmd_vweg->RegisterCommand("offline", "OVMS please go offline", [this](...) {
        m_is_control_active = false;
        ESP_LOGI(TAG, "Heartbeat sending should be stopped");
    });
    cmd_vweg->RegisterCommand("fold_mirrors", "fold mirror toggle", [this](...) {
        CommandMirrorFoldIn();
        ESP_LOGI(TAG, "Test command fold_mirrors call executed");
    });
    ESP_LOGI(TAG, "Commands for testing purposes registerd");

    OvmsCommand* cmd_xvg = MyCommandApp.RegisterCommand("xvg", "VW e-Golf controls");
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

void OvmsVehicleVWeGolf::IncomingFrameCan2(CAN_frame_t* p_frame) {
    switch (p_frame->MsgID) {
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
    // J533 bridges KCAN traffic onto CAN2; forward every frame so the KCAN
    // decoder in IncomingFrameCan3 can process it regardless of which bus it arrives on.
    IncomingFrameCan3(p_frame);
}

void OvmsVehicleVWeGolf::IncomingFrameCan3(CAN_frame_t* p_frame) {
    // Only genuine KCAN frames (origin==can3) reset the idle counter and trigger the
    // OCU keepalive. IncomingFrameCan2 forwards FCAN frames here for shared decode, but
    // those must not be treated as KCAN activity — during charging, FCAN is flooded with
    // OBC data while KCAN is completely silent, and counting FCAN frames as KCAN-alive
    // causes the heartbeat to hammer a silent bus until the TWAI controller hits bus-off.
    if (p_frame->origin == m_can3) {
        m_bus_idle_ticks = 0;

        // Detect the OEM OCU by watching for non-zero 0x5A7 frames. Our heartbeat sends
        // all-zeros; the OEM OCU sends non-zero data while the car is awake or in NM
        // ring-down (~10–15s after ignition off). We must not send NM wake while the OEM
        // OCU is active — the conflicting 0x5A7 data causes arbitration loss → bus-off.
        if (p_frame->MsgID == 0x5A7) {
            uint8_t* b = p_frame->data.u8;
            if (b[0] | b[1] | b[2] | b[3] | b[4] | b[5] | b[6] | b[7]) {
                m_oem_ocu_idle_ticks = 0;
            }
        }

        // Send OCU keepalive at ~5Hz while active. VW OSEK NM requires keepalives at
        // ~200ms intervals; Ticker1 alone (1Hz) is too slow for the ECU to stay in network.
        // Rate-limited by wall-clock tick (180ms min) so a sudden bus traffic burst from an
        // NM wake does not overflow the TX queue.
        if (m_ocu_active) {
            uint32_t now = xTaskGetTickCount();
            if ((now - m_last_heartbeat_tick) * portTICK_PERIOD_MS >= 180) {
                m_last_heartbeat_tick = now;
                SendOcuHeartbeat();
            }
        }
    }

    uint8_t* d = p_frame->data.u8;
    uint8_t tmp_u8 = 0;
    uint16_t tmp_u16 = 0;
    uint32_t tmp_u32 = 0;
    float tmp_f32 = 0.0F;

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
            if (d[3] == 0xFE) break;
            tmp_f32 = ((float)d[3]) * 0.5F;
            StandardMetrics.ms_v_bat_soc->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x0131 soc=%.1f%%", tmp_f32);
            break;
        }
        case 0x191:  // BMS current, voltage, power.
        {
            // Startup sentinel: d[2]=0xFF decodes to I=2047 A and V=1023.5 V. Discard it.
            if (d[2] == 0xFF) break;

            // Current: 12-bit, factor 1 A, offset -2047 A.
            tmp_u16 = ((uint16_t)(d[1] & 0xf0) >> 4) | ((uint16_t)(d[2]) << 4);
            tmp_f32 = ((float)tmp_u16) * 1.0F - 2047.0F;
            StandardMetrics.ms_v_bat_current->SetValue(tmp_f32);

            // Voltage: 12-bit, factor 0.25 V.
            tmp_u16 = ((uint16_t)(d[3])) | ((uint16_t)(d[4] & 0xf) << 8);
            tmp_f32 = ((float)tmp_u16) * 0.25F;
            StandardMetrics.ms_v_bat_voltage->SetValue(tmp_f32);

            // Power: negative = charging, positive = driving.
            tmp_f32 = -1.0F *
                      (StandardMetrics.ms_v_bat_voltage->AsFloat() *
                       StandardMetrics.ms_v_bat_current->AsFloat()) /
                      1000.0F;
            StandardMetrics.ms_v_bat_power->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x0191 I=%.1fA V=%.2fV", StandardMetrics.ms_v_bat_current->AsFloat(),
                     StandardMetrics.ms_v_bat_voltage->AsFloat());
            break;
        }
        case 0x2AF:  // Trip energy counters. 15-bit, factor 10 Ws → kWh.
        {
            // Regen energy: d[4] + d[5] bits [6:0]. Max raw 32767 * 10 = 327670 Ws.
            tmp_f32 = (float)(d[4] | ((uint16_t)(d[5] & 0x7f) << 8)) * 10.0F / 3600000.0F;
            StandardMetrics.ms_v_bat_energy_recd->SetValue(tmp_f32);

            // Consumed energy: d[6] + d[7] bits [6:0].
            tmp_f32 = (float)(d[6] | ((uint16_t)(d[7] & 0x7f) << 8)) * 10.0F / 3600000.0F;
            StandardMetrics.ms_v_bat_energy_used->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x02AF recd=%.4f used=%.4f kWh",
                     StandardMetrics.ms_v_bat_energy_recd->AsFloat(),
                     StandardMetrics.ms_v_bat_energy_used->AsFloat());
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
            tmp_u32 = ((uint32_t)(d[0])) | ((uint32_t)(d[1]) << 8) | ((uint32_t)(d[2]) << 16) |
                      ((uint32_t)(d[3] & 0x7) << 24);
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
            ESP_LOGV(TAG, "0x0583 locked=%u fl=%u fr=%u rl=%u rr=%u trunk=%u", (d[2] & 0x2) >> 1,
                     d[3] & 0x1, (d[3] & 0x2) >> 1, (d[3] & 0x4) >> 2, (d[3] & 0x8) >> 3,
                     (d[3] & 0x10) >> 4);
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
                    if (is_charging)
                        NotifyChargeStart();
                    else
                        NotifyChargeStopped();
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

            // remote_mode field: unreliable when ignition off (Capture 5 — always 0 in
            // remote clima, 0x5EA barely broadcasts). HVAC state driven from BAP port 0x12
            // on 0x17332510 instead — works for both OVMS-triggered and schedule-triggered.
            tmp_u8 = ((uint8_t)(d[3] & 0xc0) >> 6) | ((uint8_t)(d[4] & 0x1) << 2);
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
            tmp_u8 = ((uint8_t)(d[4] & 0xff) << 0) |
                     0;  // outerTemp Faktor 0.5 Offset -50, Minimum -50, Maximum 75 [°C] Initial 77
            tmp_u8 = (uint8_t)tmp_u8;
            if (tmp_u8 == 0xFE) break;  // ECU sentinel "no data yet" → 77°C, ignore
            tmp_f32 = ((float)tmp_u8) * 0.5F - 50.0F;
            StandardMetrics.ms_v_env_cabintemp->SetValue(tmp_f32);

            static uint8_t cnt3 = 0;
            cnt3++;
            if (cnt3 == 10) {
                cnt3 = 0;
                ESP_LOGI(TAG "-66E", "ms_v_env_cabintemp: %f", tmp_f32);
            }
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
            tmp_u32 = ((uint32_t)(d[2] & 0xf0) >> 4) | ((uint32_t)(d[3]) << 4) |
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
        case 0x17332510:  // BAP clima ECU (node 0x25) → bus responses.
        {
            // Port 0x12 status broadcast: payload[0] = 0x05 active, 0x00 idle.
            // Works for both OVMS-triggered and schedule-triggered clima (Captures 6, 12).
            // Single-frame: d[0:1] = BAP header (opcode|node, node|port), d[2+] = payload.
            //   49 52 XX → opcode=2(Status), node=0x25, port=0x12, payload[0]=XX
            // Multi-frame start: d[0]=0x80/0x90, d[1]=len, d[2:3]=BAP header, d[4+]=payload.
            //   80 07 49 52 XX → same, payload[0]=XX
            // Check for port 0x12 (0x52 in second BAP header byte, low 6 bits = 0x12).
            if (d[1] == 0x52 && (d[0] & 0xC0) == 0x40) {
                // Single-frame: d[0]=0x49 (opcode=2, node bits), d[1]=0x52 (port 0x12)
                tmp_u8 = d[2];
                StandardMetrics.ms_v_env_hvac->SetValue(tmp_u8 != 0);
                ESP_LOGV(TAG, "BAP port 0x12 single: hvac=%u (raw=0x%02X)", tmp_u8 != 0, tmp_u8);
            } else if ((d[0] & 0xC0) == 0x80 && d[3] == 0x52) {
                // Multi-frame start: d[2:3]=BAP header, d[4]=payload[0]
                tmp_u8 = d[4];
                StandardMetrics.ms_v_env_hvac->SetValue(tmp_u8 != 0);
                ESP_LOGV(TAG, "BAP port 0x12 multi: hvac=%u (raw=0x%02X)", tmp_u8 != 0, tmp_u8);
            }
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
    OvmsVehicle::Ticker1(ticker);

    if (m_bus_idle_ticks < 254) m_bus_idle_ticks++;
    if (m_oem_ocu_idle_ticks < 254) m_oem_ocu_idle_ticks++;

    bool bus_alive = m_bus_idle_ticks < VWEGOLF_BUS_TIMEOUT_SECS;
    bool just_went_idle = (m_bus_idle_ticks == VWEGOLF_BUS_TIMEOUT_SECS);
    ESP_LOGV(TAG, "Ticker1: bus_idle=%u alive=%d ocu=%d", m_bus_idle_ticks, bus_alive,
             m_ocu_active);

    if (just_went_idle) {
        m_ocu_active = false;
        ESP_LOGI(TAG, "KCAN idle: OCU node presence cleared");
    }

    if (m_ocu_active && bus_alive) {
        SendOcuHeartbeat();
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
    OvmsVehicle::Ticker10(ticker);
    m_climate_temp = (uint8_t)MyConfig.GetParamValueInt("xvg", "cc-temp", 21);
    // cc-onbat and cc-minsoc are persistent ECU settings in the e-Manager (set via the MQB
    // head unit).  The BAP write to push them to the clima ECU is pending RE work — the
    // Carnet app that originally sent these is defunct, so no capture is available yet.
    m_climate_on_battery = MyConfig.GetParamValueBool("xvg", "cc-onbat", false);
    ESP_LOGV(TAG, "Ticker10: cc_temp=%u°C cc_onbat=%d", m_climate_temp, m_climate_on_battery);
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

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandClimateControl(bool enable) {
    ESP_LOGI(TAG, "Climate control: %s", enable ? "start" : "stop");

    // If the car is sleeping, wake the bus first and let KCAN nodes come online.
    // The wake frames will TX_Fail (no ACK from sleeping nodes) but the dominant bits
    // on the bus are enough to wake the transceivers. Subsequent frames succeed once
    // the error counter recovers (~200 ms at 500 kbps with heavy traffic).
    if (!m_is_car_online) {
        CommandWakeup();
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    m_is_control_active = true;

    // Rolling counter — never zero, wraps 0xFF → 0x01
    m_bap_counter = (m_bap_counter == 0xFF) ? 0x01 : (m_bap_counter + 1);

    uint8_t data[8];

    // Frame 1: BAP multi-frame start — port 0x19 parameters (8-byte payload)
    // Header 29 59: opcode=2 (Status push), node=0x25, port=0x19
    data[0] = 0x80;           // multi-frame start, channel 0
    data[1] = 0x08;           // total payload length = 8 bytes
    data[2] = 0x29;           // BAP header byte 0
    data[3] = 0x59;           // BAP header byte 1 (node=0x25, port=0x19)
    data[4] = m_bap_counter;  // payload byte 0: rolling counter
    data[5] = 0x06;           // payload byte 1: duration (unit TBD — thomasakarlsen default)
    data[6] = 0x00;           // payload byte 2: unknown
    data[7] = 0x01;           // payload byte 3: unknown (mode/profile flag)
    m_can3->WriteExtended(0x17332501, 8, data);

    // Frame 2: BAP multi-frame continuation — remaining 4 payload bytes
    // TODO: data[3] is the target temperature. Encoding not yet confirmed for port 0x19.
    //       Port 0x16 schedule uses celsius+35 (20°C → 0x37); port 0x19 may differ.
    //       thomasakarlsen default is 0x20. Replace with m_climate_control_temp once confirmed.
    data[0] = 0xC0;  // continuation, channel 0
    data[1] = 0x06;  // payload byte 4: unknown
    data[2] = 0x00;  // payload byte 5: unknown
    data[3] = 0x20;  // payload byte 6: temperature (TODO: confirm encoding, use m_climate_control_temp)
    data[4] = 0x00;  // payload byte 7: unknown / padding
    m_can3->WriteExtended(0x17332501, 5, data);

    // Frame 3: BAP single-frame — port 0x18 start/stop trigger
    // Header 29 58: opcode=2, node=0x25, port=0x18
    data[0] = 0x29;                    // BAP header byte 0
    data[1] = 0x58;                    // BAP header byte 1 (node=0x25, port=0x18)
    data[2] = 0x00;                    // unknown
    data[3] = enable ? 0x01 : 0x00;   // 0x01 = start, 0x00 = stop
    m_can3->WriteExtended(0x17332501, 4, data);

    ESP_LOGI(TAG, "Climate %s sequence sent (counter=0x%02x)", enable ? "start" : "stop",
             m_bap_counter);
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

    comfBus->WriteStandard(0x5A7, length, data);
    ESP_LOGV(TAG, "Heartbeat 0x5A7: %02x %02x %02x %02x %02x %02x %02x %02x", data[0], data[1],
             data[2], data[3], data[4], data[5], data[6], data[7]);
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
    // Wait 1 s after wake for the NM-join flood to settle before sending BAP.
    if (m_bus_idle_ticks >= VWEGOLF_CLIMA_WAKE_SECS &&
        m_oem_ocu_idle_ticks >= VWEGOLF_CLIMA_WAKE_SECS) {
        ESP_LOGI(TAG, "Climate control: KCAN quiet %u s, OEM OCU quiet %u s — waking bus",
                 m_bus_idle_ticks, m_oem_ocu_idle_ticks);
        WakeKcanBus();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Rolling counter: echoed back (| 0x80) in the ECU's ACK to match the command.
    m_bap_counter = (m_bap_counter == 0xFF) ? 0x01 : m_bap_counter + 1;

    // Suppress heartbeat for the duration of the 3-frame BAP sequence; a heartbeat
    // queuing between frames blocks the continuation and the ECU discards the message.
    m_last_heartbeat_tick = xTaskGetTickCount();

    uint8_t data[8];

    // Frame 1: BAP multi-frame start → port 0x19 (clima parameters), 8-byte payload.
    data[0] = 0x80;           // multi-frame start, channel 0
    data[1] = 0x08;           // total payload length
    data[2] = 0x29;           // BAP opcode=2, node=0x25 [5:2]
    data[3] = 0x59;           // BAP node=0x25 [1:0], port=0x19
    data[4] = m_bap_counter;  // rolling counter; ACK echoes this | 0x80
    data[5] = 0x06;           // duration: 0x06 = 15 min (0x04 = 10 min)
    data[6] = 0x00;
    data[7] = 0x01;
    esp_err_t ok1 = m_can3->WriteExtended(0x17332501, 8, data, pdMS_TO_TICKS(200));
    if (ok1 == ESP_FAIL) {
        ESP_LOGW(TAG, "BAP clima frame 1 TX queue overflow");
        m_ocu_active = false;
        return Fail;
    }

    // Frame 2: BAP continuation — remaining payload bytes including target temperature.
    // Temperature encoding: raw = (celsius - 10) * 10  (e.g. 21°C → 0x6E)
    // Read config directly — Ticker10 cache may be up to 10 s stale (Capture 7 bug).
    m_climate_temp = (uint8_t)MyConfig.GetParamValueInt("xvg", "cc-temp", 21);
    uint8_t temp_raw = (uint8_t)((m_climate_temp - 10) * 10);
    data[0] = 0xC0;  // multi-frame continuation, channel 0
    data[1] = 0x06;
    data[2] = 0x00;
    data[3] = temp_raw;  // target temperature
    data[4] = 0x00;
    esp_err_t ok2 = m_can3->WriteExtended(0x17332501, 5, data, pdMS_TO_TICKS(200));
    if (ok2 == ESP_FAIL) {
        ESP_LOGW(TAG, "BAP clima frame 2 TX queue overflow");
        m_ocu_active = false;
        return Fail;
    }

    // Frame 3: BAP single-frame trigger → port 0x18 (immediate start/stop).
    // d[3] = 0x01 start / 0x00 stop.
    data[0] = 0x29;
    data[1] = 0x58;
    data[2] = 0x00;
    data[3] = enable ? 0x01 : 0x00;
    esp_err_t ok3 = m_can3->WriteExtended(0x17332501, 4, data, pdMS_TO_TICKS(200));
    if (ok3 == ESP_FAIL) {
        ESP_LOGW(TAG, "BAP clima frame 3 TX queue overflow");
        m_ocu_active = false;
        return Fail;
    }

    ESP_LOGI(TAG, "BAP clima %s sent: counter=0x%02X temp=%u°C", enable ? "start" : "stop",
             m_bap_counter, m_climate_temp);

    // Optimistic update: give the app immediate feedback. 0x05EA will overwrite this
    // with ground truth once the ECU responds.
    StandardMetrics.ms_v_env_hvac->SetValue(enable);
    m_ocu_active = enable;  // stay in NM ring while clima runs; leave after stop
    return Success;
}
