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

    // Frames 1+2: SetGet on ProfilesArray (LSG 0x25 function 0x19) — compact
    // (RecordAddr 6) partial update of profile 0: enable climate + climate-on-battery.
    // Field semantics per smartkar-cano-new BAP_BATTERY_CONTROL.md and MIB2 firmware RE
    // (PR #1430 review); see clima-control-bap.md.
    data[0] = 0x80;           // long BAP message start, group 0
    data[1] = 0x08;           // payload length: 4-byte array header + 4-byte compact record
    data[2] = 0x29;           // BAP header: OpCode 0x02 SetGet, LSG 0x25 [5:2]
    data[3] = 0x59;           // LSG 0x25 [1:0], function 0x19 ProfilesArray
    data[4] = m_bap_counter;  // array header [ASG-ID:4|Transaction-ID:4]; FSG Status
                              // response echoes it (observed as our value | 0x80)
    data[5] = 0x06;           // array header: RecordAddr = 6 (compact record format)
    data[6] = 0x00;           // array header: startIndex
    data[7] = 0x01;           // array header: elementCount = 1
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

    // Frame 2: continuation — the 4-byte compact profile record. No temperature here:
    // the car climatizes to the setpoint stored in its global profile (infotainment).
    // An explicit setpoint would need a RecordAddr-0 profile write, not yet implemented.
    data[0] = 0xC0;  // long BAP continuation, group 0, index 0
    data[1] = 0x06;  // operation: climate | climateWithoutExternalSupply
    data[2] = 0x00;  // operation2: none
    data[3] = 0x20;  // maxCurrent = 32 A (1 A/LSB)
    data[4] = 0x00;  // targetChargeLevel = 0 (not charging)
    esp_err_t ok2 = m_can3->WriteExtended(0x17332501, 5, data, pdMS_TO_TICKS(200));
    if (ok2 == ESP_FAIL) {
        ESP_LOGW(TAG, "BAP clima frame 2 TX queue overflow");
        m_bap_burst_active = false;
        m_ocu_active = false;
        return Fail;
    }

    // Frame 3: short BAP trigger — SetGet on function 0x18 (ClimateOperationMode).
    // Payload: profileId 0 (global), then start bitmask (bit0 = immediately) / 0x00 stop.
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

    ESP_LOGI(TAG, "BAP clima %s sent: tid=0x%02X", enable ? "start" : "stop", m_bap_counter);

    // Optimistic update for responsive UX — reflect the command immediately, then let
    // 0x03B5 ClimaRunning confirm/sustain it.
    //   start: set true and give the blower the full hold window to spin up; clear any
    //          stop-suppression.
    //   stop:  set false now; expire the hold so it can't keep the metric true, and start
    //          the spin-down suppression so the blower's trailing 0x03B5=running doesn't
    //          flick it back on (until the suppress window lapses — see case 0x03B5).
    StandardMetrics.ms_v_env_hvac->SetValue(enable);
    if (enable) {
        m_clima_run_secs = 0;
        m_hvac_stop_secs = 255;
    } else {
        m_clima_run_secs = 255;
        m_hvac_stop_secs = 0;
    }

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

    // A deferred burst from a previous command is still in flight (woken, waiting on the
    // settle window, or mid-send — see m_clima_pending). Re-target it and return rather
    // than starting a second burst: the bus is awake by now, so this command would take
    // the warm-bus path below and race the pending Ticker1 burst, interleaving two BAP
    // multi-frame sequences at the ECU. Ticker1 fires once with the latest intent.
    if (m_clima_pending) {
        m_clima_pending_enable = enable;
        ESP_LOGI(TAG, "Climate control: deferred burst pending — retargeted to %s",
                 enable ? "start" : "stop");
        return Success;
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
