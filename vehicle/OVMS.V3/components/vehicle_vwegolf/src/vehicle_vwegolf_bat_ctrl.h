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
    // Set the pre-conditioning target temperature (persistent settings edit via
    // the same RA0 read-modify-write). Takes the raw profile encoding
    // (degC * 10 - 100) so it fits the uint8_t command parameter.
    bool SetClimateTempRaw(uint8_t raw);
    bool SetChargeCurrent(uint16_t amps);  // set the charge-current limit (persistent settings edit via
                                           // profile-0 RMW, NO trigger). Amps are snapped to the car's
                                           // allowed steps {5,10,13,32} and hard-capped at 0x20 (see .cpp).

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
        BC_SET_TEMP,      // RMW temperature only, NO trigger (settings edit); confirm on the write echo
    };

    // Shared entry: reset command state, select the op, kick the wake. enable = on/off for
    // climate/charge (ignored/true for set-current); param = clamped amps for BC_SET_CURRENT.
    // Write-only settings edits: change one stored field, confirm on the write
    // echo, never fire the immediate trigger.
    bool IsSettingsEdit() const { return m_op == BC_SET_CURRENT || m_op == BC_SET_TEMP; }
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

    canbus* m_bus = nullptr;
    // Reassembles the BatteryControl FSG status stream (0x17332510). Sized to hold the full
    // profile-array telegram (49 59): the ON path GETs it to read the global profile and arm it.
    // The observed 4-profile array is 141 bytes / 21 frames; 224 covers a 5-profile / long-name
    // list with headroom (an over-length array simply fails to reassemble -> no arm -> no trigger).
    bap::AssemblerSetT<2, 224, 4> m_bap_asm;

    bap::egolf::Profile    m_profile0;   // global "Optionen" profile (pos 0), read back for the arm
    bap::egolf::TxnCounter m_txn;        // rolling array-write transaction id (BCU echoes it)

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
};

#endif  // __VEHICLE_VWEGOLF_BAT_CTRL_H__
