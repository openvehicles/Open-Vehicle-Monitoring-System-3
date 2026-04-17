# VW e-Golf OVMS — Project Tracker

Living doc. Update as resolved.
Signal defs → `docs/vwegolf.dbc`. Protocol details → `docs/clima-control-bap.md`.

**Platform:** Firmware flash limit resolved upstream (2026-04-17). Old layout: 3×4 MB slots. New layout (v3-35): 2×7 MB OTA slots + ~2 MB `/store`. Module already on new scheme — no firmware size constraint for current work.

---

## Upstream PR queue

Break `climate-control` into small focused PRs, this order (each depends on prev):

| # | PR | Status | Blocker |
|---|---|---|---|
| 1 | Decode-only baseline — all `IncomingFrameCan3`/`Can2` metric decodes, no bus writes | submitted (#1369) | under review |
| 2 | Bugfixes — 0x594 charge state, 0x2AF overflow, 0x6B7 parktime | submitted (fix branch) | PR 1 merged |
| 3 | `CommandWakeup` + OCU heartbeat — KCAN NM wake sequence, 5 Hz keepalive | ready | PR 1 merged |
| 4 | `CommandClimateControl` — clima start/stop via BAP | testing on car | PR 3 merged |
| 5 | OCU heartbeat one-shot actions — horn, indicators, panic, mirror fold | ready | PR 3 merged |
| 6 | `CommandLock` / `CommandUnlock` | blocked | auth bytes not captured (see below) |
| 7 | User guide (`docs/index.rst`) — required by maintainer for complete vehicle module | not started | — |

Each PR: pass native tests · ref `docs/` RE notes · English strings · single log `v-vwegolf` · no metric defaults in constructor.

---

## Open items

### OVMS module crashes — intermittent, investigation pending

Module reboots intermittently. Failure modes:

- During `can log start` / `can log stop` — repeatedly
- At idle, no user interaction — multiple times
- Cap 15 (20260412-181057) ended early, module crashed mid-capture

**Last crash diagnostic** (`module check` after 2026-04-12 18:51:35 CEST reboot, 3rd reset since last power cycle):

```
Detected boot reason:  Crash (12/12)
Reset reason:          Task watchdog (6)
Last crash:            abort() was called on core 0
Current task core 0:   OVMS CanRx, 1992 stack bytes free
Current task core 1:   IDLE1, 520 stack bytes free
Backtrace:             0x4008dd2e 0x4008dfc9 0x4010cbac 0x4008418e
Event at crash:        vehicle.gear.reverse@ovms-server-v3 60 secs
WDT tasks:             OVMS Events
```

WDT task = **OVMS Events** (not CanRx) — events task starved, TWDT fired. CanRx ran when abort() called.

**Suspected causes:**

1. **Power supply — AWG 23 harness.** Thin wires drop voltage on WiFi TX peaks (capture start/stop add network bursts). Brownouts → task WDT if core locks mid-instruction. Test: measure OVMS supply at `can log start`; retry with engine running (alternator → 14 V) vs ignition off.
2. **Long event handler blocking.** Cap 16 log: `Duktape: event handling for 'ticker.10' took 2460 ms` from ABRP JS plugin — events task starves if handler runs long. Look for other slow handlers, move off events task.
3. **Upstream `canlog_tcpserver` start/stop race.** Start/stop correlated with crashes; check accept/close races. Worth upstream report once root cause pinned.

**Next steps (before captures):**

- Reproduce with `log monitor on` + second SSH session — capture panic backtrace live, not just `module check` summary.
- Decode backtrace symbols: `xtensa-esp32-elf-addr2line -e build/ovms3.elf 0x4008dd2e 0x4008dfc9 0x4010cbac 0x4008418e`
- Measure supply at OVMS connector during `can log start` (multimeter/scope). Below ESP32 brownout threshold (≈ 2.43 V at 3.3 V rail, or ~4 V on USB/12V step-down) → rewire with thicker pair.
- If power fine: add `log level verbose events` trace around suspected handlers.

### CCS charge type not detected

During CCS DC fast charge, bits [3:2] of `0x0594` d[5] = 0x00 (no connector type).
`v.c.type` stays undefined. CCS indicator elsewhere — d[3] and d[5] now logged VERBOSE for next CCS session.

**Also noted:** `0x0594` default case wrote `"undefined"` to metric every idle frame, overwriting prev type. Fixed to `break`.

**Resolve:** capture verbose log during CCS charging, find `d[3]=xx d[5]=xx` differing from baseline (`d[3]=03 d[5]=00`).

### Climate on battery / min SoC — BAP write pending RE

`cc-onbat` and `cc-minsoc` = persistent ECU config in e-Manager (via MQB head unit). Separate BAP writes, not part of 3-frame start/stop. Carnet app discontinued — no capture. Head unit still works.

**Resolve:** change in head unit e-Manager while capturing KCAN. Diff vs idle for BAP port + payload.

### Lock / unlock auth bytes (blocks lock PR)

`CommandLock` / `CommandUnlock` set action bits in `0x5A7` but gateway needs auth bytes too. Without them, command ignored.

**Resolve:** capture key fob while OVMS monitors. Diff `0x5A7` vs idle to ID auth pattern.

### GPS sign bits — S/W hemisphere unconfirmed

Sign bits inferred: bit 55 (`d[6]` MSB) = lat, bit 56 (`d[7]` bit 0) = lon. Confirmed 0 for N/E (Norway). Firmware applies; `vwegolf.dbc` marks inferred.

**Resolve:** capture from S or W hemisphere.

### Charging current frame not identified

`ms_v_charge_current` and `ms_v_charge_power` never set. `0x0191` motor/inverter only (zero during charging). OBC current frame unknown.

**Resolve:** Capture 8. Filter frames only active when `ChargeInProgress` in `0x0594` set.

### UDS diagnostic access via can1 — not working yet

Can1 (OBD port on J533) receives traffic but no UDS responses (tested 2026-04-09 during AC charging). Bus speed likely 1 Mbps, though 500 kbps also received frames — needs confirmation.

Tested at 500 kbps:
- Standard ISO-TP to BMS (7E5/7ED), motor (7E0/7E8), charger (744/7AE): timeout
- VW TP2.0 (`-v` flag) to BMS: timeout
- OBD2 broadcast (7DF): timeout
- Gateway diagnostic session (200/200): timeout

**Hypotheses:**
1. Wrong bitrate — 1 Mbps may be required for UDS transmit
2. J533 only routes UDS with ignition on (KL15 active), not during charging
3. OBD port CAN on J533 requires different protocol/session setup

**Resolve:** retry UDS at 1 Mbps, ignition on, laptop for live debug (Capture 11). e-Up uses ISOTP_STD on bus 1 with same ECU addresses — if works with ignition on, gateway routing confirmed.

### Direct charge start/stop command — unknown

Charging managed by OBC via e-Manager. Controls:
- **Min SoC**: charges to this level when plugged in (no schedule)
- **Schedule slots**: 3 departure-time profiles with max SoC and location
- **Direct start/stop**: unknown if CAN command exists

Others built charge stop buttons on CAN but unpublished protocol. If no direct command: write min SoC via BAP (set high to charge, set to current SoC to stop).

**Resolve:** Capture 13 — charge start/stop investigation.

---

## Pending captures

| ID | Bus | Goal | Status |
|---|---|---|---|
| 8 | can3 | OBC charge current during AC charging — filter `ChargeInProgress` in `0x0594` | partial (2 captures 2026-04-09) |
| 9 | can3 | Lock/unlock auth bytes — fob while OVMS monitors, diff `0x5A7` vs idle | pending |
| 10 | can3 | Min SoC BAP frame — change in head unit e-Manager, diff vs idle | pending |
| 11 | can1 | UDS with ignition on — retry all ECU addresses at 500k + 1 Mbps | pending |
| 13 | can3 | Charge start/stop — min SoC 0%→100% and back, diff vs idle | pending |
| 14 | can3 | OBC 29-bit frames during AC charging — focus on `0x1A5554A8` b4/all bytes vs `0x594` state | pending |
| 17d | can2 | Sustained regen coast-down — disambiguate torque vs power for `0x0B4` (b3,b4) | pending |
| 17e | can2 | WOT accel — confirm MotorTorqueB on forward load, check sign byte | pending |

---

## Resolved captures (summary)

- ~~Cap 5~~ — cabin temp not broadcast in remote mode (ignition-off ECU sleep). `v.e.cabintemp` stale during remote clima.
- ~~Cap 6~~ — natural stop: port 0x12 `05`→`00`. `v.e.hvac` bug: drive from port 0x12, not `0x5EA` remote_mode.
- ~~Cap 7~~ — BAP port 0x19: `raw = (celsius - 10) * 10`. Bug: read config in `CommandClimateControl`, not Ticker10 cache.
- ~~Cap 12~~ — schedule-triggered: different BAP signaling, same port 0x12 stop detection.
- ~~Cap 15~~ — gear: `0x187` authoritative. `0x0B3` = drive-armed latch. See `.md`.
- ~~Cap 16~~ — pedal: `0x365` b4b0 = brake pressure, `0x3BE` b3 = brake switch, `0x3BE` b5b0 = throttle. DBC stubs added. Need re-confirm.
- ~~Cap 17/17c~~ — drive: `0x0B2` = wheel speeds (1/128 km/h/bit). `0x0B4` b3:4/b6:7 = LE16 motor torque pairs. `0x057` b3 = torque request. DBC updated.

---

## FCAN decode captures — `investigation` branch

Idle baseline: `can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-175611.crtd` (CCS idle, 26127 frames). Per capture: 5–10 s pre/post idle, log stimulus + timestamps in `.md`.

### Capture 14 — Type 2 AC charging cycle

**Goal:** isolate OBC/charge frames active only during AC charging. Complements Capture 8 (KCAN side). Primary target: `v.c.current` / `v.c.power` (charge rate gap in app).

**Sequence:** park, ignition off, **can3** capture running (KCAN — OBC/BAP live here). 5 s idle → plug Type 2 → 60+ s charging → unplug → 5 s idle. Stop. Note: earlier can2 variant useless — OBC on KCAN, not FCAN.

**Look for:** byte deltas inside idle-present frames (Apr 7 audit: **0 charge-only 11-bit IDs** — OBC hidden in existing frames or 29-bit extended). Top target: `0x1A5554A8` (29-bit, see audit).

**Key finding (Apr 7 audit):** zero charge-only 11-bit IDs. OBC data in 29-bit extended frames. Primary target: `0x1A5554A8` — silent during cable-full pause, b4 = 1 Hz charge-energy increment. OVMS needs extended-ID frame hook to decode it. Audit scripts in `tests/analysis/scratch/apr7_*.py`.

### ~~Cap 15~~ — gear stimulation done. `0x187` authoritative. `0x0B3` = drive-armed latch. `0x0B4` b3/4 = torque candidate (D/B only). See `.md`.

### ~~Cap 16~~ — pedal stimulation done. `0x057` eliminated (motor angle). Candidates: `0x365` b4b0 = brake pressure, `0x3BE` b3b4/b6 = brake switch, `0x3BE` b5b0 = throttle active. In DBC as stubs. Need timestamped re-confirm before wiring to metrics.

### ~~Cap 17 / 17c~~ — drive captures done. `0x0B2` = wheel speeds confirmed (1/128 km/h/bit, 4×LE16). `0x0B4` (b3:4, b6:7) = LE16 motor torque pairs (general, not regen-only). `0x0B4` b2 = regen intensity byte. `0x057` b3 = motor torque request (linear with speed). DBC updated. See `.md`.

**Pending — Captures 17d / 17e:**
- 17d: sustained regen coast-down — disambiguate torque vs power for `0x0B4` (b3,b4)
- 17e: WOT accel — confirm MotorTorqueB on forward load, check for sign byte

---

## Roadmap

### Phase 1 — Climate control (in progress)

- [x] BAP frame construction and wake sequence
- [x] Native test coverage (36 clima tests)
- [ ] Confirm on car (flash `feat/vwegolf-climate-control`, Captures 5-7, 12)
- [ ] Submit upstream PR (after PR 1 merged)

### Phase 2 — Charge control

- [ ] ID charge start/stop mechanism (Capture 13)
- [ ] ID OBC charge current frame (Capture 8)
- [ ] ID min SoC BAP write (Capture 10)
- [ ] Implement charge current decode
- [ ] Implement charge control command

### Phase 3 — UDS / battery health / ABRP

- [ ] Get UDS working on can1 (Capture 11)
- [ ] BMS SoH + CAC polling (e-Up PID 0x74CB on ECU 8C)
- [ ] Cell voltage polling (e-Up PIDs 0x1E40–0x1EA5)
- [ ] Cell temperature polling (e-Up PIDs 0x1EAE–0x1EBD)
- [ ] Charger AC/DC voltage + current (e-Up PIDs on ECU C6)
- [ ] ABRP integration (needs SoC, speed, position, bat temp, SoH — most already done)

### Phase 4 — Polish

- [ ] DTC monitoring (standard UDS service 0x19)
- [ ] TPMS (frame not yet identified)
- [ ] Lock/unlock (needs auth byte capture)
- [ ] Complete user guide (`docs/index.rst`)

---

## Metrics — 53 / 85 (62%)

✓=confirmed · ~=active-only · ✗=not decoded

**Battery (12/20):** soc✓(0x131) temp✓(0x59E) capacity✓(0x5CA) range.est/ideal✓(0x5F5) energy.recd/used✓(0x2AF) current/voltage/power~(0x191) 12v.voltage/ref✓(ADC) — ✗ soh/cac(UDS P3) range.full c.voltage c.temp p.voltage coulomb

**Charging (9/14):** charging/timermode/type/duration.full/time/state/substate/voltage✓(0x594) cp✓ — ✗ current(cand:0x1A5554A8 Cap14) power(blocked) kwh(cand:0x1A5554A8 b4) limit.soc(Cap10) mode

**Environment (11/18):** on✓ locked✓(0x583) gear/drivemode✓(0x187) parktime✓(0x6B7) drivetime✓ cabinsetpoint✓(0x594) hvac~(0x5EA) cabintemp~(0x5EA/0x66E ignition-on only) regenbrake~(0x0B4 b3:4/b6:7 partial) throttle~(0x057 b3 partial) — ✗ temp(not broadcast remote) heating cooling cabinfan handbrake headlights footbrake

**Doors (7/7):** fl/fr/rl/rr✓(0x583) trunk/hood✓(0x65A) cp✓(0x594)

**Position (13/15):** speed✓(0xFD) lat/lon✓(0x486 sign inferred) odometer✓(0x6B7) gps*✓(hardware) location✓(framework) — ✗ trip acceleration

**Identity (2/2) · Motor/inverter (0/5 UDS P3) · TPMS (0/4 frame unknown) · v.g.* N/A**

---

## High-value gaps

1. **Charge start/stop** — direct command or min SoC write — Captures 10, 13
2. `v.c.current` / `v.c.power` — OBC charge current — Capture 8
3. UDS on can1 — unlocks SoH, cell data, ABRP — Capture 11
4. `v.b.soh` — state of health via UDS — Phase 3
5. `v.t.pressure` — TPMS alerts — frame not yet identified

