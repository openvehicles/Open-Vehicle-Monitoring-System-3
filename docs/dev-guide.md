# Dev Guide — VW e-Golf OVMS

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

## Architecture

### Component System

All functionality in `vehicle/OVMS.V3/components/`. Enable `CONFIG_OVMS_VEHICLE_VWEGOLF` via `make menuconfig` or sdkconfig. Each component has `CMakeLists.txt`, `component.mk`, `src/`.

### Poll States

- `VWEGOLF_OFF` (0) — sleeping
- `VWEGOLF_AWAKE` (1) — base systems online
- `VWEGOLF_CHARGING` (2) — charging
- `VWEGOLF_ON` (3) — drivable

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

## CAN Capture & Replay

Core loop: capture real frames once in specific state, replay against new builds without needing car.

**Capture** (SD card in module, laptop on OVMS hotspot, SSH key for `ovms@ovms.local`):
```bash
bash vehicle/OVMS.V3/components/vehicle_vwegolf/tests/capture.sh        # can3 (default)
bash vehicle/OVMS.V3/components/vehicle_vwegolf/tests/capture.sh can2   # FCAN (rarely needed)
```
Script connects via SSH (one handshake), checks SD mounted, prompts for description, starts `can log start vfs crtd /sd/...` on the module, waits for Ctrl-C, stops log, SCPs the file from SD, writes `.crtd` + companion `.md` in `tests/candumps/`.

Requires SD card in module (`/sd`). No persistent SSH session during capture — avoids ESP32 heap exhaustion/watchdog resets.

Filter specific CAN IDs (manual, SD):
```
can log start vfs crtd /sd/capture.crtd 3:131
```

**Note:** CCS DC fast charging = KCAN silent. AC Type 2 required for OBC captures.

**Capture analysis (Python):** `tests/analysis/crtd.py` shared parser. Drop one-shot scripts in `tests/analysis/scratch/` (gitignored). API: `load(path) → Capture`, `byte_stats`, `trajectory_byte/_le16/_be16`, `le16/be16`, `gear_timeline/gear_windows`, `GEAR_NAMES`, `downsample`, `hexdump_window`. See `tests/analysis/README.md`.

Promote reusable helpers to `crtd.py` — scratch scripts are glue + the specific question, not parsers.

Findings → `docs/vwegolf.dbc` and `tests/candumps/*.md`. Scratch scripts are disposable.

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
log level info v-vwegolf
```

**Check metrics:**
```
metrics list ms_v_bat
metrics list ms_v_env
metrics list ms_v_pos
```
