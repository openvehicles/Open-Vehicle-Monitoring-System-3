// test_can_decode.cpp — Native laptop tests for vehicle_vwegolf CAN frame decoding.
//
// Each test builds a CAN_frame_t with real byte values (taken from car captures
// or the decode comments in vehicle_vwegolf.cpp), calls IncomingFrameCan3,
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

// v.e.locked from 0x583 d[7] (0x80 locked / 0x40 unlocked). Frames are real KCAN captures
// (2026-11-12 / 2026-11-20); d[2] is the lock-state value that the old decode read bit 1 of.
void test_lock_0x583() {
    printf("\ntest_lock_0x583\n");
    auto* v = make_vehicle();

    auto f = make_frame(0x583, {0x00, 0x00, 0x0A, 0x00, 0x00, 0x08, 0x57, 0x80});  // fob lock
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_locked->AsBool() == true, "fob lock (d[2]=0x0A) -> locked");

    f = make_frame(0x583, {0x00, 0x00, 0x40, 0x00, 0x00, 0x08, 0x53, 0x40});  // fob unlock, single press
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_locked->AsBool() == false, "partly unlocked (d[2]=0x40) -> unlocked");

    // Regression: locked from inside (central-lock button) has d[2] bit 1 clear.
    f = make_frame(0x583, {0x00, 0x00, 0x05, 0x00, 0x00, 0x08, 0x57, 0x80});
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_locked->AsBool() == true, "locked from inside (d[2]=0x05) -> locked");

    f = make_frame(0x583, {0x00, 0x00, 0x80, 0x00, 0x00, 0x08, 0x43, 0x40});  // all unlocked
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_locked->AsBool() == false, "all unlocked (d[2]=0x80) -> unlocked");

    // Locked from inside with a valid key and ignition on (alarm disarmed, d[1]=0x10).
    f = make_frame(0x583, {0x00, 0x10, 0x05, 0x00, 0x00, 0x08, 0x57, 0x80});
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_locked->AsBool() == true, "locked, ignition on (d[1]=0x10) -> locked");

    // A d[7] value never seen on-car leaves the metric unchanged.
    f = make_frame(0x583, {0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_locked->AsBool() == true, "unknown d[7]=0x00 -> unchanged");

    // Door bits are unaffected: driver door open while all unlocked.
    f = make_frame(0x583, {0x00, 0x00, 0x80, 0x01, 0x00, 0x08, 0x43, 0x40});
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_door_fl->AsBool() == true &&
          StandardMetrics.ms_v_env_locked->AsBool() == false, "FL open + unlocked");

    delete v;
}

// Body signals: windows, per-door locks, bonnet, side lights, steering side. Frames are real KCAN
// captures (2026-11-12 / 2026-11-20, LHD car) except the RHD 0x32A value, which comes from the OEM OCU
// firmware and has not been seen on a car.
static double num(const char* name) { return g_metrics.numbers[name]; }

void test_body_signals() {
    printf("\ntest_body_signals\n");
    auto* v = make_vehicle();
    auto feed = [&](uint32_t id, std::initializer_list<uint8_t> bytes) {
        auto f = make_frame(id, bytes);
        v->IncomingFrameCan3(&f);
    };

    // Windows (LHD until 0x32A says otherwise): closed, moving, half, fully open, invalid.
    CHECK(num("xvg.v.d.window.fl") == -1, "window not seen yet -> -1");
    feed(0x3D0, {0x02, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x03, 0x00});  // driver: closed, locked
    CHECK(num("xvg.v.d.window.fl") == 0 && num("xvg.v.d.lock.fl") == 1, "driver closed + locked -> fl 0 %, fl locked");
    feed(0x3D0, {0x02, 0x00, 0x80, 0x06, 0x82, 0x00, 0x03, 0x00});  // moving down, raw 6
    CHECK(num("xvg.v.d.window.fl") == 3, "driver moving down, raw 6 -> 3 %");
    feed(0x3D0, {0x02, 0x00, 0x00, 0x73, 0x80, 0x00, 0x03, 0x00});  // raw 115 ("about half")
    CHECK(num("xvg.v.d.window.fl") == 58, "driver raw 115 -> 58 %");
    feed(0x3D0, {0x02, 0x00, 0x00, 0xc1, 0x80, 0x00, 0x03, 0x00});  // raw 193, fully down
    CHECK(num("xvg.v.d.window.fl") == 97, "driver raw 193 -> 97 %");
    feed(0x3CF, {0x00, 0x00, 0x00, 0xc8, 0x88, 0x10, 0x80, 0x00});  // rear passenger side, raw 200, end stop
    CHECK(num("xvg.v.d.window.rr") == 100, "rear passenger side raw 200 -> rr 100 %");
    feed(0x3CE, {0x00, 0x00, 0x00, 0x02, 0x84, 0x10, 0x80, 0x00});  // rear driver side closed, reads 2
    CHECK(num("xvg.v.d.window.rl") == 0, "rear driver side at closed stop (raw 2) -> rl 0 %");
    feed(0x3CF, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01});  // first frame after a wake: invalid
    CHECK(num("xvg.v.d.window.rr") == 100, "invalid position (d[4] bit 7 clear) -> rr unchanged");
    feed(0x3D1, {0x00, 0x00, 0x01, 0x00, 0x8c, 0x00, 0x00, 0x30});  // passenger: closed, unlocked
    CHECK(num("xvg.v.d.window.fr") == 0 && num("xvg.v.d.lock.fr") == 0, "passenger closed + unlocked");

    // Rear door locks from 0x583 d[6] (the rear window frames carry no lock state).
    feed(0x583, {0x00, 0x00, 0x0a, 0x00, 0x00, 0x08, 0x57, 0x80});  // locked with the fob
    CHECK(num("xvg.v.d.lock.rl") == 1 && num("xvg.v.d.lock.rr") == 1, "fob lock -> rl / rr locked");
    feed(0x583, {0x00, 0x00, 0x40, 0x00, 0x00, 0x08, 0x53, 0x40});  // single-press unlock
    CHECK(num("xvg.v.d.lock.rl") == 0 && num("xvg.v.d.lock.rr") == 1, "single-press unlock -> rl unlocked, rr locked");
    feed(0x583, {0x00, 0x00, 0x80, 0x04, 0x00, 0x08, 0x43, 0x40});  // all unlocked, rear left open
    CHECK(num("xvg.v.d.lock.rr") == 0 && num("ms_v_door_rl") == 1 && num("ms_v_door_fl") == 0,
          "all unlocked, rear driver side door open -> rr unlocked, v.d.rl open");

    // Bonnet: d[3] bit 7 is the switch; d[4] bit 0 follows 10 s later and stayed set after closing with
    // the ignition off (body-check, 2026-11-20).
    feed(0x65A, {0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x3c, 0x00});  // release pulled: popped open
    CHECK(num("ms_v_door_hood") == 1, "bonnet released -> v.d.hood open at once");
    feed(0x65A, {0x00, 0x00, 0x00, 0x88, 0x01, 0x00, 0x3c, 0x00});  // 10 s later: d[4] bit 0 too
    CHECK(num("ms_v_door_hood") == 1, "d[4] bit 0 set as well -> still open");
    feed(0x65A, {0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x3c, 0x00});  // closed, d[4] bit 0 still set
    CHECK(num("ms_v_door_hood") == 0, "bonnet closed, d[4] bit 0 still set -> v.d.hood closed");
    feed(0x65A, {0x00, 0x00, 0x00, 0x08, 0x00, 0x80, 0x3d, 0x00});
    CHECK(num("ms_v_door_hood") == 0, "both bits clear -> closed");

    // Side lights.
    feed(0x3D5, {0x00, 0x00, 0x40, 0x02, 0x04, 0x00, 0x00, 0x00});
    CHECK(num("xvg.v.e.sidelights") == 1, "light switch on side lights -> sidelights on");
    feed(0x3D5, {0x00, 0x00, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00});
    CHECK(num("xvg.v.e.sidelights") == 0, "light switch off -> sidelights off");

    // Steering side: LHD as read on the test car, then RHD swaps every left / right pair.
    feed(0x32A, {0x1c, 0x01, 0x00, 0x40, 0x80, 0x02, 0x00, 0x00});  // d[5] bits 1-2 = 01
    CHECK(num("xvg.v.rhd") == 0, "0x32A d[5]=0x02 -> LHD");
    feed(0x32A, {0x1c, 0x01, 0x00, 0x40, 0x80, 0x04, 0x00, 0x00});  // d[5] bits 1-2 = 10
    CHECK(num("xvg.v.rhd") == 1, "0x32A d[5]=0x04 -> RHD");
    feed(0x3D0, {0x02, 0x00, 0x00, 0x73, 0x80, 0x00, 0x03, 0x00});  // driver's door: right on RHD
    CHECK(num("xvg.v.d.window.fr") == 58 && num("xvg.v.d.lock.fr") == 1, "RHD: driver's window / lock -> fr");
    feed(0x3CE, {0x00, 0x00, 0x00, 0xc8, 0x88, 0x10, 0x80, 0x00});  // rear driver side: right on RHD
    CHECK(num("xvg.v.d.window.rr") == 100, "RHD: rear driver side window -> rr");
    feed(0x583, {0x00, 0x00, 0x40, 0x01, 0x00, 0x08, 0x53, 0x40});  // driver's door open, rear driver side unlocked
    CHECK(num("ms_v_door_fr") == 1 && num("ms_v_door_fl") == 0, "RHD: driver's door open -> v.d.fr");
    CHECK(num("xvg.v.d.lock.rr") == 0 && num("xvg.v.d.lock.rl") == 1, "RHD: rear driver side unlocked -> rr");
    feed(0x32A, {0x1c, 0x01, 0x00, 0x40, 0x80, 0x00, 0x00, 0x00});  // d[5] bits 1-2 = 00: not a side
    CHECK(num("xvg.v.rhd") == 1, "0x32A side value 0 -> last side kept");

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
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_gear->AsValue() == 0, "Park → gear 0");

    // 3 = Reverse
    f = make_frame(0x187, {0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_gear->AsValue() == -1, "Reverse → gear -1");

    // 5 = Drive
    f = make_frame(0x187, {0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_env_gear->AsValue() == 1, "Drive → gear 1");

    delete v;
}

// 12V charging (v.e.charging12v): 0x187 d[1] bit 7 = HV system active (Motor_EV_01 HV_Bordnetz_aktiv).
// Evaluated in Ticker1, OR'ed with charge-in-progress and HVAC; cleared when KCAN goes idle.
void test_charging12v() {
    printf("\ntest_charging12v\n");
    auto* v = make_vehicle();
    auto tick = [v]() { static_cast<OvmsVehicle*>(v)->Ticker1(1); };

    auto f = make_frame(0x187, {0x00, 0x80, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00});  // HV active, Park
    v->IncomingFrameCan3(&f);
    tick();
    CHECK(StandardMetrics.ms_v_env_charging12v->AsBool(), "0x187 HV-active bit -> charging12v true");
    CHECK(StandardMetrics.ms_v_env_gear->AsValue() == 0, "gear decode unaffected (Park)");

    f = make_frame(0x187, {0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00});      // HV inactive
    v->IncomingFrameCan3(&f);
    tick();
    CHECK(!StandardMetrics.ms_v_env_charging12v->AsBool(), "HV-active bit clear -> charging12v false");

    StandardMetrics.ms_v_charge_inprogress->SetValue(true);
    tick();
    CHECK(StandardMetrics.ms_v_env_charging12v->AsBool(), "HV charging -> charging12v true");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);

    StandardMetrics.ms_v_env_hvac->SetValue(true);
    tick();
    CHECK(StandardMetrics.ms_v_env_charging12v->AsBool(), "remote climate running -> charging12v true");
    StandardMetrics.ms_v_env_hvac->SetValue(false);

    f = make_frame(0x187, {0x00, 0x80, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00});      // HV active again
    v->IncomingFrameCan3(&f);
    tick();
    CHECK(StandardMetrics.ms_v_env_charging12v->AsBool(), "HV active again -> true");
    for (int i = 0; i < VWEGOLF_BUS_TIMEOUT_SECS + 1; i++) tick();                // KCAN goes silent
    CHECK(!StandardMetrics.ms_v_env_charging12v->AsBool(), "KCAN idle -> charging12v false");

    delete v;
}

void test_vin_0x6B4() {
    printf("\ntest_vin_0x6B4\n");
    auto* v = make_vehicle();

    // VIN is split across 3 frames identified by byte0 (0, 1, 2).
    // Frame 0: bytes 5-7 = chars 0-2
    auto f0 = make_frame(0x6B4, {0x00, 0x00, 0x00, 0x00, 0x00, 'W', 'V', 'W'});
    v->IncomingFrameCan3(&f0);

    // Frame 1: bytes 1-7 = chars 3-9
    auto f1 = make_frame(0x6B4, {0x01, 'Z', 'Z', 'Z', '1', '2', '3', '4'});
    v->IncomingFrameCan3(&f1);

    // Frame 2: bytes 1-7 = chars 10-16
    auto f2 = make_frame(0x6B4, {0x02, '5', '6', '7', '8', '9', 'A', 'B'});
    v->IncomingFrameCan3(&f2);

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
// Charge current / power mirroring (0x191 while charging, gated by 0x594)
// ---------------------------------------------------------------------------
void test_charge_current_0x191() {
    printf("\ntest_charge_current_0x191\n");

    // Real on-car capture bytes (mid-charge): 0x191 d1=0x30 d2=0x81 d3=0x00 d4=0x05
    // decode -> I = 2047 - ((0x30&0xf0)>>4 | (0x81<<4)) = 2047 - 2067 = -20 A (charge),
    //           V = (0x00 | (0x05&0xf)<<8) * 0.25 = 1280 * 0.25 = 320.0 V,
    //           P = 320 * -20 / 1000 = -6.4 kW. Charge side flips the sign: +20 A / +6.4 kW.
    auto charge_frame = make_frame(0x191, {0x00, 0x30, 0x81, 0x00, 0x05, 0x00, 0x00, 0x00});

    // Not charging yet: 0x191 must NOT touch the charge metrics.
    {
        auto* v = make_vehicle();
        v->IncomingFrameCan3(&charge_frame);
        CHECK(near(StandardMetrics.ms_v_bat_current->AsFloat(), -20.0f), "0x191 bat_current -20 A");
        CHECK(near(StandardMetrics.ms_v_charge_current->AsFloat(), 0.0f),
              "charge_current stays 0 A when not charging");
        CHECK(near(StandardMetrics.ms_v_charge_power->AsFloat(), 0.0f),
              "charge_power stays 0 kW when not charging");
        delete v;
    }

    // Charging (0x594 d[3] bit5 set) then 0x191: charge current/power mirror the pack, sign flipped.
    {
        auto* v = make_vehicle();
        // Real steady-charge 0x594 capture: d[3]=0xA3, bit5 set = charging.
        auto on = make_frame(0x594, {0x00, 0xF0, 0x21, 0xA3, 0x06, 0x34, 0x30, 0x0D});
        v->IncomingFrameCan3(&on);
        CHECK(StandardMetrics.ms_v_charge_inprogress->AsBool(), "0x594 sets charge in progress");
        v->IncomingFrameCan3(&charge_frame);
        CHECK(near(StandardMetrics.ms_v_charge_current->AsFloat(), 20.0f),
              "charge_current +20 A while charging");
        CHECK(near(StandardMetrics.ms_v_charge_power->AsFloat(), 6.4f),
              "charge_power +6.4 kW while charging");

        // Charge ends: 0x594 with bit5 clear (d[3]=0x03) zeroes the charge metrics.
        auto off = make_frame(0x594, {0x00, 0x00, 0x20, 0x03, 0x00, 0x00, 0x10, 0x0D});
        v->IncomingFrameCan3(&off);
        CHECK(!StandardMetrics.ms_v_charge_inprogress->AsBool(), "0x594 clears charge in progress");
        CHECK(near(StandardMetrics.ms_v_charge_current->AsFloat(), 0.0f),
              "charge_current zeroed on charge stop");
        CHECK(near(StandardMetrics.ms_v_charge_power->AsFloat(), 0.0f),
              "charge_power zeroed on charge stop");
        delete v;
    }
}

// ---------------------------------------------------------------------------
// Spurious "Not charging" alert on power-off (charge_state must not flip ""->"stopped")
// ---------------------------------------------------------------------------
void test_charge_state_no_spurious_stop() {
    printf("\ntest_charge_state_no_spurious_stop\n");

    // Fresh boot: charge_state is "" and charging was never in progress. An idle 0x594
    // (not charging, d[3]=0x03) must NOT write charge_state -> "stopped": that ""->"stopped"
    // change makes the vehicle framework fire a spurious "Not charging" alert every power cycle.
    {
        auto* v = make_vehicle();
        auto idle = make_frame(0x594, {0x00, 0x00, 0x20, 0x03, 0x00, 0x00, 0x10, 0x0D});
        v->IncomingFrameCan3(&idle);
        CHECK(!StandardMetrics.ms_v_charge_inprogress->AsBool(), "idle 0x594 -> not charging");
        CHECK(StandardMetrics.ms_v_charge_state->AsString().empty(),
              "charge_state stays empty when never charging (no spurious 'stopped')");
        CHECK(g_metrics.writes["ms_v_charge_state"] == 0,
              "charge_state not written on an idle-from-boot frame");
        delete v;
    }

    // A real charge session still transitions charge_state charging -> stopped.
    {
        auto* v = make_vehicle();
        auto on = make_frame(0x594, {0x00, 0xF0, 0x21, 0xA3, 0x06, 0x34, 0x30, 0x0D});
        v->IncomingFrameCan3(&on);
        CHECK(StandardMetrics.ms_v_charge_state->AsString() == "charging",
              "charging -> charge_state 'charging'");
        auto off = make_frame(0x594, {0x00, 0x00, 0x20, 0x03, 0x00, 0x00, 0x10, 0x0D});
        v->IncomingFrameCan3(&off);
        CHECK(StandardMetrics.ms_v_charge_state->AsString() == "stopped",
              "charging->stop -> charge_state 'stopped' (genuine transition)");
        delete v;
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

extern void test_crtd_replay();
extern void test_bat_ctrl_all();

int main() {
    printf("=== vehicle_vwegolf CAN decode tests ===\n");

    test_soc_0x131();
    test_lock_0x583();
    test_body_signals();
    test_speed_0xFD();
    test_gear_0x187_park();
    test_charging12v();
    test_vin_0x6B4();
    test_gps_0x486();
    test_sentinel_filters();
    test_charge_current_0x191();
    test_charge_state_no_spurious_stop();
    test_crtd_replay();
    test_bat_ctrl_all();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
