# CLAUDE.md

Guidance for Claude Code in this repo. Build/framework/capture details → `docs/dev-guide.md`.

## Project Overview

Fork of OVMS3 — VW e-Golf integration. ESP32 vehicle telemetry/control via CAN bus. Remote monitoring, diagnostics, control.

Primary focus: `vehicle/OVMS.V3/components/vehicle_vwegolf/`

**"The app"** = OVMS Connect by CrashOverride2 (App Store / Google Play). All user-facing decisions — metric naming, command behaviour, response timing — target this app.

**Key constraint:** OBD port = Diagnostic CAN only, works when car on. Remote features (climate, wake, lock, charge) need KCAN via J533 gateway harness. e-Golf connects via J533, not direct to ECUs.

**BAP:** Climate/comfort functions use proprietary BAP (*Bedien- und Anzeigeprotokoll*) on KCAN — not ISO-TP/UDS. Can't use standard OVMS poller; needs direct CAN frame construction. See `docs/vw-bap-protocol.md` and `docs/clima-control-bap.md`.

**External RE resource:** https://github.com/thomasakarlsen/e-golf-comfort-can — primary KCAN/BAP reference.

**AI-generated code warning:** Upstream OVMS warns against unvalidated AI code. All code must validate against real vehicle CAN data before upstream submission.

## VW e-Golf Specifics

- **CAN bus 2** (`m_can2`, FCAN): Powertrain — gear, BMS, VIN
- **CAN bus 3** (`m_can3`, KCAN): Convenience — speed, SoC, charging, body, climate
- **Prefix:** `xvg` (CLI, config, custom metrics)
- **Achievable via KCAN/BAP:** wake · clima start/stop · temp/duration · min charge limit · charge profiles · horn/indicators

## Code Style

Google style, 4-space indent, 100-col limit (`.clang-format` in component dir):
```bash
clang-format -i vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.cpp \
                vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.h
```

## Naming Conventions

| Element | Pattern | Example |
|---|---|---|
| Vehicle ID | 2+ chars uppercase | `VWEG` |
| Log tag | `v-` + lowercase, dashes | `v-vwegolf` |
| Config/metric prefix | `x` + 2+ chars lowercase | `xvg` |
| CLI namespace | same as prefix | `xvg` |
| Config instance names | all lowercase | `cc-temp` |
| CLI subcommands | all lowercase | `xvg climate` |

## Development Workflow

Each fix/feature: branch → test → build → OTA flash to the-module → verify on car → PR upstream. One thing per branch/PR.

- **`master`** — tracks upstream, no direct commits
- **`investigation`** — RE notes, captures, docs only
- **`climate-control`** — implementation branch

Git hooks: `fix/*` and `feat/*` blocked on push unless native tests pass. Bypass: `git push --no-verify` (emergencies only).

Upstream PRs: extract small focused PRs per order in `PROJECT.md`. Each PR: pass native tests · ref `docs/` RE notes · English strings · single tag `v-vwegolf` · no metric defaults in constructor.

## Maintainer Code Review Rules

From dexterbg review of PR #1327. Violating = PR returned.

**Variables:** No static module-level vars for vehicle state — all state in class members.

**Metrics:** Never set metric default in constructor — breaks persistent metrics. Only set when real data decoded.

**Configuration:** Instance names all lowercase (case-sensitive). Self-documenting names. Every param documented in user guide. No unused params.

**Commands:** CLI namespace = config prefix (`xvg`). All lowercase. English only — no German in user-visible text.

**Logging:** Single tag per component (`v-vwegolf`). Dashes, not underscores.

**Unfinished code:** Stubs/placeholders OK but must have comment with plan. No silent dead code.

**User guide:** Every merged vehicle module needs `docs/index.rst` covering hardware wiring, J533 adapter, all config params and commands.
