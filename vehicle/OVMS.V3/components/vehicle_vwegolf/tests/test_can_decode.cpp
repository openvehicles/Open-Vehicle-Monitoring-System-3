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

void test_gps_0x486() {
    printf("\ntest_gps_0x486\n");

    // Real frame from kcan-can3-clima_on_off.crtd, verified against hardware GPS.
    // lat_raw = 0x0395cc6d → 60148845 * 1e-6 = 60.148845°N
    // lon_raw = 0x00e7b586 → 15185286 * 1e-6 = 15.185286°E
    // bit55 (d[6] MSB) = 0 → North, bit56 (d[7] bit0) = 0 → East
    {
        auto* v = make_vehicle();
        auto f = make_frame(0x486, {0x6d, 0xcc, 0x95, 0x33, 0xac, 0x3d, 0x07, 0xd0});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_pos_latitude->AsFloat(),  60.148845f, 0.000001f),
              "Latitude  60.148845°N from real capture");
        CHECK(near(StandardMetrics.ms_v_pos_longitude->AsFloat(), 15.185286f, 0.000001f),
              "Longitude 15.185286°E from real capture");
        CHECK(StandardMetrics.ms_v_pos_gpslock->AsBool(), "gpslock true for valid frame");
        delete v;
    }

    // Sentinel frame (all 0xFF except d[7]): lat=134°, lon=268° — both out of range.
    // Metrics must NOT be overwritten; gpslock must be cleared.
    {
        auto* v = make_vehicle();
        // Pre-seed a known good position so we can verify it is NOT overwritten.
        StandardMetrics.ms_v_pos_latitude->SetValue(60.0f);
        StandardMetrics.ms_v_pos_longitude->SetValue(15.0f);
        auto f = make_frame(0x486, {0xfe, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0x3d});
        v->IncomingFrameCan3(&f);
        CHECK(!StandardMetrics.ms_v_pos_gpslock->AsBool(),
              "gpslock false for sentinel frame");
        CHECK(near(StandardMetrics.ms_v_pos_latitude->AsFloat(),  60.0f, 0.001f),
              "Latitude not overwritten by sentinel");
        CHECK(near(StandardMetrics.ms_v_pos_longitude->AsFloat(), 15.0f, 0.001f),
              "Longitude not overwritten by sentinel");
        delete v;
    }

    // Southern hemisphere: same magnitude as N/E frame but d[6] bit7 set → lat negative.
    {
        auto* v = make_vehicle();
        // d[6] = 0x07 | 0x80 = 0x87 sets the lat sign bit (Southern)
        auto f = make_frame(0x486, {0x6d, 0xcc, 0x95, 0x33, 0xac, 0x3d, 0x87, 0xd0});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_pos_latitude->AsFloat(), -60.148845f, 0.000001f),
              "Latitude -60.148845°S (sign bit d[6] MSB)");
        CHECK(near(StandardMetrics.ms_v_pos_longitude->AsFloat(), 15.185286f, 0.000001f),
              "Longitude unchanged (East) when only lat sign set");
        CHECK(StandardMetrics.ms_v_pos_gpslock->AsBool(), "gpslock true for S hemisphere");
        delete v;
    }

    // Western hemisphere: same magnitude but d[7] bit0 set → lon negative.
    {
        auto* v = make_vehicle();
        // d[7] = 0xd0 | 0x01 = 0xd1 sets the lon sign bit (Western)
        auto f = make_frame(0x486, {0x6d, 0xcc, 0x95, 0x33, 0xac, 0x3d, 0x07, 0xd1});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_pos_latitude->AsFloat(),  60.148845f, 0.000001f),
              "Latitude unchanged (North) when only lon sign set");
        CHECK(near(StandardMetrics.ms_v_pos_longitude->AsFloat(), -15.185286f, 0.000001f),
              "Longitude -15.185286°W (sign bit d[7] bit0)");
        CHECK(StandardMetrics.ms_v_pos_gpslock->AsBool(), "gpslock true for W hemisphere");
        delete v;
    }
}

// ---------------------------------------------------------------------------
// Sentinel filter tests
// ---------------------------------------------------------------------------
// Each frame below carries the "not ready" sentinel value from real startup
// captures (logs-capture2.md). The decoder must discard it; the pre-seeded
// good value must survive unchanged.

void test_sentinel_filters() {
    printf("\ntest_sentinel_filters\n");

    // 0x131 SoC — d[3]=0xFE sentinel (decodes to 127%)
    {
        auto* v = make_vehicle();
        StandardMetrics.ms_v_bat_soc->SetValue(55.0f);
        auto f = make_frame(0x131, {0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_bat_soc->AsFloat(), 55.0f),
              "0x131 SoC not overwritten by 0xFE sentinel");
        delete v;
    }

    // 0x191 BMS — d[2]=0xFF sentinel (decodes to I=2047A, V=1023.5V)
    {
        auto* v = make_vehicle();
        StandardMetrics.ms_v_bat_current->SetValue(10.0f);
        StandardMetrics.ms_v_bat_voltage->SetValue(350.0f);
        auto f = make_frame(0x191, {0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_bat_current->AsFloat(), 10.0f),
              "0x191 bat_current not overwritten by 0xFF sentinel");
        CHECK(near(StandardMetrics.ms_v_bat_voltage->AsFloat(), 350.0f),
              "0x191 bat_voltage not overwritten by 0xFF sentinel");
        delete v;
    }

    // 0x59E bat_temp — d[2]=0xFE sentinel (decodes to 87°C)
    {
        auto* v = make_vehicle();
        StandardMetrics.ms_v_bat_temp->SetValue(25.0f);
        auto f = make_frame(0x59E, {0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_bat_temp->AsFloat(), 25.0f),
              "0x59E bat_temp not overwritten by 0xFE sentinel");
        delete v;
    }

    // 0x5CA bat_capacity — (d[2]&0x7F)==0x7F sentinel (decodes to ~102 kWh)
    {
        auto* v = make_vehicle();
        StandardMetrics.ms_v_bat_capacity->SetValue(35.0f);
        auto f = make_frame(0x5CA, {0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_bat_capacity->AsFloat(), 35.0f),
              "0x5CA bat_capacity not overwritten by 0x7F sentinel");
        delete v;
    }

    // 0x5EA cabin temp — 10-bit raw >=0x3FE sentinel (decodes to ~62°C)
    // raw = ((d[6]&0xfc)>>2) | ((d[7]&0xf)<<6). Set d[6]=0xFC, d[7]=0x0F → raw=0x3FF.
    {
        auto* v = make_vehicle();
        StandardMetrics.ms_v_env_cabintemp->SetValue(20.0f);
        auto f = make_frame(0x5EA, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x0F});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_env_cabintemp->AsFloat(), 20.0f),
              "0x5EA cabin_temp not overwritten by 0x3FF sentinel");
        delete v;
    }
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
    test_gps_0x486();
    test_sentinel_filters();
    test_crtd_replay();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
