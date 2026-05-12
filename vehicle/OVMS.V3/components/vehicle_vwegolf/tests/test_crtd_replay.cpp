// test_crtd_replay.cpp — Feed a real CRTD capture through the decode pipeline.
//
// Reads candumps/kcan-capture.crtd (relative to the tests/ directory), builds
// CAN_frame_t objects from each data line, and dispatches them to the vehicle
// module exactly as the OVMS runtime would.  After the replay we check that
// the metrics contain plausible values derived from the real capture.

#include "mock/mock_ovms.hpp"
#include "../src/vehicle_vwegolf.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Declared in test_can_decode.cpp
extern int tests_run;
extern int tests_passed;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else       { printf("  FAIL: %s\n", msg); } \
} while(0)

// Catches "set once, never updates" regressions — the stuck-range_est class.
// A pure value check passes when a metric was written once at boot from a
// persistent store, even if the decoder never ran again. Asserting a write
// floor against the frame count exposes the gap.
#define CHECK_WRITES_AT_LEAST(name, n, msg) do { \
    tests_run++; \
    int w = g_metrics.write_count(name); \
    if (w >= (n)) { \
        tests_passed++; \
        printf("  PASS: %s (writes=%d, expected >=%d)\n", msg, w, (n)); \
    } else { \
        printf("  FAIL: %s (writes=%d, expected >=%d)\n", msg, w, (n)); \
    } \
} while(0)

static bool near_f(float a, float b, float tol = 0.1f) {
    return std::fabs(a - b) < tol;
}

// ---------------------------------------------------------------------------
// CRTD parser
// ---------------------------------------------------------------------------

// Returns the number of frames dispatched, or -1 on file-open failure.
static int replay_crtd(OvmsVehicleVWeGolf* v, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;

    char line[512];
    int dispatched = 0;

    while (fgets(line, sizeof(line), f)) {
        // Strip trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        // Fields: <timestamp> <type> [<id> <b0> <b1> ...]
        char ts[32], rtype[16];
        if (sscanf(line, "%31s %15s", ts, rtype) < 2) continue;

        // Only handle standard and extended receive frames.
        // Bus number is the first character of the type field (1=can1, 2=can2, 3=can3).
        int bus = rtype[0] - '0';
        bool is_11bit = (rtype[1] == 'R' && rtype[2] == '1' && rtype[3] == '1');
        bool is_29bit = (rtype[1] == 'R' && rtype[2] == '2' && rtype[3] == '9');
        if (!is_11bit && !is_29bit) continue;

        // Parse the rest of the line: <id_hex> <b0> <b1> ...
        const char* p = line;
        // skip timestamp
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        // skip type
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;

        // parse ID
        char id_str[16];
        if (sscanf(p, "%15s", id_str) < 1) continue;
        uint32_t msg_id = (uint32_t)strtoul(id_str, nullptr, 16);

        // advance past ID field
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;

        // parse up to 8 data bytes
        CAN_frame_t frame{};
        frame.MsgID = msg_id;
        frame.FIR.B.FF = is_29bit ? CAN_frame_ext : CAN_frame_std;
        uint8_t dlc = 0;
        while (dlc < 8 && *p) {
            char byte_str[4];
            if (sscanf(p, "%3s", byte_str) < 1) break;
            frame.data.u8[dlc++] = (uint8_t)strtoul(byte_str, nullptr, 16);
            while (*p && *p != ' ') p++;
            while (*p == ' ') p++;
        }
        frame.FIR.B.DLC = dlc;

        // Dispatch by the CRTD bus tag. Captures pre-dating the bus-filter
        // fix could mis-tag KCAN frames as bus 2; those captures are now
        // purged (commit 0b9fd9211). New captures tag each bus correctly,
        // so IncomingFrameCanN matches the physical bus the firmware sees.
        if (bus == 2)      v->IncomingFrameCan2(&frame);
        else if (bus == 3) v->IncomingFrameCan3(&frame);
        else               continue;
        dispatched++;
    }

    fclose(f);
    return dispatched;
}

// ---------------------------------------------------------------------------
// Test
// ---------------------------------------------------------------------------

void test_crtd_replay() {
    printf("\ntest_crtd_replay (FCAN-tagged capture, exercises IncomingFrameCan2)\n");

    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // kcan-capture.crtd dates from before commit 9fdbf8b7 fixed the
    // bus-filter bug, so every frame is bus-tagged 2 regardless of physical
    // origin. The replay dispatches it through IncomingFrameCan2, which
    // exercises only the FCAN-side decoders. KCAN-routed metrics (range,
    // speed, charging, doors, cabin temp) are covered by the second
    // replay below against a properly-tagged bus-3 capture.
    int n = replay_crtd(v, "candumps/kcan-capture.crtd");

    if (n < 0) {
        printf("  SKIP: candumps/kcan-capture.crtd not found\n");
        delete v;
        return;
    }
    printf("  replayed %d frames\n", n);

    // --- SoC: 0x131 d[3]=0x79 → 60.5%. Decoded in IncomingFrameCan2
    //     (line 135 — added in commit fc4de583b alongside the can3 decoder).
    float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
    CHECK(near_f(soc, 60.5f, 1.0f), "SoC ~60.5% (d[3]=0x79 from kcan-capture)");

    // --- Gear: 0x187 byte2=0x12, nibble=2 → Park → gear=0 ---
    int gear = StandardMetrics.ms_v_env_gear->AsValue();
    CHECK(gear == 0, "Gear = 0 (Park)");

    // --- Write floors for FCAN-side decoders. Asserting the metric was
    //     written at least ~80 % of the frames-in-capture catches the
    //     "set once at boot, decoder never fires again" class of bug.
    //     A pure value check passes when a persistent metric is restored
    //     to its last-known value at boot — the stuck-range_est class.
    //
    //     Frame counts in kcan-capture.crtd (via tests/analysis):
    //       0x131 BMS_SoC      2155 frames (sentinels filtered)
    //       0x187 GearSelector  456 frames
    CHECK_WRITES_AT_LEAST("ms_v_bat_soc", 1800,
                          "SoC decoded ~every 0x131 frame");
    CHECK_WRITES_AT_LEAST("ms_v_env_gear", 400,
                          "Gear decoded ~every 0x187 frame");

    delete v;
}

// ---------------------------------------------------------------------------
// Second replay — a properly-tagged bus-3 KCAN capture. Exercises
// IncomingFrameCan3 and the KCAN-side decoders. Catches the stuck-update
// class of bug (range_est, speed, charging frames, cabin temp, doors)
// that the FCAN-tagged replay can't reach.
// ---------------------------------------------------------------------------

void test_crtd_replay_kcan() {
    printf("\ntest_crtd_replay_kcan (bus-3 tagged, exercises IncomingFrameCan3)\n");

    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    // 20260428-095821: 23.5 s clean KCAN capture, 182 IDs, every frame
    // tagged bus 3. Spans bus-up, frames flow into IncomingFrameCan3.
    int n = replay_crtd(v, "candumps/can3-1aec82f33_ota_0_edge-20260428-095821.crtd");

    if (n < 0) {
        printf("  SKIP: can3-1aec82f33_ota_0_edge-20260428-095821.crtd not found\n");
        delete v;
        return;
    }
    printf("  replayed %d frames\n", n);

    // --- Plausibility on a couple of KCAN-sourced metrics. Range_est is
    //     the specific bug class this test exists to surface ---
    float range_est = StandardMetrics.ms_v_bat_range_est->AsFloat();
    CHECK(range_est > 0.0f && range_est < 500.0f,
          "range_est plausible (0..500 km)");

    float speed = StandardMetrics.ms_v_pos_speed->AsFloat();
    CHECK(speed >= 0.0f && speed < 250.0f,
          "speed plausible (0..250 km/h)");

    // --- Write floors for KCAN-side decoders. Frame counts in the
    //     capture (via tests/analysis):
    //       0x0FD ESP_Speed         823 frames
    //       0x131 BMS_SoC          1173 frames (also FCAN, decoded in both)
    //       0x187 GearSelector      235 frames (also FCAN — same)
    //       0x5F5 Instruments_Range  24 frames
    //       0x594 ChargeManagement   47 frames
    //       0x66E InnenTemp          14 frames (filter 0xFE sentinel ok)
    //       0x6B7 AmbientTemp        24 frames
    //       0x65A BCM_01             23 frames
    //
    //     Floors are ~80 % of frame count to allow for sentinel filtering.
    CHECK_WRITES_AT_LEAST("ms_v_pos_speed", 650,
                          "Speed decoded ~every 0x0FD frame");
    CHECK_WRITES_AT_LEAST("ms_v_bat_range_est", 20,
                          "range_est decoded ~every 0x5F5 frame "
                          "(the stuck-at-boot regression class)");
    CHECK_WRITES_AT_LEAST("ms_v_door_hood", 20,
                          "hood door decoded ~every 0x65A frame");
    CHECK_WRITES_AT_LEAST("ms_v_env_temp", 20,
                          "outside temp decoded ~every 0x6B7 frame");

    delete v;
}
