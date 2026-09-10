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

#ifndef __VEHICLE_VWEGOLF_BAT_CTRL_H__
#define __VEHICLE_VWEGOLF_BAT_CTRL_H__

#include "can.h"
#include "egolf/battery_control.h"  // BatteryControl (LSG 0x25) BAP command + status codec

// Climate uses a dedicated, non-colliding CAN-NM wake from a SPARE (unused) node id
// instead of impersonating the real OCU. On a car with a live OCU, sending the OCU's
// node-0x67 NM frame and/or its 0x5A7 heartbeat collides with it (same id, different data)
// and left OCU DTCs U001100/U120100 in this car's fault memory; this climate path does
// neither. Once the battery-control unit (BCU) accepts the BAP climate command it and the
// comfort/EV cluster sustain their own NM, so the wake is only held as a short bridge and
// then released.
#define VWEGOLF_NM_WAKE_NODE       0x7D  // spare node id (verified unused in captures)
#define VWEGOLF_BATCTRL_WAKE_SECS  20    // max seconds to sustain the NM-wake bridge
#define VWEGOLF_BATCTRL_RETRY_SECS 2     // backstop: re-send a step only if its reply is lost this long
// Lost-reply backstop toggle. 1 = re-send the current step if its reply is lost (robust: replies and
// frames can be dropped on the comfort bus). 0 = single-shot (send each step once, never re-send),
// which distinguishes a slow BCU from a bus-swallowed frame.
#define VWEGOLF_BATCTRL_RETRY_ENABLED 1

// Self-contained e-Golf BatteryControl (LSG 0x25) controller: remote climate (pre-heat /
// pre-cool) AND charge (start/stop, set charge-current).
//
// Climate and charge share the ENTIRE BAP command path — profile 0 is a single scratchpad and
// the immediate trigger acts on whatever op bits it holds. On the car the two CAN run at the same
// time (each is a separate arm+trigger), but this controller serializes the command SEQUENCES (one
// arm+trigger in flight at a time; see BcOp) — they differ only in the arm's op bits and the
// confirm. Owns ALL of that state so vehicle_vwegolf.cpp stays free of BCU logic — it
// only forwards a per-second tick and two RX frames. Independent of the OCU 0x5A7 heartbeat
// that vehicle_vwegolf.cpp uses for the body commands (mirror/lock/horn): this rides its own
// spare-node NM wake and never touches m_ocu_active.
//
// EVENT-DRIVEN: the command is a small state machine advanced by the BCU's own BAP replies
// on 0x17332510, NOT by blind per-tick re-sending. Command() arms the wake bridge and
// returns; the first BCU frame heard sends the channel-open handshake; the handshake ack
// (49 41/49 42) then releases the profile GET; the profile-array reply drives the
// read-modify-write arm + trigger; the OperationMode echo (49 58 <flag>) confirms. Waiting
// for the handshake ack before the GET is REQUIRED: the BCU silently drops a function GET
// that arrives before it has acked registration (confirmed on-car). Ticker1 only (a) holds
// the NM-wake bridge up ~1 Hz and (b) re-sends the current step if its reply is lost
// (VWEGOLF_BATCTRL_RETRY_SECS) — so each step fires the instant its predecessor is
// acknowledged rather than waiting for the next tick, and profile 0 is written exactly once
// (re-writing it on every tick would reset the BCU's start and stretch the confirmation).
//
// THREADING: this state machine is touched from three OVMS tasks — the vehicle CAN-RX task
// (IncomingBapStatus, via IncomingFrameCan3), the events task (Ticker1, the "ticker.1" event),
// and the command task (Climate/Charge/SetChargeCurrent -> Begin). It is deliberately lock-free:
// the flow is event-driven off the ~1 Hz tick and the BCU's replies, so the only overlap is a lost-
// reply retry firing in the same ~ms a reply lands. That can at worst emit a duplicate/garbled RA0
// write, which the BCU rejects and the next retry re-sends — it can NOT exceed the maxCurrent hard
// cap (SendArm clamps a LOCAL Profile copy, so every emitted byte is <= 0x20 regardless of
// interleaving) and is not otherwise harmful. Matches the no-lock ticker-vs-RX convention of the
// other OVMS vehicle modules; add a critical section here if a future op makes a torn read unsafe.
class VWeGolfBatteryControl {
 public:
    // bus = the KCAN / comfort bus (can3) the BCU lives on. Must outlive this object.
    void SetBus(canbus* bus) { m_bus = bus; }

    // Entry points — each arms the wake bridge and kicks the first wake, then returns immediately
    // (true = request accepted); the sequence runs event-driven as the BCU responds. Only ONE command
    // runs at a time (see BcOp); on the car climate and charge can be active simultaneously.
    bool Climate(bool enable);             // remote pre-heat / pre-cool  (on-car validated)
    bool Charge(bool enable);              // remote charge start / stop  (on-car validated on a 2020 e-Golf.
                                           // The immediate charge trigger has no factory reference — the
                                           // car's own UI only edits timers. Gated no-arm->no-trigger like
                                           // climate. STOP is op-specific: OFF arms the pure-charge op
                                           // first, so it's blocked while climate is on.)
    bool SetChargeCurrent(uint16_t amps);  // set the charge-current limit (persistent settings edit via
                                           // profile-0 RMW, NO trigger). Amps are snapped to the car's
                                           // allowed steps {5,10,13,32} and hard-capped at 0x20 (see .cpp).

    // Read-only: fetch the full charge-profile array (all "charge locations", profiles 0-3) and
    // publish them for the CLI. Runs the same wake -> handshake -> profile-array GET path as the
    // climate/charge commands but STOPS at the read — it never arms, triggers, or writes a profile,
    // so it is safe on any car. Refused (returns false) if another command is already in flight.
    bool ListProfiles();

    // Result of the most recent ListProfiles(). The RX task publishes LIST_READY/LIST_FAILED; the
    // command task polls this. LIST_PENDING while the read is in flight, LIST_NONE for other ops.
    enum ListState : uint8_t { LIST_NONE = 0, LIST_PENDING, LIST_READY, LIST_FAILED };
    ListState ListStatus() const { return m_list_state; }

    // Copy the profiles captured by the last successful ListProfiles() into out[0..maxOut). Returns
    // the number written. Meaningful once ListStatus() == LIST_READY.
    uint8_t GetProfiles(bap::egolf::Profile* out, uint8_t maxOut) const;

    // One profile-field edit for SetProfile(): a bitmask of the fields to change plus their new values.
    // The RMW reads the whole record back and overwrites ONLY the flagged fields, so every unmodeled
    // byte and the name are preserved verbatim. Which fields apply is per-profile and enforced by the
    // CLI (profile 0 = current/minSoC/temp; charge locations 1-3 = flags/current/targetSoC); this struct
    // itself just carries whatever the caller flagged.
    struct ProfileEdit {
        enum Field : uint8_t {
            F_CURRENT = 0x01,  // maxCurrent (amps; hard-clamped <= 0x20 on write)
            F_MINSOC  = 0x02,  // minChargeLevel %   (profile 0 "Optionen" only)
            F_TGTSOC  = 0x04,  // targetChargeLevel % (charge locations only)
            F_TEMP    = 0x08,  // temperatureRaw     (profile 0 "Optionen" only)
            F_OP      = 0x10,  // operation charge/climate bits (charge locations only)
        };
        uint8_t fields = 0;               // OR of Field bits actually being written
        uint8_t maxCurrent = 0;
        uint8_t minChargeLevel = 0;
        uint8_t targetChargeLevel = 0;
        uint8_t temperatureRaw = 0;
        uint8_t operation = 0;            // desired PO_CHARGING/PO_CLIMATE bits (with F_OP)
    };

    // Write one or more fields of charge profile `pos` (0 = global "Optionen", 1.. = charge locations)
    // via a full-record read-modify-write: read the profile array, overwrite only the fields flagged in
    // `edit`, keep every other byte (incl. the name), and write it back — NO trigger (this is a settings
    // edit, not a start). Confirmed on the BCU's write echo. Reports via ListStatus()/LIST_*. Refused
    // (false) while another command is in flight or if `edit.fields` is empty. maxCurrent is hard-clamped
    // to 0x20 regardless of the requested value (a higher value bricks the car's charging).
    bool SetProfile(uint8_t pos, const ProfileEdit& edit);

    // Profiles ("charge locations") are a VARIABLE-length array (global "Optionen" + N named
    // locations); cap the read at what the 224-byte status reassembler can hold. NOT the timer count.
    static constexpr uint8_t kMaxProfiles = 8;
    // Departure timers are a fixed slot family (func 0x14..). A DIFFERENT data model from profiles:
    // each timer binds to a profile by refId. The e-Golf exposes **3** slots (funcs 0x14/0x15/0x16) —
    // its UI + every capture only ever show timers 1-3 (slot 4 / func 0x17 never broadcasts). See
    // egolf::Timer / funcForTimerSlot.
    static constexpr uint8_t kNumTimers = 3;

    // ---- Departure timers (xvg charge timer ...) ------------------------------------------------
    // All read/publish through the same wake->handshake path as the profile ops and report via
    // ListStatus()/LIST_* (one command in flight at a time; refused with false while busy).
    //
    // Read-only: collect the 4 timer slots + the TimerState enable mask (from the BCU's periodic
    // HEARTBEAT/STATUS broadcast on 0x17332510). Publishes LIST_READY when collected.
    bool ListTimers();
    // Write a recurring departure timer into `slot` (1..4) and enable it. `t` is a full 8-byte record
    // (date bytes 0xFF for recurring; weekday mask; refId = the bound profile 0..3). The record is
    // written on 0x17332501 (our KCAN command id); the slot is then enabled via an RMW of the mask.
    bool SetTimer(uint8_t slot, const bap::egolf::Timer& t);
    // Enable/disable a slot: RMW the TimerState mask only (no record write).
    bool EnableTimer(uint8_t slot, bool enable);
    // Clear a slot: write an inert recurring record (no weekdays -> never fires) and disable it.
    bool ClearTimer(uint8_t slot);
    // Copy the timers captured by the last ListTimers() into out[0..min(maxOut,kNumTimers)); sets
    // enabledMask and seen[i] (whether slot i broadcast a record). Meaningful once LIST_READY.
    uint8_t GetTimers(bap::egolf::Timer* out, uint8_t maxOut, uint8_t& enabledMask, bool* seen) const;
    // Whether the TimerState enable mask was actually read this command. If false, enabledMask is the
    // reset default (0) and must NOT be shown as "all disabled" — the enable state is simply unknown.
    bool TimerMaskSeen() const { return m_timer_mask_seen; }

    // Per-second tick, forwarded from OvmsVehicleVWeGolf::Ticker1. Holds the NM-wake bridge
    // and backstops lost replies. bus_alive = KCAN has had live traffic within the timeout.
    void Ticker1(bool bus_alive);

    // Feed the BatteryControl BAP status frame (0x17332510) — this DRIVES the state machine
    // (channel open, profile read + arm, command confirmation, HVAC state).
    void IncomingBapStatus(const CAN_frame_t* p_frame);
    // Feed the clima ECU status frame (0x5EA) — the HVAC conditioning bit.
    void IncomingClimaEcuStatus(const CAN_frame_t* p_frame);

    // Abort an in-flight climate wake (e.g. `xvg offline`): release the NM bridge.
    void Abort() { m_phase = CP_IDLE; m_wake_hold = 0; }

    bool WakeActive() const { return m_wake_hold > 0; }

 private:
    // What the in-flight command is doing. Climate and charge share the ENTIRE BCU command path
    // (handshake -> GET profile 0 -> RMW-arm profile 0 -> trigger) because profile 0 is a single
    // shared scratchpad and the immediate trigger acts on whatever op bits it currently holds — so
    // only ONE can be in flight at a time; they differ only in the arm's op bits and the confirm.
    enum BcOp : uint8_t {
        BC_CLIMATE = 0,   // arm PO_CLIMATE  -> trigger start/stop; confirm 49 58 -> ms_v_env_hvac
        BC_CHARGE,        // arm PO_CHARGING -> trigger start/stop; confirm 49 58 (charge state via 0x594)
        BC_SET_CURRENT,   // RMW maxCurrent only, NO trigger (settings edit); confirm on the write echo
        BC_LIST,          // read-only: capture the whole profile array (all charge locations); NO write
        BC_PROFILE_SET,   // RMW selected fields of one profile (pos 0..3), NO trigger; confirm on echo
        BC_TIMER_LIST,    // read-only: collect the 4 departure timers + enable mask; NO write
        BC_TIMER_SET,     // write one timer record (29 5x on 0x17332501) + RMW-enable it; confirm on echo
        BC_TIMER_ENABLE,  // RMW the TimerState enable mask only (enable/disable a slot); confirm on echo
    };

    // Shared entry: reset command state, select the op, kick the wake. enable = on/off for
    // climate/charge (ignored/true for set-current); param = clamped amps for BC_SET_CURRENT.
    bool Begin(BcOp op, bool enable, uint16_t param);

    // Command progress. The BCU's replies advance it; Ticker1 only holds the wake + backstops.
    enum ClimatePhase : uint8_t {
        CP_IDLE = 0,   // no command in flight
        CP_WAKE,       // waking the bus, waiting to first hear the BCU's BAP layer
        CP_HANDSHAKE,  // sent the channel-open handshake, waiting for its ack (49 41/49 42) before
                       // issuing any function GET — the BCU DROPS a GET sent before registration acks
        CP_PROFILE,    // ON: requested the profile array, waiting to read + arm profile 0
        CP_ARM_WAIT,   // arm write sent, waiting for its "49 59 bx" echo BEFORE the trigger — else the
                       // trigger races the (multi-frame) write and runs the STALE profile-0 op
        CP_CONFIRM,    // trigger sent, waiting for the BCU's "49 58 <flag>" confirmation
        CP_DONE,       // confirmed; NM bridge releasing
    };

    void SendNmWake();
    bool TxFrame(const uint8_t* frame, uint8_t dlc);  // one BAP frame -> BCU command id (0x17332501)
    bool SendHandshake();   // channel-open GETs "19 42" + "19 41"
    bool SendProfileGet();  // GET ProfilesArray "19 59" (reply drives the arm)
    bool SendArm();         // RA0 read-modify-write: arm the global profile 0 for climate
    bool SendTrigger(bool on);  // OperationMode immediate "29 58 00 <flag>"
    bool SendProfileSetWrite();  // BC_PROFILE_SET: apply m_prof_edit onto m_prof_base and write it back

    // Departure-timer TX (all on kCanIdCommand / 0x17332501, like every other command — see TxFrame).
    bool SendTimerReads();          // GET-prompt the timer table + mask (best-effort; BCU ignores it)
    bool SendTimerRecord();         // write m_timer_write into m_timer_slot (29 5x)
    bool SendTimerMask(uint8_t mask);  // write the TimerState enable mask (29 53)
    // Keep the persistent timer cache current from ANY timer status/broadcast frame (func 0x13 mask /
    // 0x14..17 record). Called for every such frame on 0x17332510, even with no command in flight, so a
    // later set/list can act on the cache immediately instead of waiting ~13 s for a fresh broadcast.
    void UpdateTimerCache(const bap::Element& el);
    // Timer STATUS/HEARTBEAT handler: drive the in-flight timer command off the (already-updated) cache
    // — write once the enable mask is known, confirm on the 49 53 echo, finish a list. RX task only.
    void HandleTimerStatus(const bap::Element& el);
    // If BC_TIMER_LIST and the cache holds the mask + every slot, publish it (instant list). CP_PROFILE only.
    void TimerListMaybeComplete();
    // If BC_TIMER_SET/ENABLE and the enable mask is cached, RMW-write now (record + mask) and go to
    // CP_CONFIRM. Returns true if the write was issued. CP_PROFILE only; no-op until the mask is known.
    bool TimerWriteMaybeStart();
    bool isTimerOp() const {
        return m_op == BC_TIMER_LIST || m_op == BC_TIMER_SET || m_op == BC_TIMER_ENABLE;
    }
    bool isCliOp() const {  // reports via m_list_state (polled by the CLI), no user notification
        return m_op == BC_LIST || m_op == BC_PROFILE_SET || isTimerOp();
    }
    // NB timers use the SAME command id as climate/charge/profiles: kCanIdCommand (0x17332501). That id
    // is the comfort/"bus-4" channel our KCAN node uses, and the J533 gateway BRIDGES it to the FCAN bus
    // where the BCU lives (on-car: our 0x17332501 writes are forwarded to FCAN and answered; the MIB's
    // 0x17332500 is FCAN-LOCAL and NOT bridged, so a 0x17332500 write from us on KCAN never reaches the
    // BCU — confirmed on-car). We must never transmit on the MIB's 0x17332500 from KCAN.

    canbus* m_bus = nullptr;
    // Reassembles the BatteryControl FSG status stream (0x17332510). Sized to hold the full
    // profile-array telegram (49 59): the ON path GETs it to read the global profile and arm it.
    // The observed 4-profile array is 141 bytes / 21 frames; 224 covers a 5-profile / long-name
    // list with headroom (an over-length array simply fails to reassemble -> no arm -> no trigger).
    bap::AssemblerSetT<2, 224, 4> m_bap_asm;

    bap::egolf::Profile    m_profile0;   // global "Optionen" profile (pos 0), read back for the arm
    bap::egolf::TxnCounter m_txn;        // rolling array-write transaction id (BCU echoes it)

    // SetProfile() pending edit: the target position, the field deltas, and the record read back as the
    // RMW base (kept separate from m_profile0, which is the arm path's pos-0 read-back).
    uint8_t             m_prof_set_pos = 0;
    ProfileEdit         m_prof_edit;
    bap::egolf::Profile m_prof_base;

    // ListProfiles() results: all profiles from the last read-only fetch. The RX task fills these and
    // publishes m_list_state; the command task polls m_list_state, then copies via GetProfiles().
    bap::egolf::Profile m_profiles[kMaxProfiles];   // captured profiles (valid when LIST_READY)
    uint8_t m_profile_count = 0;                    // number of valid entries in m_profiles[]
    // m_list_state is the shared CLI-command result signal (RX publishes, the command task polls): it
    // reports the outcome of the console read/write ops — ListProfiles, ListTimers, Set/Enable/Clear
    // timer. LIST_PENDING while in flight, LIST_READY on success, LIST_FAILED on failure/timeout. The
    // remote ops (climate/charge/set-current) don't use it; they notify instead.
    volatile ListState m_list_state = LIST_NONE;

    // Departure-timer PERSISTENT cache: m_timers/m_timer_seen/m_timer_mask are filled from ANY timer
    // status/broadcast on 0x17332510 (UpdateTimerCache), always, and are NOT reset per command or on
    // sleep — so set/list use them immediately. The car broadcasts on every change (incl. a one-shot
    // firing, which wakes the bus), so the cache self-corrects; "not seen" means only never-yet-heard.
    // The m_timer_* write fields carry the pending SET/ENABLE.
    bap::egolf::Timer m_timers[kNumTimers];         // cached timer records (valid when seen[i])
    bool     m_timer_seen[kNumTimers]  = {};        // slot i has broadcast a record at least once
    uint8_t  m_timer_mask       = 0;                // cached TimerState enable mask (bits 0..3)
    bool     m_timer_mask_seen  = false;            // the enable mask has been cached at least once
    uint8_t  m_timer_slot       = 0;                // SET/ENABLE target slot (1..4)
    bap::egolf::Timer m_timer_write;                // SET: the record to write
    bool     m_timer_write_record = false;          // SET writes a record; ENABLE does not
    bool     m_timer_enable     = false;            // desired enabled state of the target slot
    uint8_t  m_timer_target_mask = 0;               // the enable mask we wrote (confirm when it echoes)

    ClimatePhase m_phase       = CP_IDLE;
    BcOp    m_op            = BC_CLIMATE;  // which operation the in-flight command performs
    uint8_t m_param         = 0;      // BC_SET_CURRENT: clamped charge-current limit (amps)
    bool    m_enable        = false;  // requested on/off for the in-flight command
    bool    m_bcu_seen      = false;  // BCU (0x17332510) heard since this command's wake
    bool    m_have_profile0 = false;  // global profile 0 read back this command cycle (the arm gate)
    bool    m_arm_skipped   = false;  // SendArm() skipped the write (op already correct) -> no echo coming
    bool    m_apply         = false;  // BC_SET_CURRENT while charging: re-fire the start after the write
                                      // so the RUNNING charge picks up the new current (else it ignores it)
    bool    m_confirmed     = false;  // BCU echoed "49 58 <flag>" matching the request
    bool    m_error         = false;  // BCU returned a BAP ERROR response to the request
    bool    m_tx_fail       = false;  // a CAN write failed and the BCU was never heard
    uint8_t m_wake_hold     = 0;      // Ticker1 countdown for the NM-wake bridge (seconds)
    uint8_t m_phase_secs    = 0;      // seconds in the current phase (lost-reply backstop timer)
    uint8_t m_collect_secs  = 0;      // BC_TIMER_LIST: seconds spent collecting the timer broadcast
};

#endif  // __VEHICLE_VWEGOLF_BAT_CTRL_H__
