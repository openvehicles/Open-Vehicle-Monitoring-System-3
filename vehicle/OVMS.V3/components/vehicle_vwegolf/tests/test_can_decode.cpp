// test_can_decode.cpp — Native laptop tests for vehicle_vwegolf CAN frame decoding.
//
// Each test builds a CAN_frame_t with real byte values (taken from car captures
// or the decode comments in vehicle_vwegolf.cpp), calls IncomingFrameCan2/3,
// then checks that the expected metric was set to the expected value.
//
// Run:  make test   (from the tests/ directory)

#include "mock/mock_ovms.hpp"
#include "../src/vehicle_vwegolf.h"

#include <cassert>
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static OvmsVehicleVWeGolf* make_vehicle() {
    g_metrics = MetricStore{};  // reset all metrics before each test
    return new OvmsVehicleVWeGolf();
}

static CAN_frame_t make_frame(uint32_t id, std::initializer_list<uint8_t> bytes) {
    CAN_frame_t f{};
    f.MsgID = id;
    f.FIR.B.DLC = static_cast<uint8_t>(bytes.size());
    int i = 0;
    for (uint8_t b : bytes) f.data.u8[i++] = b;
    return f;
}

static bool near(float a, float b, float tol = 0.01f) {
    return std::fabs(a - b) < tol;
}

int tests_run = 0;
int tests_passed = 0;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else       { printf("  FAIL: %s\n", msg); } \
} while(0)

// ---------------------------------------------------------------------------
// KCAN (CAN3) tests
// ---------------------------------------------------------------------------

void test_soc_0x131() {
    printf("\ntest_soc_0x131\n");
    auto* v = make_vehicle();

    // SoC: byte 3 * 0.5. byte3 = 0xC8 (200 dec) → 200 * 0.5 = 100%
    auto f = make_frame(0x131, {0x00, 0x00, 0x00, 0xC8, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(near(StandardMetrics.ms_v_bat_soc->AsFloat(), 100.0f), "SoC 100% from 0xC8");

    g_metrics = MetricStore{};
    // byte3 = 0x64 (100 dec) → 100 * 0.5 = 50%
    f = make_frame(0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(near(StandardMetrics.ms_v_bat_soc->AsFloat(), 50.0f), "SoC 50% from 0x64");

    delete v;
}

void test_speed_0xFD() {
    printf("\ntest_speed_0xFD\n");
    auto* v = make_vehicle();

    // Speed: bytes 4-5 little-endian * 0.01. 0xE803 = 1000 → 10.00 km/h
    auto f = make_frame(0xFD, {0x00, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(near(StandardMetrics.ms_v_pos_speed->AsFloat(), 10.0f), "Speed 10 km/h");

    delete v;
}

void test_gear_0x187_park() {
    printf("\ntest_gear_0x187 (PRNDL)\n");
    auto* v = make_vehicle();

    // Gear nibble is byte2 & 0x0F. 2 = Park
    auto f = make_frame(0x187, {0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan2(&f);
    CHECK(StandardMetrics.ms_v_env_gear->AsValue() == 0, "Park → gear 0");

    // 3 = Reverse
    f = make_frame(0x187, {0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan2(&f);
    CHECK(StandardMetrics.ms_v_env_gear->AsValue() == -1, "Reverse → gear -1");

    // 5 = Drive
    f = make_frame(0x187, {0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan2(&f);
    CHECK(StandardMetrics.ms_v_env_gear->AsValue() == 1, "Drive → gear 1");

    delete v;
}

void test_vin_0x6B4() {
    printf("\ntest_vin_0x6B4\n");
    auto* v = make_vehicle();

    // VIN is split across 3 frames identified by byte0 (0, 1, 2).
    // Frame 0: bytes 5-7 = chars 0-2
    auto f0 = make_frame(0x6B4, {0x00, 0x00, 0x00, 0x00, 0x00, 'W', 'V', 'W'});
    v->IncomingFrameCan2(&f0);

    // Frame 1: bytes 1-7 = chars 3-9
    auto f1 = make_frame(0x6B4, {0x01, 'Z', 'Z', 'Z', '1', '2', '3', '4'});
    v->IncomingFrameCan2(&f1);

    // Frame 2: bytes 1-7 = chars 10-16
    auto f2 = make_frame(0x6B4, {0x02, '5', '6', '7', '8', '9', 'A', 'B'});
    v->IncomingFrameCan2(&f2);

    CHECK(StandardMetrics.ms_v_vin->AsValue() == "WVWZZZ123456789AB",
          "VIN assembled from 3 frames");

    delete v;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

extern void test_crtd_replay();

int main() {
    printf("=== vehicle_vwegolf CAN decode tests ===\n");

    test_soc_0x131();
    test_speed_0xFD();
    test_gear_0x187_park();
    test_vin_0x6B4();
    test_crtd_replay();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
