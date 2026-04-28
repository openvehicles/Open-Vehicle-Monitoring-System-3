# CLAUDE.md

Build/framework/capture → `docs/dev-guide.md`.

## Project

OVMS3 fork, VW e-Golf. ESP32 telemetry/ctrl via CAN. Focus: `vehicle/OVMS.V3/components/vehicle_vwegolf/`.

**App** = OVMS Connect by CrashOverride2 (iOS/Android). Metric names, cmd behavior, timing → target this app.

**OBD port** = Diag CAN, ign-on only. Remote (clima/wake/lock/charge) needs KCAN via J533 harness. e-Golf via J533, not direct ECU.

**BAP** = VW *Bedien-/Anzeigeprotokoll*. KCAN, not ISO-TP/UDS. No std poller — direct frame build. Refs: `vehicle/OVMS.V3/components/vehicle_vwegolf/docs/vw-bap-protocol.md`, `…/docs/clima-control-bap.md`.

**Ext RE:** https://github.com/thomasakarlsen/e-golf-comfort-can — primary KCAN/BAP ref.

**AI warn:** upstream rejects unvalidated AI code. Validate vs real CAN before upstream PR.

## e-Golf bus map

- CAN2 (`m_can2`, FCAN): powertrain — gear, BMS, VIN
- CAN3 (`m_can3`, KCAN): convenience — speed, SoC, charge, body, clima
- Prefix: `xvg` (CLI/cfg/metrics)
- KCAN/BAP capable: wake · clima start/stop · temp/dur · min charge · profiles · horn/indicator

## Frame decode docs

**Single source:** `vehicle/OVMS.V3/components/vehicle_vwegolf/docs/vwegolf.dbc`. All signal definitions (CAN ID, bit pos, scaling, units, sender/receiver, comments). Other docs (PROJECT.md, captures.tsv, per-cap .md, BAP refs) reference signal names — DO NOT restate decode details. Re-statement = drift = bugs.

When a capture confirms a new signal: add to `vwegolf.dbc`, point cap notes at it.

## Knowledge graph

Graph lives in `vehicle/OVMS.V3/components/vehicle_vwegolf/graphify-out/`. 281 nodes, 442 edges.

**Read graph before reading source.** Architecture/structure/relationship questions → graph first.

1. `graphify-out/GRAPH_REPORT.md` — god nodes, community map, surprises. Read this.
2. Deeper: load `graphify-out/graph.json` via networkx, BFS/DFS from matching nodes.

God nodes: `IncomingFrameCan3()` · `main()` · `make_vehicle()` · `make_frame()` · `test_clima_all()`.

Communities: C0=CRTD utils · C1=CAN decode tests · C2=clima/wake suite · C3=vehicle core · C6=BAP docs · C7=clima bugs · C8=J533 wiring · C18=OBC charge telemetry.

Files changed → `/graphify --update` (incremental, cached).

## CRTD captures — NEVER read whole

`.crtd` files = 10k–60k+ lines each. Reading whole = context burn for nothing.

**Use:** `vehicle/OVMS.V3/components/vehicle_vwegolf/tests/analysis/crtd.py` (in `.venv`).

API: `load(path, bus=N)` → `Capture` · `load_multi(path)` for mixed-bus · `iter_frames`/`iter_crtd` streaming · `trajectory_*`/`byte_stats`/`hexdump_window` for analysis · `gear_timeline`/`gear_windows` for FCAN drive caps. See `README.md` for mislabeled-bus list (use `bus=N` explicit).

Pattern: write throwaway script in `tests/analysis/scratch/`, run via `.venv`, read script output — not the raw frames.

## Style

Google, 4sp, 100col. `.clang-format` in component dir.
```bash
clang-format -i vehicle/OVMS.V3/components/vehicle_vwegolf/src/*.{cpp,h}
```

## Naming

| Element | Pattern | Ex |
|---|---|---|
| Vehicle ID | 2+ UPPER | `VWEG` |
| Log tag | `v-` lower-dash | `v-vwegolf` |
| Cfg/metric prefix | `x` + 2+ lower | `xvg` |
| CLI ns | = prefix | `xvg` |
| Cfg instance | all lower | `cc-temp` |
| CLI subcmd | all lower | `xvg climate` |

## Workflow

Per fix/feat: branch → test → build → OTA flash to-the-module → verify on car → upstream PR. One thing/branch.

- `master` — tracks upstream, no direct commits
- `vwegolf` — active dev branch

Hooks: `fix/*`/`feat/*` push blocked unless native tests pass. Bypass `--no-verify` emergencies only.

Upstream PRs: small focused per `PROJECT.md` order. Each: pass native tests · ref RE notes in `…/vehicle_vwegolf/docs/` · EN strings · single tag `v-vwegolf` · NO metric defaults in ctor.

## Maintainer review (dexterbg PR #1327)

Violation = PR returned.

- **State:** no static module-lvl vars — all in class members
- **Metrics:** never set default in ctor (breaks persistent metrics). Set only on real decode
- **Cfg:** instance names all lower (case-sens), self-doc, every param in user guide, no unused
- **Cmds:** CLI ns = cfg prefix (`xvg`), all lower, EN only — no German user-visible
- **Log:** single tag `v-vwegolf`, dashes not underscores
- **Stubs:** OK if commented w/ plan. No silent dead code
- **User guide:** every merged module → `docs/index.rst` w/ HW wiring, J533, all params/cmds
