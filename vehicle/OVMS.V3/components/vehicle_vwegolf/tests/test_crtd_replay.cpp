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

        // Only handle standard and extended receive frames
        bool is_11bit = (strcmp(rtype, "3R11") == 0);
        bool is_29bit = (strcmp(rtype, "3R29") == 0);
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

        v->IncomingFrameCan3(&frame);
        dispatched++;
    }

    fclose(f);
    return dispatched;
}

// ---------------------------------------------------------------------------
// Test
// ---------------------------------------------------------------------------

void test_crtd_replay() {
    printf("\ntest_crtd_replay (real KCAN capture)\n");

    g_metrics = MetricStore{};
    auto* v = new OvmsVehicleVWeGolf();

    int n = replay_crtd(v, "candumps/kcan-capture.crtd");

    if (n < 0) {
        printf("  SKIP: candumps/kcan-capture.crtd not found\n");
        delete v;
        return;
    }
    printf("  replayed %d frames\n", n);

    // --- Speed: capture taken while car was parked, expect 0 km/h ---
    float speed = StandardMetrics.ms_v_pos_speed->AsFloat();
    CHECK(near_f(speed, 0.0f, 1.0f), "Speed ~0 km/h (car parked during capture)");

    // --- SoC: byte3=0xFE → 127.0 (the ECU's initial/no-data sentinel) ---
    float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
    CHECK(soc > 0.0f, "SoC metric was set (> 0)");

    // --- Gear: 0x187 byte2=0x12, nibble=2 → Park → gear=0 ---
    int gear = StandardMetrics.ms_v_env_gear->AsValue();
    CHECK(gear == 0, "Gear = 0 (Park)");

    // VIN (0x6B4) is decoded in IncomingFrameCan2 (FCAN), not IncomingFrameCan3.
    // This CRTD capture is KCAN-only so VIN is not exercised here; see test_vin_0x6B4().

    delete v;
}
