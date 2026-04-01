# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a fork of the Open Vehicle Monitoring System v3 (OVMS3) focused on VW e-Golf integration. OVMS3 is an ESP32-based vehicle telemetry and control system that connects to a vehicle's OBD2/CAN port and provides remote monitoring, diagnostics, and control via WiFi, cellular, and Bluetooth.

The primary focus of this fork is `vehicle/OVMS.V3/components/vehicle_vwegolf/`.

## Build System

The firmware uses ESP-IDF (Espressif IoT Development Framework) with a legacy Make wrapper.

**Prerequisites:**
- ESP-IDF (OVMS uses a custom fork): `https://github.com/openvehicles/esp-idf`
- Xtensa ESP32 toolchain
- Set `IDF_PATH` and add the toolchain to `PATH`

**Build steps:**
```bash
# Configure (choose a base sdkconfig)
cp vehicle/OVMS.V3/support/sdkconfig.default.hw31 vehicle/OVMS.V3/sdkconfig

# Build
make -C vehicle/OVMS.V3 -j5

# Flash
make -C vehicle/OVMS.V3 flash

# Monitor serial
make -C vehicle/OVMS.V3 monitor
```

Hardware variants: `sdkconfig.default.hw30`, `sdkconfig.default.hw31`, `sdkconfig.bluetooth.hw31`, `sdkconfig.lilygo_tc`. Production hardware is hw31.

## Code Style

The VW e-Golf component uses `.clang-format` (Google style, 4-space indent, 100-column limit):
```bash
clang-format -i vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.cpp \
                vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.h
```

## Architecture

### Component System

All functionality lives in `vehicle/OVMS.V3/components/`. Each component has:
- `CMakeLists.txt` — conditionally compiled via Kconfig (`CONFIG_OVMS_VEHICLE_*`)
- `component.mk` — legacy Make equivalent
- `src/` — implementation

Vehicle support is compiled in/out via Kconfig. Enable `CONFIG_OVMS_VEHICLE_VWEGOLF` via `make menuconfig` or sdkconfig.

### Vehicle Module Pattern

Every vehicle module follows the same structure:

1. **Class** inherits from `OvmsVehicle` (in `components/main/vehicle.h`)
2. **Registration** via a static init object with `__attribute__((init_priority(9000)))`:
   ```cpp
   MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeGolf>("VWEG", "VW e-Golf");
   ```
3. **CAN buses** registered in constructor with `RegisterCanBus(bus, mode, speed)`
4. **Incoming frames** handled by overriding `IncomingFrameCan1/2/3(CAN_frame_t*)`
5. **Periodic ticks** via `Ticker1()` (every 1s) and `Ticker10()` (every 10s)
6. **Metrics** set via `StandardMetrics.*->SetValue(...)` (standard) or custom metrics
7. **Commands** registered via `MyCommandApp.RegisterCommand()`
8. **Config** registered via `MyConfig.RegisterParam("xvg", ...)` (prefix `xvg` for VW e-Golf)

### VW e-Golf Specifics (`vehicle_vwegolf/`)

- **CAN bus 2** (FCAN): Powertrain CAN — gear position, BMS data, VIN
- **CAN bus 3** (KCAN): Convenience CAN — speed, SoC, charging, body control, climate control

Poll states defined in header:
- `VWEGOLF_OFF` (0) — all sleeping
- `VWEGOLF_AWAKE` (1) — base systems online
- `VWEGOLF_CHARGING` (2) — charging
- `VWEGOLF_ON` (3) — drivable

Custom CLI namespace is `xvg` (matches config param prefix).

**Important**: The OBD port only carries Diagnostic CAN and only works when the car is powered on. It cannot be used for remote control or wake-up. All remote features (climate control, wake, lock, charge control) require KCAN access via the J533 gateway harness.

**What is known to be achievable via KCAN/BAP** (reverse-engineered by thomasakarlsen, see reference below):
- Wake the car from sleep
- Start/stop climatisation
- Set climatisation temperature and duration (default 10 min)
- Set "allow climatisation on battery" flag
- Set minimum charge limit
- Read/write charge profiles (profile 0 is the hidden global profile for minimum charge limit, temperature, etc. — updating it is the same as updating any named profile)
- Trigger horn and indicator lights

### Key Framework Singletons

- `StandardMetrics` — standard vehicle metrics (battery, speed, VIN, etc.)
- `MyEvents` — event bus for subscribing/emitting named events
- `MyCommandApp` — command registration/dispatch
- `MyConfig` — persistent configuration
- `MyVehicleFactory` — vehicle type registry
- `MyMetrics` — metric registration and lookup
- `ESP_LOGI/V/W/E(TAG, ...)` — logging (use `#define TAG "v-vwegolf"` pattern)

### Metrics

Standard metrics live in `StandardMetrics.*` (e.g. `ms_v_bat_soc`, `ms_v_pos_speed`, `ms_v_bat_voltage`). Custom vehicle-specific metrics use the vehicle's prefix (`xvg.*`) and are registered as class members:

```cpp
// In constructor — create custom metric
m_my_metric = MyMetrics.Find("xvg.custom");
if (!m_my_metric)
  m_my_metric = new OvmsMetricFloat("xvg.custom", SM_STALE_MIN, Native);
```

Types: `OvmsMetricBool`, `OvmsMetricInt`, `OvmsMetricFloat`, `OvmsMetricString`. Metrics support `SetAutoStale(seconds)` to mark data as stale if not updated.

### CAN Polling (OBD2/UDS)

For active ECU queries (ISO-TP / VW TP 2.0) rather than passive frame listening, use the poller framework. Define a static poll list and register it:

```cpp
static const OvmsPoller::poll_pid_t vehicle_poll_list[] = {
  // { txid, rxid, type, pid, {state0_secs, state1_secs, ...}, bus, protocol }
  { 0x7E0, 0x7E8, VEHICLE_POLL_TYPE_READDATA, 0x1234, {0, 10, 60, 0}, 1, ISOTP_STD },
  POLL_LIST_END
};

// In constructor:
PollSetPidList(m_can1, vehicle_poll_list);
PollSetState(0);  // index matches polltime array positions
```

Poll state indices correspond to `VWEGOLF_OFF/AWAKE/CHARGING/ON`. Change state with `PollSetState(state)`. Responses arrive in `IncomingPollReply()`:

```cpp
void OvmsVehicleVWeGolf::IncomingPollReply(const OvmsPoller::poll_job_t &job,
                                           uint8_t* data, uint8_t length) {
  if (job.type == VEHICLE_POLL_TYPE_READDATA && job.pid == 0x1234) {
    // decode data
  }
}
```

For VW TP 2.0 (ECU communication through the J533 gateway): use the gateway base ID (typically `0x200`) as txid and the logical ECU ID as rxid, with `protocol = VWTP_20`.

For one-off blocking requests: `PollSingleRequest(bus, txid, rxid, type, pid, response, timeout_ms, protocol)` returns `POLLSINGLE_OK` or `POLLSINGLE_TIMEOUT`.

Common poll types: `VEHICLE_POLL_TYPE_READDATA` (0x22, 16-bit PID), `VEHICLE_POLL_TYPE_READMEMORY` (0x23), `VEHICLE_POLL_TYPE_IOCONTROL` (0x2F).

### Events

```cpp
// Subscribe (usually in constructor)
MyEvents.RegisterEvent(TAG, "vehicle.charge.start",
  [](std::string event, void* data) { /* ... */ });

// Emit
MyEvents.SignalEvent("xvg.custom.event", nullptr);

// Unsubscribe (in destructor)
MyEvents.DeregisterEvent(TAG);
```

Standard events include `vehicle.on`, `vehicle.off`, `vehicle.charge.start`, `vehicle.charge.stop`, `vehicle.charge.done`, `system.shuttingdown`.

### Configuration

```cpp
// Register in constructor
MyConfig.RegisterParam("xvg", "VW e-Golf", true /*writeable*/, true /*readable*/);

// Read
std::string val = MyConfig.GetParamValue("xvg", "instance_name");
int ival        = MyConfig.GetParamValueInt("xvg", "instance_name", default_val);
bool bval       = MyConfig.GetParamValueBool("xvg", "instance_name", false);

// Write
MyConfig.SetParamValue("xvg", "instance_name", "value");
```

Values are stored as strings; type conversion is automatic. Mark `readable=false` for secrets/passwords.

### Ticker Callbacks

Override any combination of: `Ticker1()` (1 s), `Ticker10()` (10 s), `Ticker60()`, `Ticker300()`, `Ticker600()`, `Ticker3600()`. The `ticker` parameter is a monotonically increasing counter — use `ticker % N` for sub-intervals. Always call the parent implementation.

### Vehicle State Hooks

Override notification methods for state changes rather than polling metrics:

```cpp
void NotifyVehicleOn();   void NotifyVehicleOff();
void NotifyChargeStart(); void NotifyChargeDone(); void NotifyChargeStop();
void Notify12vCritical(); void Notify12vRecovered();
```

Also override `MetricModified(OvmsMetric* metric)` to react to any metric change, and `ConfigChanged(OvmsConfigParam* param)` for config changes.

### Vehicle Commands

Standard commands return `vehicle_command_t` (`Success`, `Fail`, `NotImplemented`). Override:

```cpp
vehicle_command_t CommandLock(const char* pin) override;
vehicle_command_t CommandUnlock(const char* pin) override;
vehicle_command_t CommandWakeup() override;
vehicle_command_t CommandStartCharge() override;
vehicle_command_t CommandStopCharge() override;
vehicle_command_t CommandClimateControl(bool enable) override;
```

### Naming Conventions

| Element | Pattern | VW e-Golf example |
|---|---|---|
| Short code (vehicle ID) | 2+ chars uppercase | `VWEG` |
| Log tag | `v-` + lowercase, dashes not underscores | `v-vwegolf` |
| Config/metric prefix | `x` + 2+ chars lowercase | `xvg` |
| CLI namespace | same as config prefix | `xvg` |
| Config instance names | all lowercase | `cc-temp`, `enabled` |
| CLI subcommands | all lowercase | `xvg climate` |

### Key Reference Files

- `components/main/vehicle.h` — base vehicle class and all overridable hooks
- `components/poller/src/vehicle_poller.h` — poll_pid_t, poll types, IncomingPollReply
- `components/main/ovms_metrics.h` — metric types and StandardMetrics
- `components/main/ovms_events.h` — event system
- `components/main/ovms_command.h` — command framework
- `components/main/ovms_config.h` — configuration system
- `components/vehicle_vweup/` — similar VW vehicle, useful reference implementation
- `components/vehicle_hyundai_ioniqvfl/` — recommended by maintainer as a concise but complete reference showcasing most commonly used framework elements

## Iterative Development Workflow

This is an embedded target — there is no native unit test runner. The workflow is: write a small change, build, flash to the device, verify on the car. Keep changes small enough that each one can be independently verified.

### Build and flash

```bash
# Build
make -C vehicle/OVMS.V3 -j5

# Flash directly over USB
make -C vehicle/OVMS.V3 flash

# Flash over WiFi (once the device is running): copy the built .bin to an HTTP
# server (or SD card) and run from the OVMS shell:
ota flash http http://<your-server>/ovms3.bin
# or via SD card:
ota flash vfs /sd/ovms3.bin
```

The OVMS shell is accessible via USB serial (`make monitor`), SSH, or the built-in web terminal. All verification commands below run there.

### CAN frame capture and replay

This is the core testing loop — capture real frames from the car once, then replay them repeatedly against new firmware builds at your desk without needing the car running.

**Capture** (on the car, written to SD card):
```
can log start vfs crtd /sd/kcan-session.crtd can3
# drive / charge / do the thing you want to test
can log stop
```

**Replay** (at your desk, replays frames into the vehicle module as if they came from the bus):
```
can play start vfs crtd /sd/kcan-session.crtd
can play status
can play stop
```

After replay, check metrics:
```
metrics list
metrics list ms_v_bat_soc
```

### Verbose logging

To see every frame your module processes:
```
log level verbose v-vwegolf
```

Reset afterwards:
```
log level info v-vwegolf
```

### Injecting a single hand-crafted frame

The OVMS shell has `can send` for sending a raw frame on a bus:
```
# can send <bus> <id> <data-hex>
can send can3 131 7F
```

Use this to test a specific message ID decode path without a full log replay.

### Verifying metrics after a change

```
metrics list ms_v_bat
metrics list ms_v_env
metrics list ms_v_pos
```

Values show their current data and whether they are stale (not updated recently).

### Bit extraction utility

Rather than manual byte shifting, use `canbitset` from `canutils.h` to extract signal values from CAN frames safely:

```cpp
#include "canutils.h"

// Load the full 8-byte frame
canbitset<uint64_t> canbits(p_frame->data.u8, 8);

// Extract bits 12-23 (inclusive) as an unsigned value
uint32_t val = canbits.get(12, 23);
```

This avoids shift/mask bugs and makes the bit positions self-documenting.

### Branch strategy

- **`master`** — tracks upstream, never commit directly here
- **`investigation`** — documentation, notes, RE findings, CAN log captures
- **`climate-control`** — implementation work, branched from `investigation` when ready

Each testable change on a feature branch should be a single commit covering one decode or control path, so it can be reverted cleanly if it causes problems on the car.

## Maintainer Code Review Rules

These rules come directly from maintainer (dexterbg) review of PR #1327 (initial e-Golf submission). Violating them will cause a PR to be sent back.

**Variables**
- Never use static module-level variables for vehicle state — all state belongs in class member variables.

**Metrics**
- Do not set metrics to a default value in the constructor. Keep them undefined until the real value is known from the CAN bus. Setting them in the constructor breaks the framework's persistent metric feature (values are retained across reboots; overwriting in the constructor silently discards the saved value).
- Only set a metric when you have received and decoded actual data for it.

**Configuration**
- Config instance names must be **all lowercase** — the config system is case-sensitive and users must type these names.
- Make the purpose of every config parameter clear from its name alone.
- Every config parameter must be documented in the user guide.
- Don't create config parameters that aren't actually used.

**Commands**
- The CLI namespace for custom commands must match the config/metrics prefix (`xvg`).
- All command names and subcommand names must be **all lowercase**.
- User-facing command text must be in **English** — German terms are tolerable inside code, but must never appear in help strings, error messages, or any text shown to users.

**Logging**
- Use a single log tag per component (`v-vwegolf`). Do not create multiple per-subsystem tags within one vehicle module — users reporting issues need to set one log level, not many.
- Log tag names use **dashes**, not underscores: `v-vwegolf`, not `v_vwegolf`.

**Unfinished code**
- Stubs, placeholders, and incomplete sections are acceptable in a PR, but must have a comment explaining the plan or a TODO description. Don't leave dead code silently in place.

**Documentation / User Guide**
- Every vehicle module merged into upstream must have a user guide page (`docs/index.rst`). Hardware connection instructions, adapter wiring, and installation images belong in the user guide, not only in the PR discussion.

## Important Notes

- The upstream OVMS project explicitly warns against unvalidated AI-generated code. All code added to this vehicle module must be carefully validated against real vehicle CAN data before submission upstream.
- Pull requests to upstream should focus on a single issue/vehicle and not mix unrelated changes.
- The VW e-Golf connects via the J533 gateway (diagnostic CAN connector), not directly to individual ECUs.
- Climate control and other comfort functions use the proprietary **BAP** (*Bedien- und Anzeigeprotokoll*) protocol on KCAN, not ISO-TP/UDS. BAP cannot be handled by the standard OVMS poller — it requires direct CAN frame construction. See `vehicle_vwegolf/docs/vw-bap-protocol.md` for the protocol reference.
- **Key external RE resource**: https://github.com/thomasakarlsen/e-golf-comfort-can — reverse-engineering documentation specifically for e-Golf KCAN/BAP, covering climatisation, charge profiles, wake, horn/lights. This is the primary reference for implementing climate control and other remote features.
