/*
;    Project:       Open Vehicle Monitor System
;    Subproject:    Integrate VW e-Golf — BatteryControl: remote climate + charge
;
;    (C) 2026  Jona Wagner <jona@jonawagner.me>
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

#include "vehicle_vwegolf_bat_ctrl.h"

#include <cstdio>  // snprintf — compose the per-operation failure notification

#include "metrics_standard.h"
#include "ovms_log.h"
#include "ovms_notify.h"  // MyNotify — user notification for the climate outcome

#undef TAG
#define TAG "v-vwegolf"

// BC_TIMER_LIST: seconds to collect the BCU's periodic timer-table broadcast before publishing. The
// records arrive as a HEARTBEAT burst (one slot/sec) that REPEATS only every ~11-13 s (measured), so
// the window must span more than one broadcast period — 5 s missed the burst entirely (mask seen, no
// records). Early-completion (all slots + mask) still finishes a lucky wake in a few seconds.
static constexpr uint8_t kTimerCollectSecs = 15;

// ---------------------------------------------------------------------------
// Command entry
// ---------------------------------------------------------------------------

bool VWeGolfBatteryControl::Climate(bool enable) { return Begin(BC_CLIMATE, enable, 0); }

bool VWeGolfBatteryControl::Charge(bool enable) {
    // Charge STOP is op-specific: it works only with profile 0 armed to charge (0x01), which clears
    // the climate bits — so stopping charge while climate is running would disrupt climate. Block it
    // (stop climate first). Climate stop is unaffected and stays allowed.
    if (!enable && StandardMetrics.ms_v_env_hvac->AsBool()) {
        ESP_LOGW(TAG, "Charge STOP blocked: climate is on (would disrupt it — stop climate first)");
        MyNotify.NotifyString("alert", "xvg.charge",
                              "Can't stop charging while climate is on — stop climate first");
        return false;
    }
    return Begin(BC_CHARGE, enable, 0);
}

bool VWeGolfBatteryControl::SetChargeCurrent(uint16_t amps) {
    // Snap to an allowed BCU step {5,10,13,32} — the car turns any out-of-set value into 10 A (it
    // echoes the requested value but charges at 10 A), so snapping to the nearest valid step is what
    // the user actually gets. clampMaxCurrent also enforces the 0x20 hard cap (again capped in
    // SendArm) — a maxCurrent above 0x20 bricks the car's charging until a factory reset.
    return Begin(BC_SET_CURRENT, /*enable=*/true, bap::egolf::clampMaxCurrent(amps));
}

bool VWeGolfBatteryControl::ListProfiles() {
    // Read-only profile-array fetch (BC_LIST): wake -> handshake -> GET -> capture ALL profiles, then
    // stop. No arm, no trigger, no profile write of any kind. Refused while another BatteryControl
    // command is in flight (one at a time; see BcOp) so it can't stomp an active climate/charge run.
    if (m_phase != CP_IDLE) {
        ESP_LOGW(TAG, "ListProfiles refused: a BatteryControl command is already in flight");
        return false;
    }
    return Begin(BC_LIST, /*enable=*/false, 0);
}

uint8_t VWeGolfBatteryControl::GetProfiles(bap::egolf::Profile* out, uint8_t maxOut) const {
    uint8_t n = m_profile_count < maxOut ? m_profile_count : maxOut;
    for (uint8_t i = 0; i < n; i++) out[i] = m_profiles[i];
    return n;
}

bool VWeGolfBatteryControl::SetProfile(uint8_t pos, const ProfileEdit& edit) {
    // RMW one profile: wake -> handshake -> GET the array -> overwrite ONLY the flagged fields of the
    // profile at `pos`, keeping every other byte + the name -> write it back. NO trigger (settings edit).
    if (edit.fields == 0) return false;                 // nothing to write
    if (pos >= kMaxProfiles) return false;              // absurd position (CLI already bounds 0..3)
    if (m_phase != CP_IDLE) {
        ESP_LOGW(TAG, "SetProfile refused: a BatteryControl command is already in flight");
        return false;
    }
    m_prof_set_pos = pos;
    m_prof_edit    = edit;
    return Begin(BC_PROFILE_SET, /*enable=*/false, 0);
}

// ---------------------------------------------------------------------------
// Departure timers — command entry points
// ---------------------------------------------------------------------------

bool VWeGolfBatteryControl::ListTimers() {
    if (m_phase != CP_IDLE) {
        ESP_LOGW(TAG, "ListTimers refused: a BatteryControl command is already in flight");
        return false;
    }
    return Begin(BC_TIMER_LIST, /*enable=*/false, 0);
}

bool VWeGolfBatteryControl::SetTimer(uint8_t slot, const bap::egolf::Timer& t) {
    if (slot < 1 || slot > kNumTimers) return false;
    if (m_phase != CP_IDLE) {
        ESP_LOGW(TAG, "SetTimer refused: a BatteryControl command is already in flight");
        return false;
    }
    m_timer_slot         = slot;
    m_timer_write        = t;
    m_timer_write_record = true;   // write the record...
    m_timer_enable       = true;   // ...and enable the slot
    return Begin(BC_TIMER_SET, /*enable=*/true, 0);
}

bool VWeGolfBatteryControl::EnableTimer(uint8_t slot, bool enable) {
    if (slot < 1 || slot > kNumTimers) return false;
    if (m_phase != CP_IDLE) {
        ESP_LOGW(TAG, "EnableTimer refused: a BatteryControl command is already in flight");
        return false;
    }
    m_timer_slot         = slot;
    m_timer_write_record = false;  // mask-only RMW, no record write
    m_timer_enable       = enable;
    return Begin(BC_TIMER_ENABLE, enable, 0);
}

bool VWeGolfBatteryControl::ClearTimer(uint8_t slot) {
    if (slot < 1 || slot > kNumTimers) return false;
    if (m_phase != CP_IDLE) {
        ESP_LOGW(TAG, "ClearTimer refused: a BatteryControl command is already in flight");
        return false;
    }
    m_timer_slot         = slot;
    m_timer_write        = bap::egolf::Timer();  // inert recurring record (no weekdays -> never fires)
    m_timer_write_record = true;
    m_timer_enable       = false;                // and disable the slot
    return Begin(BC_TIMER_SET, /*enable=*/false, 0);
}

uint8_t VWeGolfBatteryControl::GetTimers(bap::egolf::Timer* out, uint8_t maxOut,
                                         uint8_t& enabledMask, bool* seen) const {
    uint8_t n = kNumTimers < maxOut ? kNumTimers : maxOut;
    for (uint8_t i = 0; i < n; i++) {
        out[i] = m_timers[i];
        if (seen) seen[i] = m_timer_seen[i];
    }
    enabledMask = m_timer_mask;
    return n;
}

bool VWeGolfBatteryControl::Begin(BcOp op, bool enable, uint16_t param) {
    if (op == BC_SET_CURRENT)
        ESP_LOGI(TAG, "BatteryControl SetChargeCurrent -> %u A", (unsigned)param);
    else if (op == BC_PROFILE_SET)
        ESP_LOGI(TAG, "BatteryControl SetProfile pos %u (fields 0x%02x)", m_prof_set_pos,
                 m_prof_edit.fields);
    else if (op == BC_TIMER_LIST)
        ESP_LOGI(TAG, "BatteryControl ListTimers");
    else if (op == BC_TIMER_SET)
        ESP_LOGI(TAG, "BatteryControl SetTimer slot %u", m_timer_slot);
    else if (op == BC_TIMER_ENABLE)
        ESP_LOGI(TAG, "BatteryControl %s timer %u", enable ? "Enable" : "Disable", m_timer_slot);
    else
        ESP_LOGI(TAG, "BatteryControl %s %s", op == BC_CHARGE ? "Charge" : "Climate",
                 enable ? "ON" : "OFF");
    // Non-colliding path (independent of the OCU 0x5A7 heartbeat): assert a spare-node NM wake
    // to bring up the comfort/EV cluster; the actual BAP command runs event-driven from
    // IncomingBapStatus as the BCU responds. The wake is held only as a short bridge in Ticker1
    // — once the BCU accepts the command it and the cluster sustain their own NM. No OCU
    // impersonation (which risks the OCU-collision DTCs U001100/U120100 seen on this car).
    m_op            = op;
    m_param         = (uint8_t)param;
    m_enable        = enable;
    // Auto-apply: a SetChargeCurrent issued WHILE a charge is running must re-fire the start after
    // the profile write, or the running charge keeps its old current (it ignores later profile edits).
    // When no charge is running, stay write-only (don't kick off a charge from a settings change).
    m_apply         = (op == BC_SET_CURRENT) && StandardMetrics.ms_v_charge_inprogress->AsBool();
    m_confirmed     = false;
    m_error         = false;
    m_tx_fail       = false;
    m_have_profile0 = false;
    m_arm_skipped   = false;
    m_bcu_seen      = false;
    m_list_state    = isCliOp() ? LIST_PENDING : LIST_NONE;  // CLI read/write ops publish PENDING to poll
    m_profile_count = 0;
    // Timer collection state (the pending write fields m_timer_slot/write/enable are set by the caller
    // BEFORE Begin and must NOT be reset here).
    // The timer cache (m_timers / m_timer_mask / m_timer_seen) PERSISTS across commands and sleep — it
    // is filled continuously by UpdateTimerCache from ambient broadcasts. Only reset command progress.
    m_timer_target_mask = 0;
    m_collect_secs = 0;
    m_phase         = CP_WAKE;
    m_phase_secs    = 0;
    m_wake_hold     = VWEGOLF_BATCTRL_WAKE_SECS;
    SendNmWake();  // kick the wake now; Ticker1 sustains it; the BCU's first reply kicks off the BAP
    return true;
}

// ---------------------------------------------------------------------------
// Per-second tick: NM-wake bridge + lost-reply backstop (NOT the primary driver)
// ---------------------------------------------------------------------------

void VWeGolfBatteryControl::Ticker1(bool bus_alive) {
    // The clima ECU (0x5EA) and the BCU (0x17332510) go silent once the bus sleeps and so
    // can't refresh HVAC; clear it as a backstop so climate doesn't read "on" forever after
    // conditioning ends and the car sleeps.
    if (!bus_alive) {
        StandardMetrics.ms_v_env_hvac->SetValue(false);
    }

    if (m_phase == CP_IDLE || m_wake_hold == 0) {
        return;
    }

    SendNmWake();  // ~1 Hz, within the AUTOSAR NM timeout, to wake and hold the cluster

    m_phase_secs++;
#if VWEGOLF_BATCTRL_RETRY_ENABLED
    // Lost-reply backstop: the sequence is normally advanced by the BCU's replies
    // (IncomingBapStatus). Only if a reply never arrives (a dropped frame) is the current step
    // re-sent. This is NOT a blind per-tick resend — once confirmed it stops, and profile 0
    // is armed exactly once (re-arming mid-start resets the BCU and stretches the confirmation).
    if (!m_confirmed && m_phase_secs >= VWEGOLF_BATCTRL_RETRY_SECS) {
        m_phase_secs = 0;
        switch (m_phase) {
            case CP_HANDSHAKE:
                SendHandshake();  // ack lost — re-open the channel; do NOT GET until it's acked
                break;
            case CP_PROFILE:
                if (isTimerOp()) {
                    // Re-prompt the timer GETs; the periodic HEARTBEAT broadcast is the real source, so
                    // this is only a nudge. BC_TIMER_LIST finalizes on the collection window (below);
                    // SET/ENABLE advance in HandleTimerStatus once the enable mask is heard.
                    SendTimerReads();
                    break;
                }
                if (m_have_profile0) {
                    if (m_op == BC_PROFILE_SET) {
                        // Base captured but the field write's TX failed — re-send it; confirm on the echo.
                        if (SendProfileSetWrite()) m_phase = CP_CONFIRM;
                        break;
                    }
                    // Profile was read but the arm TX failed earlier — retry the arm. Advance the
                    // same way the reply path does: SET_CURRENT / skipped-arm -> CP_CONFIRM (with a
                    // trigger if skipped); a real write -> CP_ARM_WAIT (trigger on the write echo).
                    if (SendArm()) {
                        // Mirror the reply path (see the "49 59 bx" handler): SET_CURRENT waits for
                        // the write echo in CP_ARM_WAIT when it must auto-apply (re-fire the start for
                        // a running charge), else CP_CONFIRM; climate/charge trigger immediately on a
                        // skipped arm, else wait for the echo in CP_ARM_WAIT.
                        if (m_op == BC_SET_CURRENT)   m_phase = m_apply ? CP_ARM_WAIT : CP_CONFIRM;
                        else if (m_arm_skipped)     { SendTrigger(m_enable); m_phase = CP_CONFIRM; }
                        else                          m_phase = CP_ARM_WAIT;
                    }
                } else {
                    // Re-request the profile array ONLY. Do NOT re-send the handshake here:
                    // registration is already complete, and re-registering every retry churns the
                    // BCU session (and puts a fresh GET right behind an un-acked handshake again).
                    // The profile-array reply drives the arm.
                    SendProfileGet();
                }
                break;
            case CP_ARM_WAIT:
                SendArm();  // arm-write echo lost — re-send the write; its echo releases the trigger
                break;
            case CP_CONFIRM:
                // Re-fire the LAST step whose echo was lost. Timer set/enable is waiting on the
                // TimerState mask echo -> re-send the record (SET) + mask. Write-only SET_CURRENT is
                // waiting on its write echo -> re-write. Everything else (climate/charge, or SET_CURRENT
                // auto-apply past its re-trigger) is waiting on the 49 58 -> re-fire the trigger. Never
                // re-arm a climate/charge start (that resets the BCU's spin-up).
                if (isTimerOp()) {
                    if (m_timer_write_record) SendTimerRecord();
                    SendTimerMask(m_timer_target_mask);
                } else if (m_op == BC_PROFILE_SET) SendProfileSetWrite();  // re-send the field write
                else if (m_op == BC_SET_CURRENT && !m_apply) SendArm();
                else SendTrigger(m_enable);
                break;
            default:  // CP_WAKE: keep NM-waking until the BCU is heard; CP_DONE: nothing
                break;
        }
    }
#else
    // Single-shot mode (VWEGOLF_BATCTRL_RETRY_ENABLED == 0): no re-send. Each step is fired exactly
    // once by IncomingBapStatus; if a reply is lost the command simply times out at the wake-window
    // cap. This distinguishes a slow BCU from a bus-swallowed frame. (m_phase_secs still counts, just
    // unused here.)
    (void)m_phase_secs;
#endif

    // BC_TIMER_LIST collection window: the BCU dribbles the timer records + mask over a few seconds
    // after the wake. Once the window elapses, publish whatever arrived (a never-programmed slot may
    // not broadcast, so we don't wait for all four). If nothing arrived, stay PENDING and let the
    // wake-window terminal outcome mark it FAILED.
    if (m_op == BC_TIMER_LIST && m_phase == CP_PROFILE && ++m_collect_secs >= kTimerCollectSecs) {
        bool any_seen = m_timer_mask_seen;
        for (uint8_t i = 0; i < kNumTimers; i++) if (m_timer_seen[i]) any_seen = true;
        if (any_seen) {
            m_confirmed  = true;
            m_list_state = LIST_READY;
            m_phase      = CP_DONE;
            if (m_wake_hold > 1) m_wake_hold = 1;
            ESP_LOGI(TAG, "timer list read: mask 0x%x (collection window)", m_timer_mask);
        }
    }

    if (m_wake_hold > 0) m_wake_hold--;
    if (m_wake_hold == 0) {
        // Terminal outcome (fires once): release the NM. Success needs no separate notification
        // (climate is reflected in ms_v_env_hvac, charge on 0x594); on failure it notifies with the
        // distinguished cause so a remote user knows what happened.
        const char* what = m_op == BC_CHARGE       ? "Charge"
                         : m_op == BC_SET_CURRENT  ? "SetChargeCurrent"
                         : m_op == BC_LIST         ? "ListProfiles"
                         : m_op == BC_PROFILE_SET  ? "SetProfile"
                         : m_op == BC_TIMER_LIST   ? "ListTimers"
                         : m_op == BC_TIMER_SET    ? "SetTimer"
                         : m_op == BC_TIMER_ENABLE ? "TimerEnable"
                                                   : "Climate";
        if (m_confirmed) {
            ESP_LOGI(TAG, "%s %s confirmed — releasing NM, cluster self-sustains", what,
                     (isCliOp() || m_op == BC_SET_CURRENT) ? "" : (m_enable ? "ON" : "OFF"));
        } else if (isCliOp()) {
            // Console read/write op: the CLI command reports the failure to its own writer, so don't
            // push a user notification (climate/charge notify because they're remote fire-and-forget).
            m_list_state = LIST_FAILED;
            ESP_LOGW(TAG, "%s FAILED — releasing NM (no response / not confirmed)", what);
        } else {
            char reason[96];
            // Every command (ON, OFF, SET_CURRENT) reads profile 0 first to arm the matching op, so a
            // missing read is always a valid failure cause.
            const char* cause =
                m_error     ? "rejected by the battery control unit"
                : m_tx_fail ? "CAN transmit error (try 'can can3 reset')"
                : !m_have_profile0
                            ? "could not read the vehicle's profile (no response)"
                            : "no response from ECU (timeout)";
            snprintf(reason, sizeof(reason), "%s command failed: %s", what, cause);
            ESP_LOGE(TAG, "%s FAILED — releasing NM: %s", what, reason);
            MyNotify.NotifyString("alert", m_op == BC_CLIMATE ? "xvg.climate" : "xvg.charge", reason);
        }
        m_phase = CP_IDLE;
    }
}

// ---------------------------------------------------------------------------
// Incoming frames — the BCU's replies DRIVE the state machine
// ---------------------------------------------------------------------------

void VWeGolfBatteryControl::IncomingBapStatus(const CAN_frame_t* p_frame) {
    // BatteryControl FSG status stream on 0x17332510 (LSG 0x25). Any frame here means the BCU's
    // BAP layer is up and its receive path is working (it received the module's TX), so it both
    // opens the command and clears a transient TX-fail. The reply telegrams then step the sequence:
    // profile array -> arm+trigger, OperationMode echo -> confirm.
    //
    // Start each command with a CLEAN reassembler. This is the RX task — the only one that touches
    // m_bap_asm — so resetting here (rather than in Command()'s task) can't race feed(). Command()
    // cleared m_bcu_seen, so the first status frame of a new command triggers this once. Without
    // it, long telegrams left half-reassembled from the previous command / broadcast flood keep the
    // 4 pending slots occupied, and the 21-frame profile-array (49 59) reply's continuations get
    // misrouted so it never completes on the 2nd+ command — short frames still complete (so the GET
    // still goes out), but the profile is never read and the command times out.
    if (!m_bcu_seen) m_bap_asm.reset();
    m_bcu_seen = true;

    const uint8_t* d = p_frame->data.u8;
    bap::Element el;
    if (!m_bap_asm.feed(p_frame->MsgID, d, p_frame->FIR.B.DLC, el) || el.lsg != bap::egolf::kLsg) {
        return;
    }
    m_tx_fail = false;  // the BCU is answering, so the transmit path is healthy

    // Always keep the persistent timer cache current from ambient status/broadcast — even with no
    // command in flight — so a later set/list acts on it immediately instead of waiting for a fresh
    // broadcast. Timer funcs only (0x13 TimerState / 0x14..0x17 slots).
    if ((el.opcode == bap::OP_STATUS || el.opcode == bap::OP_HEARTBEAT) &&
        (el.func == bap::egolf::FUNC_TIMER_STATE || bap::egolf::timerSlotForFunc(el.func) != 0)) {
        UpdateTimerCache(el);
    }

    // The OperationMode echo "49 58 <flag>" is the immediate-operation on/off state — reflect it on
    // EVERY such frame, in any phase (including the very first frame, which also opens the handshake
    // below). It maps to HVAC ONLY for a climate command: the SAME echo also confirms a CHARGE
    // start (profile 0 armed for charge), which must NOT touch ms_v_env_hvac (charge state comes
    // from 0x594). Confirming the command itself is done later, once CP_CONFIRM is reached.
    const bool opmode = el.opcode == bap::OP_STATUS &&
                        el.func == bap::egolf::FUNC_OPERATION_MODE && el.bodyLen >= 1;
    if (opmode) {
        if (m_phase != CP_DONE) m_error = false;  // a good status supersedes a transient error, but
                                                  // never un-fails a command already aborted on error
        if (m_op == BC_CLIMATE) {
            bool hvac_on = el.body[0] != 0;
            StandardMetrics.ms_v_env_hvac->SetValue(hvac_on);
            ESP_LOGI(TAG, "BatteryControl OperationMode 49 58 %02x -> HVAC %s", el.body[0],
                     hvac_on ? "ON" : "OFF");
        } else {
            ESP_LOGI(TAG, "BatteryControl OperationMode 49 58 %02x", el.body[0]);
        }
    }

    // First contact: the BCU's BAP layer is up. Send the channel-open handshake ONCE and then
    // WAIT for its ack before issuing any function GET. This ordering is essential: the BCU
    // silently DROPS a func GET that arrives before it has acked registration (confirmed on-car).
    // A GET issued directly after the handshake, before the "49 41"/"49 42" ack lands, is dropped
    // every cycle. Do NOT act further on this frame; the handshake ack (below) advances the
    // sequence.
    if (m_phase == CP_WAKE) {
        SendHandshake();
        m_phase = CP_HANDSHAKE;
        m_phase_secs = 0;
        return;
    }

    // Handshake ack: the BCU replied to the channel-open GETs — "49 41" (func 0x01 GetAll, the
    // registration data) or "49 42" (func 0x02 BapConfig). Either means registration is complete, so
    // NOW read profile 0 — for ON, OFF, and SET_CURRENT alike. STOP is OP-SPECIFIC (a charge stop
    // only works with profile 0 armed to charge; confirmed on-car), so OFF must arm the matching op
    // before the stop trigger, just like ON arms it before the start.
    if (m_phase == CP_HANDSHAKE && el.opcode == bap::OP_STATUS &&
        (el.func == bap::egolf::FUNC_BAP_GETALL || el.func == bap::egolf::FUNC_BAP_CONFIG)) {
        if (isTimerOp()) SendTimerReads();  // best-effort nudge (BCU has no timer GET handler; the
                                            // persistent cache from the broadcast is the real source)
        else             SendProfileGet();  // read profile 0 -> arm the matching op -> trigger
        m_phase = CP_PROFILE;
        m_phase_secs = 0;
        m_collect_secs = 0;
        // Act on the persistent cache right away (no ~13 s broadcast wait): finish a list if the cache
        // is already complete, or issue the set/enable write if the enable mask is already cached. If
        // the cache is still empty (never heard a broadcast), these are no-ops and we wait as before.
        if (m_op == BC_TIMER_LIST)                                    TimerListMaybeComplete();
        else if (m_op == BC_TIMER_SET || m_op == BC_TIMER_ENABLE)     TimerWriteMaybeStart();
        return;
    }

    // Departure-timer read-back: TimerState mask (func 0x13) + per-slot records (func 0x14..0x17), as
    // a write echo (STATUS 49 5x) or the BCU's periodic table broadcast (HEARTBEAT 39 5x). Drives the
    // in-flight timer command (collect for list; RMW-write + confirm for set/enable). Placed before the
    // profile-array block; timer funcs never match FUNC_PROFILES_ARRAY (0x19) so the two never overlap.
    if (isTimerOp() && (el.opcode == bap::OP_STATUS || el.opcode == bap::OP_HEARTBEAT)) {
        HandleTimerStatus(el);
        return;
    }

    if (el.opcode == bap::OP_ERROR) {
        // A BAP ERROR is a DEFINITIVE rejection (e.g. charge not possible when unplugged), NOT a
        // dropped reply — so STOP immediately: end the command and do NOT keep re-firing the trigger
        // (the Ticker1 backstop retries only while the phase is live; CP_DONE stops it). Retrying
        // past a rejection would keep hammering the BCU and could then falsely confirm on a stray
        // later echo. Ticker1's terminal outcome notifies the failure and releases the wake bridge.
        m_error = true;
        m_phase = CP_DONE;
        if (m_wake_hold > 1) m_wake_hold = 1;  // release + notify on the next tick
        ESP_LOGW(TAG, "BatteryControl BAP ERROR (func 0x%02X) — rejected; aborting (no retry)",
                 el.func);
        return;
    }

    if (opmode) {
        // The "49 58" echo confirms a climate/charge START/STOP, OR a SetChargeCurrent auto-apply
        // (which re-fired the start). A write-only SET_CURRENT has no trigger and confirms on its
        // write echo instead (array handler below). Match the flag to the request.
        if ((m_op == BC_CLIMATE || m_op == BC_CHARGE || (m_op == BC_SET_CURRENT && m_apply)) &&
            m_phase == CP_CONFIRM && (el.body[0] != 0) == m_enable) {
            m_confirmed = true;
            m_phase = CP_DONE;
            if (m_wake_hold > 2) m_wake_hold = 2;  // release soon, small grace
        }
        return;
    }

    if (el.opcode == bap::OP_STATUS && el.func == bap::egolf::FUNC_PROFILES_ARRAY) {
        // Profile-array status "49 59". While waiting to arm, read back the global "Optionen"
        // record (pos 0) and arm it, then trigger — all off this reply, no tick wait. The arm
        // keeps every stored byte (temp, min charge, current, name, and the "Climatise on battery
        // power" bit) and only makes the operation deterministically climate.
        //
        // Match the module's OWN reply. The leading byte is [asgId:4|txn:4]; the BCU echoes it as-is
        // for a GET reply (0x3X) or with bit7 set for a SET reply (0xbX = 0x3X|0x80). Mask bit7 and
        // match asgId == kAsgIdOvms — the module's own distinct client id (0x3). Because the GET is
        // issued only AFTER the handshake ack, the BCU answers this ranged GET directly (0x3X reply),
        // so the read works while already awake too, with no dependence on the OCU's ambient id-2
        // polls (which are silent when the car never slept). The MIB (0x1x) and OCU (0x2x/0xax)
        // replies are ignored entirely.
        //
        // Guard the body read: a short "49 59" (DLC 2) or a segmented start declaring a zero-length
        // body both reassemble to bodyLen 0 / body == nullptr, so reading body[0] would dereference
        // null and crash the RX task on a single malformed/glitched comfort-bus frame. Nothing here
        // is actionable without at least the leading [asgId|txn] byte, so drop it.
        if (el.bodyLen < 1 || el.body == nullptr) return;
        const uint8_t lead = el.body[0];
        if (((lead >> 4) & 0x07) != bap::egolf::kAsgIdOvms) return;

        // The module's SET reply (arm/write echo — bit7 set, 0xbX). This is the BCU ACKING that the
        // profile-0 write actually LANDED; only now is it safe to act on the newly-written op.
        if (lead & 0x80) {
            // BC_PROFILE_SET: the profile-field write landed -> the edit is committed. No trigger; the
            // write IS the whole command, so confirm here (mirrors the idle SET_CURRENT write echo).
            if (m_op == BC_PROFILE_SET) {
                if (m_phase == CP_CONFIRM) {
                    m_confirmed  = true;
                    m_list_state = LIST_READY;
                    m_phase      = CP_DONE;
                    if (m_wake_hold > 2) m_wake_hold = 2;
                    ESP_LOGI(TAG, "SetProfile %u written — BCU echoed 49 59 %02x", m_prof_set_pos, lead);
                }
                return;
            }
            // SET_CURRENT: the write landed -> reflect the applied limit (the read-back left it
            // untouched, so there's no jump-back).
            if (m_op == BC_SET_CURRENT) {
                StandardMetrics.ms_v_charge_climit->SetValue((float)m_param, Amps);
                if (m_apply && m_phase == CP_ARM_WAIT) {
                    // Auto-apply: re-fire the start so the RUNNING charge re-reads the profile and
                    // picks up the new current. Confirm on the 49 58 echo (like a charge start).
                    ESP_LOGI(TAG, "SetChargeCurrent %u A written — re-applying to running charge", m_param);
                    SendTrigger(true);
                    m_phase = CP_CONFIRM;
                    m_phase_secs = 0;
                    return;
                }
                if (!m_apply && m_phase == CP_CONFIRM) {  // idle: the write IS the whole command
                    m_confirmed = true;
                    m_phase = CP_DONE;
                    if (m_wake_hold > 2) m_wake_hold = 2;
                    ESP_LOGI(TAG, "SetChargeCurrent %u A written — BCU echoed 49 59 %02x", m_param, lead);
                    return;
                }
                return;
            }
            // Climate/charge: the arm write is acked -> NOW fire the trigger, so it runs the settled,
            // newly-armed op instead of racing the multi-frame write and running the STALE op.
            if ((m_op == BC_CLIMATE || m_op == BC_CHARGE) && m_phase == CP_ARM_WAIT) {
                ESP_LOGI(TAG, "arm write acked (49 59 %02x) — triggering", lead);
                SendTrigger(m_enable);
                m_phase = CP_CONFIRM;
                m_phase_secs = 0;
                return;
            }
            return;  // stray / duplicate SET echo
        }

        // GET reply (0x3X) in the read phase: decode the profile array.
        if (m_phase == CP_PROFILE && !m_have_profile0 && el.bodyLen >= 6) {
            bap::egolf::Profile profs[kMaxProfiles];
            bap::egolf::ArrayResult ares;
            size_t n = bap::egolf::decodeProfileArray(el.body, el.bodyLen, profs, kMaxProfiles, &ares);

            // BC_LIST: read-only dump — capture ALL decoded profiles and finish. No arm, no trigger,
            // no profile write of any kind. This is the safe read path `xvg charge profile list` uses;
            // it's also the on-car instrument for confirming the per-profile target-SoC decode.
            if (m_op == BC_LIST) {
                if (n == 0) return;  // nothing usable yet; the CP_PROFILE backstop re-issues the GET
                for (size_t i = 0; i < n; i++) m_profiles[i] = profs[i];
                m_profile_count = (uint8_t)n;       // store the count BEFORE publishing readiness
                m_have_profile0 = true;             // read done — ignore a duplicate / late reply
                m_confirmed     = true;             // terminal outcome = success (no failure notify)
                m_list_state    = LIST_READY;       // publish LAST: the command task reads this first
                m_phase         = CP_DONE;
                if (m_wake_hold > 2) m_wake_hold = 2;  // release the NM bridge soon
                ESP_LOGI(TAG, "profile list read: %u profile(s)%s", (unsigned)n,
                         ares.truncated ? " (more present, list truncated)" : "");
                return;
            }

            // BC_PROFILE_SET: find the target profile in the array, apply the edit, write it back. NO
            // trigger — a settings edit; confirm on the write echo (0xbX), like the idle SET_CURRENT.
            if (m_op == BC_PROFILE_SET) {
                for (size_t i = 0; !ares.malformed && i < n; i++) {
                    if (profs[i].position != m_prof_set_pos) continue;
                    m_prof_base = profs[i];
                    if (m_prof_base.nameTruncated) {
                        // Can't safely RMW: the read-back name overflowed kProfileMaxName, so re-encoding
                        // would write a SHORTER name and corrupt the record. Abort (fail fast, no write).
                        ESP_LOGW(TAG, "SetProfile: profile %u name too long to safely edit — aborting",
                                 m_prof_set_pos);
                        m_phase = CP_DONE;
                        if (m_wake_hold > 1) m_wake_hold = 1;  // terminal FAILED next tick (CLI op)
                        return;
                    }
                    m_have_profile0 = true;  // base captured; a lost write echo is re-sent by the backstop
                    if (SendProfileSetWrite()) {
                        m_phase      = CP_CONFIRM;  // await the write echo (0xbX)
                        m_phase_secs = 0;
                    }  // else TX failed: stay CP_PROFILE (m_have_profile0 set) -> backstop re-writes
                    return;
                }
                // Loop finished without finding the target: not present in a fully-decoded (or malformed)
                // array -> fail fast rather than waiting out the whole wake window. A truncated read
                // (target maybe beyond maxOut — impossible for pos 0..3) falls to the backstop re-GET.
                if (ares.complete || ares.malformed) {
                    ESP_LOGW(TAG, "SetProfile: profile %u not found in the vehicle's array",
                             m_prof_set_pos);
                    m_phase = CP_DONE;
                    if (m_wake_hold > 1) m_wake_hold = 1;
                }
                return;
            }

            for (size_t i = 0; !ares.malformed && i < n; i++) {
                if (profs[i].position == 0) {  // the global / immediate profile
                    m_profile0 = profs[i];
                    m_have_profile0 = true;
                    // Reflect the car's stored global-profile settings so the app shows real values
                    // (ms_v_charge_climit flows to the server-v2 status record automatically). Only
                    // the fields that are meaningful for profile 0: maxCurrent (the charge-current
                    // limit) and the pre-conditioning setpoint. This deliberately does NOT set
                    // ms_v_charge_limit_soc from targetChargeLevel — profile 0 (the global "Optionen"
                    // immediate profile) has no charge-to-SoC setting; that lives in the timer
                    // profiles 1-3 only, so its byte here is not a user charge limit.
                    //
                    // For a SetChargeCurrent command DON'T push the read-back (OLD) value here: the
                    // app has already optimistically shown the NEW value, and setting climit to the
                    // old one makes it visibly jump back then forward. Leave climit untouched; the
                    // write echo sets it to the NEW value on success, and on failure it stays at the
                    // old value (a natural rollback) — see the SET_CURRENT confirm + failure handling.
                    if (m_op != BC_SET_CURRENT)
                        StandardMetrics.ms_v_charge_climit->SetValue((float)m_profile0.maxCurrent, Amps);
                    if (m_profile0.temperatureRaw != 0xFF)  // 0xFF = unset/padding
                        StandardMetrics.ms_v_env_cabinsetpoint->SetValue(
                            bap::egolf::rawToTemp(m_profile0.temperatureRaw), Celcius);
                    if (!SendArm()) break;  // arm TX failed; the CP_PROFILE backstop re-arms
                    if (m_op == BC_SET_CURRENT) {
                        // Idle: confirm on the write echo (0xbX), no trigger. Auto-apply (charging):
                        // wait for the echo, then re-fire the start (handled in CP_ARM_WAIT).
                        m_phase = m_apply ? CP_ARM_WAIT : CP_CONFIRM;
                    } else if (m_arm_skipped) {
                        // Op already correct -> no write, so no echo is coming -> trigger immediately
                        // (there's no stale-op race when nothing was written).
                        SendTrigger(m_enable);
                        m_phase = CP_CONFIRM;
                    } else {
                        // Wait for the arm-write echo (0xbX) BEFORE the trigger — firing it now would
                        // race the multi-frame write (~1.5 s to be acked) and run the stale op.
                        m_phase = CP_ARM_WAIT;
                    }
                    m_phase_secs = 0;
                    break;
                }
            }
        }
        return;
    }
}

void VWeGolfBatteryControl::UpdateTimerCache(const bap::Element& el) {
    // Store a read-back into the PERSISTENT cache: the TimerState enable mask (func 0x13) or a per-slot
    // 8-byte record (func 0x14..0x17). Called for EVERY such frame on 0x17332510 (STATUS write-echo or
    // HEARTBEAT broadcast), whether or not a command is in flight — so set/list can use it immediately.
    if (el.func == bap::egolf::FUNC_TIMER_STATE) {
        if (el.bodyLen < 1 || el.body == nullptr) return;
        m_timer_mask = (uint8_t)(el.body[0] & 0x0F);
        m_timer_mask_seen = true;
    } else {
        uint8_t slot = bap::egolf::timerSlotForFunc(el.func);
        if (slot < 1 || slot > kNumTimers) return;  // not a timer element (e.g. a stray 49 42)
        if (el.bodyLen >= 8 && el.body != nullptr) {
            bap::egolf::Timer t;
            if (bap::egolf::decodeTimer(el.body, el.bodyLen, t)) {
                m_timers[slot - 1] = t;
                m_timer_seen[slot - 1] = true;
            }
        }
    }
}

void VWeGolfBatteryControl::TimerListMaybeComplete() {
    // Publish the list the instant the cache holds the mask + every slot (usually already true from
    // ambient traffic, so the list is immediate). An incomplete cache falls through to the collection
    // window in Ticker1, which publishes whatever is cached (an unprogrammed slot may never broadcast).
    if (m_op != BC_TIMER_LIST || m_phase != CP_PROFILE) return;
    bool all = m_timer_mask_seen;
    for (uint8_t i = 0; i < kNumTimers; i++) if (!m_timer_seen[i]) all = false;
    if (!all) return;
    m_confirmed  = true;
    m_list_state = LIST_READY;
    m_phase      = CP_DONE;
    if (m_wake_hold > 1) m_wake_hold = 1;
    ESP_LOGI(TAG, "timer list ready from cache: mask 0x%x, all %u slots", m_timer_mask, kNumTimers);
}

bool VWeGolfBatteryControl::TimerWriteMaybeStart() {
    // Issue the RMW write as soon as the enable mask is CACHED (no ~13 s broadcast wait) — the record
    // (SET only) then the new mask; confirm on the 49 53 echo. Both gated on the mask being known, so a
    // never-cached mask leaves NO partial state (we just wait for the first broadcast).
    if ((m_op != BC_TIMER_SET && m_op != BC_TIMER_ENABLE) || m_phase != CP_PROFILE) return false;
    if (!m_timer_mask_seen) return false;  // mask not cached yet -> wait for a broadcast to populate it
    const uint8_t bit = (uint8_t)(1u << (m_timer_slot - 1));
    if (m_timer_write_record) SendTimerRecord();  // 29 5x on 0x17332501 (full record)
    m_timer_target_mask = m_timer_enable ? (uint8_t)(m_timer_mask | bit)
                                         : (uint8_t)(m_timer_mask & (uint8_t)~bit);
    SendTimerMask(m_timer_target_mask);           // 29 53 on 0x17332501 (enable-mask RMW)
    m_phase = CP_CONFIRM;
    m_phase_secs = 0;
    return true;
}

void VWeGolfBatteryControl::HandleTimerStatus(const bap::Element& el) {
    // The persistent cache was already updated (UpdateTimerCache, top of IncomingBapStatus). Drive the
    // in-flight command off it: finish a list, issue a pending write, or confirm one.
    if (m_op == BC_TIMER_LIST) { TimerListMaybeComplete(); return; }

    // SET / ENABLE: fire the write as soon as the enable mask is cached (no-op until then).
    if (m_phase == CP_PROFILE) { TimerWriteMaybeStart(); return; }

    // Confirm ONLY on a STATUS echo (49 53) of the enable mask matching what we wrote — NOT on a
    // periodic HEARTBEAT (39 53) broadcast, which would falsely confirm when the target equals the
    // pre-existing mask (e.g. re-setting an already-enabled slot) even though the BCU dropped our write.
    if (m_phase == CP_CONFIRM && el.opcode == bap::OP_STATUS &&
        el.func == bap::egolf::FUNC_TIMER_STATE && m_timer_mask == m_timer_target_mask) {
        m_confirmed  = true;
        m_list_state = LIST_READY;
        m_phase      = CP_DONE;
        if (m_wake_hold > 2) m_wake_hold = 2;
        ESP_LOGI(TAG, "timer slot %u %s confirmed (mask echo -> 0x%x)", m_timer_slot,
                 m_timer_write_record ? "set" : (m_timer_enable ? "enabled" : "disabled"),
                 m_timer_mask);
    }
}

void VWeGolfBatteryControl::IncomingClimaEcuStatus(const CAN_frame_t* p_frame) {
    // Clima ECU status 0x5EA. HVAC on = climate is actively conditioning: d[3] bit 3 (0x08).
    // NOTE: d[3] bits 6-7 mark a remote-climate *session* but read the same (=2) whether merely
    // armed (d3=0x80) or actually running (d3=0x88), so keying HVAC on those bits leaves it stuck
    // true after a stop (the BCU drops 0x88 -> 0x80 when conditioning ends). The conditioning bit
    // (0x08) clears on stop, so use it.
    const uint8_t* d = p_frame->data.u8;
    StandardMetrics.ms_v_env_hvac->SetValue((d[3] & 0x08) != 0);
}

// ---------------------------------------------------------------------------
// CAN transmit
// ---------------------------------------------------------------------------

void VWeGolfBatteryControl::SendNmWake() {
    // AUTOSAR CAN-NM wake from a SPARE (unused) node id (id = 0x1B000000 + node), so it never
    // impersonates the real OCU's node 0x67. Impersonating the OCU (this NM id and/or its 0x5A7
    // heartbeat) collided with the car's live OCU and left DTCs U001100/U120100. The payload
    // requests the comfort/EV partial-network cluster:
    //   byte0 = source node id (spare)          byte1 = 0x10 CBV active-wakeup
    //   byte2 = 0x49 = 0x40 charge | 0x08 climate PNC | 0x01 comfort baseline
    //   byte3 = 0x85 = 0x84 (observed wake request) | 0x01 (Climatronic 0x46 PNC)
    //   byte4 = 0x14 (observed wake request bits)
    uint8_t data[8] = {VWEGOLF_NM_WAKE_NODE, 0x10, 0x49, 0x85, 0x14, 0x00, 0x00, 0x00};
    m_bus->WriteExtended(0x1B000000u | VWEGOLF_NM_WAKE_NODE, 8, data);
}

bool VWeGolfBatteryControl::TxFrame(const uint8_t* frame, uint8_t dlc) {
    // One BAP frame onto the LSG-0x25 command id 0x17332501 (kCanIdCommand) — the comfort/"bus-4" id our
    // KCAN node uses for ALL commands (climate, charge, profiles, timers); the J533 gateway bridges it to
    // the FCAN bus where the BCU lives. ESP_OK = frame in a HW TX buffer; ESP_QUEUED = accepted into the
    // driver's SW TX queue (HW buffers busy — routine while the controller is error-passive right after
    // the NM wake). Both mean the frame WILL transmit in order, so both are success; only ESP_FAIL (SW
    // queue overflow / controller off / bus-off) is a real failure. No inter-frame delay: sends are driven
    // from the RX path, so a step's few frames are queued for the driver to drain, not blocking CAN RX.
    esp_err_t r = m_bus->WriteExtended(bap::egolf::kCanIdCommand, dlc, const_cast<uint8_t*>(frame));
    return r == ESP_OK || r == ESP_QUEUED;
}

bool VWeGolfBatteryControl::SendHandshake() {
    // BAP channel-open handshake: GET BapConfig (func 0x02, "19 42") + GetAll (func 0x01, "19 41")
    // open/sync the logical channel with the BCU. Atomic: both must be accepted by the controller
    // (queued counts). A genuinely rejected GET (ESP_FAIL) leaves the channel half-open.
    auto sink = [this](const uint8_t* f, uint8_t d) -> bool { return TxFrame(f, d); };
    bap::SendResult g1 = bap::sendElement(sink, bap::OP_GET, bap::egolf::kLsg,
                                          bap::egolf::FUNC_BAP_CONFIG, nullptr, 0);
    bap::SendResult g2 = bap::sendElement(sink, bap::OP_GET, bap::egolf::kLsg,
                                          bap::egolf::FUNC_BAP_GETALL, nullptr, 0);
    if (!g1.ok() || !g2.ok()) {
        m_tx_fail = true;
        ESP_LOGW(TAG, "Climate handshake GET TX rejected (g1=%d g2=%d)", g1.ok(), g2.ok());
        return false;
    }
    return true;
}

bool VWeGolfBatteryControl::SendProfileGet() {
    // GET the ProfilesArray (func 0x19) as the factory OCU does on the wire, byte-for-byte except
    // the OVMS asgId nibble: a "80"-SEGMENTED get-all request "80 04 19 59 <asgTxn> 00 00 04".
    // Confirmed on-car: even with the handshake completed FIRST, the BCU drops a BARE ranged GET
    // "19 59 <asgTxn> 00 00 01" while answering the OCU's segmented get-all form, so the func-0x19
    // GET handler requires this exact framing (unlike the bare func-1/2 handshake GETs, which it
    // answers either way). Issued only after the handshake ack; the reply "49 59 <txn>" (matched on
    // the OVMS asgId in IncomingBapStatus) drives the arm.
    //
    //   80 = start-segment ch0 | 04 = BAP message length (4) | 19 59 = GET func 0x19 header
    //   <asgTxn> = [asgId 3 | txn] | 00 = get-all param | 00 04 = trailing pad (ignored; matches OCU)
    uint8_t asgTxn = bap::egolf::asgTxnByte(bap::egolf::kAsgIdOvms, m_txn.next());
    uint8_t frame[8] = {0x80, 0x04, 0x19, 0x59, asgTxn, 0x00, 0x00, 0x04};
    if (!TxFrame(frame, sizeof(frame))) {
        m_tx_fail = true;
        ESP_LOGW(TAG, "Climate profile-array GET TX rejected");
        return false;
    }
    return true;
}

bool VWeGolfBatteryControl::SendArm() {
    // Read-modify-write the global "Optionen" profile (pos 0). Take the record the BCU just handed
    // back (m_profile0), change ONLY the field this command owns, and leave everything else exactly
    // as read (target temperature, min charge %, hold-times, provider id, name, and the user's
    // climateWithoutExternalSupply "Climatise on battery power" bit is preserved except where a
    // command must own it, below). Re-encoding the car's own bytes means no other stored setting
    // can be clobbered.
    //   BC_CLIMATE / BC_CHARGE: flip ONLY the operation byte so the immediate trigger runs a
    //     KNOWN op — climate sets PO_CLIMATE + PO_ALLOW_BATTERY and clears PO_CHARGING; charge sets
    //     PO_CHARGING + clears PO_CLIMATE and PO_ALLOW_BATTERY (deterministic charge).
    //   BC_SET_CURRENT: change ONLY maxCurrent; op is left as-is (this is a settings edit, not a
    //     start) — no trigger follows.
    // Emits "29 59 <3x-txn> 00 00 01 <full RA0 record>" (asgId 3 = OVMS; recordAddr 0 = RA0;
    // startIndex 0 = pos 0; count 1) — the full-record read-modify-write the factory MIB does.
    bap::egolf::Profile p = m_profile0;
    switch (m_op) {
        case BC_CLIMATE:
            // Set climate, clear charge, and FORCE-SET the on-battery bit (PO_ALLOW_BATTERY, the
            // car's "Climatise on battery power" option). It is an ALLOW flag, not a force: the car's
            // own profile 0 carries it (op 0x06) yet still uses wall power when plugged, so setting it
            // is safe. We must re-assert it because a charge command clears it (see BC_CHARGE) and
            // that write is persistent — without re-setting it here, climate would no longer work on
            // battery power after any charge. Result: climate always arms op 0x06.
            p.operation = (uint8_t)((p.operation | bap::egolf::PO_CLIMATE | bap::egolf::PO_ALLOW_BATTERY)
                                    & ~bap::egolf::PO_CHARGING);
            break;
        case BC_CHARGE:
            // Charge: PURE charge — clear BOTH climate bits, PO_CLIMATE *and* the on-battery bit
            // PO_ALLOW_BATTERY. PO_ALLOW_BATTERY ("climatise without external supply") is a STANDALONE
            // climate trigger, not just a modifier: leaving it set made a charge start (op 0x05) run
            // climate-on-battery when charging couldn't (unplugged) — confirmed on-car.
            p.operation = (uint8_t)((p.operation | bap::egolf::PO_CHARGING)
                                    & ~(bap::egolf::PO_CLIMATE | bap::egolf::PO_ALLOW_BATTERY));
            break;
        case BC_SET_CURRENT:
            p.maxCurrent = m_param;
            // Auto-apply (a charge is running): also set the op to charge so the re-fired start
            // re-applies CHARGE with the new current. Idle: op left unchanged (pure settings edit).
            if (m_apply)
                p.operation = (uint8_t)((p.operation | bap::egolf::PO_CHARGING)
                                        & ~(bap::egolf::PO_CLIMATE | bap::egolf::PO_ALLOW_BATTERY));
            break;
        case BC_LIST:
        case BC_PROFILE_SET:
        case BC_TIMER_LIST:
        case BC_TIMER_SET:
        case BC_TIMER_ENABLE:
            // SendArm() is never reached for these (the list handler returns before arming; BC_PROFILE_SET
            // has its own SendProfileSetWrite(); timer ops never call it). Leave the profile untouched so
            // the skip-write path below emits nothing even if this were ever reached — these ops must
            // never arm/trigger profile 0.
            break;
    }
    // ⚠ HARD SAFETY CLAMP (see kMaxCurrentHardLimit): NEVER write a maxCurrent above 0x20 — a higher
    // value bricks the car's charging until a factory reset. This is the single chokepoint EVERY
    // profile write passes through, so it also guards the climate/charge path where maxCurrent is
    // preserved verbatim from the read-back (which could be garbled or, in future, out of range).
    if (p.maxCurrent > bap::egolf::kMaxCurrentHardLimit)
        p.maxCurrent = bap::egolf::kMaxCurrentHardLimit;
    // Skip-write optimization: for climate/charge the arm usually re-writes the SAME op profile 0
    // already holds, so the ~5-frame RA0 write is a no-op — skip it and let the trigger run the
    // already-correct op. (SET_CURRENT always writes — applying the current IS the command, and it
    // confirms on the write echo. A write still happens if the hard cap changed maxCurrent, i.e. p differs.)
    if (m_op != BC_SET_CURRENT &&
        p.operation == m_profile0.operation && p.maxCurrent == m_profile0.maxCurrent) {
        m_arm_skipped = true;  // no write -> no "49 59 bx" echo coming -> caller triggers immediately
        ESP_LOGI(TAG, "%s arm: profile 0 already op 0x%02x — no write needed",
                 m_op == BC_CHARGE ? "Charge" : "Climate", p.operation);
        return true;
    }
    m_arm_skipped = false;  // a write goes out -> caller waits for its "49 59 bx" echo before triggering
    auto sink = [this](const uint8_t* f, uint8_t d) -> bool { return TxFrame(f, d); };
    bap::SendResult r = bap::egolf::sendProfileWrite(sink, m_txn, bap::egolf::kAsgIdOvms,
                                                     /*recordAddr=*/0, /*startIndex=*/0, p);
    if (!r.ok()) {
        m_tx_fail = true;
        ESP_LOGW(TAG, "BatteryControl arm (profile-0 RA0 write) TX FAILED (status %d)", (int)r.status);
        return false;
    }
    if (m_op == BC_SET_CURRENT)
        ESP_LOGI(TAG, "SetChargeCurrent arm: profile 0 maxCurrent 0x%02x -> 0x%02x (%u frames)",
                 m_profile0.maxCurrent, p.maxCurrent, r.framesSent);
    else
        ESP_LOGI(TAG, "%s arm: profile 0 op 0x%02x -> 0x%02x written (%u frames)",
                 m_op == BC_CHARGE ? "Charge" : "Climate", m_profile0.operation, p.operation,
                 r.framesSent);
    return true;
}

bool VWeGolfBatteryControl::SendTrigger(bool on) {
    // OperationMode immediate start/stop of the (for ON, now-armed) global profile:
    // "29 58 00 01" start / "29 58 00 00" stop. The BCU echoes "49 58 <flag>" to confirm.
    auto sink = [this](const uint8_t* f, uint8_t d) -> bool { return TxFrame(f, d); };
    bap::SendResult r = bap::egolf::sendClimate(sink, on);  // generic OperationMode trigger
    if (!r.ok()) {
        m_tx_fail = true;
        ESP_LOGW(TAG, "BatteryControl BAP trigger CAN write FAILED (status %d, %u/%u frames)",
                 (int)r.status, r.framesSent, bap::expectedFrames(2));
        return false;
    }
    ESP_LOGI(TAG, "%s %s BAP trigger sent to 0x%08x (%u frames)",
             m_op == BC_CHARGE ? "Charge" : "Climate", on ? "START" : "STOP",
             (unsigned)bap::egolf::kCanIdCommand, r.framesSent);
    return true;
}

bool VWeGolfBatteryControl::SendProfileSetWrite() {
    // BC_PROFILE_SET RMW: take the record the BCU handed back (m_prof_base) and overwrite ONLY the
    // fields flagged in m_prof_edit, leaving every other byte — the [RE] middle fields AND the name —
    // exactly as read, so nothing else is clobbered. Emits "29 59 <3x-txn> 00 <pos> 01 <full RA0
    // record>" (asgId 3 = OVMS; recordAddr 0; startIndex = the target position; count 1) — the same
    // full-record array-write the factory MIB uses, just aimed at an arbitrary profile position.
    bap::egolf::Profile p = m_prof_base;
    const ProfileEdit& e = m_prof_edit;
    if (e.fields & ProfileEdit::F_CURRENT) p.maxCurrent        = e.maxCurrent;
    if (e.fields & ProfileEdit::F_MINSOC)  p.minChargeLevel    = e.minChargeLevel;
    if (e.fields & ProfileEdit::F_TGTSOC)  p.targetChargeLevel = e.targetChargeLevel;
    if (e.fields & ProfileEdit::F_TEMP)    p.temperatureRaw    = e.temperatureRaw;
    if (e.fields & ProfileEdit::F_OP)
        // Set ONLY the charge/climate bits from the request; preserve every other operation bit the
        // profile already carries (e.g. PO_ALLOW_BATTERY / operation2) — RMW, don't clobber.
        p.operation = (uint8_t)((p.operation & ~(bap::egolf::PO_CHARGING | bap::egolf::PO_CLIMATE)) |
                                (e.operation & (bap::egolf::PO_CHARGING | bap::egolf::PO_CLIMATE)));
    // ⚠ HARD SAFETY CLAMP (see kMaxCurrentHardLimit): NEVER write a maxCurrent above 0x20 — a higher
    // value bricks the car's charging until a factory reset. Guards both an F_CURRENT value and a
    // preserved/garbled read-back byte, exactly like SendArm().
    if (p.maxCurrent > bap::egolf::kMaxCurrentHardLimit)
        p.maxCurrent = bap::egolf::kMaxCurrentHardLimit;

    auto sink = [this](const uint8_t* f, uint8_t d) -> bool { return TxFrame(f, d); };
    bap::SendResult r = bap::egolf::sendProfileWrite(sink, m_txn, bap::egolf::kAsgIdOvms,
                                                     /*recordAddr=*/0, /*startIndex=*/m_prof_set_pos, p);
    if (!r.ok()) {
        m_tx_fail = true;
        ESP_LOGW(TAG, "SetProfile %u write TX FAILED (status %d)", m_prof_set_pos, (int)r.status);
        return false;
    }
    ESP_LOGI(TAG, "SetProfile %u: op 0x%02x cur 0x%02x minSoC %u tgtSoC %u temp 0x%02x written (%u frames)",
             m_prof_set_pos, p.operation, p.maxCurrent, p.minChargeLevel, p.targetChargeLevel,
             p.temperatureRaw, r.framesSent);
    return true;
}

// ---------------------------------------------------------------------------
// Departure-timer TX (handshake + GET + record + enable-mask writes, all on kCanIdCommand / 0x17332501)
// ---------------------------------------------------------------------------

bool VWeGolfBatteryControl::SendTimerReads() {
    // Actively GET the timer table + enable mask using the SAME "80"-SEGMENTED get-all framing that
    // works for the func-0x19 profile GET: "80 04 19 <0x40|func> <asgTxn> 00 00 04" (header byte1 =
    // 0x40|func → 0x53 for TimerState, 0x54/55/56 for slots 1-3). A BARE "19 5x" GET is silently
    // dropped by the BCU (confirmed for func 0x19), so bare GETs never elicited the timer records —
    // the MIB never GETs timers at all, it rides the periodic broadcast. We do both: this active GET
    // (in case the BCU has a func-0x1x GET handler) AND collect the HEARTBEAT broadcast (39 5x / 39 53),
    // which arrives in a burst only every ~11-13 s. Sent on kCanIdCommand (0x17332501) like every other
    // command — the gatewayed channel that reaches the BCU from our KCAN node.
    auto get = [this](uint8_t func) {
        uint8_t asgTxn = bap::egolf::asgTxnByte(bap::egolf::kAsgIdOvms, m_txn.next());
        uint8_t frame[8] = {0x80, 0x04, 0x19, (uint8_t)(0x40 | func), asgTxn, 0x00, 0x00, 0x04};
        TxFrame(frame, sizeof(frame));  // kCanIdCommand (0x17332501) — our bus's gatewayed channel
    };
    get(bap::egolf::FUNC_TIMER_STATE);
    for (uint8_t slot = 1; slot <= kNumTimers; slot++) get(bap::egolf::funcForTimerSlot(slot));
    return true;
}

bool VWeGolfBatteryControl::SendTimerRecord() {
    // Timer records are written on kCanIdCommand (0x17332501) — our KCAN node's channel, which the J533
    // gateway BRIDGES to the FCAN bus where the BCU lives. (The MIB writes on 0x17332500, but that id is
    // FCAN-local and NOT bridged, so a 0x17332500 write from KCAN never reaches the BCU — on-car finding.)
    auto sink = [this](const uint8_t* f, uint8_t d) -> bool { return TxFrame(f, d); };
    bap::SendResult r = bap::egolf::sendTimerWrite(sink, m_timer_slot, m_timer_write);
    if (!r.ok()) {
        m_tx_fail = true;
        ESP_LOGW(TAG, "timer slot %u record write TX failed (status %d)", m_timer_slot, (int)r.status);
        return false;
    }
    ESP_LOGI(TAG, "timer slot %u record written on 0x%08x (%u frames)", m_timer_slot,
             (unsigned)bap::egolf::kCanIdCommand, r.framesSent);
    return true;
}

bool VWeGolfBatteryControl::SendTimerMask(uint8_t mask) {
    auto sink = [this](const uint8_t* f, uint8_t d) -> bool { return TxFrame(f, d); };
    bap::SendResult r = bap::egolf::sendTimerState(sink, mask);
    if (!r.ok()) {
        m_tx_fail = true;
        ESP_LOGW(TAG, "TimerState write TX failed (mask 0x%x, status %d)", mask, (int)r.status);
        return false;
    }
    ESP_LOGI(TAG, "TimerState mask -> 0x%x written on 0x%08x", mask, (unsigned)bap::egolf::kCanIdCommand);
    return true;
}
