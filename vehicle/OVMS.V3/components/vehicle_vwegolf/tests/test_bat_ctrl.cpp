/*
;    Project:       Open Vehicle Monitor System
;    Subproject:    Integrate VW e-Golf — BatteryControl tests (native harness)
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

// test_bat_ctrl.cpp — Tests for the BatteryControl controller (vehicle_vwegolf_bat_ctrl):
// remote climate AND charge, which share one BAP command path.
//
// The path is self-contained: the OVMS command (climate/charge/set-current) arms a spare-node
// NM-wake bridge and returns; Ticker1 drives the wake + retry-until-confirmed state machine;
// the BCU's BAP status echo on 0x17332510 confirms. These tests drive the whole thing through
// the vehicle so the RX/tick forwarding is exercised too.

#include "mock/mock_ovms.hpp"
#include "../src/vehicle_vwegolf.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

extern int tests_run;
extern int tests_passed;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else       { printf("  FAIL: %s\n", msg); } \
} while(0)

// Helper: get the KCAN (can3) stub from a vehicle instance.
static canbus* kcan(OvmsVehicleVWeGolf* v) { return v->m_can3; }

// Ticker1 is protected in OvmsVehicleVWeGolf. Call via base-class pointer (public virtual
// in the mock OvmsVehicle) to dispatch to the override.
static void call_ticker1(OvmsVehicleVWeGolf* v, uint32_t tick) {
    static_cast<OvmsVehicle*>(v)->Ticker1(tick);
}

// Helper: make a KCAN frame with origin set to m_can3.
static CAN_frame_t make_kcan_frame(OvmsVehicleVWeGolf* v, uint32_t id,
                                   std::initializer_list<uint8_t> bytes) {
    CAN_frame_t f{};
    f.MsgID = id;
    f.origin = v->m_can3;
    f.FIR.B.DLC = static_cast<uint8_t>(bytes.size());
    int i = 0;
    for (uint8_t b : bytes) f.data.u8[i++] = b;
    return f;
}

// Collect the extended TX frames on a given CAN id, in order.
static std::vector<TxRecord> ext_frames(canbus* bus, uint32_t id) {
    std::vector<TxRecord> out;
    for (auto& r : bus->tx_log)
        if (r.extended && r.id == id) out.push_back(r);
    return out;
}

static bool hvac_on() { return StandardMetrics.ms_v_env_hvac->AsBool(); }

static const uint32_t NM_WAKE_ID = 0x1B000000u | VWEGOLF_NM_WAKE_NODE;  // 0x1B00007D
static const uint32_t BAP_CMD_ID = 0x17332501;                          // ASG / command
static const uint32_t BAP_STS_ID = 0x17332510;                          // FSG / status

// Frame `body` as a BAP status telegram (op=STATUS, LSG 0x25, `func`) exactly as the BCU
// would, and feed each resulting CAN frame to the vehicle on the status id — so the climate
// reassembler sees the real segmented stream, not a hand-built shortcut.
static void inject_bap_status(OvmsVehicleVWeGolf* v, uint8_t func,
                              const std::vector<uint8_t>& body) {
    auto sink = [&](const uint8_t* frame, uint8_t dlc) -> bool {
        CAN_frame_t f{};
        f.MsgID = BAP_STS_ID;
        f.origin = v->m_can3;
        f.FIR.B.DLC = dlc;
        for (uint8_t i = 0; i < dlc; i++) f.data.u8[i] = frame[i];
        v->IncomingFrameCan3(&f);
        return true;
    };
    bap::sendElement(sink, bap::OP_STATUS, bap::egolf::kLsg, func, body.data(),
                     static_cast<uint16_t>(body.size()));
}

// Inject the BCU's arm-write echo (49 59 bx, bit7 set, OVMS asgId 3): the controller waits for this
// before firing the trigger (CP_ARM_WAIT), so a climate/charge start that WROTE the profile needs it.
static void inject_arm_echo(OvmsVehicleVWeGolf* v) {
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, {0xb1, 0x00, 0x00, 0x01});
}

// Build a profile-array STATUS body (arrayId 0x29 full list) carrying ONE global profile at
// pos 0, with the given operation byte and a recognizable field set so a test can verify the
// arm sets the op deterministically while preserving every other stored byte.
// lead = the [asgId:4|txn:4] byte the BCU echoes. It is accepted only if the asgId nibble == the
// OVMS client id kAsgIdOvms (0x3), masking the 0x80 SET-echo flag — so 0x31 (GET reply) and 0xb1
// (SET echo) match, while the MIB (0x1x) and OCU (0x2x/0xax) are ignored. Default 0x31 = id 3 + txn 1.
static std::vector<uint8_t> make_profile0_array(uint8_t op, uint8_t lead = 0x31,
                                                uint8_t maxCurrent = 0x20) {
    return {
        lead,        // [asgId:4|txn:4] — accepted only if asgId nibble == kAsgIdOvms (0x3)
        0x01,        // totalElementsInList
        0x40,        // flags: PosTransmit=1, recordAddr=0 (RA0)
        0x00,        // startIndex
        0x01,        // elementCount
        0x00,        // element 0: position = 0 (global "Optionen")
        // --- RA0 record (20 fixed bytes + length-prefixed name) ---
        op,          // [0]  operation
        0x00,        // [1]  operation2
        maxCurrent,  // [2]  maxCurrent (default 0x20 = 32 A)
        0x50,        // [3]  minChargeLevel = 80 %
        0xff, 0xff,  // [4:5] minRange
        0x00,        // [6]  targetChargeLevel
        0xff,        // [7]  targetChargeDuration
        0xff, 0xff,  // [8:9] targetChargeRange
        0xff,        // [10] unitRange
        0x01,        // [11] rangeCalculationSetup
        0x78,        // [12] temperature = 22.0 C
        0x00,        // [13] temperatureUnit
        0x1e,        // [14] leadTime
        0x1e,        // [15] holdingTimePlug
        0x0f,        // [16] holdingTimeBattery
        0x00, 0x00,  // [17:18] providerDataId
        0x08,        // [19] nameLength
        'O', 'p', 't', 'i', 'o', 'n', 'e', 'n',
    };
}

// Reassemble the ARM telegram (SET_GET, LSG 0x25, func 0x19) from the command-id TX frames and
// return its body: [asgTxn][recordAddr][startIndex][count][RA0 record...]. Empty if none.
static std::vector<uint8_t> arm_write_body(canbus* bus) {
    bap::AssemblerT<256, 4> asmb;
    for (auto& r : bus->tx_log) {
        if (!r.extended || r.id != BAP_CMD_ID) continue;
        bap::Element el;
        if (asmb.feed(r.data, r.len, el) && el.opcode == bap::OP_SET_GET &&
            el.lsg == bap::egolf::kLsg && el.func == bap::egolf::FUNC_PROFILES_ARRAY) {
            return std::vector<uint8_t>(el.body, el.body + el.bodyLen);
        }
    }
    return {};
}

// True if any command-id frame is the OperationMode trigger "29 58 00 xx".
static bool sent_trigger(canbus* bus) {
    for (auto& f : ext_frames(bus, BAP_CMD_ID))
        if (f.len == 4 && f.data[0] == 0x29 && f.data[1] == 0x58) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Command arms the wake, but does NOT send the BAP command until the BCU is heard
// ---------------------------------------------------------------------------

void test_climate_command_kicks_nm_wake() {
    printf("\ntest_climate_command_kicks_nm_wake\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    auto result = v->CommandClimateControl(true);
    CHECK(result == Success, "CommandClimateControl(true) returns Success immediately");

    auto wake = ext_frames(kcan(v), NM_WAKE_ID);
    CHECK(wake.size() == 1, "Command emits exactly one spare-node NM wake");
    if (!wake.empty()) {
        CHECK(wake[0].len == 8, "NM wake DLC = 8");
        CHECK(wake[0].data[0] == VWEGOLF_NM_WAKE_NODE, "NM wake byte0 = spare node 0x7D");
        CHECK(wake[0].data[1] == 0x10 && wake[0].data[2] == 0x49 && wake[0].data[3] == 0x85 &&
              wake[0].data[4] == 0x14,
              "NM wake payload = 7D 10 49 85 14 ..");
    }
    // No BAP command yet — the BCU has not been heard.
    CHECK(ext_frames(kcan(v), BAP_CMD_ID).empty(), "No BAP command before the BCU is heard");
    // Climate never touches the OCU 0x5A7 heartbeat.
    bool any_5a7 = false;
    for (auto& r : kcan(v)->tx_log) if (!r.extended && r.id == 0x5A7) any_5a7 = true;
    CHECK(!any_5a7, "Climate does not send the OCU 0x5A7 heartbeat");

    delete v;
}

void test_climate_ticks_wait_for_bcu() {
    printf("\ntest_climate_ticks_wait_for_bcu\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    // Keep the bus alive but never deliver a BCU status frame.
    auto keep = make_kcan_frame(v, 0x131, {0, 0, 0, 0x64, 0, 0, 0, 0});
    v->IncomingFrameCan3(&keep);
    kcan(v)->tx_log.clear();

    call_ticker1(v, 1);
    CHECK(!ext_frames(kcan(v), NM_WAKE_ID).empty(), "Tick keeps sending the NM wake");
    CHECK(ext_frames(kcan(v), BAP_CMD_ID).empty(),
          "No BAP command while the BCU is still unheard");

    delete v;
}

// ---------------------------------------------------------------------------
// Handshake + trigger once the BCU is heard
// ---------------------------------------------------------------------------

void test_climate_start_sequence() {
    printf("\ntest_climate_start_sequence\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    kcan(v)->tx_log.clear();

    // Event 1: the first BCU frame heard opens the channel with the handshake ONLY (19 42, 19 41).
    // The profile GET must NOT go out yet — the BCU drops a GET issued before it acks registration.
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&sts);
    auto k = ext_frames(kcan(v), BAP_CMD_ID);
    CHECK(k.size() == 2, "First BCU frame sends the handshake only (19 42, 19 41) — no GET yet");
    if (k.size() == 2) {
        CHECK(k[0].data[0] == 0x19 && k[0].data[1] == 0x42, "Frame 1 = GET BapConfig  '19 42'");
        CHECK(k[1].data[0] == 0x19 && k[1].data[1] == 0x41, "Frame 2 = GetAll  '19 41'");
    }
    bool got_get = false;
    for (auto& f : k) if (f.len >= 2 && f.data[1] == 0x59) got_get = true;
    CHECK(!got_get, "No profile GET (19 59) before the handshake is acked");
    CHECK(!sent_trigger(kcan(v)), "No trigger before the handshake is acked");

    // Event 2: the BCU acks the handshake ("49 42" BapConfig) — NOW the segmented get-all profile
    // GET goes out, byte-for-byte like the OCU's answered request except the OVMS asgId nibble.
    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});
    auto g = ext_frames(kcan(v), BAP_CMD_ID);
    CHECK(g.size() == 1 && g[0].len == 8 && g[0].data[0] == 0x80 && g[0].data[1] == 0x04 &&
          g[0].data[2] == 0x19 && g[0].data[3] == 0x59 &&
          (g[0].data[4] >> 4) == bap::egolf::kAsgIdOvms &&   // the OVMS asgId nibble (0x3 = kAsgIdOvms)
          g[0].data[5] == 0x00,                              // get-all param
          "Handshake ack releases the segmented get-all GET  '80 04 19 59 <3x-txn> 00 ..'");

    // Event 3: the OVMS profile-array reply (asgId nibble 0x3) drives the arm. Profile 0 reads op 0x01
    // (not climate), so the arm WRITES it back as climate + on-battery (0x06). The trigger is GATED on
    // the arm echo (so it can't race the multi-frame write) — so no trigger yet.
    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x01));
    CHECK(!arm_write_body(kcan(v)).empty(), "profile reply drives the RA0 arm write");
    CHECK(!sent_trigger(kcan(v)), "no trigger yet — waiting for the arm-write echo");

    // Event 4: the BCU echoes the arm write (49 59 bx) -> NOW the trigger fires.
    inject_arm_echo(v);
    CHECK(sent_trigger(kcan(v)), "arm echo releases the trigger '29 58 00 01'");

    // Event 5: the BCU's OperationMode echo confirms -> HVAC ON.
    auto echo = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});
    v->IncomingFrameCan3(&echo);
    CHECK(hvac_on(), "OperationMode echo '49 58 01' confirms -> HVAC ON");

    delete v;
}

void test_climate_stop_sequence() {
    printf("\ntest_climate_stop_sequence\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(false);
    kcan(v)->tx_log.clear();

    // OFF is OP-SPECIFIC, not global: the stop trigger "29 58 00 00" acts on whatever op profile 0
    // holds, so OFF reads + arms the matching op FIRST (just like ON), then triggers the stop. Same
    // handshake -> GET -> arm gate as the start.
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});  // currently ON
    v->IncomingFrameCan3(&sts);
    CHECK(!sent_trigger(kcan(v)), "OFF: no STOP trigger before the handshake ack");

    // Handshake ack -> the profile GET goes out (OFF reads profile 0 too).
    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});
    CHECK(!ext_frames(kcan(v), BAP_CMD_ID).empty(), "ack releases the profile GET (OFF reads too)");
    CHECK(!sent_trigger(kcan(v)), "OFF: no STOP trigger from the bare ack");

    // Profile 0 already holds a climate op (0x06 = climate + on-battery, as it does while climate
    // runs), so the climate arm matches -> the write is SKIPPED and the stop trigger fires at once.
    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));
    CHECK(arm_write_body(kcan(v)).empty(), "op already climate -> arm write skipped");
    auto cmd = ext_frames(kcan(v), BAP_CMD_ID);
    CHECK(cmd.size() == 1 && cmd[0].len == 4 && cmd[0].data[0] == 0x29 && cmd[0].data[1] == 0x58 &&
          cmd[0].data[3] == 0x00, "matched arm -> STOP trigger '29 58 00 00'");

    // BCU confirms OFF.
    auto echo = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&echo);
    CHECK(!hvac_on(), "OperationMode echo '49 58 00' -> HVAC OFF");

    delete v;
}

// ---------------------------------------------------------------------------
// Profile 0 is read ONLY from the OVMS reply (asgId 0x3), never from other nodes' polls
// ---------------------------------------------------------------------------

void test_climate_ignores_foreign_asg_array() {
    printf("\ntest_climate_ignores_foreign_asg_array\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&sts);  // kickoff -> handshake
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});  // ack -> GET
    kcan(v)->tx_log.clear();

    // Arrays from the MIB (0x11) and the OCU (0x28) must BOTH be ignored — only the OVMS client id
    // (0x3) is read. The controller does not coast on the OCU's ambient id-2 polls; the BCU answers
    // the OVMS GET directly because the handshake completes first.
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06, 0x11));  // MIB
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06, 0x28));  // OCU
    CHECK(arm_write_body(kcan(v)).empty() && !sent_trigger(kcan(v)),
          "MIB (0x11) and OCU (0x28) arrays ignored — no arm, no trigger");

    // A GET reply on OUR own id 0x3 (lead 0x31) IS accepted -> drives the arm. Op 0x01 (not climate)
    // so the climate arm actually writes it back (0x02), not skips. (The trigger is separately gated
    // on the arm echo; here we just verify the own-id reply is the one that's acted on.)
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x01, 0x31));
    CHECK(!arm_write_body(kcan(v)).empty(),
          "our own id 0x3 (GET reply 0x31) drives the arm write");

    delete v;
}

// ---------------------------------------------------------------------------
// The arm read-modify-writes profile 0: set climate + on-battery, clear charge, preserve the rest
// ---------------------------------------------------------------------------

void test_climate_arm_sets_op_preserves_fields() {
    printf("\ntest_climate_arm_sets_op_preserves_fields\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&sts);  // kickoff: handshake
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});  // ack -> GET
    kcan(v)->tx_log.clear();

    // Profile 0 currently reads charge-only (0x01) — the state a charge command leaves behind (it
    // clears the climate bits). The climate arm must clear charge, set climate, AND force-set the
    // on-battery bit -> 0x06, leaving every other byte intact. Force-setting on-battery is what lets
    // climate still run on battery after a charge cleared it (the write is persistent).
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x01));

    auto arm = arm_write_body(kcan(v));
    CHECK(arm.size() >= 4 + 20, "arm carries a full RA0 record");
    if (arm.size() >= 4 + 20) {
        const uint8_t* rec = arm.data() + 4;  // past [asgTxn][recordAddr][startIndex][count]
        CHECK(rec[0] == 0x06,
              "operation 0x01->0x06: charge cleared, climate set, on-battery FORCE-SET");
        CHECK(rec[2] == 0x20, "maxCurrent preserved (0x20 = 32 A)");
        CHECK(rec[3] == 0x50, "minChargeLevel preserved (0x50 = 80 %)");
        CHECK(rec[12] == 0x78, "target temperature preserved (0x78 = 22.0 C)");
        CHECK(rec[19] == 0x08 && memcmp(rec + 20, "Optionen", 8) == 0,
              "profile name 'Optionen' preserved");
    }

    delete v;
}

// ---------------------------------------------------------------------------
// Safety: ON must NOT trigger unless it read + armed the profile first
// ---------------------------------------------------------------------------

void test_climate_no_trigger_without_profile() {
    printf("\ntest_climate_no_trigger_without_profile\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    // First BCU frame kicks off the handshake; the ack releases the profile GET — but the BCU never
    // returns the profile array, so the arm gate never opens.
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&sts);
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});  // ack -> GET

    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);

    CHECK(!sent_trigger(kcan(v)), "no OperationMode trigger without a read + armed profile");
    CHECK(arm_write_body(kcan(v)).empty(), "no arm write without the profile read-back");
    CHECK(MyNotify.count >= 1, "the command times out with a notification");
    CHECK(MyNotify.last_value.find("profile") != std::string::npos,
          "notification says the profile could not be read");

    delete v;
}

// ---------------------------------------------------------------------------
// Regression: the reassembler is reset per command (else the profile read jams after cmd 1)
// ---------------------------------------------------------------------------

void test_climate_multiple_commands_reset_assembler() {
    printf("\ntest_climate_multiple_commands_reset_assembler\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // --- Command 1: full ON cycle ---
    v->CommandClimateControl(true);
    auto s1 = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&s1);  // kickoff -> handshake
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});  // ack -> GET
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));
    CHECK(sent_trigger(kcan(v)), "command 1: profile read -> trigger");
    auto e1 = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});
    v->IncomingFrameCan3(&e1);  // confirm

    // Jam the reassembler BETWEEN commands: feed long-telegram START frames that are never
    // continued, filling its pending slots (mirrors the wake-flood leaving half-reassembled
    // telegrams). No command is in flight, so these must NOT be reset here.
    CAN_frame_t pf{};
    pf.MsgID = BAP_STS_ID; pf.origin = v->m_can3; pf.FIR.B.DLC = 8;
    const uint8_t partial[8] = {0x80, 0x28, 0x49, 0x59, 0x29, 0x01, 0x40, 0x00};  // long start, no cont
    for (int i = 0; i < 8; i++) pf.data.u8[i] = partial[i];
    for (int i = 0; i < 6; i++) v->IncomingFrameCan3(&pf);

    // --- Command 2: must STILL read the profile and fire. The kickoff resets the jammed
    // reassembler, so the 21-frame profile reply reassembles again. Without the reset this hangs
    // (the stale slots steal the array's continuations) and the command times out.
    kcan(v)->tx_log.clear();
    g_metrics = MetricStore{};  // clear hvac
    v->CommandClimateControl(true);
    auto s2 = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&s2);  // kickoff resets the reassembler
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});  // ack -> GET
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));
    CHECK(sent_trigger(kcan(v)),
          "command 2: profile STILL read after reassembler churn -> trigger (reset works)");
    auto e2 = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});
    v->IncomingFrameCan3(&e2);
    CHECK(hvac_on(), "command 2 confirmed");

    delete v;
}

// ---------------------------------------------------------------------------
// Confirmation + HVAC state
// ---------------------------------------------------------------------------

void test_climate_confirm_sets_hvac() {
    printf("\ntest_climate_confirm_sets_hvac\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    // BCU echoes ON (element "49 58 01") — confirms our start request.
    auto echo = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});
    v->IncomingFrameCan3(&echo);
    CHECK(hvac_on(), "OperationMode echo '49 58 01' sets ms_v_env_hvac ON");

    delete v;
}

void test_climate_hvac_from_5ea() {
    printf("\ntest_climate_hvac_from_5ea\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // 0x5EA d[3] bit 3 = actively conditioning. Use a sentinel temperature so only the
    // HVAC bit is under test.
    auto on = make_kcan_frame(v, 0x5EA, {0, 0, 0, 0x08, 0, 0, 0xFC, 0x0F});
    v->IncomingFrameCan3(&on);
    CHECK(hvac_on(), "0x5EA d3 bit3 set -> HVAC ON");

    auto off = make_kcan_frame(v, 0x5EA, {0, 0, 0, 0x00, 0, 0, 0xFC, 0x0F});
    v->IncomingFrameCan3(&off);
    CHECK(!hvac_on(), "0x5EA d3 bit3 clear -> HVAC OFF");

    delete v;
}

void test_climate_hvac_clears_on_sleep() {
    printf("\ntest_climate_hvac_clears_on_sleep\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    auto on = make_kcan_frame(v, 0x5EA, {0, 0, 0, 0x08, 0, 0, 0xFC, 0x0F});
    v->IncomingFrameCan3(&on);
    CHECK(hvac_on(), "HVAC ON before sleep");

    // No further frames: after BUS_TIMEOUT ticks the bus is idle and HVAC clears.
    for (int i = 0; i <= VWEGOLF_BUS_TIMEOUT_SECS + 1; i++) call_ticker1(v, i);
    CHECK(!hvac_on(), "HVAC cleared once the bus sleeps");

    delete v;
}

// ---------------------------------------------------------------------------
// Failure paths surface a notification
// ---------------------------------------------------------------------------

void test_climate_tx_fail_notifies() {
    printf("\ntest_climate_tx_fail_notifies\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    kcan(v)->fail_tx = true;  // every WriteExtended returns ESP_FAIL
    v->CommandClimateControl(true);
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&sts);  // BCU heard -> ticks will try (and fail) to send

    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);
    CHECK(MyNotify.count >= 1, "TX failure raises a notification");
    CHECK(MyNotify.last_value.find("transmit") != std::string::npos,
          "Notification names a CAN transmit error");

    delete v;
}

void test_climate_error_notifies() {
    printf("\ntest_climate_error_notifies\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    // The first BCU frame opens the handshake; a subsequent BAP ERROR (opcode 7) for LSG 0x25
    // func 0x18 — element "79 58" — is then surfaced (a rejection of our request).
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&sts);  // kickoff -> handshake
    auto err = make_kcan_frame(v, BAP_STS_ID, {0x79, 0x58});
    v->IncomingFrameCan3(&err);

    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);
    CHECK(MyNotify.count >= 1, "BAP ERROR raises a notification");
    CHECK(MyNotify.last_value.find("rejected") != std::string::npos,
          "Notification says the command was rejected");
    CHECK(!hvac_on(), "HVAC not set on a rejected command");

    delete v;
}

// ---------------------------------------------------------------------------
// offline aborts an in-flight climate wake
// ---------------------------------------------------------------------------

void test_climate_abort_releases_wake() {
    printf("\ntest_climate_abort_releases_wake\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    CHECK(v->test_batctrl().WakeActive(), "wake bridge active after command");
    v->test_batctrl().Abort();
    CHECK(!v->test_batctrl().WakeActive(), "Abort() releases the wake bridge");

    delete v;
}

// ---------------------------------------------------------------------------
// Charge control shares the climate BCU path; only the arm's op bits + confirm differ
// ---------------------------------------------------------------------------

// Drive a command through wake -> handshake -> ack, leaving the controller in CP_PROFILE with the
// GET issued. `sts` kicks the handshake; the BapConfig ack releases the GET.
static void advance_to_profile_phase(OvmsVehicleVWeGolf* v) {
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});
    v->IncomingFrameCan3(&sts);                                                     // kickoff -> handshake
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});  // ack -> GET
}

void test_charge_start_sequence() {
    printf("\ntest_charge_start_sequence\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    auto result = v->CommandStartCharge();
    CHECK(result == Success, "CommandStartCharge() returns Success immediately");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();

    // Profile 0 reads climate|on-battery (0x06). The CHARGE arm produces a PURE charge op -> 0x01:
    // charge bit set, BOTH climate bits (climate AND on-battery) cleared so it can never run climate,
    // while preserving every other byte, then triggers. (On-car: leaving on-battery set made a charge
    // start run climate-on-battery when unplugged.)
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));
    auto arm = arm_write_body(kcan(v));
    CHECK(arm.size() >= 4 + 20, "charge arm carries a full RA0 record");
    if (arm.size() >= 4 + 20) {
        const uint8_t* rec = arm.data() + 4;
        CHECK(rec[0] == 0x01, "operation 0x06->0x01: pure charge (climate + on-battery bits cleared)");
        CHECK(rec[2] == 0x20, "maxCurrent preserved (0x20 = 32 A)");
        CHECK(rec[12] == 0x78, "target temperature preserved");
    }
    CHECK(!sent_trigger(kcan(v)), "no trigger yet — gated on the arm-write echo");
    inject_arm_echo(v);  // BCU acks the arm write -> trigger fires
    CHECK(sent_trigger(kcan(v)), "charge ON triggers '29 58 00 01' after the arm echo");
    CHECK(!hvac_on(), "charge start does NOT set ms_v_env_hvac (that's climate only)");

    // BCU echoes the immediate-op ON -> command confirmed, no failure notification on release.
    auto echo = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});
    v->IncomingFrameCan3(&echo);
    CHECK(!hvac_on(), "charge confirm still does not touch HVAC");
    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);
    CHECK(MyNotify.count == 0, "confirmed charge start releases with no failure notification");

    delete v;
}

void test_charge_stop_sequence() {
    printf("\ntest_charge_stop_sequence\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    auto result = v->CommandStopCharge();
    CHECK(result == Success, "CommandStopCharge() accepted (climate off -> not blocked)");
    kcan(v)->tx_log.clear();

    // Charge OFF is OP-SPECIFIC, not global: the stop trigger only stops the charge if profile 0 is
    // armed to charge (0x01). So OFF reads profile 0 and arms the pure-charge op FIRST, just like the
    // start. Handshake, then the GET.
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});
    v->IncomingFrameCan3(&sts);
    CHECK(!sent_trigger(kcan(v)), "charge OFF: no trigger before the handshake ack");

    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_BAP_CONFIG, {0x03, 0x00, 0x25, 0x00, 0x04, 0x00});
    CHECK(!ext_frames(kcan(v), BAP_CMD_ID).empty(), "charge OFF: ack releases the profile GET");
    CHECK(!sent_trigger(kcan(v)), "charge OFF: no trigger from the bare ack");

    // Profile 0 already reads pure-charge (0x01, as it does while charging), so the charge arm matches
    // -> the write is SKIPPED and the stop trigger fires at once.
    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x01));
    CHECK(arm_write_body(kcan(v)).empty(), "op already charge -> arm write skipped");
    auto cmd = ext_frames(kcan(v), BAP_CMD_ID);
    CHECK(cmd.size() == 1 && cmd[0].len == 4 && cmd[0].data[0] == 0x29 && cmd[0].data[1] == 0x58 &&
          cmd[0].data[3] == 0x00, "matched charge arm -> STOP trigger '29 58 00 00'");

    delete v;
}

void test_charge_stop_blocked_when_climate_on() {
    printf("\ntest_charge_stop_blocked_when_climate_on\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    // Charge stop arms the PURE-charge op (0x01), which clears the climate bits — so stopping charge
    // while climate runs would disrupt climate. Block it: with HVAC on, CommandStopCharge must fail
    // and emit NOTHING on the bus (the user must stop climate first).
    StandardMetrics.ms_v_env_hvac->SetValue(true);
    kcan(v)->tx_log.clear();
    auto result = v->CommandStopCharge();
    CHECK(result == Fail, "CommandStopCharge() returns Fail while climate is on");
    CHECK(kcan(v)->tx_log.empty(), "blocked charge stop sends no frames");
    CHECK(MyNotify.count == 1, "blocked charge stop raises one alert");

    // Sanity: with climate OFF the same command is accepted again.
    StandardMetrics.ms_v_env_hvac->SetValue(false);
    CHECK(v->CommandStopCharge() == Success, "charge stop accepted once climate is off");

    delete v;
}

void test_set_charge_current() {
    printf("\ntest_set_charge_current\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    auto result = v->CommandSetChargeCurrent(10);
    CHECK(result == Success, "CommandSetChargeCurrent(10) returns Success");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();

    // Read profile 0 (op 0x06, maxCurrent 0x20). SetChargeCurrent must change ONLY maxCurrent (to
    // 10 A = 0x0A), leave the operation UNCHANGED (0x06 — this is a settings edit, not a start),
    // and must NOT emit any trigger.
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));
    auto arm = arm_write_body(kcan(v));
    CHECK(arm.size() >= 4 + 20, "SetChargeCurrent writes a full RA0 record");
    if (arm.size() >= 4 + 20) {
        const uint8_t* rec = arm.data() + 4;
        CHECK(rec[2] == 0x0A, "maxCurrent 0x20->0x0A (10 A)");
        CHECK(rec[0] == 0x06, "operation left UNCHANGED (settings edit, not a start)");
        CHECK(rec[3] == 0x50, "minChargeLevel preserved");
    }
    CHECK(!sent_trigger(kcan(v)), "SetChargeCurrent emits NO OperationMode trigger");
    // The read must NOT push the OLD value (0x20 = 32 A) into climit — pushing it would make the app
    // slider jump back then forward. climit stays put (unset here) until the write echo confirms.
    CHECK(StandardMetrics.ms_v_charge_climit->AsFloat() == 0.0f,
          "climit NOT set to the old read-back value on the read (no jump-back)");

    // Confirmed by the BCU's SET echo (0xb1 = 0x31 | 0x80), not a 49 58 echo.
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, {0xb1, 0x00, 0x00, 0x01});
    CHECK(StandardMetrics.ms_v_charge_climit->AsFloat() == 10.0f,
          "confirmed write updates ms_v_charge_climit to the applied 10 A");
    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);
    CHECK(MyNotify.count == 0, "write echo confirms the edit -> releases with no failure notification");

    delete v;
}

void test_arm_skips_write_when_op_matches() {
    printf("\ntest_arm_skips_write_when_op_matches\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Climate start when profile 0 already reads climate|on-battery (0x06): the arm would re-write
    // the SAME op, so the ~5-frame RA0 write is SKIPPED — but the trigger still fires.
    v->CommandClimateControl(true);
    advance_to_profile_phase(v);
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));
    CHECK(arm_write_body(kcan(v)).empty(), "op already 0x06 -> arm write SKIPPED (no redundant write)");
    CHECK(sent_trigger(kcan(v)), "trigger still fires on the already-correct profile");

    delete v;
}

void test_error_aborts_no_retry_no_confirm() {
    printf("\ntest_error_aborts_no_retry_no_confirm\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    // Charge start through to the trigger, then the BCU REJECTS it (BAP ERROR on func 0x18 — e.g.
    // unplugged, can't charge). We must STOP: no trigger re-fire, and a stray later "49 58 01" must
    // NOT be reported as success. (Re-firing past a rejection would let a failed charge keep
    // hammering the BCU and could then falsely confirm on a stray later echo.)
    v->CommandStartCharge();
    advance_to_profile_phase(v);
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));  // arm
    inject_arm_echo(v);                                                                // echo -> trigger
    CHECK(sent_trigger(kcan(v)), "charge trigger fired");
    kcan(v)->tx_log.clear();

    // Rejection, then a stray OperationMode ON arrives before release.
    auto err = make_kcan_frame(v, BAP_STS_ID, {0x79, 0x58});   // OP_ERROR, func 0x18
    v->IncomingFrameCan3(&err);
    auto echo = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});
    v->IncomingFrameCan3(&echo);

    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);
    CHECK(!sent_trigger(kcan(v)), "no trigger re-fire after the BAP ERROR (retry stopped)");
    CHECK(MyNotify.count >= 1, "rejected command raises a failure notification");
    CHECK(MyNotify.last_value.find("rejected") != std::string::npos,
          "reason is 'rejected' — the stray 49 58 01 did NOT confirm success");

    delete v;
}

void test_profile_read_populates_metrics() {
    printf("\ntest_profile_read_populates_metrics\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Any command that reads profile 0 surfaces the car's stored settings to the app. Drive a
    // climate ON through the read of profile 0 (maxCurrent 0x20 = 32 A, temperature 0x78 = 22.0 C).
    v->CommandClimateControl(true);
    advance_to_profile_phase(v);
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));

    CHECK(StandardMetrics.ms_v_charge_climit->AsFloat() == 32.0f,
          "profile read -> ms_v_charge_climit = 32 A (maxCurrent 0x20)");
    CHECK(StandardMetrics.ms_v_env_cabinsetpoint->AsFloat() == 22.0f,
          "profile read -> ms_v_env_cabinsetpoint = 22.0 C (temperature 0x78)");

    delete v;
}

void test_set_charge_current_auto_apply() {
    printf("\ntest_set_charge_current_auto_apply\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();
    StandardMetrics.ms_v_charge_inprogress->SetValue(true);  // a charge is running

    // SetChargeCurrent WHILE charging must re-apply: write maxCurrent + set op=charge, wait for the
    // write echo, then re-fire the START so the running charge picks up the new current.
    v->CommandSetChargeCurrent(13);
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));  // read op 0x06
    auto arm = arm_write_body(kcan(v));
    CHECK(arm.size() >= 4 + 20, "auto-apply writes a full RA0 record");
    if (arm.size() >= 4 + 20) {
        const uint8_t* rec = arm.data() + 4;
        CHECK(rec[2] == 0x0D, "maxCurrent -> 0x0D (13 A)");
        CHECK(rec[0] == 0x01, "op set to charge (0x01) so the re-fired start applies to charge");
    }
    CHECK(!sent_trigger(kcan(v)), "no re-trigger before the arm echo (same gate)");
    inject_arm_echo(v);
    CHECK(sent_trigger(kcan(v)), "arm echo -> re-fires the START '29 58 00 01' to apply the new current");

    // 49 58 01 confirms the re-applied charge -> clean release, no failure notification.
    auto echo = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x01});
    v->IncomingFrameCan3(&echo);
    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);
    CHECK(MyNotify.count == 0, "auto-apply confirmed on 49 58 -> no failure notification");

    delete v;
}

void test_set_charge_current_clamps() {
    printf("\ntest_set_charge_current_clamps\n");
    // maxCurrent must snap to an allowed BCU step {5,10,13,32} (the car's actual selection; no 16).
    CHECK(bap::egolf::clampMaxCurrent(13) == 13, "13 A is exact (allowed)");
    CHECK(bap::egolf::clampMaxCurrent(12) == 13, "12 A snaps to 13");
    CHECK(bap::egolf::clampMaxCurrent(16) == 13, "16 A snaps to 13 (not an allowed step)");
    CHECK(bap::egolf::clampMaxCurrent(7)  == 5,  "7 A snaps to 5");
    CHECK(bap::egolf::clampMaxCurrent(0)  == 5,  "0 A snaps to 5 (min)");
    // SAFETY: never above the 0x20 hard limit (a higher value bricks the car's charging).
    CHECK(bap::egolf::clampMaxCurrent(32)  == 0x20, "32 A = 'Max' = the hard ceiling 0x20");
    CHECK(bap::egolf::clampMaxCurrent(40)  == 0x20, "40 A capped to 0x20 (hard limit)");
    CHECK(bap::egolf::clampMaxCurrent(255) == 0x20, "255 A capped to 0x20 (hard limit)");
}

void test_arm_hard_caps_max_current() {
    printf("\ntest_arm_hard_caps_max_current\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // SAFETY chokepoint: a climate arm preserves the read-back maxCurrent verbatim — but a value
    // above the 0x20 hard limit (which would brick charging until a factory reset) must be capped
    // in the write, never sent through. Feed a profile whose maxCurrent reads 0xFF and confirm the
    // arm writes 0x20.
    v->CommandClimateControl(true);
    advance_to_profile_phase(v);
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06, 0x31, 0xFF));
    auto arm = arm_write_body(kcan(v));
    CHECK(arm.size() >= 4 + 20, "arm written");
    if (arm.size() >= 4 + 20)
        CHECK(arm[4 + 2] == 0x20, "maxCurrent 0xFF read-back HARD-CAPPED to 0x20 in the write");

    delete v;
}

// A malformed / short "49 59" profile-array status with an EMPTY body must be dropped, not
// dereferenced. bodyLen 0 -> body == nullptr; reading body[0] would dereference null and crash the
// RX task, so one glitched comfort-bus frame could reboot the module. The command must survive and still complete
// when a well-formed array follows.
void test_empty_profile_array_no_crash() {
    printf("\ntest_empty_profile_array_no_crash\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    v->CommandClimateControl(true);
    advance_to_profile_phase(v);  // handshake acked -> waiting for the profile array
    kcan(v)->tx_log.clear();

    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, {});  // empty body -> must be ignored, no crash
    CHECK(arm_write_body(kcan(v)).empty(), "empty 49 59 dropped — no arm");
    CHECK(!sent_trigger(kcan(v)), "empty 49 59 dropped — no trigger");

    // A well-formed array afterwards still drives the command to completion.
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06));
    CHECK(sent_trigger(kcan(v)), "valid array after the empty one still triggers");

    delete v;
}

// ---------------------------------------------------------------------------
// Read-only profile list (xvg charge profile list): captures ALL profiles, writes nothing
// ---------------------------------------------------------------------------

// Append one full RA0 profile record (position + 20 fixed bytes + length-prefixed name) to a
// profile-array STATUS body, mirroring make_profile0_array's layout for a single element.
static void append_profile_record(std::vector<uint8_t>& b, uint8_t pos, uint8_t op,
                                   uint8_t maxCurrent, uint8_t minSoc, uint8_t tgtSoc,
                                   uint8_t tempRaw, const char* name) {
    b.push_back(pos);                                        // element position (PosTransmit)
    b.push_back(op); b.push_back(0x00); b.push_back(maxCurrent); b.push_back(minSoc);
    b.push_back(0xff); b.push_back(0xff);                    // [4:5] minRange
    b.push_back(tgtSoc); b.push_back(0xff);                  // [6] targetChargeLevel, [7] duration
    b.push_back(0xff); b.push_back(0xff);                    // [8:9] targetChargeRange
    b.push_back(0xff); b.push_back(0x01);                    // [10] unitRange, [11] rangeCalcSetup
    b.push_back(tempRaw); b.push_back(0x00);                 // [12] temperature, [13] unit
    b.push_back(0x1e); b.push_back(0x1e); b.push_back(0x0f); // [14] lead, [15] holdPlug, [16] holdBatt
    b.push_back(0x00); b.push_back(0x00);                    // [17:18] providerDataId
    uint8_t nl = 0; while (name[nl]) nl++;
    b.push_back(nl);                                         // [19] nameLength
    for (uint8_t i = 0; i < nl; i++) b.push_back((uint8_t)name[i]);
}

// A full profile-array STATUS body with `count` profiles at positions 0..count-1: the global
// "Optionen" (pos 0) carries a MinSoC and NO target; the charge locations (1-3) carry a target SoC.
static std::vector<uint8_t> make_profiles_array(uint8_t count, uint8_t lead = 0x31) {
    std::vector<uint8_t> b = { lead, count, 0x40, 0x00, count };  // arrayId/total/flags(PosTx,RA0)/start/count
    if (count > 0) append_profile_record(b, 0, 0x06, 0x20, 0x50, 0x00, 0x78, "Optionen"); // MinSoC 80%, 22.0C
    if (count > 1) append_profile_record(b, 1, 0x01, 0x0d, 0x00, 0x50, 0xff, "Zuhause");  // TgtSoC 80%, 13A
    if (count > 2) append_profile_record(b, 2, 0x06, 0x20, 0x00, 0x64, 0x6e, "Klima");    // TgtSoC 100%, 21.0C
    if (count > 3) append_profile_record(b, 3, 0x00, 0x00, 0x00, 0x00, 0xff, "");         // empty slot
    return b;
}

void test_list_profiles_readonly() {
    printf("\ntest_list_profiles_readonly\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    CHECK(v->test_batctrl().ListProfiles(), "ListProfiles() accepted (no command in flight)");
    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_PENDING,
          "list state PENDING while the read is in flight");
    advance_to_profile_phase(v);   // wake -> handshake -> ack -> GET issued
    kcan(v)->tx_log.clear();

    // The BCU returns all four profiles. The read-only path must capture every one and STOP.
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profiles_array(4));

    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_READY,
          "list state READY after the profile array arrives");
    CHECK(arm_write_body(kcan(v)).empty(), "read-only: NO profile write (arm) emitted");
    CHECK(!sent_trigger(kcan(v)), "read-only: NO OperationMode trigger emitted");

    bap::egolf::Profile got[4];
    uint8_t n = v->test_batctrl().GetProfiles(got, 4);
    CHECK(n == 4, "all 4 profiles captured");
    if (n == 4) {
        CHECK(got[0].position == 0 && strcmp(got[0].name, "Optionen") == 0, "profile 0 = 'Optionen'");
        CHECK(got[0].minChargeLevel == 0x50 && got[0].targetChargeLevel == 0x00,
              "profile 0 carries MinSoC (80%) and NO target SoC");
        CHECK(got[1].position == 1 && strcmp(got[1].name, "Zuhause") == 0, "profile 1 = 'Zuhause'");
        CHECK(got[1].targetChargeLevel == 0x50 && got[1].maxCurrent == 0x0d,
              "charge location 1 carries a target SoC (80%) at 13 A");
        CHECK(got[2].targetChargeLevel == 0x64, "charge location 2 target SoC = 100%");
        CHECK(got[3].position == 3 && got[3].nameLen == 0, "profile 3 = empty slot");
    }

    // No user notification for a read-only diagnostic (unlike a climate/charge failure).
    CHECK(MyNotify.last_value.empty(), "list read pushes no user notification");

    delete v;
}

// A second ListProfiles() is refused while one is already in flight (one command at a time).
void test_list_profiles_refused_when_busy() {
    printf("\ntest_list_profiles_refused_when_busy\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    CHECK(v->CommandClimateControl(true) == Success, "climate command starts");
    CHECK(!v->test_batctrl().ListProfiles(), "ListProfiles refused while a command is in flight");

    delete v;
}

// ---------------------------------------------------------------------------
// Profile field write (xvg charge profile set): RMW one profile, no trigger
// ---------------------------------------------------------------------------

using PE = VWeGolfBatteryControl::ProfileEdit;

// A charge location (pos 1..3) RMW: change ONLY the flagged fields (target SoC + current), keep the
// operation, the name, and every other byte, target the right array position, and emit NO trigger.
void test_set_profile_location_soc_current() {
    printf("\ntest_set_profile_location_soc_current\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    PE e;
    e.fields = PE::F_TGTSOC | PE::F_CURRENT;
    e.targetChargeLevel = 90;   // 0x5a
    e.maxCurrent = 10;          // 0x0a
    CHECK(v->test_batctrl().SetProfile(1, e), "SetProfile(1) accepted");
    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_PENDING, "PENDING while in flight");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();

    // BCU returns the full array; profile 1 ("Zuhause", op 0x01, maxC 0x0d, tgtSoC 0x50).
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profiles_array(4));

    auto w = arm_write_body(kcan(v));
    CHECK(w.size() >= 4 + 20, "a full RA0 record is written back");
    if (w.size() >= 4 + 20) {
        CHECK(w[2] == 1, "startIndex targets profile position 1");
        CHECK(w[3] == 1, "count = 1 (single-record write)");
        const uint8_t* rec = w.data() + 4;
        CHECK(rec[6] == 0x5a, "targetChargeLevel 0x50 -> 0x5a (90%)");
        CHECK(rec[2] == 0x0a, "maxCurrent 0x0d -> 0x0a (10 A)");
        CHECK(rec[0] == 0x01, "operation UNCHANGED (F_OP not set)");
        CHECK(rec[19] == 7 && memcmp(rec + 20, "Zuhause", 7) == 0, "name 'Zuhause' preserved verbatim");
    }
    CHECK(!sent_trigger(kcan(v)), "profile set emits NO OperationMode trigger");

    // Confirmed on the BCU's SET echo (0xb1, bit7), like the idle SetChargeCurrent write.
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, {0xb1, 0x00, 0x00, 0x01});
    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_READY,
          "profile set confirmed on the write echo");
    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);
    CHECK(MyNotify.count == 0, "CLI op: reports via ListStatus, pushes no user notification");

    delete v;
}

// The global "Optionen" profile (pos 0) RMW: minSoC + target temp change; operation, maxCurrent, and
// the name are all preserved.
void test_set_profile_global_minsoc_temp() {
    printf("\ntest_set_profile_global_minsoc_temp\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    PE e;
    e.fields = PE::F_MINSOC | PE::F_TEMP;
    e.minChargeLevel = 20;                                   // 0x14
    e.temperatureRaw = bap::egolf::tempToRawDeci(210);       // 21.0 C -> 0x6e
    CHECK(v->test_batctrl().SetProfile(0, e), "SetProfile(0) accepted");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();

    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profiles_array(4));
    auto w = arm_write_body(kcan(v));
    CHECK(w.size() >= 4 + 20, "a full RA0 record is written back");
    if (w.size() >= 4 + 20) {
        CHECK(w[2] == 0, "startIndex targets profile position 0 (global)");
        const uint8_t* rec = w.data() + 4;
        CHECK(rec[3] == 0x14, "minChargeLevel 0x50 -> 0x14 (20%)");
        CHECK(rec[12] == 0x6e, "temperature 0x78 -> 0x6e (21.0 C)");
        CHECK(rec[0] == 0x06, "operation preserved (climate control owns profile 0's op, not 'set')");
        CHECK(rec[2] == 0x20, "maxCurrent preserved (not in the edit)");
        CHECK(rec[19] == 8 && memcmp(rec + 20, "Optionen", 8) == 0, "name 'Optionen' preserved");
    }
    CHECK(!sent_trigger(kcan(v)), "profile set emits NO trigger");

    delete v;
}

// Setting the flags (charge/climate) on a location is a bit-RMW: set only the requested charge/climate
// bits, PRESERVE every other operation bit (e.g. PO_ALLOW_BATTERY) the profile already carries.
void test_set_profile_flags_preserve_other_bits() {
    printf("\ntest_set_profile_flags_preserve_other_bits\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    PE e;
    e.fields = PE::F_OP;
    e.operation = bap::egolf::PO_CLIMATE;   // "flags climate": climate only (clears charge)
    CHECK(v->test_batctrl().SetProfile(1, e), "SetProfile(1, flags) accepted");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();

    // Profile 1 read back as op 0x05 = charge (0x01) + climatise-on-battery (0x04).
    std::vector<uint8_t> body = {0x31, 0x01, 0x40, 0x00, 0x01};
    append_profile_record(body, 1, 0x05, 0x0d, 0x00, 0x50, 0xff, "Zuhause");
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, body);

    auto w = arm_write_body(kcan(v));
    CHECK(w.size() >= 4 + 20, "record written");
    if (w.size() >= 4 + 20)
        // (0x05 & ~0x03) | (0x02 & 0x03) = 0x04 | 0x02 = 0x06: charge cleared, climate set,
        // PO_ALLOW_BATTERY (0x04) preserved.
        CHECK(w[4 + 0] == 0x06, "op 0x05 -> 0x06: climate set, charge cleared, allow-battery preserved");

    delete v;
}

// SAFETY chokepoint: a profile-set write must hard-cap maxCurrent at 0x20 even when the read-back byte
// is garbled above the limit and the edit does NOT touch current (a value >0x20 bricks charging).
void test_set_profile_hard_caps_current() {
    printf("\ntest_set_profile_hard_caps_current\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    PE e;
    e.fields = PE::F_MINSOC;   // edit minSoC only — current is NOT in the edit
    e.minChargeLevel = 30;
    CHECK(v->test_batctrl().SetProfile(0, e), "SetProfile(0) accepted");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();

    // Profile 0 read back with a garbled maxCurrent 0xFF.
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, make_profile0_array(0x06, 0x31, 0xFF));
    auto w = arm_write_body(kcan(v));
    CHECK(w.size() >= 4 + 20, "record written");
    if (w.size() >= 4 + 20)
        CHECK(w[4 + 2] == 0x20, "maxCurrent 0xFF read-back HARD-CAPPED to 0x20 in the write");

    delete v;
}

// A profile whose stored name is too long to model (nameTruncated) can't be safely RMW'd — re-encoding
// would write a shorter name and corrupt the record. The set must ABORT with no write, then report FAILED.
void test_set_profile_truncated_name_aborts() {
    printf("\ntest_set_profile_truncated_name_aborts\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    PE e;
    e.fields = PE::F_TGTSOC;
    e.targetChargeLevel = 80;
    CHECK(v->test_batctrl().SetProfile(1, e), "SetProfile(1) accepted");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();

    // Profile 1 with a 30-char name (> kProfileMaxName 24) -> decodes nameTruncated.
    std::vector<uint8_t> body = {0x31, 0x01, 0x40, 0x00, 0x01};
    append_profile_record(body, 1, 0x01, 0x0d, 0x00, 0x50, 0xff, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    inject_bap_status(v, bap::egolf::FUNC_PROFILES_ARRAY, body);

    CHECK(arm_write_body(kcan(v)).empty(), "truncated-name profile: NO write emitted (unsafe RMW aborted)");
    for (int i = 0; i <= VWEGOLF_BATCTRL_WAKE_SECS + 1; i++) call_ticker1(v, i);
    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_FAILED,
          "unsafe-to-edit profile reports FAILED, not a silent corruption");

    delete v;
}

// Guard rails: an empty edit is rejected, and a set is refused while another command is in flight.
void test_set_profile_guards() {
    printf("\ntest_set_profile_guards\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    PE empty;  // fields == 0
    CHECK(!v->test_batctrl().SetProfile(1, empty), "empty edit (no fields) refused");

    PE e; e.fields = PE::F_TGTSOC; e.targetChargeLevel = 80;
    CHECK(v->CommandClimateControl(true) == Success, "climate command starts");
    CHECK(!v->test_batctrl().SetProfile(1, e), "SetProfile refused while a command is in flight");

    delete v;
}

// ---------------------------------------------------------------------------
// Departure timers (xvg charge timer ...): read-back decode + write framing on 0x17332500
// ---------------------------------------------------------------------------

static const uint32_t BAP_TIMER_ID = 0x17332500;  // the MIB's FCAN-local id — we must NOT transmit here

// An 8-byte timer record [year mon day hr min weekdays refId pad]. Recurring => date bytes 0xFF.
static std::vector<uint8_t> timer_rec(uint8_t hr, uint8_t min, uint8_t weekdays, uint8_t refId) {
    return {0xff, 0xff, 0xff, hr, min, weekdays, refId, 0x00};
}

// Reassemble a timer RECORD element (SET_GET, func 0x14..0x17) written on 0x17332500; func_out = its
// func. Empty if none.
static std::vector<uint8_t> timer_record_body(canbus* bus, uint8_t& func_out) {
    bap::AssemblerT<256, 4> asmb;
    for (auto& r : bus->tx_log) {
        if (!r.extended || r.id != BAP_CMD_ID) continue;
        bap::Element el;
        if (asmb.feed(r.data, r.len, el) && el.opcode == bap::OP_SET_GET &&
            el.lsg == bap::egolf::kLsg && bap::egolf::timerSlotForFunc(el.func) >= 1) {
            func_out = el.func;
            return std::vector<uint8_t>(el.body, el.body + el.bodyLen);
        }
    }
    return {};
}

// The last TimerState mask (29 53) written on 0x17332500, or -1 if none.
static int timer_mask_written(canbus* bus) {
    bap::AssemblerT<256, 4> asmb;
    int mask = -1;
    for (auto& r : bus->tx_log) {
        if (!r.extended || r.id != BAP_CMD_ID) continue;
        bap::Element el;
        if (asmb.feed(r.data, r.len, el) && el.opcode == bap::OP_SET_GET &&
            el.lsg == bap::egolf::kLsg && el.func == bap::egolf::FUNC_TIMER_STATE && el.bodyLen >= 1)
            mask = el.body[0];
    }
    return mask;
}

void test_timer_list_readonly() {
    printf("\ntest_timer_list_readonly\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    CHECK(v->test_batctrl().ListTimers(), "ListTimers() accepted");
    advance_to_profile_phase(v);   // wake -> handshake -> ack -> timer GET prompts
    kcan(v)->tx_log.clear();

    // The BCU broadcasts the 3 records + the enable mask (inject as STATUS 49 5x / 49 53). Mask last so
    // the "all slots + mask" early-finish fires on the last frame. (A slot-4 broadcast is ignored — the
    // e-Golf has 3 slots; injecting FUNC_TIMER_4 must NOT be stored.)
    inject_bap_status(v, bap::egolf::FUNC_TIMER_1, timer_rec(0x07, 0x00, 0x3e, 0x01));  // 07:00 Mon-Fri p1
    inject_bap_status(v, bap::egolf::FUNC_TIMER_2, timer_rec(0x0a, 0x00, 0x40, 0x02));  // 10:00 Sat    p2
    inject_bap_status(v, bap::egolf::FUNC_TIMER_4, timer_rec(0x06, 0x00, 0x02, 0x03));  // slot 4: IGNORED
    inject_bap_status(v, bap::egolf::FUNC_TIMER_3, timer_rec(0x10, 0x1e, 0xfe, 0x00));  // 16:30 daily  p0
    inject_bap_status(v, bap::egolf::FUNC_TIMER_STATE, {0x05, 0x00});                   // slots 1+3 enabled

    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_READY,
          "timer list READY once mask + all 3 records are in");
    uint8_t f = 0;
    CHECK(timer_record_body(kcan(v), f).empty(), "read-only: NO timer record write emitted");
    CHECK(timer_mask_written(kcan(v)) == -1, "read-only: NO TimerState write emitted");

    bap::egolf::Timer got[4];
    bool seen[4] = {};
    uint8_t mask = 0;
    uint8_t n = v->test_batctrl().GetTimers(got, 4, mask, seen);
    CHECK(n == 3, "3 timer slots returned (e-Golf has 3, not 4)");
    CHECK(mask == 0x05, "enable mask = 0x05 (slots 1+3)");
    CHECK(!seen[3], "slot-4 broadcast was ignored (only 3 slots)");
    if (n == 3) {
        CHECK(seen[0] && got[0].hour == 7 && got[0].weekdays == 0x3e && got[0].refId == 1,
              "slot 1 = 07:00 Mon-Fri, profile 1");
        CHECK(got[2].hour == 16 && got[2].minute == 30 && got[2].weekdays == 0xfe && got[2].refId == 0,
              "slot 3 = 16:30 daily, profile 0");
    }
    CHECK(MyNotify.last_value.empty(), "timer list pushes no user notification");
    delete v;
}

void test_timer_set_writes_record_and_mask() {
    printf("\ntest_timer_set_writes_record_and_mask\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    bap::egolf::Timer t;
    t.hour = 7; t.minute = 0; t.weekdays = 0x3e; t.refId = 1;  // 07:00 Mon-Fri, profile 1 (recurring)
    CHECK(v->test_batctrl().SetTimer(2, t), "SetTimer(slot 2) accepted");
    advance_to_profile_phase(v);

    // ALL timer traffic goes on kCanIdCommand (0x17332501) — our KCAN node's channel, which the gateway
    // bridges to the FCAN bus where the BCU lives. The MIB's 0x17332500 is FCAN-local and NOT bridged,
    // so a write there from KCAN never reaches the BCU (confirmed on-car).
    bool hs_cmd = false;
    for (auto& f : ext_frames(kcan(v), BAP_CMD_ID)) if (f.len >= 2 && f.data[0] == 0x19 && f.data[1] == 0x42) hs_cmd = true;
    CHECK(hs_cmd, "timer handshake goes on 0x17332501 (our bus's gatewayed channel)");
    CHECK(ext_frames(kcan(v), BAP_TIMER_ID).empty(), "NO timer traffic on the FCAN-local 0x17332500");

    kcan(v)->tx_log.clear();

    // The write is gated on reading the current enable mask. Inject it (mask 0 = nothing enabled).
    inject_bap_status(v, bap::egolf::FUNC_TIMER_STATE, {0x00, 0x00});

    uint8_t func = 0;
    auto rec = timer_record_body(kcan(v), func);
    CHECK(rec.size() == 8, "a full 8-byte timer record was written");
    CHECK(func == bap::egolf::FUNC_TIMER_2, "slot 2 -> func 0x15 (header '29 55'), slot in the func");
    if (rec.size() == 8) {
        CHECK(rec[0] == 0xff && rec[1] == 0xff && rec[2] == 0xff, "recurring: date bytes 0xFF");
        CHECK(rec[3] == 0x07 && rec[4] == 0x00, "07:00 -> hour 0x07, minute 0x00");
        CHECK(rec[5] == 0x3e, "weekdays = Mon-Fri (0x3e)");
        CHECK(rec[6] == 0x01, "refId = profile 1");
        CHECK(rec[7] == 0x00, "trailing pad 0x00");
    }
    CHECK(timer_mask_written(kcan(v)) == 0x02, "enable mask RMW: 0x00 | slot-2 bit = 0x02");

    // BCU echoes the new mask -> confirmed.
    inject_bap_status(v, bap::egolf::FUNC_TIMER_STATE, {0x02, 0x00});
    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_READY,
          "SetTimer confirmed once the mask echo reflects the write");
    delete v;
}

void test_timer_enable_disable_rmw() {
    printf("\ntest_timer_enable_disable_rmw\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};

    // Enable slot 3 with slot 1 already on: mask 0x01 -> 0x05, NO record write.
    auto* v = new OvmsVehicleVWeGolf();
    CHECK(v->test_batctrl().EnableTimer(3, true), "EnableTimer(3, on) accepted");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_TIMER_STATE, {0x01, 0x00});
    uint8_t f = 0;
    CHECK(timer_record_body(kcan(v), f).empty(), "enable: NO timer record write (mask-only RMW)");
    CHECK(timer_mask_written(kcan(v)) == 0x05, "enable RMW: 0x01 | slot-3 bit = 0x05");
    inject_bap_status(v, bap::egolf::FUNC_TIMER_STATE, {0x05, 0x00});
    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_READY, "enable confirmed");
    delete v;

    // Disable slot 2 with slots 1+2+3 on: mask 0x07 -> 0x05.
    v = new OvmsVehicleVWeGolf();
    CHECK(v->test_batctrl().EnableTimer(2, false), "EnableTimer(2, off) accepted");
    advance_to_profile_phase(v);
    kcan(v)->tx_log.clear();
    inject_bap_status(v, bap::egolf::FUNC_TIMER_STATE, {0x07, 0x00});
    CHECK(timer_mask_written(kcan(v)) == 0x05, "disable RMW: 0x07 & ~slot-2 bit = 0x05");
    delete v;
}

// The persistent cache: an ambient TimerState broadcast BEFORE any command lets a later SET write
// immediately at the handshake ack, with no in-command mask-broadcast wait (the ~13 s -> ~instant win).
void test_timer_set_uses_cache() {
    printf("\ntest_timer_set_uses_cache\n");
    g_metrics = MetricStore{};
    MyNotify = OvmsNotify{};
    auto* v = new OvmsVehicleVWeGolf();

    // Ambient broadcast (no command in flight) populates the cache: mask 0x01 (slot 1 enabled).
    auto sts = make_kcan_frame(v, BAP_STS_ID, {0x49, 0x58, 0x00});  // first BCU frame (opens the asm)
    v->IncomingFrameCan3(&sts);
    inject_bap_status(v, bap::egolf::FUNC_TIMER_STATE, {0x01, 0x00});
    CHECK(v->test_batctrl().TimerMaskSeen(), "ambient broadcast populated the persistent mask cache");

    // SET slot 2 -> the write must fire from the cache at the handshake ack (no mask injected here).
    bap::egolf::Timer t;
    t.hour = 7; t.minute = 0; t.weekdays = 0x3e; t.refId = 1;
    CHECK(v->test_batctrl().SetTimer(2, t), "SetTimer accepted");
    advance_to_profile_phase(v);  // wake -> handshake -> ack; write should fire HERE from the cache

    uint8_t func = 0;
    auto rec = timer_record_body(kcan(v), func);
    CHECK(rec.size() == 8 && func == bap::egolf::FUNC_TIMER_2,
          "record written immediately from cache at the handshake ack (no broadcast wait)");
    CHECK(timer_mask_written(kcan(v)) == 0x03, "mask RMW from cache: 0x01 | slot-2 bit = 0x03");

    // BCU echoes the new mask -> confirmed.
    inject_bap_status(v, bap::egolf::FUNC_TIMER_STATE, {0x03, 0x00});
    CHECK(v->test_batctrl().ListStatus() == VWeGolfBatteryControl::LIST_READY, "SetTimer confirmed");
    delete v;
}

// ---------------------------------------------------------------------------

void test_bat_ctrl_all() {
    test_climate_command_kicks_nm_wake();
    test_climate_ticks_wait_for_bcu();
    test_climate_start_sequence();
    test_climate_stop_sequence();
    test_climate_ignores_foreign_asg_array();
    test_climate_arm_sets_op_preserves_fields();
    test_climate_no_trigger_without_profile();
    test_climate_multiple_commands_reset_assembler();
    test_climate_confirm_sets_hvac();
    test_climate_hvac_from_5ea();
    test_climate_hvac_clears_on_sleep();
    test_climate_tx_fail_notifies();
    test_climate_error_notifies();
    test_climate_abort_releases_wake();
    // charge control (shared BCU path)
    test_charge_start_sequence();
    test_charge_stop_sequence();
    test_charge_stop_blocked_when_climate_on();
    test_empty_profile_array_no_crash();
    test_set_charge_current();
    test_set_charge_current_auto_apply();
    test_set_charge_current_clamps();
    test_arm_hard_caps_max_current();
    test_arm_skips_write_when_op_matches();
    test_error_aborts_no_retry_no_confirm();
    test_profile_read_populates_metrics();
    // read-only profile list (xvg charge profile list)
    test_list_profiles_readonly();
    test_list_profiles_refused_when_busy();
    // profile field write (xvg charge profile set)
    test_set_profile_location_soc_current();
    test_set_profile_global_minsoc_temp();
    test_set_profile_flags_preserve_other_bits();
    test_set_profile_hard_caps_current();
    test_set_profile_truncated_name_aborts();
    test_set_profile_guards();
    // departure timers (xvg charge timer ...)
    test_timer_list_readonly();
    test_timer_set_writes_record_and_mask();
    test_timer_enable_disable_rmw();
    test_timer_set_uses_cache();
}
