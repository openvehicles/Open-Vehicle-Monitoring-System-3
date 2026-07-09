// test_uds_poll.cpp — Native tests for UDS BMS polling (issue #3).
//
// Covers the poll-state machine (UpdatePollState via Ticker1) and the
// IncomingPollReply decode (vweup-derived scaling, charge-only metric writes).
//
// Run:  make test   (from the tests/ directory)

#include "mock/mock_ovms.hpp"
#include "../src/vehicle_vwegolf.h"

#include <cmath>
#include <cstdio>

extern int tests_run;
extern int tests_passed;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else       { printf("  FAIL: %s\n", msg); } \
} while(0)

namespace {

OvmsVehicleVWeGolf* make_vehicle() {
    g_metrics = MetricStore{};
    return new OvmsVehicleVWeGolf();
}

CAN_frame_t make_frame(uint32_t id, std::initializer_list<uint8_t> bytes) {
    CAN_frame_t f{};
    f.MsgID = id;
    f.FIR.B.DLC = static_cast<uint8_t>(bytes.size());
    int i = 0;
    for (uint8_t b : bytes) f.data.u8[i++] = b;
    return f;
}

bool near(float a, float b, float tol = 0.01f) {
    return std::fabs(a - b) < tol;
}

// Build a poll_job_t as the poller would deliver it for a BMS reply.
OvmsPoller::poll_job_t make_job(uint16_t pid) {
    OvmsPoller::poll_job_t job{};
    job.moduleid_rec = 0x7ED;
    job.type = 0x22;
    job.pid = pid;
    return job;
}

// Drive the vehicle into a given poll state via real frame/ticker paths.
// Ticker1 is protected in OvmsVehicleVWeGolf — call via base-class pointer
// (access is checked on the static type; the mock base declares it public).
void tick(OvmsVehicleVWeGolf* v, int seconds = 1) {
    for (int i = 0; i < seconds; i++) static_cast<OvmsVehicle*>(v)->Ticker1(1);
}

void feed_kcan(OvmsVehicleVWeGolf* v) {
    auto f = make_frame(0x3FE, {0, 0, 0, 0, 0, 0, 0, 0});
    v->IncomingFrameCan3(&f);
}

void feed_fcan(OvmsVehicleVWeGolf* v) {
    auto f = make_frame(0x187, {0, 0, 0x05, 0x01, 0, 0, 0, 0});  // gear D
    v->IncomingFrameCan2(&f);
}

void feed_charging(OvmsVehicleVWeGolf* v, bool charging) {
    // 0x594: charging-in-progress = bit 5 of d[3]
    auto f = make_frame(0x594, {0, 0, 0, static_cast<uint8_t>(charging ? 0x20 : 0x00),
                                0, 0, 0, 0});
    v->IncomingFrameCan3(&f);
}

}  // namespace

void test_poll_state_machine() {
    printf("\ntest_poll_state_machine\n");
    auto* v = make_vehicle();

    tick(v);
    CHECK(v->m_poll_state == VWEGOLF_OFF, "cold boot -> OFF");

    feed_kcan(v);
    tick(v);
    CHECK(v->m_poll_state == VWEGOLF_AWAKE, "KCAN traffic only -> AWAKE");

    feed_fcan(v);
    tick(v);
    CHECK(v->m_poll_state == VWEGOLF_ON, "FCAN traffic -> ON");

    feed_charging(v, true);
    tick(v);
    CHECK(v->m_poll_state == VWEGOLF_CHARGING, "charge gate -> CHARGING");

    // Charging wins over FCAN-alive (DC charge keeps FCAN partially awake).
    feed_fcan(v);
    tick(v);
    CHECK(v->m_poll_state == VWEGOLF_CHARGING, "CHARGING wins over FCAN-alive");

    feed_charging(v, false);
    tick(v);
    CHECK(v->m_poll_state == VWEGOLF_ON, "charge stop, FCAN alive -> ON");

    // Both buses idle out -> OFF.
    tick(v, VWEGOLF_BUS_TIMEOUT_SECS + 1);
    CHECK(v->m_poll_state == VWEGOLF_OFF, "buses idle -> OFF");

    CHECK(v->m_poll_bus == v->m_can1, "poll list registered on can1");
    CHECK(v->m_poll_plist != nullptr, "poll list set");

    delete v;
}

void test_poll_reply_charging() {
    printf("\ntest_poll_reply_charging\n");
    auto* v = make_vehicle();

    feed_charging(v, true);
    tick(v);

    // Voltage DID 0x1E3B: raw u16 BE / 4.0. 1439 / 4 = 359.75 V
    uint8_t u_data[] = {0x05, 0x9F};  // 1439
    auto job = make_job(0x1E3B);
    v->IncomingPollReply(job, u_data, 2);
    CHECK(near(StandardMetrics.ms_v_bat_voltage->AsFloat(), 359.75f),
          "voltage 359.75 V from raw 1439");

    // Current DID 0x1E3D: (raw - 2044) / 4, negated. raw 1884 -> (1884-2044)/4 = -40
    // -> +40 A would be discharge; negated => -(-40) ... ECU negative = out of battery,
    // so raw < 2044 means discharge -> OVMS positive. raw 1884 => +40 A discharge.
    uint8_t i_data[] = {0x07, 0x5C};  // 1884
    job = make_job(0x1E3D);
    v->IncomingPollReply(job, i_data, 2);
    CHECK(near(StandardMetrics.ms_v_bat_current->AsFloat(), 40.0f),
          "current +40 A (discharge) from raw 1884");
    CHECK(near(StandardMetrics.ms_v_bat_power->AsFloat(), 359.75f * 40.0f / 1000.0f),
          "power derived from voltage * current");

    // Charging direction: raw 2204 -> (2204-2044)/4 = +40 -> negated = -40 A
    // (charge = negative current, negative power: OVMS convention).
    uint8_t c_data[] = {0x08, 0x9C};  // 2204
    job = make_job(0x1E3D);
    v->IncomingPollReply(job, c_data, 2);
    CHECK(near(StandardMetrics.ms_v_bat_current->AsFloat(), -40.0f),
          "current -40 A (charging) from raw 2204");
    CHECK(StandardMetrics.ms_v_bat_power->AsFloat() < 0.0f, "charging power negative");

    // Charge metrics are pack-derived (0x0569 is not the OBC AC inlet). They use the
    // positive-magnitude convention: charge voltage = pack voltage, current/power are the
    // negated bat values.
    CHECK(near(StandardMetrics.ms_v_charge_voltage->AsFloat(), 359.75f),
          "charge voltage = pack voltage 359.75 V");
    CHECK(near(StandardMetrics.ms_v_charge_current->AsFloat(), 40.0f),
          "charge current +40 A (magnitude of charging current)");
    CHECK(near(StandardMetrics.ms_v_charge_power->AsFloat(), 359.75f * 40.0f / 1000.0f),
          "charge power = positive magnitude of pack power");

    // Charge stop: 0x0594 clears charge_inprogress -> charge current/power zeroed
    // (the UDS poll stops, so they must not freeze at the last sample).
    feed_charging(v, false);
    CHECK(near(StandardMetrics.ms_v_charge_current->AsFloat(), 0.0f),
          "charge current zeroed on charge stop");
    CHECK(near(StandardMetrics.ms_v_charge_power->AsFloat(), 0.0f),
          "charge power zeroed on charge stop");

    delete v;
}

void test_poll_reply_log_only_when_driving() {
    printf("\ntest_poll_reply_log_only_when_driving\n");
    auto* v = make_vehicle();

    // ON state (driving): replies must NOT touch metrics — 0x191 owns them.
    feed_fcan(v);
    tick(v);
    CHECK(v->m_poll_state == VWEGOLF_ON, "precondition: ON state");

    uint8_t u_data[] = {0x05, 0x9F};
    auto job = make_job(0x1E3B);
    v->IncomingPollReply(job, u_data, 2);
    CHECK(StandardMetrics.ms_v_bat_voltage->AsFloat() == 0.0f,
          "voltage metric untouched while driving");

    // Wrong response module id: ignored entirely.
    feed_charging(v, true);
    tick(v);
    job = make_job(0x1E3B);
    job.moduleid_rec = 0x7EC;
    v->IncomingPollReply(job, u_data, 2);
    CHECK(StandardMetrics.ms_v_bat_voltage->AsFloat() == 0.0f,
          "foreign module id ignored");

    // Short payload: ignored.
    job = make_job(0x1E3B);
    v->IncomingPollReply(job, u_data, 1);
    CHECK(StandardMetrics.ms_v_bat_voltage->AsFloat() == 0.0f, "short payload ignored");

    delete v;
}

void test_uds_poll_all() {
    test_poll_state_machine();
    test_poll_reply_charging();
    test_poll_reply_log_only_when_driving();
}
