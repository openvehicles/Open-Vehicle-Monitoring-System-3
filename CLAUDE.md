# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Fork of Open Vehicle Monitoring System v3 (OVMS3) focused on VW e-Golf integration. OVMS3 is an ESP32-based vehicle telemetry and control system connecting to a vehicle's CAN bus via OBD2 and providing remote monitoring, diagnostics, and control.

Primary focus: `vehicle/OVMS.V3/components/vehicle_vwegolf/`

**"The app"** always refers to OVMS Connect by CrashOverride2 (Apple App Store / Google Play). All end-user-facing decisions — metric naming, command behaviour, response timing — target this app as the client. Vehicle list: https://github.com/CrashOverride2/OVMS-Connect-Vehicles

**Key constraint:** The OBD port only carries Diagnostic CAN and only works when the car is on. All remote features (climate, wake, lock, charge) require KCAN access via the J533 gateway harness. The e-Golf connects via J533, not directly to individual ECUs.

**BAP:** Climate control and other comfort functions use the proprietary BAP (*Bedien- und Anzeigeprotokoll*) protocol on KCAN — not ISO-TP/UDS. BAP cannot be handled by the standard OVMS poller; it requires direct CAN frame construction. See `docs/vw-bap-protocol.md` and `docs/clima-control-bap.md`.

**External RE resource:** https://github.com/thomasakarlsen/e-golf-comfort-can — primary reference for KCAN/BAP: climatisation, charge profiles, wake, horn/lights.

**AI-generated code warning:** The upstream OVMS project explicitly warns against unvalidated AI-generated code. All code must be validated against real vehicle CAN data before upstream submission.

## Build & Deploy

```bash
# First time: system packages (Arch Linux)
sudo pacman -S --needed base-devel git python python-pip gperf dos2unix

# First time: toolchain + ESP-IDF + Python venv (installs to ~/ovms-toolchain/)
bash scripts/setup-toolchain.sh

# First time: git hooks (pre-push test gate on fix/* and feat/*)
bash scripts/install-hooks.sh

# Activate toolchain (run each session, or add to ~/.zshrc)
source <(bash scripts/setup-toolchain.sh --env)

# Build (runs tests first, aborts on failure)
bash scripts/build.sh

# Build + serve for OTA flash (laptop on OVMS hotspot)
bash scripts/build.sh --deploy
```

`build.sh --deploy` starts a Python HTTP server and prints the OTA command to paste into the OVMS shell. Production hardware is hw31 (`sdkconfig.default.hw31`).

OVMS shell: `http://192.168.4.1` (web terminal) or SSH to `192.168.4.1`.

OTA command (printed by build.sh):
```
ota flash http http://<laptop-ip>:8080/ovms3.bin
```

## Code Style

Google style, 4-space indent, 100-column limit (`.clang-format` in component dir):
```bash
clang-format -i vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.cpp \
                vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.h
```

## Architecture

### Component System

All functionality lives in `vehicle/OVMS.V3/components/`. Enable `CONFIG_OVMS_VEHICLE_VWEGOLF` via `make menuconfig` or sdkconfig. Each component has `CMakeLists.txt`, `component.mk`, and `src/`.

### VW e-Golf Specifics

- **CAN bus 2** (`m_can2`, FCAN): Powertrain CAN — gear position, BMS data, VIN
- **CAN bus 3** (`m_can3`, KCAN): Convenience CAN — speed, SoC, charging, body control, climate
- **Custom CLI/config/metric prefix:** `xvg`

Poll states:
- `VWEGOLF_OFF` (0) — sleeping
- `VWEGOLF_AWAKE` (1) — base systems online
- `VWEGOLF_CHARGING` (2) — charging
- `VWEGOLF_ON` (3) — drivable

**Achievable via KCAN/BAP:** wake from sleep · start/stop climatisation · set clima temp and duration · set "allow clima on battery" flag · set minimum charge limit · read/write charge profiles · trigger horn and indicators.

### Framework Quick Reference

Key singletons: `StandardMetrics`, `MyEvents`, `MyCommandApp`, `MyConfig`, `MyVehicleFactory`, `MyMetrics`. Logging: `ESP_LOGI/V/W/E(TAG, ...)` with `#define TAG "v-vwegolf"`.

Vehicle class inherits `OvmsVehicle`. Override `IncomingFrameCan1/2/3()` for passive frame decoding, `Ticker1()`/`Ticker10()` etc. for periodic tasks, `IncomingPollReply()` for active UDS/TP2.0 responses. Always call the parent implementation in ticker overrides.

**Bit extraction** — use `canbitset` from `canutils.h` rather than manual shifts:
```cpp
canbitset<uint64_t> canbits(p_frame->data.u8, 8);
uint32_t val = canbits.get(12, 23);  // bits 12-23 inclusive
```

**Custom metrics** — register as class members, never set a default value in the constructor:
```cpp
m_my_metric = MyMetrics.Find("xvg.custom");
if (!m_my_metric)
    m_my_metric = new OvmsMetricFloat("xvg.custom", SM_STALE_MIN, Native);
```

### Naming Conventions

| Element | Pattern | Example |
|---|---|---|
| Vehicle ID | 2+ chars uppercase | `VWEG` |
| Log tag | `v-` + lowercase, dashes | `v-vwegolf` |
| Config/metric prefix | `x` + 2+ chars lowercase | `xvg` |
| CLI namespace | same as prefix | `xvg` |
| Config instance names | all lowercase | `cc-temp` |
| CLI subcommands | all lowercase | `xvg climate` |

### Key Reference Files

- `components/main/vehicle.h` — base vehicle class and all overridable hooks
- `components/poller/src/vehicle_poller.h` — poll_pid_t, poll types, IncomingPollReply
- `components/main/ovms_metrics.h` — metric types and StandardMetrics
- `components/main/ovms_events.h` — event system
- `components/main/ovms_command.h` — command framework
- `components/main/ovms_config.h` — configuration system
- `components/vehicle_vweup/` — similar VW vehicle, useful reference
- `components/vehicle_hyundai_ioniqvfl/` — concise but complete reference recommended by maintainer
- `components/vehicle_vwegolf/docs/vwegolf.dbc` — canonical CAN signal definitions: bit positions, scaling, confirmation status, BAP frame stubs. Update whenever a new signal is confirmed from a capture.

## Development Workflow

Workflow for each fix or feature: branch → test → build → OTA flash to the-module → verify on car → PR upstream. One thing per branch/PR.

### Git hooks

`scripts/install-hooks.sh` installs a pre-push hook that blocks pushes on `fix/*` and `feat/*` unless all native tests pass. `investigation` and `master` push freely. Bypass only in genuine emergencies: `git push --no-verify`.

### CAN capture and replay

Core testing loop: capture real frames once while car is in a specific state, then replay repeatedly against new firmware builds without needing the car in that state again.

**Capture** (laptop on OVMS hotspot, SSH key for `ovms@192.168.4.1` required):
```bash
bash vehicle/OVMS.V3/components/vehicle_vwegolf/tests/capture.sh        # can3 (default)
bash vehicle/OVMS.V3/components/vehicle_vwegolf/tests/capture.sh can2   # FCAN (rarely needed)
```
The script queries firmware version via SSH, prompts for a one-line description, starts the log automatically, streams frames until Ctrl-C, stops the log, and writes both a `.crtd` and a companion `.md` in `tests/candumps/`.

To filter specific CAN IDs (manually, if needed):
```
can log start tcpserver transmit crtd 3000 3:131
```

**Note:** CCS DC fast charging keeps KCAN completely silent — no frames visible to OVMS. AC Type 2 charging is required for any OBC-related captures.

**Replay:**
```
can play start vfs crtd /sd/capture.crtd
can play stop
```

**Single frame injection:**
```
can send can3 131 7F
```

**Verbose logging:**
```
log level verbose v-vwegolf
log level info v-vwegolf     # reset
```

**Check metrics:**
```
metrics list ms_v_bat
metrics list ms_v_env
metrics list ms_v_pos
```

### Branch strategy

- **`master`** — tracks upstream, never commit directly
- **`investigation`** — RE notes, captures, documentation only
- **`climate-control`** — implementation branch

Each commit should cover one decode or control path so it can be reverted cleanly.

### Upstream PRs

The `climate-control` branch is **not** submitted as a single PR. Extract small focused PRs in the order tracked in `PROJECT.md`. Each PR must: pass native tests · reference relevant `docs/` RE notes · English user-facing strings only · single log tag `v-vwegolf` · no metric defaults in constructor.

## Maintainer Code Review Rules

From maintainer (dexterbg) review of PR #1327. Violating these causes a PR to be returned.

**Variables:** No static module-level variables for vehicle state — all state in class members.

**Metrics:** Never set a metric to a default in the constructor — breaks persistent metric feature (values survive reboots; constructor writes silently discard them). Only set a metric when real data has been decoded.

**Configuration:** Instance names all lowercase (case-sensitive). Name must be self-documenting. Every param must be documented in the user guide. Don't create unused params.

**Commands:** CLI namespace = config prefix (`xvg`). All command names lowercase. User-facing text in English only — no German in help strings, errors, or any user-visible text.

**Logging:** Single log tag per component (`v-vwegolf`). Tag names use dashes, not underscores.

**Unfinished code:** Stubs and placeholders are OK in a PR but must have a comment explaining the plan. No silent dead code.

**User guide:** Every merged vehicle module needs a `docs/index.rst` page covering hardware wiring, J533 adapter, all config params and commands.
