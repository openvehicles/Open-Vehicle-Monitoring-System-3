# VW e-Golf OVMS — Tracker

Living doc. Update as resolved.
Signals → `vehicle/OVMS.V3/components/vehicle_vwegolf/docs/vwegolf.dbc`. Protocol → `…/docs/clima-control-bap.md`.

**Platform:** flash limit resolved upstream 2026-04-17. New layout (v3-35): 2×7 MB OTA + ~2 MB `/store`. Module on new scheme — no size constraint.

---

## Upstream PR queue

All work on `vwegolf` branch. Decompose for upstream as separate PRs.

| # | PR | Status | Blocker |
|---|---|---|---|
| 1 | Decode-only baseline (`IncomingFrameCan2/3` decodes, no writes) | **merged #1369** | — |
| 2 | Bugfixes (0x594/0x2AF/0x6B7) | **merged in #1369** | — |
| 3 | `CommandWakeup` + OCU heartbeat (NM wake, 5 Hz keepalive) | bundled into PR-CLIMA draft | — |
| 4 | `CommandClimateControl` (BAP start/stop) | bundled into PR-CLIMA draft, on-car testing | car verify (caps 18/19) |
| 5 | OCU heartbeat one-shot (horn/indicator/panic/mirror) | ready | PR-CLIMA merged |
| 6 | `CommandLock`/`CommandUnlock` | blocked | auth bytes uncaptured (cap 9) |
| 7 | User guide `docs/index.rst` (maintainer-required) | not started | — |

PR-CLIMA = `PR-CLIMA.md` draft = wake+heartbeat+clima bundled (PRs 3+4 in queue).

Each PR: pass native tests · ref RE notes in `…/vehicle_vwegolf/docs/` · EN strings · single tag `v-vwegolf` · NO metric defaults in ctor.

**Pre-ship verify (all PRs):** exercise every use case on real car before submit. Clima specifically: cold sleep, ign-off twilight 3–10 s, ign-off >15 s (OCU ring-down done), charging, locked, unlocked-door bus-awake. Document timing constraints in user guide.

---

## Open items

### OVMS module crashes — intermittent

Reboots: `can log start/stop` repeated · idle no-input · cap 15 ended early.

**Last diag** (`module check` after 2026-04-12 18:51:35 CEST, 3rd reset):

```
Boot:      Crash (12/12)  Reset: Task watchdog (6)
Last:      abort() core 0
core 0:    OVMS CanRx, 1992 stack free
core 1:    IDLE1, 520 stack free
BT:        0x4008dd2e 0x4008dfc9 0x4010cbac 0x4008418e
Event:     vehicle.gear.reverse@ovms-server-v3 60s
WDT task:  OVMS Events
```

WDT = Events task (not CanRx). Events starved → TWDT. CanRx ran on abort.

**Suspects:**

1. **can2 FCAN ISR storm — top.** Cap `can3-20260405-091701`: clima active → can2 840k intr/977s (~860/s), `errflags=0x22401c02`, `txpkt=0`. J533 wakes FCAN during clima → MCP2515 floods → CAN ISR starves events → TWDT. Reproduces under normal clima.
2. **Power — AWG23 harness.** WiFi TX peak → V drop → brownout → WDT. Test engine-running (14 V).
3. **Slow handler.** Cap 16: `ticker.10 took 2460 ms` (ABRP). Events starves.
4. **`canlog_tcpserver` start/stop race.** Crashes correlate w/ log start/stop.

**Next:**

- Reproduce w/ `log monitor on` + 2nd SSH — live panic BT, not just `module check` summary.
- Decode BT: `xtensa-esp32-elf-addr2line -e build/ovms3.elf 0x4008dd2e 0x4008dfc9 0x4010cbac 0x4008418e`
- Measure supply at OVMS connector during `can log start` (DMM/scope). Below ESP32 brownout (~2.43 V at 3.3 V rail / ~4 V on USB-12V step-down) → rewire thicker.
- If power OK: `log level verbose events` around suspect handlers.

### CCS charge type not detected

CCS DC FC: `0x0594` d[5] bits[3:2] = 0x00 (no connector). `v.c.type` undefined. CCS indicator elsewhere — d[3]/d[5] now VERBOSE for next CCS session.

**Also:** `0x0594` default case wrote `"undefined"` every idle frame, overwriting prev type. Fixed → `break`.

**Resolve:** verbose log during CCS, find `d[3]=xx d[5]=xx` ≠ baseline (`d[3]=03 d[5]=00`).

### Climate on-bat / min-SoC — BAP write pending RE

`cc-onbat`/`cc-minsoc` = persistent ECU cfg in e-Manager (via MQB head unit). Separate BAP writes, not in 3-frame start/stop. Carnet discontinued — no capture. Head unit still works.

**Resolve:** change in head unit e-Manager while capturing KCAN. Diff vs idle for BAP port + payload.

### Lock/unlock auth bytes (blocks lock PR)

`CommandLock`/`Unlock` set action bits in `0x5A7` but gateway needs auth too. Without → ignored.

**Resolve:** capture key fob while OVMS monitors. Diff `0x5A7` vs idle.

### GPS sign bits — S/W hemisphere unconfirmed

Inferred: bit 55 (`d[6]` MSB) = lat, bit 56 (`d[7]` bit 0) = lon. Confirmed 0 = N/E (Norway). FW applies; DBC marks inferred.

**Resolve:** capture from S or W.

### Charging current frame unknown

`ms_v_charge_current`/`_power` never set. `0x0191` = motor/inverter only (zero charging). OBC current frame TBD.

**Resolve:** Cap 8. Filter frames active only when `ChargeInProgress` in `0x0594` set.

### UDS via can1 — not working

Can1 (OBD on J533) RX traffic but no UDS resp (tested 2026-04-09 AC charging). Bus likely 1 Mbps; 500k also RX'd frames — confirm.

500k tested:
- ISO-TP std → BMS (7E5/7ED), motor (7E0/7E8), charger (744/7AE): timeout
- VW TP2.0 (`-v`) → BMS: timeout
- OBD2 broadcast (7DF): timeout
- Gateway diag session (200/200): timeout

**Hyp:**
1. Wrong bitrate — 1 Mbps for TX
2. J533 routes UDS only w/ ign-on (KL15), not charging
3. OBD CAN on J533 needs different proto/session

**Resolve:** retry UDS @ 1 Mbps, ign-on, laptop live debug (Cap 11). e-Up uses ISOTP_STD bus 1 same ECU addrs — works ign-on → confirms gateway routing.

### Charge start/stop — unknown

OBC managed by e-Manager. Ctrl:
- **Min SoC** — charges to lvl when plugged (no schedule)
- **Schedule slots** — 3 dep-time profiles w/ max SoC + location
- **Direct start/stop** — unknown if CAN cmd exists

Others built CAN charge stop but unpublished. Fallback: write min SoC via BAP (high → charge, current SoC → stop).

**Resolve:** Cap 13.

---

## Captures

Index → `vehicle/OVMS.V3/components/vehicle_vwegolf/docs/captures.tsv` (id · bus · status · finding).
Per-capture detail → `vehicle/OVMS.V3/components/vehicle_vwegolf/tests/candumps/*.md` (date in filename matches notes).

---

## Roadmap

### Phase 1 — Climate (in progress)

- [x] BAP frame construction + wake seq
- [x] Native test cov (36 clima tests)
- [x] On-car capture verify (caps 18/19, 20260422)
- [ ] Submit upstream PR-CLIMA (analysis of 20260422 caps → fix issues → submit)

### Phase 2 — Charge ctrl

- [ ] ID charge start/stop mech (Cap 13)
- [ ] ID OBC charge current frame (Cap 8)
- [ ] ID min SoC BAP write (Cap 10)
- [ ] Implement charge current decode
- [ ] Implement charge ctrl cmd

### Phase 3 — UDS / battery health / ABRP

- [ ] UDS on can1 (Cap 11)
- [ ] BMS SoH + CAC poll (e-Up PID 0x74CB ECU 8C)
- [ ] Cell V poll (e-Up PIDs 0x1E40–0x1EA5)
- [ ] Cell T poll (e-Up PIDs 0x1EAE–0x1EBD)
- [ ] Charger AC/DC V+I (e-Up PIDs ECU C6)
- [ ] ABRP integ (needs SoC, speed, pos, bat T, SoH — most done)

### Phase 4 — Polish

- [ ] DTC monitor (UDS svc 0x19)
- [ ] TPMS (frame TBD)
- [ ] Lock/unlock (auth bytes)
- [ ] Complete user guide `docs/index.rst`
  - Document **"wait ~15 s after ign-off before remote clima"** (OEM OCU NM ring-down window — 0x5A7 conflict causes arbitration loss → bus-off ~110 ms/cycle). **Verify at impl wrap-up** before shipping PR — must exercise all use cases (cold sleep, ign-off twilight 3–10 s, ign-off >15 s, charging, locked, unlocked-door bus-awake) and confirm timing.

---

## Metrics — 53/85 (62%)

✓=confirmed · ~=active-only · ✗=not decoded

**Battery (12/20):** soc✓(0x131) temp✓(0x59E) cap✓(0x5CA) range.est/ideal✓(0x5F5) energy.recd/used✓(0x2AF) current/voltage/power~(0x191) 12v.voltage/ref✓(ADC) — ✗ soh/cac(UDS P3) range.full c.voltage c.temp p.voltage coulomb

**Charging (9/14):** charging/timermode/type/duration.full/time/state/substate/voltage✓(0x594) cp✓ — ✗ current(cand:0x1A5554A8 Cap14) power(blocked) kwh(cand:0x1A5554A8 b4) limit.soc(Cap10) mode

**Environment (11/18):** on✓ locked✓(0x583) gear/drivemode✓(0x187) parktime✓(0x6B7) drivetime✓ cabinsetpoint✓(0x594) hvac~(0x5EA) cabintemp~(0x5EA/0x66E ign-on only) regenbrake~(0x0B4 b3:4/b6:7 partial) throttle~(0x057 b3 partial) — ✗ temp(not broadcast remote) heating cooling cabinfan handbrake headlights footbrake

**Doors (7/7):** fl/fr/rl/rr✓(0x583) trunk/hood✓(0x65A) cp✓(0x594)

**Position (13/15):** speed✓(0xFD) lat/lon✓(0x486 sign inferred) odometer✓(0x6B7) gps*✓(HW) location✓(framework) — ✗ trip acceleration

**Identity (2/2) · Motor/inverter (0/5 UDS P3) · TPMS (0/4 frame TBD) · v.g.* N/A**

---

## High-value gaps

1. **Charge start/stop** — direct cmd or min SoC write — Caps 10, 13
2. `v.c.current`/`v.c.power` — OBC charge current — Cap 8
3. UDS on can1 — unlocks SoH, cell data, ABRP — Cap 11
4. `v.b.soh` — SoH via UDS — Phase 3
5. `v.t.pressure` — TPMS — frame TBD
