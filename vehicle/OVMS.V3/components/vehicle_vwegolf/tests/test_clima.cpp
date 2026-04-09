// test_clima.cpp — Tests for climate control, wakeup, heartbeat, and bus idle logic.

#include "mock/mock_ovms.hpp"
#include "../src/vehicle_vwegolf.h"

#include <cassert>
#include <cstdio>

extern int tests_run;
extern int tests_passed;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else       { printf("  FAIL: %s\n", msg); } \
} while(0)

// Helper: get the KCAN (can3) stub from a vehicle instance.
static canbus* kcan(OvmsVehicleVWeGolf* v) {
    return v->m_can3;
}

// Ticker1/Ticker10 are protected in OvmsVehicleVWeGolf. Call via base-class pointer
// (public virtual in the mock OvmsVehicle) to dispatch correctly.
static void call_ticker1(OvmsVehicleVWeGolf* v, uint32_t tick) {
    static_cast<OvmsVehicle*>(v)->Ticker1(tick);
}

// Helper: make a KCAN frame with origin set to m_can3 so the bus-idle logic accepts it.
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

// ---------------------------------------------------------------------------
// CommandWakeup
// ---------------------------------------------------------------------------

void test_wakeup_sends_nm_alive() {
    printf("\ntest_wakeup_sends_nm_alive\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Bus starts idle (m_bus_idle_ticks == VWEGOLF_BUS_TIMEOUT_SECS).
    auto result = v->CommandWakeup();
    CHECK(result == Success, "CommandWakeup returns Success");

    auto& log = kcan(v)->tx_log;
    CHECK(log.size() >= 2, "At least 2 frames sent (wake + NM alive)");

    if (log.size() >= 2) {
        // Frame 1: 0x17330301 dominant-bit wake frame
        CHECK(log[0].extended && log[0].id == 0x17330301,
              "Frame 1 is extended 0x17330301 (wake)");

        // Frame 2: 0x1B000067 NM alive for OVMS node 0x67
        CHECK(log[1].extended && log[1].id == 0x1B000067,
              "Frame 2 is extended 0x1B000067 (NM alive)");
        CHECK(log[1].data[0] == 0x67, "NM alive byte 0 = node ID 0x67");
    }

    delete v;
}

void test_wakeup_skips_when_bus_active() {
    printf("\ntest_wakeup_skips_when_bus_active\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Simulate bus activity by sending a KCAN frame (resets idle counter).
    auto f = make_kcan_frame(v, 0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);

    kcan(v)->tx_log.clear();
    auto result = v->CommandWakeup();
    CHECK(result == Success, "Returns Success when bus already active");
    CHECK(kcan(v)->tx_log.empty(), "No frames sent when bus already active");

    delete v;
}

// ---------------------------------------------------------------------------
// CommandClimateControl — BAP frame construction
// ---------------------------------------------------------------------------

void test_clima_start_bap_frames() {
    printf("\ntest_clima_start_bap_frames\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Simulate bus active so clima doesn't trigger wakeup.
    auto f = make_kcan_frame(v, 0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    kcan(v)->tx_log.clear();

    auto result = v->CommandClimateControl(true);
    CHECK(result == Success, "CommandClimateControl(true) returns Success");

    auto& log = kcan(v)->tx_log;
    // Should have at least 3 extended frames (the BAP sequence).
    // May also have heartbeat frames from IncomingFrameCan3, so check the extended ones.
    size_t ext_count = 0;
    for (auto& r : log) if (r.extended) ext_count++;
    CHECK(ext_count == 3, "3 extended BAP frames sent");

    // Find the 3 extended frames.
    std::vector<TxRecord> bap;
    for (auto& r : log) if (r.extended) bap.push_back(r);

    if (bap.size() >= 3) {
        // Frame 1: multi-frame start to port 0x19
        CHECK(bap[0].id == 0x17332501, "BAP frame 1 ID = 0x17332501");
        CHECK(bap[0].len == 8, "BAP frame 1 DLC = 8");
        CHECK(bap[0].data[0] == 0x80, "BAP frame 1 byte 0 = 0x80 (multi-frame start)");
        CHECK(bap[0].data[1] == 0x08, "BAP frame 1 byte 1 = 0x08 (payload length)");
        CHECK(bap[0].data[2] == 0x29, "BAP frame 1 byte 2 = 0x29 (opcode=2, node=0x25 hi)");
        CHECK(bap[0].data[3] == 0x59, "BAP frame 1 byte 3 = 0x59 (node=0x25 lo, port=0x19)");
        CHECK(bap[0].data[4] == 0x01, "BAP frame 1 byte 4 = counter (first call = 0x01)");

        // Frame 2: continuation with temperature
        CHECK(bap[1].id == 0x17332501, "BAP frame 2 ID = 0x17332501");
        CHECK(bap[1].data[0] == 0xC0, "BAP frame 2 byte 0 = 0xC0 (continuation)");
        // Default temp = 21°C → (21-10)*10 = 110 = 0x6E
        CHECK(bap[1].data[3] == 0x6E, "BAP frame 2 temp byte = 0x6E (21°C)");

        // Frame 3: single-frame trigger to port 0x18, start=0x01
        CHECK(bap[2].id == 0x17332501, "BAP frame 3 ID = 0x17332501");
        CHECK(bap[2].len == 4, "BAP frame 3 DLC = 4");
        CHECK(bap[2].data[0] == 0x29, "BAP frame 3 byte 0 = 0x29");
        CHECK(bap[2].data[1] == 0x58, "BAP frame 3 byte 1 = 0x58 (port 0x18)");
        CHECK(bap[2].data[3] == 0x01, "BAP frame 3 byte 3 = 0x01 (start)");
    }

    // Optimistic metric update
    CHECK(StandardMetrics.ms_v_env_hvac->AsBool(), "HVAC metric set to true after start");

    delete v;
}

void test_clima_stop_bap_trigger() {
    printf("\ntest_clima_stop_bap_trigger\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    auto f = make_kcan_frame(v, 0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    kcan(v)->tx_log.clear();

    v->CommandClimateControl(false);

    // Find the trigger frame (frame 3)
    std::vector<TxRecord> bap;
    for (auto& r : kcan(v)->tx_log) if (r.extended) bap.push_back(r);

    CHECK(bap.size() >= 3, "3 BAP frames sent for stop");
    if (bap.size() >= 3) {
        CHECK(bap[2].data[3] == 0x00, "BAP frame 3 byte 3 = 0x00 (stop)");
    }

    CHECK(!StandardMetrics.ms_v_env_hvac->AsBool(), "HVAC metric set to false after stop");

    delete v;
}

void test_clima_busoff_aborts() {
    printf("\ntest_clima_busoff_aborts\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Simulate bus active
    auto f = make_kcan_frame(v, 0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);

    kcan(v)->error_state = CAN_errorstate_busoff;
    auto result = v->CommandClimateControl(true);
    CHECK(result == Fail, "Returns Fail when KCAN is in bus-off");

    delete v;
}

void test_clima_wakes_sleeping_bus() {
    printf("\ntest_clima_wakes_sleeping_bus\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Bus starts idle (no frames received). CommandClimateControl should wake first.
    auto result = v->CommandClimateControl(true);
    CHECK(result == Success, "Clima start succeeds from idle bus");

    auto& log = kcan(v)->tx_log;
    // Should have wake frames (0x17330301, 0x1B000067) then BAP sequence.
    bool has_wake = false;
    bool has_nm = false;
    bool has_bap = false;
    for (auto& r : log) {
        if (r.extended && r.id == 0x17330301) has_wake = true;
        if (r.extended && r.id == 0x1B000067) has_nm = true;
        if (r.extended && r.id == 0x17332501) has_bap = true;
    }
    CHECK(has_wake, "Wake frame 0x17330301 sent before BAP");
    CHECK(has_nm, "NM alive 0x1B000067 sent before BAP");
    CHECK(has_bap, "BAP frames sent after wake");

    delete v;
}

void test_clima_counter_increments() {
    printf("\ntest_clima_counter_increments\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Make bus active
    auto f = make_kcan_frame(v, 0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);

    // First call: counter should be 0x01
    kcan(v)->tx_log.clear();
    v->CommandClimateControl(true);
    std::vector<TxRecord> bap1;
    for (auto& r : kcan(v)->tx_log) if (r.extended) bap1.push_back(r);
    uint8_t ctr1 = bap1.size() > 0 ? bap1[0].data[4] : 0;

    // Second call: counter should be 0x02
    kcan(v)->tx_log.clear();
    v->CommandClimateControl(false);
    std::vector<TxRecord> bap2;
    for (auto& r : kcan(v)->tx_log) if (r.extended) bap2.push_back(r);
    uint8_t ctr2 = bap2.size() > 0 ? bap2[0].data[4] : 0;

    CHECK(ctr1 == 0x01, "First call counter = 0x01");
    CHECK(ctr2 == 0x02, "Second call counter = 0x02");

    delete v;
}

// ---------------------------------------------------------------------------
// Ticker1 bus idle logic
// ---------------------------------------------------------------------------

void test_ticker1_bus_idle_timeout() {
    printf("\ntest_ticker1_bus_idle_timeout\n");
    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // Make bus active and start OCU
    auto f = make_kcan_frame(v, 0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    v->CommandWakeup();  // wakeup is a no-op since bus is active, but sets m_ocu_active... actually no
    // After receiving a frame, bus is active. We need to explicitly start ocu.
    // Use CommandClimateControl which sets m_ocu_active = true.
    v->CommandClimateControl(true);

    kcan(v)->tx_log.clear();

    // Tick 10 times with no incoming frames → bus goes idle
    for (int i = 0; i < 10; i++) {
        call_ticker1(v, i);
    }

    // After 10 ticks of silence, OCU should be deactivated.
    // Tick 11 should NOT produce heartbeats.
    kcan(v)->tx_log.clear();
    call_ticker1(v, 10);

    bool has_heartbeat = false;
    for (auto& r : kcan(v)->tx_log) {
        if (!r.extended && r.id == 0x5A7) has_heartbeat = true;
    }
    CHECK(!has_heartbeat, "No heartbeat after bus idle timeout");

    delete v;
}

// ---------------------------------------------------------------------------
// Entry point (called from test_can_decode.cpp main)
// ---------------------------------------------------------------------------

void test_clima_all() {
    printf("\n=== climate control tests ===\n");
    test_wakeup_sends_nm_alive();
    test_wakeup_skips_when_bus_active();
    test_clima_start_bap_frames();
    test_clima_stop_bap_trigger();
    test_clima_busoff_aborts();
    test_clima_wakes_sleeping_bus();
    test_clima_counter_increments();
    test_ticker1_bus_idle_timeout();
}
