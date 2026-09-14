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

#include <cstdio>   // snprintf / sscanf — format the profile/timer list rows, parse HH:MM
#include <cstdlib>  // atoi — parse timer slot / profile args
#include <cstring>  // strchr / strlen — tokenize the weekday list
#include "vehicle_vwegolf.h"

#undef TAG
#define TAG "v-vwegolf"

OvmsVehicleVWeGolf::OvmsVehicleVWeGolf() {
    ESP_LOGI(TAG, "Start vehicle module: VW e-Golf");

    // init configs:
    MyConfig.RegisterParam("xvg", "VW e-Golf", true, true);

    // Regenerative-braking strength (numeric, cheap to transmit). Decoded from the
    // gear-selector frame 0x187 in IncomingFrameCan2. -1 until first seen in D/B.
    m_recup_level = MyMetrics.InitInt("xvg.v.recup", SM_STALE_MIN, -1);

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

    // The climate controller drives the BCU over the KCAN (comfort) bus.
    m_batctrl.SetBus(m_can3);

    OvmsCommand* cmd_vweg = MyCommandApp.RegisterCommand("xvg", "VW e-Golf controls");
    cmd_vweg->RegisterCommand("offline", "Stop sending OCU keepalive (diagnostic)", [this](...) {
        m_ocu_active = false;
        m_batctrl.Abort();  // also release any in-flight climate NM-wake bridge
        ESP_LOGI(TAG, "OCU keepalive stopped");
    });
    cmd_vweg->RegisterCommand("fold_mirrors", "Fold mirrors in",
                              [this](...) { CommandMirrorFoldIn(); });

    // Charge-profile / departure-timer management. `xvg charge profile list` dumps the car's charge
    // profiles ("charge locations"); `xvg charge timer ...` lists and sets the 4 departure timers.
    OvmsCommand* cmd_charge  = cmd_vweg->RegisterCommand("charge", "Charge profile / timer management");
    OvmsCommand* cmd_profile = cmd_charge->RegisterCommand("profile", "Charge profiles (charge locations)");
    cmd_profile->RegisterCommand(
        "list", "List the vehicle's charge profiles",
        [this](int, OvmsWriter* writer, OvmsCommand*, int, const char* const*) {
            CommandListProfiles(writer);
        });
    // `set` writes selected fields of ONE profile (read-modify-write, no charge/climate trigger). The
    // valid fields differ per profile: profile 0 "Optionen" = current / minsoc / temp; charge locations
    // 1-3 = flags / current / soc. min 3 args (profile + one field/value); max 7 (profile + 3 pairs).
    cmd_profile->RegisterCommand(
        "set", "Set fields of a charge profile",
        [this](int, OvmsWriter* writer, OvmsCommand*, int argc, const char* const* argv) {
            CommandSetProfile(writer, argc, argv);
        },
        "<profile 0-3> <field value>...  (0: current/minsoc/temp; 1-3: flags/current/soc)", 3, 7);

    // Departure timers (recurring). list = read-only; set/enable/disable/clear write to the car
    // (on 0x17332501, like climate/charge). A timer binds to a charge profile by index -> its target SoC.
    OvmsCommand* cmd_timer = cmd_charge->RegisterCommand("timer", "Departure timers");
    cmd_timer->RegisterCommand(
        "list", "List the vehicle's departure timers",
        [this](int, OvmsWriter* writer, OvmsCommand*, int, const char* const*) {
            CommandListTimers(writer);
        });
    cmd_timer->RegisterCommand(
        "set", "Set a recurring departure timer",
        [this](int, OvmsWriter* writer, OvmsCommand*, int argc, const char* const* argv) {
            CommandSetTimer(writer, argc, argv);
        },
        "<slot 1-3> <HH:MM> <days> <profile 0-3>", 4, 5);
    cmd_timer->RegisterCommand(
        "enable", "Enable a departure timer",
        [this](int, OvmsWriter* writer, OvmsCommand*, int argc, const char* const* argv) {
            CommandTimerEnable(writer, argc, argv, true);
        },
        "<slot 1-3>", 1, 1);
    cmd_timer->RegisterCommand(
        "disable", "Disable a departure timer",
        [this](int, OvmsWriter* writer, OvmsCommand*, int argc, const char* const* argv) {
            CommandTimerEnable(writer, argc, argv, false);
        },
        "<slot 1-3>", 1, 1);
    cmd_timer->RegisterCommand(
        "clear", "Clear (blank + disable) a departure timer",
        [this](int, OvmsWriter* writer, OvmsCommand*, int argc, const char* const* argv) {
            CommandClearTimer(writer, argc, argv);
        },
        "<slot 1-3>", 1, 1);
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
            // Drive mode (Normal/Eco/Eco+) is NOT derived here — B is a gear/regen
            // selection, not a Charisma drive profile. ms_v_env_drivemode is set from
            // the Charisma active profile in frame 0x386 (IncomingFrameCan3).
            if (gear_nibble == 2) {
                // Park
                StandardMetrics.ms_v_env_gear->SetValue(0);
            } else if (gear_nibble == 3) {
                // Reverse
                StandardMetrics.ms_v_env_gear->SetValue(-1);
            } else if (gear_nibble == 4) {
                // Neutral
                StandardMetrics.ms_v_env_gear->SetValue(0);
            } else if (gear_nibble == 5) {
                // Drive
                StandardMetrics.ms_v_env_gear->SetValue(1);
            } else if (gear_nibble == 6) {
                // B mode
                StandardMetrics.ms_v_env_gear->SetValue(1);
            }

            // Regenerative-braking (recuperation) strength. The e-Golf has five
            // regen levels: D0 (coast, no regen), D1, D2, D3, and B (max). D0..D3
            // are selected with the paddles while in gear D; B is its own gear.
            // Exposed as a 0..4 strength (least->most) on xvg.v.recup.
            //
            // State is in this frame's d[1] high nibble; the top bit is always set
            // in operation, so mask it off (rc = low 3 bits). In gear D:
            //   0 = D0 coast (just shifted into D, no stage selected)
            //   1 = D1, 2 = D2, 3 = D3  (paddle regen stages)
            //   5 = D0 with recuperation switched off by the driver (also coast)
            // In gear B rc reads 0, but the gear itself means max regen.
            const uint8_t rc = (p_frame->data.u8[1] >> 4) & 0x7;
            int recup = -1;                                 // N/A unless in D or B
            if (gear_nibble == 6) {
                recup = 4;                                  // B — max regen
            } else if (gear_nibble == 5) {
                recup = (rc >= 1 && rc <= 3) ? rc : 0;      // D1/D2/D3, else D0 (coast)
            }
            m_recup_level->SetValue(recup);
            ESP_LOGV(TAG, "0x187 gear=%u rc=%u recup=%d", gear_nibble, rc, recup);
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
    m_bus_idle_ticks = 0;

    // Send OCU keepalive at ~5Hz while active. VW OSEK NM requires keepalives at
    // ~200ms intervals; Ticker1 alone (1Hz) is too slow for the ECU to stay in network.
    // SendOcuHeartbeat self-throttles (180ms min) against TX queue overflow on bus bursts.
    if (m_ocu_active) {
        SendOcuHeartbeat();
    }

    uint8_t* d = p_frame->data.u8;

    // Track OEM OCU activity: any non-zero 0x5A7 means the car's OCU is still active.
    // Reset the idle counter so we don't wake while it would conflict with our heartbeat.
    if (p_frame->MsgID == 0x5A7) {
        if (d[0] | d[1] | d[2] | d[3] | d[4] | d[5] | d[6] | d[7]) {
            m_oem_ocu_idle_ticks = 0;
        }
    }

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

            // Current: 12-bit, factor 1 A. The raw field is charge-positive; negate to
            // the OVMS convention (ms_v_bat_current is output=positive, i.e. discharge
            // positive / charge negative): I = 2047 - raw.
            tmp_u16 = ((uint16_t)(d[1] & 0xf0) >> 4) | ((uint16_t)(d[2]) << 4);
            tmp_f32 = 2047.0F - (float)tmp_u16;
            StandardMetrics.ms_v_bat_current->SetValue(tmp_f32);

            // Voltage: 12-bit, factor 0.25 V.
            tmp_u16 = ((uint16_t)(d[3])) | ((uint16_t)(d[4] & 0xf) << 8);
            tmp_f32 = ((float)tmp_u16) * 0.25F;
            StandardMetrics.ms_v_bat_voltage->SetValue(tmp_f32);

            // Power = V * I, following the output=positive current sign above
            // (ms_v_bat_power is output=positive: positive = driving, negative = charging).
            tmp_f32 = (StandardMetrics.ms_v_bat_voltage->AsFloat() *
                       StandardMetrics.ms_v_bat_current->AsFloat()) /
                      1000.0F;
            StandardMetrics.ms_v_bat_power->SetValue(tmp_f32);

            // Report the DC pack-side current/power as the charge current/power. The comfort
            // bus carries no AC charger-side telemetry — a scan of all ~214 KCAN ids during a
            // full charge found no AC line voltage, AC current, or charger-input power frame;
            // the only live charge measurement is the DC pack side here (0x5AC byte 3 mirrors it
            // as integer amps). ms_v_charge_voltage is likewise the pack voltage (set in 0x594).
            // Sign flips to the charge convention: ms_v_charge_current / ms_v_charge_power are
            // positive while charging, whereas ms_v_bat_current / ms_v_bat_power are output-
            // positive (negative while charging). Without this the app's charge screen shows 0 A.
            // The charging flag is driven by 0x594; when it clears we zero these there.
            if (StandardMetrics.ms_v_charge_inprogress->AsBool()) {
                StandardMetrics.ms_v_charge_current->SetValue(
                    -StandardMetrics.ms_v_bat_current->AsFloat());
                StandardMetrics.ms_v_charge_power->SetValue(
                    -StandardMetrics.ms_v_bat_power->AsFloat());
            }
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
        case 0x386:  // Drive mode (Charisma / Fahrprofilauswahl active profile).
        {
            // d[5] = active drive profile: 0x02 = Normal, 0x05 = Eco, 0x08 = Eco+
            // (matches the MIB CharismaProfiles enum auto_normal=2/efficiency=5/range=8).
            // Mapped to ms_v_env_drivemode as 1 = Normal, 2 = Eco, 3 = Eco+, matching the
            // sibling VW e-Up module's v.e.drivemode encoding (1=STD, 2=ECO, 3=ECO+).
            switch (d[5]) {
                case 0x02:
                    StandardMetrics.ms_v_env_drivemode->SetValue(1);
                    break;
                case 0x05:
                    StandardMetrics.ms_v_env_drivemode->SetValue(2);
                    break;
                case 0x08:
                    StandardMetrics.ms_v_env_drivemode->SetValue(3);
                    break;
                default:
                    // Unknown profile value (0x00 = inactive when not drivable) — leave
                    // the last known drive mode.
                    break;
            }
            ESP_LOGV(TAG, "0x0386 drivemode raw=0x%02x", d[5]);
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
                if (is_charging) {
                    StdMetrics.ms_v_charge_state->SetValue("charging");
                    StdMetrics.ms_v_charge_voltage->SetValue(
                        StandardMetrics.ms_v_bat_voltage->AsFloat());
                } else if (was_charging) {
                    // Only flag "stopped" on a genuine charging->not-charging transition, NOT on
                    // every idle frame. Writing "stopped" unconditionally means the first idle
                    // 0x594 after any OVMS (re)start changes ms_v_charge_state from "" to
                    // "stopped", which the vehicle framework reports as a charge-stop — a spurious
                    // "Not charging" alert every time the parked/unplugged car is powered off.
                    // Leaving charge_state untouched when never charging avoids it; CommandStat
                    // still renders "Not charging" for an empty/plug-less state.
                    StdMetrics.ms_v_charge_state->SetValue("stopped");
                    // Charge ended: 0x191 stops mirroring the pack current into the charge
                    // metrics, so zero them here or the app keeps showing the last charge value.
                    StdMetrics.ms_v_charge_current->SetValue(0);
                    StdMetrics.ms_v_charge_power->SetValue(0);
                }
                // The charge start/stop push notifications are emitted by the vehicle framework
                // off the ms_v_charge_state transitions above (OvmsVehicle::NotifyChargeState);
                // calling NotifyChargeStart()/NotifyChargeStopped() here too would double-notify.
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
        case 0x5EA:  // Clima ECU status: cabin temperature and HVAC conditioning bit.
        {
            // HVAC conditioning state (d[3] bit 3) is owned by the climate controller;
            // forward it there. Done before the cabin-temp sentinel guard so HVAC always
            // tracks even when the temperature reads the startup sentinel.
            m_batctrl.IncomingClimaEcuStatus(p_frame);

            // Cabin temperature: 10-bit, factor 0.1°C, offset -40°C.
            // Near-max raw value is a startup sentinel decoding to ~62°C. Discard it.
            tmp_u16 = ((uint16_t)(d[6] & 0xfc) >> 2) | ((uint16_t)(d[7] & 0xf) << 6);
            if (tmp_u16 >= 0x3FE) break;
            tmp_f32 = ((float)tmp_u16) * 0.1F - 40.0F;
            StandardMetrics.ms_v_env_cabintemp->SetValue(tmp_f32);
            ESP_LOGV(TAG, "0x05EA clima_cabin=%.1f°C d3=%02x", tmp_f32, d[3]);
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
            // The field saturates at its 17-bit max (0x1FFFF ≈ 36.5 h); ignore that
            // clamped value so v.e.parktime falls back to OVMS's native (uncapped) counter.
            tmp_u32 = ((uint32_t)(d[2] & 0xf0) >> 4) | ((uint32_t)(d[3]) << 4) |
                      ((uint32_t)(d[4] & 0x1f) << 12);
            if (tmp_u32 != 0x1FFFF) {
                StandardMetrics.ms_v_env_parktime->SetValue(tmp_u32);
                ESP_LOGV(TAG, "0x06B7 parktime=%u", tmp_u32);
            }

            tmp_u8 = ((uint8_t)(d[7] & 0xff) << 0) |
                     0;  // outerTemp Faktor 0.5 Offset -50, Minimum -50, Maximum 75 [°C] Initial 77
            tmp_u8 = (uint8_t)tmp_u8;
            tmp_f32 = ((float)tmp_u8) * 0.5F - 50.0F;
            StandardMetrics.ms_v_env_temp->SetValue(tmp_f32);  // working
            ESP_LOGV(TAG, "0x06B7 outside=%.1f°C", tmp_f32);
            break;
        }
        case 0x391:  // OBD_01: drivetrain READY status
        {
            // d[7] bit 5 is OBD_Driving_Cycle: it goes high only once the drivetrain is fully
            // up and the car is ready to drive; it stays clear during charging, remote climate
            // and while the ignition is merely on but not yet READY. The frame keeps
            // broadcasting after the ignition goes off and the bit stays latched high for
            // several seconds into the power-down, so it is combined with KL_15 for v.e.on
            // rather than used on its own. if only ignition is turned on again this bit is cleared.
            // (d[5] carries the accelerator pedal position, OBD_Abs_Pedal_Pos - not mapped.)
            m_drivetrain_ready = (d[7] & 0x20) != 0;
            StandardMetrics.ms_v_env_on->SetValue(m_kl15_on && m_drivetrain_ready);
            ESP_LOGV(TAG, "0x391 READY=%u", m_drivetrain_ready);
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
            // KL_15 (terminal 15 = ignition) means the car is awake and switched on by the
            // user; drivable (v.e.on) additionally requires the drivetrain to report READY.
            m_kl15_on = (d[2] & 0x02) != 0;
            StandardMetrics.ms_v_env_awake->SetValue(m_kl15_on);
            StandardMetrics.ms_v_env_on->SetValue(m_kl15_on && m_drivetrain_ready);
            ESP_LOGV(TAG, "0x3C0 KL_15=%u KL_S=%u", m_kl15_on, d[2] & 0x01);
            break;
        }
        case 0x17332510: {
            // BatteryControl (BCU, node 0x25) BAP status stream, extended 29-bit frame.
            // The climate controller reassembles it and reads the authoritative HVAC state
            // and command confirmation from the OperationMode echo ("49 58 <flag>").
            m_batctrl.IncomingBapStatus(p_frame);
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

    // When the bus goes silent the car is asleep: clear the awake / drivable state as a
    // backstop in case the terminal frames stopped before signalling the off transition.
    if (!bus_alive) {
        m_kl15_on = false;
        m_drivetrain_ready = false;
        StandardMetrics.ms_v_env_awake->SetValue(false);
        StandardMetrics.ms_v_env_on->SetValue(false);
        // Regen level (xvg.v.recup) is a driving concept — clear it to N/A once the
        // car is off/asleep so it doesn't linger on the last D/B value (the gear
        // frame 0x187 stops broadcasting when the car sleeps).
        m_recup_level->SetValue(-1);
    }

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

    // Drive the climate controller's wake + retry-until-confirmed state machine. It is
    // self-contained (spare-node NM wake on KCAN, independent of the OCU heartbeat above)
    // and owns ms_v_env_hvac, including clearing it when the bus sleeps.
    m_batctrl.Ticker1(bus_alive);
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
    // long enough for warm-bus commands. The OEM 0x67 cadence is ~1.3 s (observed on-car),
    // which Ticker1's 1 Hz tick matches.
    uint8_t data[8] = {0x67, 0x10, 0x41, 0x84, 0x14, 0x00, 0x00, 0x00};
    m_can3->WriteExtended(0x1B000067, 8, data);
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandClimateControl(bool enable) {
    // Delegated to the self-contained BatteryControl controller (vehicle_vwegolf_bat_ctrl.cpp):
    // spare-node NM wake + BAP command over KCAN, independent of the OCU 0x5A7 heartbeat.
    return m_batctrl.Climate(enable) ? Success : Fail;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandSetChargeCurrent(uint16_t limit) {
    // Persistent charge-current-limit edit: RMW profile 0's maxCurrent (snapped to an allowed BCU
    // step). This is a settings change the car honors on its next charge — it does NOT start a
    // charge. Shares the BatteryControl command path with climate (one command in flight at a time).
    return m_batctrl.SetChargeCurrent(limit) ? Success : Fail;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandStartCharge() {
    // Arm profile 0 for charge + immediate OperationMode trigger. Validated on-car (2020 e-Golf).
    // The immediate charge trigger has no factory reference — the factory MIB only edits departure
    // timers — so this drives Function 0x18 directly.
    return m_batctrl.Charge(true) ? Success : Fail;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandStopCharge() {
    // Op-specific stop (see VWeGolfBatteryControl::Charge): OFF arms the pure-charge op first, so it
    // is refused while climate is on (that arm would kill climate — stop climate first).
    return m_batctrl.Charge(false) ? Success : Fail;
}

bool OvmsVehicleVWeGolf::PollBatCtrlResult(OvmsWriter* writer) {
    // Block the command task (bounded) while the BatteryControl controller wakes the BCU and completes.
    // Poll a fixed number of iterations (independent of tick advancement, so it always terminates); the
    // controller declares failure at its wake-window cap, so wait a little past it.
    const int poll_ms = 250;
    const int max_iters = ((VWEGOLF_BATCTRL_WAKE_SECS + 2) * 1000) / poll_ms;
    VWeGolfBatteryControl::ListState st = VWeGolfBatteryControl::LIST_PENDING;
    for (int i = 0; i < max_iters; i++) {
        st = m_batctrl.ListStatus();
        if (st == VWeGolfBatteryControl::LIST_READY || st == VWeGolfBatteryControl::LIST_FAILED)
            break;
        vTaskDelay(pdMS_TO_TICKS(poll_ms));
    }
    if (st == VWeGolfBatteryControl::LIST_READY) return true;
    writer->puts(st == VWeGolfBatteryControl::LIST_FAILED
                     ? "The vehicle did not respond (no data / not confirmed)."
                     : "Timed out waiting for the vehicle.");
    return false;
}

void OvmsVehicleVWeGolf::CommandListProfiles(OvmsWriter* writer) {
    // Kick a read-only profile fetch and block (bounded) for the result. The controller wakes the
    // BCU, handshakes, GETs the profile array, and publishes it — NO profile is written.
    if (!m_batctrl.ListProfiles()) {
        writer->puts("Charge-profile read unavailable — another command is in progress. Try again.");
        return;
    }
    writer->puts("Reading charge profiles from the vehicle (waking it if asleep)...");
    if (!PollBatCtrlResult(writer)) return;

    // Profiles are a variable-length array — read up to kMaxProfiles (the reassembler's practical cap).
    bap::egolf::Profile profs[VWeGolfBatteryControl::kMaxProfiles];
    uint8_t n = m_batctrl.GetProfiles(profs, VWeGolfBatteryControl::kMaxProfiles);
    writer->printf("Charge profiles (%u):\n", (unsigned)n);
    writer->printf("  #  %-16s  Op    Flags       MaxA  MinSoC  TgtSoC  Temp\n", "Name");
    for (uint8_t i = 0; i < n; i++) {
        const bap::egolf::Profile& p = profs[i];
        // Operation flags — the WIRE-confirmed bits: charge / climate / climatise-on-battery.
        char flags[16];
        snprintf(flags, sizeof(flags), "%s%s%s",
                 (p.operation & bap::egolf::PO_CHARGING)      ? "chg " : "",
                 (p.operation & bap::egolf::PO_CLIMATE)       ? "cli " : "",
                 (p.operation & bap::egolf::PO_ALLOW_BATTERY) ? "bat"  : "");
        if (flags[0] == '\0') { flags[0] = '-'; flags[1] = '\0'; }

        // A 0 byte = "not set for this profile". minChargeLevel (MinSoC) is meaningful on the global
        // profile 0; targetChargeLevel (TgtSoC) on the charge locations 1-3 — the raw bytes reproduce
        // that split, so both columns are shown and the inapplicable one reads '-'.
        char maxc[8], minc[8], tgtc[8], temp[10];
        if (p.maxCurrent)        snprintf(maxc, sizeof(maxc), "%uA",  p.maxCurrent);        else snprintf(maxc, sizeof(maxc), "-");
        if (p.minChargeLevel)    snprintf(minc, sizeof(minc), "%u%%", p.minChargeLevel);    else snprintf(minc, sizeof(minc), "-");
        if (p.targetChargeLevel) snprintf(tgtc, sizeof(tgtc), "%u%%", p.targetChargeLevel); else snprintf(tgtc, sizeof(tgtc), "-");
        // Temperature is valid only in the car's setpoint range (raw 0x37..0xC8 = 15.5..30.0 C).
        if (p.temperatureRaw >= 0x37 && p.temperatureRaw <= 0xC8)
            snprintf(temp, sizeof(temp), "%.1fC", bap::egolf::rawToTemp(p.temperatureRaw));
        else snprintf(temp, sizeof(temp), "-");

        writer->printf("  %u  %-16s  0x%02x  %-10s  %-4s  %-6s  %-6s  %s\n",
                       (unsigned)p.position, p.name[0] ? p.name : "(unnamed)", p.operation, flags,
                       maxc, minc, tgtc, temp);
    }
}

// ---------------------------------------------------------------------------
// Departure-timer CLI (xvg charge timer ...)
// ---------------------------------------------------------------------------

// Case-insensitive whole-string compare (small, avoids depending on strcasecmp availability).
static bool vwe_ciequal(const char* a, const char* b) {
    for (;; a++, b++) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return false;
        if (ca == '\0') return true;
    }
}

// Weekday token ("mon".."sun", any case, >=3 letters) -> its egolf::Weekday bit, or 0.
static uint8_t vwe_day_bit(const char* t) {
    static const struct { const char* n; uint8_t b; } days[] = {
        {"mon", bap::egolf::WD_MON}, {"tue", bap::egolf::WD_TUE}, {"wed", bap::egolf::WD_WED},
        {"thu", bap::egolf::WD_THU}, {"fri", bap::egolf::WD_FRI}, {"sat", bap::egolf::WD_SAT},
        {"sun", bap::egolf::WD_SUN},
    };
    for (auto& d : days) {
        bool m = true;
        for (int i = 0; i < 3; i++) {
            char c = t[i]; if (c >= 'A' && c <= 'Z') c += 32;
            if (c != d.n[i]) { m = false; break; }
        }
        if (m) return d.b;
    }
    return 0;
}

// Parse a recurring-days spec into a weekday mask (bit0 one-shot stays clear). Accepts keywords
// (daily / mon-fri / weekends) or a comma list (mon,tue,fri). Returns false if no day was recognized.
static bool vwe_parse_weekdays(const char* s, uint8_t& mask) {
    mask = 0;
    static const struct { const char* kw; uint8_t m; } kws[] = {
        {"daily", 0xFE}, {"all", 0xFE}, {"everyday", 0xFE},
        {"weekdays", 0x3E}, {"mon-fri", 0x3E}, {"mo-fr", 0x3E},
        {"weekends", 0xC0}, {"weekend", 0xC0},
    };
    for (auto& k : kws) if (vwe_ciequal(s, k.kw)) { mask = k.m; return true; }
    char buf[64];
    size_t j = 0;
    for (size_t i = 0; s[i] && j + 1 < sizeof(buf); i++) buf[j++] = s[i];
    buf[j] = '\0';
    for (char* tok = buf; tok && *tok;) {
        char* comma = strchr(tok, ',');
        if (comma) *comma = '\0';
        mask |= vwe_day_bit(tok);
        tok = comma ? comma + 1 : nullptr;
    }
    return mask != 0;
}

// Weekday mask -> human string (into out). One-shot shows "once"; recurring shows daily / Mon-Fri /
// Sat,Sun / an explicit day list.
static void vwe_weekdays_str(uint8_t mask, char* out, size_t cap) {
    if (mask & bap::egolf::WD_ONESHOT) { snprintf(out, cap, "once"); return; }
    uint8_t days = mask & 0xFE;
    if (days == 0xFE) { snprintf(out, cap, "daily");   return; }
    if (days == 0x3E) { snprintf(out, cap, "Mon-Fri"); return; }
    if (days == 0xC0) { snprintf(out, cap, "Sat,Sun"); return; }
    static const char* names[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    size_t len = 0; out[0] = '\0';
    for (int i = 0; i < 7 && len + 5 < cap; i++)
        if (days & (1u << (i + 1))) len += snprintf(out + len, cap - len, "%s%s", len ? "," : "", names[i]);
    if (len == 0) snprintf(out, cap, "(none)");
}

// Parse "HH:MM" (24h). Returns false if malformed or out of range.
static bool vwe_parse_hhmm(const char* s, uint8_t& h, uint8_t& m) {
    int hh = -1, mm = -1;
    if (sscanf(s, "%d:%d", &hh, &mm) != 2) return false;
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return false;
    h = (uint8_t)hh; m = (uint8_t)mm; return true;
}

// Parse a Celsius temperature ("21" or "21.5") into deci-degrees (tenths). Returns false if malformed
// or outside the car's setpoint range 15.5-30.0 C. Runs on the command task (FP is fine there).
static bool vwe_parse_temp_deci(const char* s, int& deci) {
    float c = 0;
    if (sscanf(s, "%f", &c) != 1) return false;
    int d = (int)(c * 10.0f + 0.5f);         // round to the nearest 0.1 C
    if (d < 155 || d > 300) return false;    // 15.5..30.0 C (the BCU setpoint range)
    deci = d;
    return true;
}

// Parse a charge-location flags value ("charge" | "climate" | "both", or a list like "charge,climate")
// into PO_CHARGING / PO_CLIMATE bits. Returns false if nothing recognized.
static bool vwe_parse_op_flags(const char* s, uint8_t& op) {
    op = 0;
    if (vwe_ciequal(s, "both")) { op = bap::egolf::PO_CHARGING | bap::egolf::PO_CLIMATE; return true; }
    char buf[48];
    size_t j = 0;
    for (size_t i = 0; s[i] && j + 1 < sizeof(buf); i++) buf[j++] = s[i];
    buf[j] = '\0';
    for (char* tok = buf; tok && *tok;) {
        char* comma = strchr(tok, ',');
        if (comma) *comma = '\0';
        if (vwe_ciequal(tok, "charge") || vwe_ciequal(tok, "chg")) op |= bap::egolf::PO_CHARGING;
        else if (vwe_ciequal(tok, "climate") || vwe_ciequal(tok, "cli")) op |= bap::egolf::PO_CLIMATE;
        else return false;  // an unrecognized token invalidates the whole value
        tok = comma ? comma + 1 : nullptr;
    }
    return op != 0;
}

void OvmsVehicleVWeGolf::CommandSetProfile(OvmsWriter* writer, int argc, const char* const* argv) {
    // xvg charge profile set <profile 0-3> <field value>...   (read-modify-write, no charge/climate trigger)
    // The valid fields differ per profile (see the per-profile guards below): profile 0 "Optionen" =
    // current / minsoc / temp; charge locations 1-3 = flags / current / soc.
    if (argc < 3 || (argc - 1) % 2 != 0) {
        writer->puts("Usage: xvg charge profile set <profile 0-3> <field value>...");
        writer->puts("  profile 0 (Optionen):     current <A> | minsoc <%> | temp <C>");
        writer->puts("  profiles 1-3 (locations): flags <charge|climate|both> | current <A> | soc <%>");
        writer->puts("  e.g. 'set 1 soc 80 current 13'   'set 0 minsoc 20 temp 21.0'   'set 2 flags both'");
        return;
    }
    int pos = atoi(argv[0]);
    if (pos < 0 || pos > 3) { writer->puts("Profile must be 0-3 (see 'xvg charge profile list')."); return; }
    const bool isGlobal = (pos == 0);

    VWeGolfBatteryControl::ProfileEdit edit;
    using PE = VWeGolfBatteryControl::ProfileEdit;
    for (int i = 1; i + 1 < argc; i += 2) {
        const char* f = argv[i];
        const char* v = argv[i + 1];
        if (vwe_ciequal(f, "current") || vwe_ciequal(f, "cur") || vwe_ciequal(f, "amps")) {
            int a = atoi(v);
            if (a <= 0) { writer->puts("current: expected a positive amp value."); return; }
            uint8_t snapped = bap::egolf::clampMaxCurrent((uint16_t)a);  // snap to 5/10/13/32, cap 0x20
            edit.maxCurrent = snapped;
            edit.fields |= PE::F_CURRENT;
            if ((int)snapped != a)
                writer->printf("Note: current %dA snapped to %uA (allowed: 5/10/13/32).\n", a, snapped);
        } else if (vwe_ciequal(f, "minsoc") || vwe_ciequal(f, "min")) {
            if (!isGlobal) { writer->puts("minsoc applies only to profile 0 (Optionen)."); return; }
            int s = atoi(v);
            if (s < 0 || s > 100) { writer->puts("minsoc: 0-100 (%)."); return; }
            edit.minChargeLevel = (uint8_t)s;
            edit.fields |= PE::F_MINSOC;
        } else if (vwe_ciequal(f, "soc") || vwe_ciequal(f, "target") || vwe_ciequal(f, "tgtsoc")) {
            if (isGlobal) {
                writer->puts("soc (target) applies only to charge locations 1-3; profile 0 uses minsoc.");
                return;
            }
            int s = atoi(v);
            if (s < 0 || s > 100) { writer->puts("soc: 0-100 (%)."); return; }
            edit.targetChargeLevel = (uint8_t)s;
            edit.fields |= PE::F_TGTSOC;
        } else if (vwe_ciequal(f, "temp") || vwe_ciequal(f, "temperature")) {
            if (!isGlobal) { writer->puts("temp applies only to profile 0 (Optionen)."); return; }
            int deci;
            if (!vwe_parse_temp_deci(v, deci)) { writer->puts("temp: 15.5-30.0 (Celsius, 0.5 steps)."); return; }
            edit.temperatureRaw = bap::egolf::tempToRawDeci(deci);
            edit.fields |= PE::F_TEMP;
        } else if (vwe_ciequal(f, "flags") || vwe_ciequal(f, "op") || vwe_ciequal(f, "mode")) {
            if (isGlobal) {
                writer->puts("flags apply only to charge locations 1-3 "
                             "(profile 0's operation is managed by climate/charge control).");
                return;
            }
            uint8_t op;
            if (!vwe_parse_op_flags(v, op)) {
                writer->puts("flags: charge | climate | both (or a list like charge,climate).");
                return;
            }
            edit.operation = op;
            edit.fields |= PE::F_OP;
        } else {
            writer->printf("Unknown field '%s'.\n", f);
            writer->puts(isGlobal ? "Profile 0 fields: current, minsoc, temp."
                                  : "Location fields: flags, current, soc.");
            return;
        }
    }
    if (edit.fields == 0) { writer->puts("No fields to set."); return; }

    if (!m_batctrl.SetProfile((uint8_t)pos, edit)) {
        writer->puts("Profile write unavailable — another command is in progress. Try again.");
        return;
    }
    writer->printf("Writing profile %d (waking vehicle)...\n", pos);
    if (!PollBatCtrlResult(writer)) return;
    writer->printf("Profile %d updated.\n", pos);
}

void OvmsVehicleVWeGolf::CommandListTimers(OvmsWriter* writer) {
    if (!m_batctrl.ListTimers()) {
        writer->puts("Timer read unavailable — another command is in progress. Try again.");
        return;
    }
    writer->puts("Reading departure timers from the vehicle (waking it if asleep)...");
    if (!PollBatCtrlResult(writer)) return;

    bap::egolf::Timer timers[VWeGolfBatteryControl::kNumTimers];
    bool seen[VWeGolfBatteryControl::kNumTimers];
    uint8_t mask = 0;
    uint8_t n = m_batctrl.GetTimers(timers, VWeGolfBatteryControl::kNumTimers, mask, seen);
    // If the enable mask never broadcast this read, its value is the reset default (0) — show "?" for
    // the En column rather than a misleading "off".
    bool mask_seen = m_batctrl.TimerMaskSeen();

    writer->puts("Departure timers:");
    writer->printf("  #  En   Time   Days                Profile\n");
    for (uint8_t i = 0; i < n; i++) {
        const char* en = !mask_seen ? "?" : ((mask & (1u << i)) ? "on" : "off");
        if (!seen[i]) {  // enable state may be known, but this slot broadcast no record
            writer->printf("  %u  %-3s  (no record read)\n", i + 1, en);
            continue;
        }
        const bap::egolf::Timer& t = timers[i];
        char days[28];
        vwe_weekdays_str(t.weekdays, days, sizeof(days));
        if (t.oneShot() && t.hasDate()) {  // one-shot carries a fire date -> append it
            size_t l = strlen(days);
            snprintf(days + l, sizeof(days) - l, " %02u.%02u.20%02u", t.day, t.month, t.year);
        }
        writer->printf("  %u  %-3s  %02u:%02u  %-18s  %u\n", i + 1, en, t.hour, t.minute, days, t.refId);
    }
    writer->puts("Profile = the bound charge location (see 'xvg charge profile list'); it supplies the");
    writer->puts("target SoC / current / temperature the timer charges or climatises to.");
}

void OvmsVehicleVWeGolf::CommandSetTimer(OvmsWriter* writer, int argc, const char* const* argv) {
    // xvg charge timer set <slot 1-3> <HH:MM> <days> <profile 0-3>  (recurring only)
    if (argc < 4) {
        writer->puts("Usage: xvg charge timer set <slot 1-3> <HH:MM> <days> <profile 0-3>");
        writer->puts("  days:    daily | mon-fri | weekends | a list e.g. mon,tue,fri");
        writer->puts("  profile: a charge location from 'xvg charge profile list' — it sets the target");
        writer->puts("           SoC/current/temp. NB profile 0 (Optionen) has no SoC target = 100%.");
        return;
    }
    int slot = atoi(argv[0]);
    if (slot < 1 || slot > VWeGolfBatteryControl::kNumTimers) { writer->puts("Slot must be 1-3."); return; }
    uint8_t h, m;
    if (!vwe_parse_hhmm(argv[1], h, m)) { writer->puts("Time must be HH:MM (24-hour)."); return; }
    uint8_t days;
    if (!vwe_parse_weekdays(argv[2], days)) {
        writer->puts("Days must be: daily, mon-fri, weekends, or a list like mon,tue,fri.");
        return;
    }
    // Profile (refId) is REQUIRED — a timer has NO SoC target of its own; it inherits target SoC /
    // current / temperature from the bound charge location. Defaulting to profile 0 would silently
    // mean charge-to-100%, so make the user choose. Accepts "<n>" or "profile <n>".
    const char* p = argv[3];
    if (vwe_ciequal(p, "profile")) {
        if (argc < 5) { writer->puts("Missing profile number after 'profile'."); return; }
        p = argv[4];
    }
    int r = atoi(p);
    if (r < 0 || r > 3) { writer->puts("Profile must be 0-3 (see 'xvg charge profile list')."); return; }
    uint8_t refId = (uint8_t)r;

    bap::egolf::Timer t;  // recurring: date bytes stay 0xFF (no fixed date); the car fires by weekday
    t.hour = h; t.minute = m; t.weekdays = days; t.refId = refId;
    if (!m_batctrl.SetTimer((uint8_t)slot, t)) {
        writer->puts("Timer write unavailable — another command is in progress. Try again.");
        return;
    }
    char dbuf[28];
    vwe_weekdays_str(days, dbuf, sizeof(dbuf));
    writer->printf("Writing timer %d: %02u:%02u %s, profile %u (waking vehicle)...\n",
                   slot, h, m, dbuf, refId);
    if (!PollBatCtrlResult(writer)) return;
    writer->printf("Timer %d set and enabled.\n", slot);
}

void OvmsVehicleVWeGolf::CommandTimerEnable(OvmsWriter* writer, int argc, const char* const* argv,
                                            bool enable) {
    if (argc < 1) {
        writer->printf("Usage: xvg charge timer %s <slot 1-3>\n", enable ? "enable" : "disable");
        return;
    }
    int slot = atoi(argv[0]);
    if (slot < 1 || slot > VWeGolfBatteryControl::kNumTimers) { writer->puts("Slot must be 1-3."); return; }
    if (!m_batctrl.EnableTimer((uint8_t)slot, enable)) {
        writer->puts("Timer change unavailable — another command is in progress. Try again.");
        return;
    }
    writer->printf("%s timer %d (waking vehicle)...\n", enable ? "Enabling" : "Disabling", slot);
    if (!PollBatCtrlResult(writer)) return;
    writer->printf("Timer %d %s.\n", slot, enable ? "enabled" : "disabled");
}

void OvmsVehicleVWeGolf::CommandClearTimer(OvmsWriter* writer, int argc, const char* const* argv) {
    if (argc < 1) { writer->puts("Usage: xvg charge timer clear <slot 1-3>"); return; }
    int slot = atoi(argv[0]);
    if (slot < 1 || slot > VWeGolfBatteryControl::kNumTimers) { writer->puts("Slot must be 1-3."); return; }
    if (!m_batctrl.ClearTimer((uint8_t)slot)) {
        writer->puts("Timer clear unavailable — another command is in progress. Try again.");
        return;
    }
    writer->printf("Clearing timer %d (waking vehicle)...\n", slot);
    if (!PollBatCtrlResult(writer)) return;
    writer->printf("Timer %d cleared (disabled).\n", slot);
}
