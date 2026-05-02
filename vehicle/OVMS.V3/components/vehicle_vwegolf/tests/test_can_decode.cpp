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
    v->IncomingFrameCan2(&f);
    CHECK(near(StandardMetrics.ms_v_bat_soc->AsFloat(), 100.0f), "SoC 100% from 0xC8");

    g_metrics = MetricStore{};
    // byte3 = 0x64 (100 dec) → 100 * 0.5 = 50%
    f = make_frame(0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan2(&f);
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
// 0x0191 — BMS current, voltage, power
// ---------------------------------------------------------------------------

void test_bms_0x191() {
    printf("\ntest_bms_0x191\n");
    auto* v = make_vehicle();

    // current=10A: raw=2057=0x809 → d[1]&0xF0=0x90 (9<<4), d[2]=0x80 (2057>>4=128)
    // voltage=400V: raw=1600=0x640 → d[3]=0x40, d[4]&0x0F=0x06
    // power = -(400*10)/1000 = -4.0 kW (positive=drive, negative=charge convention)
    auto f = make_frame(0x191, {0x00, 0x90, 0x80, 0x40, 0x06, 0x00, 0x00, 0x00});
    v->IncomingFrameCan2(&f);
    CHECK(near(StandardMetrics.ms_v_bat_current->AsFloat(),  10.0f), "BMS current 10A");
    CHECK(near(StandardMetrics.ms_v_bat_voltage->AsFloat(), 400.0f), "BMS voltage 400V");
    CHECK(near(StandardMetrics.ms_v_bat_power->AsFloat(),    -4.0f), "BMS power -4kW (charging convention)");

    delete v;

    {
        // Sentinel: d[2]==0xFF (all-ones current field → ~2047A) must not overwrite metrics.
        auto* vs = make_vehicle();
        StandardMetrics.ms_v_bat_current->SetValue(10.0f);
        StandardMetrics.ms_v_bat_voltage->SetValue(400.0f);
        auto fs = make_frame(0x191, {0x00, 0xE0, 0xFF, 0xFE, 0x0F, 0x00, 0x00, 0x00});
        vs->IncomingFrameCan2(&fs);
        CHECK(near(StandardMetrics.ms_v_bat_current->AsFloat(),  10.0f), "0x0191 sentinel: current not overwritten");
        CHECK(near(StandardMetrics.ms_v_bat_voltage->AsFloat(), 400.0f), "0x0191 sentinel: voltage not overwritten");
        delete vs;
    }
}

// ---------------------------------------------------------------------------
// 0x02AF — trip energy (regenerated and consumed)
// ---------------------------------------------------------------------------

void test_trip_energy_0x2AF() {
    printf("\ntest_trip_energy_0x2AF\n");
    auto* v = make_vehicle();

    // recd raw=3600 (d[4]=0x10, d[5]=0x0E) → 3600*10/3600000 = 0.01 kWh
    // used raw=7200 (d[6]=0x20, d[7]=0x1C) → 7200*10/3600000 = 0.02 kWh
    auto f = make_frame(0x2AF, {0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x20, 0x1C});
    v->IncomingFrameCan2(&f);
    CHECK(near(StandardMetrics.ms_v_bat_energy_recd->AsFloat(), 0.01f, 1e-6f),
          "Trip regen 0.01 kWh");
    CHECK(near(StandardMetrics.ms_v_bat_energy_used->AsFloat(), 0.02f, 1e-6f),
          "Trip used  0.02 kWh");

    delete v;
}

// ---------------------------------------------------------------------------
// 0x0583 — central locking and door open states
// ---------------------------------------------------------------------------

void test_locks_0x583() {
    printf("\ntest_locks_0x583\n");

    {
        auto* v = make_vehicle();
        // locked externally (d[2] bit1), all doors closed
        auto f = make_frame(0x583, {0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(StandardMetrics.ms_v_env_locked->AsBool(),   "Locked when d[2] bit1 set");
        CHECK(!StandardMetrics.ms_v_door_fl->AsBool(),     "FL closed");
        CHECK(!StandardMetrics.ms_v_door_fr->AsBool(),     "FR closed");
        CHECK(!StandardMetrics.ms_v_door_rl->AsBool(),     "RL closed");
        CHECK(!StandardMetrics.ms_v_door_rr->AsBool(),     "RR closed");
        CHECK(!StandardMetrics.ms_v_door_trunk->AsBool(),  "Trunk closed");
        delete v;
    }

    {
        auto* v = make_vehicle();
        // unlocked; FL open (bit0) and trunk open (bit4) → d[3]=0x11
        auto f = make_frame(0x583, {0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(!StandardMetrics.ms_v_env_locked->AsBool(), "Unlocked when d[2] bit1 clear");
        CHECK(StandardMetrics.ms_v_door_fl->AsBool(),     "FL open (bit0)");
        CHECK(!StandardMetrics.ms_v_door_fr->AsBool(),    "FR closed");
        CHECK(StandardMetrics.ms_v_door_trunk->AsBool(),  "Trunk open (bit4)");
        delete v;
    }
}

// ---------------------------------------------------------------------------
// 0x0594 — HV charge management
// ---------------------------------------------------------------------------

void test_charge_0x594() {
    printf("\ntest_charge_0x594\n");

    {
        auto* v = make_vehicle();
        // duration_full=120min (raw=24): d[1]=0x80 (nibble=8), d[2]&0x1F=0x01 (hi=1)
        // timer enabled: d[2]&0x60=0x20
        // combined d[2] = 0x01 | 0x20 = 0x21
        // is_charging: d[3]=0x20
        // AC Type2: d[5]&0x0C=0x04 → charge_type=1
        // setpoint 20°C: (d[7]&0x1F)*0.5+15.5=20 → d[7]=0x09
        auto f = make_frame(0x594, {0x00, 0x80, 0x21, 0x20, 0x00, 0x04, 0x00, 0x09});
        v->IncomingFrameCan3(&f);
        CHECK(StandardMetrics.ms_v_charge_duration_full->AsValue() == 120,
              "Charge duration 120 min");
        CHECK(StandardMetrics.ms_v_charge_timermode->AsBool(),              "Charge timer enabled");
        CHECK(StandardMetrics.ms_v_charge_inprogress->AsBool(),             "Charging in progress");
        CHECK(StandardMetrics.ms_v_charge_state->AsValue() == "charging",   "State=charging");
        CHECK(StandardMetrics.ms_v_charge_type->AsValue() == "type2",       "Type=type2 AC");
        CHECK(StandardMetrics.ms_v_door_chargeport->AsBool(),               "Charge port open (AC type2)");
        CHECK(near(StandardMetrics.ms_v_env_cabinsetpoint->AsFloat(), 20.0f), "Cabin setpoint 20°C");
        delete v;
    }

    {
        auto* v = make_vehicle();
        // not charging, no cable, timer off, setpoint=15.5°C (raw=0)
        auto f = make_frame(0x594, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(!StandardMetrics.ms_v_charge_inprogress->AsBool(),           "Not charging");
        CHECK(StandardMetrics.ms_v_charge_state->AsValue() == "stopped",   "State=stopped");
        CHECK(!StandardMetrics.ms_v_door_chargeport->AsBool(),             "Charge port closed");
        CHECK(!StandardMetrics.ms_v_charge_timermode->AsBool(),            "Timer disabled");
        CHECK(near(StandardMetrics.ms_v_env_cabinsetpoint->AsFloat(), 15.5f), "Cabin setpoint 15.5°C (raw=0)");
        delete v;
    }
}

// ---------------------------------------------------------------------------
// 0x059E — BMS battery pack temperature
// ---------------------------------------------------------------------------

void test_bat_temp_0x59E() {
    printf("\ntest_bat_temp_0x59E\n");
    auto* v = make_vehicle();

    // 25°C: d[2] = (25+40)/0.5 = 130 = 0x82
    auto f = make_frame(0x59E, {0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(near(StandardMetrics.ms_v_bat_temp->AsFloat(), 25.0f), "Battery temp 25°C");

    delete v;

    {
        // Sentinel: d[2]==0xFE (decodes to 87°C) must not overwrite the metric.
        auto* vs = make_vehicle();
        StandardMetrics.ms_v_bat_temp->SetValue(25.0f);
        auto fs = make_frame(0x59E, {0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00});
        vs->IncomingFrameCan3(&fs);
        CHECK(near(StandardMetrics.ms_v_bat_temp->AsFloat(), 25.0f), "0x059E sentinel: bat_temp not overwritten");
        delete vs;
    }
}

// ---------------------------------------------------------------------------
// 0x05CA — HV battery energy capacity (11-bit)
// ---------------------------------------------------------------------------

void test_bat_capacity_0x5CA() {
    printf("\ntest_bat_capacity_0x5CA\n");
    auto* v = make_vehicle();

    // 35.0 kWh: raw=700=0x2BC → d[1]&0xF0=0xC0 (12<<4), d[2]&0x7F=0x2B (43)
    // check: ((0xC0&0xF0)>>4)|((0x2B&0x7F)<<4) = 12|688 = 700; 700*50/1000 = 35.0
    auto f = make_frame(0x5CA, {0x00, 0xC0, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(near(StandardMetrics.ms_v_bat_capacity->AsFloat(), 35.0f), "Battery capacity 35.0 kWh");

    delete v;

    {
        // Sentinel: d[2]==0xFF (near-max capacity field → ~102 kWh) must not overwrite.
        auto* vs = make_vehicle();
        StandardMetrics.ms_v_bat_capacity->SetValue(35.0f);
        auto fs = make_frame(0x5CA, {0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00});
        vs->IncomingFrameCan3(&fs);
        CHECK(near(StandardMetrics.ms_v_bat_capacity->AsFloat(), 35.0f), "0x05CA sentinel: capacity not overwritten");
        delete vs;
    }
}

// ---------------------------------------------------------------------------
// 0x05EA — Clima ECU status: cabin temp and HVAC state
// ---------------------------------------------------------------------------

void test_clima_status_0x5EA() {
    printf("\ntest_clima_status_0x5EA\n");

    {
        auto* v = make_vehicle();
        // cabin=20°C: raw=600=0x258 → d[6]&0xFC=0x60 (24<<2), d[7]&0x0F=0x09 (600>>6=9)
        // hvac off: remote_mode=0 → d[3]=0x00, d[4]=0x00
        auto f = make_frame(0x5EA, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x09});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_env_cabintemp->AsFloat(), 20.0f), "Cabin temp 20°C");
        CHECK(!StandardMetrics.ms_v_env_hvac->AsBool(),                   "HVAC off (remote_mode=0)");
        delete v;
    }

    {
        auto* v = make_vehicle();
        // cabin=20°C, hvac on: remote_mode=2 → d[3]&0xC0=0x80 (2<<6)
        auto f = make_frame(0x5EA, {0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x60, 0x09});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_env_cabintemp->AsFloat(), 20.0f), "Cabin temp 20°C (hvac on)");
        CHECK(StandardMetrics.ms_v_env_hvac->AsBool(),                    "HVAC on (remote_mode=2)");
        delete v;
    }

    {
        // Sentinel: u16=1022 (≥1020) must not overwrite cabin temp metric.
        // d[6]=0xF8, d[7]=0x0F → ((0xF8&0xFC)>>2)|((0x0F&0x0F)<<6) = 62|960 = 1022
        auto* v = make_vehicle();
        StandardMetrics.ms_v_env_cabintemp->SetValue(99.0f);
        auto f = make_frame(0x5EA, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x0F});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_env_cabintemp->AsFloat(), 99.0f),
              "0x05EA sentinel u16>=1020: cabin temp not overwritten");
        delete v;
    }
}

// ---------------------------------------------------------------------------
// 0x17332510 — BAP clima ACK (29-bit extended frame from clima ECU 0x25)
// ---------------------------------------------------------------------------

void test_bap_ack_0x17332510() {
    printf("\ntest_bap_ack_0x17332510\n");

    {
        auto* v = make_vehicle();
        // DLC=3, 49 58 01 → hvac on
        auto f = make_frame(0x17332510, {0x49, 0x58, 0x01});
        v->IncomingFrameCan3(&f);
        CHECK(StandardMetrics.ms_v_env_hvac->AsBool(), "BAP ACK 49 58 01 → hvac on");
        delete v;
    }

    {
        auto* v = make_vehicle();
        StandardMetrics.ms_v_env_hvac->SetValue(true);
        // DLC=3, 49 58 00 → hvac off
        auto f = make_frame(0x17332510, {0x49, 0x58, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(!StandardMetrics.ms_v_env_hvac->AsBool(), "BAP ACK 49 58 00 → hvac off");
        delete v;
    }

    {
        auto* v = make_vehicle();
        StandardMetrics.ms_v_env_hvac->SetValue(false);
        // DLC=4: guard rejects — hvac must NOT flip
        auto f = make_frame(0x17332510, {0x49, 0x58, 0x01, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(!StandardMetrics.ms_v_env_hvac->AsBool(), "BAP ACK DLC=4: ignored (DLC guard)");
        delete v;
    }
}

// ---------------------------------------------------------------------------
// 0x05F5 — range estimates (instrument cluster)
// ---------------------------------------------------------------------------

void test_range_0x5F5() {
    printf("\ntest_range_0x5F5\n");
    auto* v = make_vehicle();

    // range_est=180 km:   d[3]&0xE0=0x80 (4<<5), d[4]=0x16 (22); (4)|(22<<3)=180
    // range_ideal=200 km: d[0]=0xC8, d[1]&0x07=0;  200|(0<<8)=200
    auto f = make_frame(0x5F5, {0xC8, 0x00, 0x00, 0x80, 0x16, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(near(StandardMetrics.ms_v_bat_range_est->AsFloat(),   180.0f), "Range est 180 km");
    CHECK(near(StandardMetrics.ms_v_bat_range_ideal->AsFloat(), 200.0f), "Range ideal 200 km");

    delete v;
}

// ---------------------------------------------------------------------------
// 0x065A — bonnet/hood open indicator
// ---------------------------------------------------------------------------

void test_hood_0x65A() {
    printf("\ntest_hood_0x65A\n");
    auto* v = make_vehicle();

    auto f = make_frame(0x65A, {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(StandardMetrics.ms_v_door_hood->AsBool(), "Hood open (d[4] bit0 set)");

    g_metrics = MetricStore{};
    f = make_frame(0x65A, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    v->IncomingFrameCan3(&f);
    CHECK(!StandardMetrics.ms_v_door_hood->AsBool(), "Hood closed (d[4] bit0 clear)");

    delete v;
}

// ---------------------------------------------------------------------------
// 0x066E — InnenTemp interior sensor (0xFE sentinel must be discarded)
// ---------------------------------------------------------------------------

void test_innentemp_0x66E() {
    printf("\ntest_innentemp_0x66E\n");

    {
        auto* v = make_vehicle();
        // 20°C: d[4] = (20+50)/0.5 = 140 = 0x8C
        auto f = make_frame(0x66E, {0x00, 0x00, 0x00, 0x00, 0x8C, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_env_cabintemp->AsFloat(), 20.0f),
              "InnenTemp 20°C from 0x8C");
        delete v;
    }

    {
        // Sentinel 0xFE must not overwrite the metric
        auto* v = make_vehicle();
        StandardMetrics.ms_v_env_cabintemp->SetValue(99.0f);
        auto f = make_frame(0x66E, {0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00});
        v->IncomingFrameCan3(&f);
        CHECK(near(StandardMetrics.ms_v_env_cabintemp->AsFloat(), 99.0f),
              "InnenTemp 0xFE sentinel: metric not overwritten");
        delete v;
    }
}

// ---------------------------------------------------------------------------
// 0x06B7 — odometer, park time, outside temperature
// ---------------------------------------------------------------------------

void test_odo_0x6B7() {
    printf("\ntest_odo_0x6B7\n");
    auto* v = make_vehicle();

    // odo=12345 km (0x3039): d[0]=0x39, d[1]=0x30, d[2]&0x0F=0
    // outside temp=15°C: d[7]=(15+50)/0.5=130=0x82
    auto f = make_frame(0x6B7, {0x39, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82});
    v->IncomingFrameCan3(&f);
    CHECK(near(StandardMetrics.ms_v_pos_odometer->AsFloat(), 12345.0f), "Odometer 12345 km");
    CHECK(near(StandardMetrics.ms_v_env_temp->AsFloat(),       15.0f),  "Outside temp 15°C");

    delete v;
}

// ---------------------------------------------------------------------------
// 0x06B5 — ambient temp: solar sensor and outside air (startup sentinel d[6]==0xFE)
// ---------------------------------------------------------------------------

void test_ambient_temp_0x6B5() {
    printf("\ntest_ambient_temp_0x6B5\n");

    {
        // Normal frame: exercise decode path without crash.
        // solar raw=2000: d[6]=0xD0, d[7]&0x07=0x07 → (0xD0|(0x07<<8))*0.1-40 = 160.0°C (high but valid field)
        // air raw=300: d[2]=0x2C, d[3]&0x03=0x01 → (0x2C|(0x01<<8))*0.1-40 = -10.4°C
        auto* v = make_vehicle();
        auto f = make_frame(0x6B5, {0x00, 0x00, 0x2C, 0x01, 0x00, 0x00, 0xD0, 0x07});
        v->IncomingFrameCan3(&f);  // logs only; no metric set yet — just verify no crash
        delete v;
    }

    {
        // Sentinel: d[6]==0xFE — must be silently dropped (no crash, no metric write).
        auto* v = make_vehicle();
        auto f = make_frame(0x6B5, {0x00, 0x00, 0xFE, 0x03, 0x00, 0x00, 0xFE, 0x07});
        v->IncomingFrameCan3(&f);
        // 0x06B5 does not yet set a standard metric; test exercises the sentinel path
        delete v;
    }
}

// ---------------------------------------------------------------------------
// IncomingFrameCan3 resets the KCAN idle counter on every call.
// (CAN2→CAN3 forwarding has been removed; the framework only dispatches
// genuine can3 frames here, so no origin guard is needed.)
// ---------------------------------------------------------------------------

void test_kcan_resets_idle_counter() {
    printf("\ntest_kcan_resets_idle_counter\n");

    canbus kcan;

    auto* v = make_vehicle();
    v->m_can3 = &kcan;
    auto f = make_frame(0x131, {0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00});
    f.origin = &kcan;
    v->IncomingFrameCan3(&f);
    CHECK(v->test_bus_idle_ticks() == 0,
          "IncomingFrameCan3 resets idle counter to 0");
    delete v;
}

// ---------------------------------------------------------------------------
// FCAN acceptance filter encoding
//
// SetAcceptanceFilter sends bytes u8[3..0] as MCP2515 registers SIDH, SIDL,
// EID8, EID0.  The lambdas in the constructor must produce the exact u32 that
// maps each CAN ID to the correct register bytes.  A wrong encoding (e.g. the
// original .b.sid approach) would silently program all-zero filters and let
// every frame through — which caused the ISR storm we fixed.
// ---------------------------------------------------------------------------

void test_fcan_filter_encoding() {
    printf("\ntest_fcan_filter_encoding\n");

    // Replicate mcp_sid lambda: u32 = (SID>>3)<<24 | (SID&7)<<21
    auto mcp_sid = [](uint16_t sid) -> uint32_t {
        return (static_cast<uint32_t>(sid >> 3) << 24) |
               (static_cast<uint32_t>(sid & 0x7) << 21);
    };

    // Replicate mcp_eid lambda: packs 29-bit extended ID with EXIDE=1
    auto mcp_eid = [](uint32_t ext_id) -> uint32_t {
        uint32_t sid  = (ext_id >> 18) & 0x7FF;
        uint32_t eid  =  ext_id & 0x3FFFF;
        uint8_t  sidh = static_cast<uint8_t>(sid >> 3);
        uint8_t  sidl = static_cast<uint8_t>(((sid & 0x7) << 5) | (1 << 3) | ((eid >> 16) & 0x3));
        uint8_t  eid8 = static_cast<uint8_t>( eid >> 8);
        uint8_t  eid0 = static_cast<uint8_t>( eid);
        return (static_cast<uint32_t>(sidh) << 24) | (static_cast<uint32_t>(sidl) << 16) |
               (static_cast<uint32_t>(eid8) <<  8) |  static_cast<uint32_t>(eid0);
    };

    // Standard frame: SID=0x131 → SIDH=0x26 SIDL=0x20
    // SIDH = 0x131>>3 = 0x26, SIDL = (0x131&7)<<5 = 0x20
    CHECK(mcp_sid(0x131) == 0x26200000, "mcp_sid(0x131) == 0x26200000");
    CHECK(mcp_sid(0x187) == 0x30E00000, "mcp_sid(0x187) == 0x30E00000");
    CHECK(mcp_sid(0x191) == 0x32200000, "mcp_sid(0x191) == 0x32200000");
    CHECK(mcp_sid(0x2AF) == 0x55E00000, "mcp_sid(0x2AF) == 0x55E00000");
    CHECK(mcp_sid(0x6B4) == 0xD6800000, "mcp_sid(0x6B4) == 0xD6800000");
    CHECK(mcp_sid(0x7FF) == 0xFFE00000, "mcp_sid(0x7FF) == 0xFFE00000 (exact-match mask)");

    // Extended frame: 0x1A5554A8 → SID=0x695 EID=0x154A8 EXIDE=1
    // SIDH=0xD2 SIDL=0xA9 EID8=0x54 EID0=0xA8
    CHECK(mcp_eid(0x1A5554A8) == 0xD2A954A8, "mcp_eid(0x1A5554A8) == 0xD2A954A8");

    // Round-trip: decode the u32 back to the original extended ID
    uint32_t u32  = mcp_eid(0x1A5554A8);
    uint32_t sidh = (u32 >> 24) & 0xFF;
    uint32_t sidl = (u32 >> 16) & 0xFF;
    uint32_t eid8 = (u32 >>  8) & 0xFF;
    uint32_t eid0 =  u32        & 0xFF;
    uint32_t exide = (sidl >> 3) & 0x1;
    uint32_t sid_rt = (sidh << 3) | (sidl >> 5);
    uint32_t eid_rt = ((sidl & 0x3) << 16) | (eid8 << 8) | eid0;
    uint32_t ext_rt = (sid_rt << 18) | eid_rt;
    CHECK(exide  == 1,           "mcp_eid round-trip: EXIDE=1");
    CHECK(ext_rt == 0x1A5554A8, "mcp_eid round-trip: decoded ID == 0x1A5554A8");
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
    test_bms_0x191();
    test_trip_energy_0x2AF();
    test_locks_0x583();
    test_charge_0x594();
    test_bat_temp_0x59E();
    test_bat_capacity_0x5CA();
    test_clima_status_0x5EA();
    test_bap_ack_0x17332510();
    test_range_0x5F5();
    test_hood_0x65A();
    test_innentemp_0x66E();
    test_odo_0x6B7();
    test_ambient_temp_0x6B5();
    test_kcan_resets_idle_counter();
    test_fcan_filter_encoding();
    test_crtd_replay();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
