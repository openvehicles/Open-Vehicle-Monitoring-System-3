Here's the compressed version:

# CLAUDE.md

Guidance for Claude Code (claude.ai/code) in this repo.

## Project Overview

Fork of OVMS3 — VW e-Golf integration. ESP32 vehicle telemetry/control via CAN bus (OBD2). Remote monitoring, diagnostics, control.

Primary focus: `vehicle/OVMS.V3/components/vehicle_vwegolf/`

**"The app"** = OVMS Connect by CrashOverride2 (App Store / Google Play). All user-facing decisions — metric naming, command behaviour, response timing — target this app. Vehicle list: https://github.com/CrashOverride2/OVMS-Connect-Vehicles

**Key constraint:** OBD port = Diagnostic CAN only, works when car on. Remote features (climate, wake, lock, charge) need KCAN via J533 gateway harness. e-Golf connects via J533, not direct to ECUs.

**BAP:** Climate/comfort functions use proprietary BAP (*Bedien- und Anzeigeprotokoll*) on KCAN — not ISO-TP/UDS. Can't use standard OVMS poller; needs direct CAN frame construction. See `docs/vw-bap-protocol.md` and `docs/clima-control-bap.md`.

**External RE resource:** https://github.com/thomasakarlsen/e-golf-comfort-can — primary KCAN/BAP reference: climatisation, charge profiles, wake, horn/lights.

**AI-generated code warning:** Upstream OVMS warns against unvalidated AI code. All code must validate against real vehicle CAN data before upstream submission.

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

# Run native tests only (no toolchain needed, fast iteration)
make -C vehicle/OVMS.V3/components/vehicle_vwegolf/tests

# Build (runs tests first, aborts on failure)
bash scripts/build.sh

# Build + serve for OTA flash (laptop on OVMS hotspot)
bash scripts/build.sh --deploy
```

`build.sh --deploy` starts Python HTTP server, prints OTA command for OVMS shell. Production hw = hw31 (`sdkconfig.default.hw31`).

OVMS shell: `http://192.168.4.1` (web terminal) or SSH `192.168.4.1`.

OTA command (printed by build.sh):
```
ota flash http http://<laptop-ip>:8080/ovms3.bin
```

## Code Style

Google style, 4-space indent, 100-col limit (`.clang-format` in component dir):
```bash
clang-format -i vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.cpp \
                vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.h
```

## Architecture

### Component System

All functionality in `vehicle/OVMS.V3/components/`. Enable `CONFIG_OVMS_VEHICLE_VWEGOLF` via `make menuconfig` or sdkconfig. Each component has `CMakeLists.txt`, `component.mk`, `src/`.

### VW e-Golf Specifics

- **CAN bus 2** (`m_can2`, FCAN): Powertrain — gear, BMS, VIN
- **CAN bus 3** (`m_can3`, KCAN): Convenience — speed, SoC, charging, body, climate
- **Custom CLI/config/metric prefix:** `xvg`

Poll states:
- `VWEGOLF_OFF` (0) — sleeping
- `VWEGOLF_AWAKE` (1) — base systems online
- `VWEGOLF_CHARGING` (2) — charging
- `VWEGOLF_ON` (3) — drivable

**Achievable via KCAN/BAP:** wake · start/stop climatisation · set clima temp/duration · set "allow clima on battery" · set min charge limit · read/write charge profiles · horn/indicators.

### Framework Quick Reference

Key singletons: `StandardMetrics`, `MyEvents`, `MyCommandApp`, `MyConfig`, `MyVehicleFactory`, `MyMetrics`. Logging: `ESP_LOGI/V/W/E(TAG, ...)` with `#define TAG "v-vwegolf"`.

Vehicle class inherits `OvmsVehicle`. Override `IncomingFrameCan1/2/3()` for passive decode, `Ticker1()`/`Ticker10()` for periodic tasks, `IncomingPollReply()` for active UDS/TP2.0 responses. Always call parent in ticker overrides.

**Bit extraction** — use `canbitset` from `canutils.h`, not manual shifts:
```cpp
canbitset<uint64_t> canbits(p_frame->data.u8, 8);
uint32_t val = canbits.get(12, 23);  // bits 12-23 inclusive
```

**Custom metrics** — register as class members, never set default in constructor:
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

- `components/main/vehicle.h` — base vehicle class, all overridable hooks
- `components/poller/src/vehicle_poller.h` — poll_pid_t, poll types, IncomingPollReply
- `components/main/ovms_metrics.h` — metric types, StandardMetrics
- `components/main/ovms_events.h` — event system
- `components/main/ovms_command.h` — command framework
- `components/main/ovms_config.h` — config system
- `components/vehicle_vweup/` — similar VW vehicle, good reference
- `components/vehicle_hyundai_ioniqvfl/` — concise complete reference (maintainer recommended)
- `components/vehicle_vwegolf/docs/vwegolf.dbc` — canonical CAN signal defs: bit positions, scaling, confirmation status, BAP frame stubs. Update when new signal confirmed from capture.

## Development Workflow

Each fix/feature: branch → test → build → OTA flash to the-module → verify on car → PR upstream. One thing per branch/PR.

### Git hooks

`scripts/install-hooks.sh` installs pre-push hook blocking `fix/*` and `feat/*` unless native tests pass. `investigation` and `master` push freely. Bypass emergencies only: `git push --no-verify`.

### CAN capture and replay

Core loop: capture real frames once in specific state, replay against new builds without needing car in that state.

**Capture** (laptop on OVMS hotspot, SSH key for `ovms@192.168.4.1`):
```bash
bash vehicle/OVMS.V3/components/vehicle_vwegolf/tests/capture.sh        # can3 (default)
bash vehicle/OVMS.V3/components/vehicle_vwegolf/tests/capture.sh can2   # FCAN (rarely needed)
```
Script queries firmware version via SSH, prompts for description, starts log, streams frames til Ctrl-C, stops log, writes `.crtd` + companion `.md` in `tests/candumps/`.

Filter specific CAN IDs (manual):
```
can log start tcpserver transmit crtd 3000 3:131
```

**Note:** CCS DC fast charging = KCAN silent, no frames visible. AC Type 2 required for OBC captures.

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

- **`master`** — tracks upstream, no direct commits
- **`investigation`** — RE notes, captures, docs only
- **`climate-control`** — implementation branch

Each commit = one decode or control path, cleanly revertible.

### Upstream PRs

`climate-control` not submitted as single PR. Extract small focused PRs per order in `PROJECT.md`. Each PR must: pass native tests · reference `docs/` RE notes · English strings only · single tag `v-vwegolf` · no metric defaults in constructor.

## Maintainer Code Review Rules

From dexterbg review of PR #1327. Violating = PR returned.

**Variables:** No static module-level vars for vehicle state — all state in class members.

**Metrics:** Never set metric default in constructor — breaks persistent metrics (survive reboots; constructor writes silently discard). Only set when real data decoded.

**Configuration:** Instance names all lowercase (case-sensitive). Self-documenting names. Every param documented in user guide. No unused params.

**Commands:** CLI namespace = config prefix (`xvg`). All lowercase. English only — no German in help/errors/user-visible text.

**Logging:** Single tag per component (`v-vwegolf`). Dashes, not underscores.

**Unfinished code:** Stubs/placeholders OK but must have comment with plan. No silent dead code.

**User guide:** Every merged vehicle module needs `docs/index.rst` covering hardware wiring, J533 adapter, all config params and commands.