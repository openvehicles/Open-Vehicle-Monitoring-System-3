/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <time.h>
#include <sys/stat.h>
#include "ovms_log.h"
#include "vehicle_toyota_etnga.h"

// Poll state descriptions:
//    SLEEP (0)             : Vehicle is sleeping; no activity on the CAN bus. We are listening only.
//    AWAKE (1)             : Vehicle is alive; vehicle has been switched on by driver
//    DRIVING (2)           : Vehicle is "Ready" to drive or being driven
//    CHARGE_HANDSHAKE (3)  : Cable-negotiation fast-poll window (was CHARGING)
//    CHARGE_WAIT (4)       : Plugged in, not (yet/any longer) charging — sparse poll
//    CHARGE_AC (5)         : AC charging in progress
//    CHARGE_DC (6)         : DC fast charging in progress

// Escalating sleep cooldown schedule (seconds). Each consecutive no-activity sleep uses the
// next entry; real activity (drive/charge/charge-door/12V wake) resets to [0]. Caps at the
// last entry. See ResetSleepBackoff() and TransitionToSleepState().
static const int SLEEP_COOLDOWN_SECS[] = {10, 30, 60, 120, 300};
static const int SLEEP_COOLDOWN_STEPS  = (int)(sizeof(SLEEP_COOLDOWN_SECS) / sizeof(SLEEP_COOLDOWN_SECS[0]));

// CHARGE_WAIT 12V-drain protection: stop polling after a sustained idle wait so the bus
// idles and the 12V recovers (session is preserved; resumes on passive wake). First wait
// gets a responsive window; a wait re-entered after a prior sleep re-sleeps quickly to keep
// the oscillation duty-cycle low. See HandleChargeWaitState().
static const int CHARGE_WAIT_SLEEP_SECS   = 600;  // first wait, no charge -> sleep
static const int CHARGE_WAIT_RESLEEP_SECS = 15;   // re-entered after a wait-sleep -> sleep fast

// Aux-12V rising-edge wake detection (volts above the learned 12V reference). The latch sets
// above ref+SET and clears only below ref+CLEAR; the hysteresis band stops a level hovering at
// the threshold (ADC noise) from producing repeated edges/CAN resets. See HandleSleepState().
static const float AUX_12V_WAKE_SET_V   = 0.2f;   // v12 > ref + this  -> rising edge / latch high
static const float AUX_12V_WAKE_CLEAR_V = 0.1f;   // v12 < ref + this  -> latch clears

// AWAKE with the charge door never opening for this long forces sleep (door-watch timeout).
//
// This watchdog exists because AWAKE cannot exit on its own via the IsBusAlive() stale path.
// SLEEP -> AWAKE is driven by external CAN2 traffic, but once AWAKE the poller is itself
// transmitting on that bus and the ECU replies are CAN2 frames, so our own polling keeps
// refreshing m_last_can2_time and the 120s bus-liveness window never expires. Without this
// timer a vehicle that never reports CS_DRIVING and never gets a cable plugged in would stay
// awake indefinitely and drain the 12V battery. It must stay paired with the cooldown latch,
// or the next poll reply after the forced sleep bounces straight back to AWAKE. Anything that
// reduces poll throttling, adds PIDs to the AWAKE column of obdii_polls_base[], or shortens
// this timeout needs to be weighed against that drain.
static const int AWAKE_NO_ACTIVITY_TIMEOUT_S = 300;

void OvmsVehicleToyotaETNGA::ResetSleepBackoff()
{
    m_sleep_backoff_idx = 0;
}

void OvmsVehicleToyotaETNGA::HandleSleepState()
{
    int monotonic = StandardMetrics.ms_m_monotonic->AsInt();
    
    if (!m_allow_wake)
    {
        if ((monotonic - m_sleep_entry_time) > m_sleep_cooldown_secs)
        {
            ESP_LOGI(TAG, "Cooling off period ended (%ds), allowing wake", m_sleep_cooldown_secs);
            m_allow_wake = true;
        }
    }

    if (IsBusAlive()) {
        // There is life.
        TransitionToAwakeState();
    } else {
        // 12V wake is RISING-EDGE only. Level-triggering here oscillated: the post-drive/
        // post-charge surface charge keeps 12V above ref+0.2 for a long time with a dead
        // bus, so every SLEEP tick reset the backoff, reset the CAN controller and bounced
        // to AWAKE — a ~2s SLEEP/AWAKE loop (one CAN reset per cycle) whose sleep re-entry
        // also restarted the cooldown, so frame-wake never re-enabled until 12V decayed.
        float v12 = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
        float ref = StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat();
        bool high = v12 > ref + AUX_12V_WAKE_SET_V;
        if (high && !m_12v_was_high) {
            // Voltage just jumped: real power-up (DC-DC came on) — wake even during cooldown.
            ESP_LOGI(TAG, "Aux 12V has exceeded the threshold");
            ResetSleepBackoff();
            // Send a CAN reset.
            esp_err_t result = m_can2->Reset();
            if (result == ESP_OK) {
                ESP_LOGI(TAG, "CAN bus reset successfully");
            } else {
                ESP_LOGE(TAG, "CAN bus reset failed, error code: %d", result);
            }
            // Lift the cooldown gate so incoming CAN frames can hold us awake; without this
            // a 12V wake during cooldown bounced straight back to sleep even with the
            // vehicle genuinely on (IncomingFrameCan2 was still discarding frames).
            m_allow_wake = true;
            TransitionToAwakeState();
        }
        // Hysteresis on the latch: set above ref+SET, clear only below ref+CLEAR — a level
        // hovering at the threshold (ADC noise) must not produce repeated edges/CAN resets.
        if (high)
            m_12v_was_high = true;
        else if (v12 < ref + AUX_12V_WAKE_CLEAR_V)
            m_12v_was_high = false;
    }
}

void OvmsVehicleToyotaETNGA::HandleAwakeState()
{
    int monotonic = StandardMetrics.ms_m_monotonic->AsInt();

    // INC-3: deferred-report completion check. When a fault dump was in flight at session
    // close, we hold off on GenerateChargeReport until all DIDs are captured (or a timeout
    // elapses). This runs first so normal AWAKE handling (including the PISW-reconcile
    // below) cannot double-finalize while the deferred report is still pending.
    if (m_report_pending) {
        if (m_dump_remaining.load() <= 0 || monotonic >= m_report_pending_deadline) {
            ESP_LOGI(TAG, "Fault dump done (or timed out) — finalizing deferred report");
            FinalizeChargeSession();
            m_report_pending = false;
        }
        return;   // hold normal AWAKE handling (incl. the PISW-reconcile) until the deferred report is written
    }

    // Wake-reconcile: if a charge session was open and the cable is now gone (confirmed
    // fresh read — not stale/transient), the cable was removed while we slept.
    // Guard: only act on a non-stale PISW value to avoid the ~30s OBC post-wake transient
    // where PISW may briefly report 0x00 before the OBC fully wakes.
    // REGRESSION DEPENDENCY: this requires PID_PISW_STATUS to be polled in the AWAKE
    // column (obdii_polls_base[] in vehicle_toyota_etnga.cpp, AWAKE=5s). Without an AWAKE poll
    // the cached PISW value is stale (gap >120s) or the pre-sleep connected value
    // (gap <120s), so a fresh 0x00 never arrives and this reconcile can NEVER fire —
    // leaking in_session and blocking the next session's open-guard. Do not remove the
    // AWAKE PISW poll without replacing this reconcile trigger.
    // Debounce the cable-removed read: the OBC can briefly report PISW=0x00 for ~30s after
    // wake before it is fully up. Require two consecutive fresh 0x00 reads before finalizing,
    // so a wake-from-CHARGE_WAIT-sleep transient does not falsely close a still-plugged session.
    // Count only DISTINCT fresh PISW polls. This handler runs every 1s, but PISW is polled
    // @5s in AWAKE and the metric stays non-stale for ~120s, so without this gate one cached
    // 0x00 would be counted on every tick and trip the debounce in ~2s. LastModified advances
    // on every poll (even an unchanged value), so a new timestamp == a new reading. The
    // debounce therefore means "two consecutive ~5s AWAKE polls read 0x00", per the design.
    uint32_t pisw_ts = m_v_charge_pisw_raw->LastModified();
    if (!m_v_charge_pisw_raw->IsStale() && pisw_ts != m_pisw_last_modified) {
        m_pisw_last_modified = pisw_ts;
        if (m_v_charge_pisw_raw->AsInt() == 0x00) {
            if (m_pisw_zero_count < 2) m_pisw_zero_count++;   // only need 2; don't grow unbounded
        } else {
            m_pisw_zero_count = 0;
        }
    }
    if (m_charge_session.in_session && m_pisw_zero_count >= 2) {
        // Session ended while we slept (cable removed during the sleep gap).
        // Close the session the same way TransitionToAwakeState does: flush + report.
        // Resetting without the report silently discarded the whole session (and the
        // streamed CSV was then deleted as an orphan by PruneChargeReports).
        ESP_LOGI(TAG, "Charge session ended during sleep — finalizing");
        StandardMetrics.ms_v_charge_state->SetValue("done");
        LogChargeEvent("Unplugged (during sleep)");
        if (m_dump_remaining.load() > 0) {       // a fault dump is still capturing — wait for it
            m_report_pending = true;
            m_report_pending_deadline = monotonic + DUMP_WAIT_SECS;
            ESP_LOGI(TAG, "Report deferred — waiting for fault dump (%d DIDs left)", m_dump_remaining.load());
            m_pisw_zero_count = 0;   // consumed — reset the debounce
            return;   // exit now; next tick will complete via m_report_pending check at top
        } else {
            FinalizeChargeSession();
        }
        m_pisw_zero_count = 0;   // consumed — reset the debounce
        // fall through to normal AWAKE handling (finalize path only)
    }

    if (!IsBusAlive()) {
        // No CAN communication - bus is dead, go to sleep
        ESP_LOGI(TAG, "CAN bus idle (no recent CAN2 frames) — sleeping");
        TransitionToSleepState();
        return;
    }
    else if (m_s_controlstate->AsInt() == ControlState::CS_DRIVING) {
        // HV system is up — vehicle is READY/driving
        TransitionToDrivingState();
        return;
    }

    // PISW-based arm logic (mirrors sim S.AWAKE branch)
    int pisw = m_v_charge_pisw_raw->AsInt();
    bool lid_open = StandardMetrics.ms_v_door_chargeport->AsBool();

    // pisw is polled @5s in AWAKE (obdii_polls_base[]; also required by the wake-reconcile
    // above), so a seated cable is detected here directly. The lid-open arm logic below
    // only bounds how long we stay awake waiting for a plug-in.
    if (pisw >= 0x02) {
        // Cable is seated. If a session is already open we are resuming after a CHARGE_WAIT
        // sleep — go straight back to CHARGE_WAIT (the engage-watch + direct AC/DC checks there
        // catch the charge when it starts) and let Tier 2 re-sleep. Only a genuinely new
        // plug-in (no open session) runs the full handshake negotiation.
        if (m_charge_session.in_session) {
            TransitionToChargeWaitState();
        } else {
            TransitionToChargeHandshakeState();
        }
        return;
    }
    if (!m_armed_for_charge) {
        if (lid_open) {
            // Charge door just opened: arm and start the 15-min cable watch
            m_armed_for_charge = true;
            m_cable_watch_start = monotonic;
            ResetSleepBackoff();   // deliberate user action — resume responsive cooldowns
        } else if (monotonic - m_awake_entered > AWAKE_NO_ACTIVITY_TIMEOUT_S) {
            // Door watch expired (5 min in AWAKE, charge door never opened) → sleep
            ESP_LOGI(TAG, "Vehicle awake for over %ds with no activity — forcing sleep state",
                     AWAKE_NO_ACTIVITY_TIMEOUT_S);
            TransitionToSleepState();
            return;
        }
    } else if (monotonic - m_cable_watch_start > 900) {
        // Cable watch expired (15 min armed, no cable plug-in) → sleep
        ESP_LOGI(TAG, "Armed 15min, no cable plug-in — giving up");
        TransitionToSleepState();
        return;
    }
}

void OvmsVehicleToyotaETNGA::HandleDrivingState()
{
    if (m_s_controlstate->AsInt() != ControlState::CS_DRIVING) {
        TransitionToAwakeState();
        SetReadyStatus(false);
        SetVehicleSpeed(0);
        SetShiftPosition(0);
        StandardMetrics.ms_v_env_temp->Clear();
        StandardMetrics.ms_v_env_cabintemp->Clear();
    }
}

void OvmsVehicleToyotaETNGA::HandleChargeHandshakeState()
{
    int pisw = m_v_charge_pisw_raw->AsInt();
    int ac_op = m_v_charge_ac_op->AsInt();
    int hlc = m_v_charge_hlc->AsInt();
    int monotonic = StandardMetrics.ms_m_monotonic->AsInt();

    if (pisw == 0x00) {
        // Premature unplug — re-arm logic handled in TransitionToAwakeState
        TransitionToAwakeState();
        return;
    }
    if (hlc >= 0x0A && hlc <= 0x12) {
        // DC HLC sequence active
        TransitionToChargeDcState();
        return;
    }
    // HANDSHAKE: only ac_op==0x02 (Running), NOT 0x01 (Startup) — avoids premature
    // AC entry during negotiation. (CHARGE_WAIT deliberately accepts 0x01 too.)
    if (ac_op == 0x02) {
        TransitionToChargeAcState();
        return;
    }
    if (monotonic - m_charge_state_entry >= 60 && ac_op == 0x00 && pisw >= 0x02) {
        // Scheduled-wait heuristic: AC Op stuck at Stop for 60s with cable present
        TransitionToChargeWaitState();
        return;
    }
}

void OvmsVehicleToyotaETNGA::HandleChargeWaitState()
{
    int pisw = m_v_charge_pisw_raw->AsInt();
    int ac_op = m_v_charge_ac_op->AsInt();
    int hlc = m_v_charge_hlc->AsInt();

    if (pisw == 0x00) {
        // Cable removed — session ended
        TransitionToAwakeState();
        return;
    }
    if (ac_op == 0x01 || ac_op == 0x02) {
        // EVSE engaged (Startup or Running)
        TransitionToChargeAcState();
        return;
    }
    if (hlc >= 0x0A && hlc <= 0x12) {
        // DC engaged
        TransitionToChargeDcState();
        return;
    }
    if (!IsBusAlive()) {
        // Bus went dead during scheduled wait (OBC slept or gateway isolated OBD)
        TransitionToSleepState();
        return;
    }
    // 12V-drain protection: if we have sat in CHARGE_WAIT without charge engaging for the
    // threshold, stop polling and sleep so the bus idles and 12V recovers. The session is
    // preserved across the sleep and resumes on passive wake (see HandleAwakeState). A wait
    // re-entered after a prior sleep uses the short threshold to keep oscillation cheap.
    int monotonic = StandardMetrics.ms_m_monotonic->AsInt();
    int sleep_after = m_charge_wait_slept ? CHARGE_WAIT_RESLEEP_SECS : CHARGE_WAIT_SLEEP_SECS;
    if (monotonic - m_charge_state_entry >= sleep_after) {
        ESP_LOGI(TAG, "CHARGE_WAIT idle %ds — sleeping to protect 12V", monotonic - m_charge_state_entry);
        m_charge_wait_slept = true;
        TransitionToSleepState();
        return;
    }
}

void OvmsVehicleToyotaETNGA::HandleChargeAcState()
{
    // Lock isolation note: locking the car during AC charging causes sustained OBC poll
    // timeouts (gateway isolates OBD from OBC).  We do NOT tear down the session on
    // timeouts — only an explicit fresh PISW=Unconnected (0x00) read or a clean phase-end
    // (ac_op==0x00) terminates the session.  On unlock the OBC answers again and these
    // handlers resume normally.  Do not add timeout-based session teardown here.
    UpdateChargeSessionStats();   // aggregate peak power / temp range / type for the session report

    // Keep charge-state metrics fresh (they're otherwise set only on transition → go stale
    // at 120s, and a mid-charge reboot leaves the Status page showing "Not charging").
    if (StandardMetrics.ms_v_charge_inprogress->Age() > 60) {
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_charge_state->SetValue("charging");
    }

    int pisw = m_v_charge_pisw_raw->AsInt();
    int ac_op = m_v_charge_ac_op->AsInt();

    if (ac_op == 0x00) {
        // AC Op = Stop: phase ended
        TransitionToChargeWaitState();
        return;
    }
    if (pisw == 0x00) {
        // Cable pulled mid-charge
        TransitionToChargeWaitState();
        return;
    }
}

void OvmsVehicleToyotaETNGA::HandleChargeDcState()
{
    // Lock isolation note: same as HandleChargeAcState — sustained OBC timeouts while
    // in_session do NOT tear down the session.  Only an explicit fresh PISW=Unconnected
    // (0x00) read or hlc==0xFF (HLC Unconnected) terminates the DC phase.  On unlock the
    // OBC answers again and polling resumes.  Do not add timeout-based session teardown here.
    UpdateChargeSessionStats();   // aggregate peak power / temp range / type for the session report

    // Keep charge-state metrics fresh (they're otherwise set only on transition → go stale
    // at 120s, and a mid-charge reboot leaves the Status page showing "Not charging").
    if (StandardMetrics.ms_v_charge_inprogress->Age() > 60) {
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_charge_state->SetValue("charging");
    }

    int pisw = m_v_charge_pisw_raw->AsInt();
    int hlc = m_v_charge_hlc->AsInt();

    if (hlc == 0xFF) {
        // HLC = Unconnected (255): DC phase ended
        TransitionToChargeWaitState();
        return;
    }
    if (pisw == 0x00) {
        // Cable pulled mid-DC
        TransitionToChargeWaitState();
        return;
    }
}

void OvmsVehicleToyotaETNGA::TransitionToSleepState()
{
    int monotonic = StandardMetrics.ms_m_monotonic->AsInt();
    m_armed_for_charge = false;
    // Arm the escalating cooldown: ignore CAN-frame wakes for the current window, then step
    // the index up (clamped) so the next consecutive no-activity sleep waits longer.
    // ResetSleepBackoff() returns the index to 0 on real activity.
    m_sleep_entry_time = monotonic;
    m_sleep_cooldown_secs = SLEEP_COOLDOWN_SECS[m_sleep_backoff_idx];
    m_allow_wake = false;
    // Seed the 12V edge latch from the current reading: entering sleep with 12V still
    // elevated (post-drive/post-charge surface charge) must not count as a rising edge.
    m_12v_was_high = StandardMetrics.ms_v_bat_12v_voltage->AsFloat()
                     > (StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat() + AUX_12V_WAKE_SET_V);
    if (m_sleep_backoff_idx < SLEEP_COOLDOWN_STEPS - 1)
        m_sleep_backoff_idx++;
    SetPollState(PollState::SLEEP);   // also sets v.e.awake = false
    m_last_can2_time = 0;             // force rising-edge: next wake needs a fresh CAN2 frame
    // 12V aux metrics come only from the EV ECU (answers in READY/charging); clear the
    // PID-sourced values when leaving those states so stale readings aren't shown while off.
    StandardMetrics.ms_v_bat_12v_current->Clear();
    StandardMetrics.ms_v_env_aux12v->SetValue(false);
    m_v_bat_12v_voltage_pid->Clear();
    m_v_bat_12v_temp->Clear();
    m_v_bat_12v_cac->Clear();
    m_v_bat_12v_charge_ah->Clear();
    m_v_bat_12v_discharge_ah->Clear();
    m_v_bat_12v_readyon_h->Clear();
}

void OvmsVehicleToyotaETNGA::TransitionToAwakeState()
{
    int monotonic = StandardMetrics.ms_m_monotonic->AsInt();
    PollState oldState = static_cast<PollState>(m_poll_state);
    // If bouncing back from CHARGE_HANDSHAKE (premature unplug / DCFC retry dance),
    // re-arm and restart the cable watch so the next plug-in gets a fresh 15-min window.
    // Check oldState BEFORE calling SetPollState (sim: armed_for_charge=True on HANDSHAKE→AWAKE).
    if (oldState == PollState::CHARGE_HANDSHAKE) {
        m_armed_for_charge = true;
        m_cable_watch_start = monotonic;
        LogChargeEvent("Unplug bounce — re-armed (DCFC retry)");
        ESP_LOGD(TAG, "Re-armed after HANDSHAKE→AWAKE bounce (DCFC retry)");
    }
    // For all other transitions into AWAKE (SLEEP→AWAKE, DRIVING→AWAKE),
    // arm state is reset by TransitionToSleepState / TransitionToDrivingState.
    SetPollState(PollState::AWAKE);
    m_awake_entered = monotonic;
    // 12V aux metrics come only from the EV ECU (answers in READY/charging); clear the
    // PID-sourced values when leaving those states so stale readings aren't shown while off.
    StandardMetrics.ms_v_bat_12v_current->Clear();
    StandardMetrics.ms_v_env_aux12v->SetValue(false);
    m_v_bat_12v_voltage_pid->Clear();
    m_v_bat_12v_temp->Clear();
    m_v_bat_12v_cac->Clear();
    m_v_bat_12v_charge_ah->Clear();
    m_v_bat_12v_discharge_ah->Clear();
    m_v_bat_12v_readyon_h->Clear();
    // Leaving the charge sub-machine: close the session. ms_v_charge_state is "done" only
    // when energy was being delivered (AC/DC — normally these exit via CHARGE_WAIT; the
    // direct path is a safety net); HANDSHAKE/WAIT exits just clear it.
    if (oldState >= PollState::CHARGE_HANDSHAKE) {
        bool delivered = (oldState == PollState::CHARGE_AC || oldState == PollState::CHARGE_DC);
        StandardMetrics.ms_v_charge_state->SetValue(delivered ? "done" : "");
        StandardMetrics.ms_v_charge_mode->SetValue("");   // clear AC/DC indicator on session end
        StandardMetrics.ms_v_charge_type->SetValue("");   // clear connector type (DC sets "ccs" via state; AC safety-net)
        if (m_charge_session.in_session) {
            ESP_LOGI(TAG, "Charge session closed");
            LogChargeEvent("Unplugged");
            if (m_dump_remaining.load() > 0) {       // a fault dump is still capturing — wait for it
                m_report_pending = true;
                m_report_pending_deadline = monotonic + DUMP_WAIT_SECS;
                ESP_LOGI(TAG, "Report deferred — waiting for fault dump (%d DIDs left)", m_dump_remaining.load());
            } else {
                FinalizeChargeSession();
            }
        } else {
            m_charge_session = ChargeSessionState{};   // not in session — nothing to finalize
        }
        m_charge_wait_slept = false;   // leaving the charge sub-machine — clear wait flag
    }
}

void OvmsVehicleToyotaETNGA::TransitionToDrivingState()
{
    m_armed_for_charge = false;
    ResetSleepBackoff();   // real drive — resume responsive cooldowns
    SetPollState(PollState::DRIVING);
    m_trip_start_valid = false;
    RequestVIN();
}

void OvmsVehicleToyotaETNGA::TransitionToChargeHandshakeState()
{
    m_armed_for_charge = false;  // arm state consumed on entering handshake
    m_charge_state_entry = StandardMetrics.ms_m_monotonic->AsInt();
    SetPollState(PollState::CHARGE_HANDSHAKE);
    SetChargingStatus(false);    // not yet delivering energy (AC/DC states set true)
    SetChargeState(PollState::CHARGE_HANDSHAKE);

    // We only get here on a seated PISW (>= 0x02), which means the cable is in and the lid
    // is physically open.  Latch it: the lid (0x1625) polls @10s in AWAKE and never in the
    // charge states, so a plug-in that wakes us from SLEEP arrives here on the @5s PISW read
    // before the lid is ever sampled — without this, v.d.cp stays false for the whole session
    // and the base CommandStat (gated on ms_v_door_chargeport) prints "Not charging".
    // ResetStaleMetrics only forces the lid closed below CHARGE_HANDSHAKE, so this holds until
    // the session ends and AWAKE resumes polling 0x1625.
    SetChargingDoorStatus(true);

    if (!m_charge_session.in_session) {
        ResetSleepBackoff();   // brand-new charge session — resume responsive cooldowns
        m_charge_wait_slept = false;   // brand-new session — fresh responsive wait window
        m_pisw_zero_count = 0;   // fresh session — clear cable-removed debounce
        m_pisw_last_modified = 0;   // fresh session — clear the distinct-poll tracker too
        m_charge_session.in_session = true;
        m_charge_session.start_monotonic = StandardMetrics.ms_m_monotonic->AsInt();
        m_charge_session.start_utc = StandardMetrics.ms_m_timeutc->AsInt();
        m_charge_session.start_soc = (int) StandardMetrics.ms_v_bat_soc->AsFloat();
        m_charge_session.start_soc_bms = m_v_bat_soc_bms->IsDefined() ? m_v_bat_soc_bms->AsFloat() : -1.0f;
        // Session energy counters reset HERE (session open), NOT in NotifyChargeStart:
        // the framework fires that hook on every ms_v_charge_state change to "charging",
        // including a CHARGE_WAIT pause/resume mid-session, which wiped the energy
        // accumulated in earlier phases (and made the report's kWh phase-local).
        StandardMetrics.ms_v_bat_energy_used->SetValue(0);
        StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
        lastBatteryEnergyLogTime = 0;
        StandardMetrics.ms_v_charge_kwh->SetValue(0);
        lastChargerEnergyLogTime = 0;
        StandardMetrics.ms_v_charge_kwh_grid->SetValue(0);
        lastGridEnergyLogTime = 0;
        m_v_env_hvac_kwh->SetValue(0);
        lastHvacEnergyLogTime = 0;
        if (StandardMetrics.ms_v_pos_gpslock->AsBool()) {
            m_charge_session.has_loc = true;
            m_charge_session.start_lat = StandardMetrics.ms_v_pos_latitude->AsFloat();
            m_charge_session.start_lon = StandardMetrics.ms_v_pos_longitude->AsFloat();
        }
        // Seed the ambient range only from a fresh reading. env temp is Clear()ed when the
        // car parks, so seeding unconditionally produced a bogus "0 -> 0 C". The in-charge
        // 0x1F46 poll (enabled in the poll list) fills the range as readings arrive.
        if (StandardMetrics.ms_v_env_temp->IsDefined()) {
            float amb = StandardMetrics.ms_v_env_temp->AsFloat();
            m_charge_session.amb_seen = true;
            m_charge_session.amb_min = m_charge_session.amb_max = amb;
        }
        m_charge_session.svg_interval_s = 20;
        m_charge_session.last_sample_monotonic = 0;
        m_charge_session.last_svg_monotonic = 0;
        // basename = "<dir>/<UTC timestamp>" (no extension); files are <base>.html / <base>.csv
        {
            char ts[40];
            time_t utc = m_charge_session.start_utc;
            if (utc > 1000000000) {
                time_t st = utc; struct tm tmv; gmtime_r(&st, &tmv);
                strftime(ts, sizeof(ts), "%Y%m%dT%H%M%SZ", &tmv);
            } else {
                snprintf(ts, sizeof(ts), "charge-%d", m_charge_session.start_monotonic);
            }
            mkdir(ChargeReportDir().c_str(), 0755);
            m_charge_session.base = ChargeReportDir() + "/" + ts;
        }
        // INC-3: dump state is session-scoped — clear at session open so a fault dump from a
        // prior (possibly <0.05 kWh, report-skipped) session can never leak into this report.
        {
            OvmsMutexLock lock(&m_dump_mutex);
            m_dump_results.clear();
        }
        m_dump_phase_idx = -1;
        m_dump_outcome = -1;
        m_dump_remaining = 0;
        m_charge_fault_pending = false;
        m_charge_engaged = false;   // no charge has engaged yet this session — see PID_CHARGE_HISTORY
        LogChargeEvent("Plugged in — handshake");
        ESP_LOGI(TAG, "Charge session opened (SOC %d%%)", m_charge_session.start_soc);
    }
    RequestVIN();
}

void OvmsVehicleToyotaETNGA::TransitionToChargeWaitState()
{
    m_charge_state_entry = StandardMetrics.ms_m_monotonic->AsInt();
    SetPollState(PollState::CHARGE_WAIT);
    SetChargingStatus(false);
    SetChargeState(PollState::CHARGE_WAIT);
    LogChargeEvent("Charging paused / phase ended");
    CloseChargePhase();       // INC-1: close the active phase (pause / phase end)
    MaybeStartChargeFaultDump();   // INC-3: if a fault was flagged, snapshot the OBC diagnostic DIDs
}

void OvmsVehicleToyotaETNGA::TransitionToChargeAcState()
{
    m_charge_state_entry = StandardMetrics.ms_m_monotonic->AsInt();
    m_charge_wait_slept = false;   // real charge engaged — end the wait
    m_charge_engaged = true;       // ...and from here a 0x1688 fault code can be believed
    SetPollState(PollState::CHARGE_AC);
    SetChargingStatus(true);
    SetChargeState(PollState::CHARGE_AC);
    StandardMetrics.ms_v_charge_mode->SetValue("standard");      // AC charging (OVMS-standard v.c.mode)
    LogChargeEvent("AC charging started");
    OpenChargePhase(false);   // INC-1: AC phase
}

void OvmsVehicleToyotaETNGA::TransitionToChargeDcState()
{
    m_charge_state_entry = StandardMetrics.ms_m_monotonic->AsInt();
    m_charge_wait_slept = false;   // real charge engaged — end the wait
    m_charge_engaged = true;       // ...and from here a 0x1688 fault code can be believed
    SetPollState(PollState::CHARGE_DC);
    SetChargingStatus(true);
    SetChargeState(PollState::CHARGE_DC);
    StandardMetrics.ms_v_charge_mode->SetValue("performance");   // DC fast charge -> ABRP is_dcfc (v.c.mode == "performance")
    // v.c.type is the connector/standard axis (type1/type2/ccs/...), separate from v.c.mode.
    // 0x161C only encodes the AC voltage type (reads 00 on DC, and isn't polled in CHARGE_DC),
    // so the DC connector can't come from the poll — set it from the state we already detect.
    // The e-TNGA DC inlet is CCS. Cleared on session end in TransitionToAwakeState.
    StandardMetrics.ms_v_charge_type->SetValue("ccs");
    LogChargeEvent("DC charging started");
    OpenChargePhase(true);    // INC-1: DC phase
}
