# VW e-Golf OVMS — Project Tracker

Living doc. Update as resolved/found.
Signal defs → `docs/vwegolf.dbc`. Protocol details → `docs/clima-control-bap.md`.

---

## Upstream PR queue

Break `climate-control` branch into small focused PRs, this order (each depends on prev merged):

| # | PR | Status | Blocker |
|---|---|---|---|
| 1 | Decode-only baseline — all `IncomingFrameCan3`/`Can2` metric decodes, no bus writes | submitted (#1369) | under review |
| 2 | Bugfixes — 0x594 charge state, 0x2AF overflow, 0x6B7 parktime | submitted (fix branch) | PR 1 merged |
| 3 | `CommandWakeup` + OCU heartbeat — KCAN NM wake sequence, 5 Hz keepalive | ready | PR 1 merged |
| 4 | `CommandClimateControl` — clima start/stop via BAP | testing on car | PR 3 merged |
| 5 | OCU heartbeat one-shot actions — horn, indicators, panic, mirror fold | ready | PR 3 merged |
| 6 | `CommandLock` / `CommandUnlock` | blocked | auth bytes not captured (see below) |
| 7 | User guide (`docs/index.rst`) — required by maintainer for complete vehicle module | not started | — |

Each PR must: pass native test suite · ref relevant `docs/` RE notes · English user-facing strings only · single log tag `v-vwegolf` · no metric defaults in constructor.

---

## Open items

### ~~Startup sentinel filters missing~~ — fixed

All startup sentinel filters implemented/tested:

| Frame | Sentinel check | Fixed |
|---|---|---|
| `0x0191` | `d[2]==0xFF` → ~2047A / ~1023V | ✓ |
| `0x059E` | `d[2]==0xFE` → 87°C bat temp | ✓ |
| `0x05CA` | `d[2]==0xFF` → ~102 kWh capacity | ✓ |
| `0x05EA` | `u16>=1020` → 62+°C cabin temp | ✓ (prior) |
| `0x06B5` | `d[6]==0xFE` → max-range solar/air | ✓ |

### CCS charge type not detected

During CCS DC fast charge, bits [3:2] of `0x0594` d[5] = 0x00 (no connector type).
`v.c.type` stays undefined. CCS indicator elsewhere in frame — d[3] and d[5] raw bytes now logged VERBOSE to ID on next CCS session.

**Also noted:** `0x0594` default case previously wrote `"undefined"` to metric, overwriting prev-set type every idle frame. Fixed to `break`.

**Resolve:** capture verbose log during active CCS charging, look for `d[3]=xx d[5]=xx` values differing from non-charging baseline (`d[3]=03 d[5]=00`).

### ~~Odometer log bug~~ — fixed

### ~~CommandWakeup NM alive frame~~ — confirmed required

`0x1B000067` NM alive frame required for clima from deep sleep. Without it, clima ECU rejects BAP commands from unrecognised node. Confirmed by regression capture: no `1B000067` → no BAP ACK on `17332510`. See `clima-control-bap.md` Bug 2.

### ~~Temperature encoding for BAP port 0x19~~ — confirmed (Capture 7)

Encoding: `raw = (celsius - 10) * 10`. Confirmed on wire: 18°C → 0x50, 22°C → 0x78.

### Climate on battery / min SoC — BAP write pending RE

`cc-onbat` and `cc-minsoc` = persistent ECU config in VW e-Manager (set via MQB head unit alongside target temp and schedule slots). Separate BAP writes to clima ECU, not part of 3-frame start/stop command. Carnet app (originally sent these) discontinued — no capture available. Head unit can still set manually.

**Resolve:** change setting in head unit e-Manager while capturing KCAN. Diff against idle to ID BAP port and payload encoding.

### Lock / unlock auth bytes (blocks lock PR)

`CommandLock` / `CommandUnlock` set action bits in `0x5A7` but gateway likely requires additional auth bytes alongside action bit. Without them command ignored.

**Resolve:** capture key fob locking car while OVMS in monitor mode. Diff `0x5A7` frame against idle keepalive to ID auth pattern.

### GPS sign bits — S/W hemisphere unconfirmed

Sign bits inferred from layout: bit 55 (`d[6]` MSB) = lat sign, bit 56 (`d[7]` bit 0) = lon sign. Confirmed 0 for known N/E location (Norway). Firmware applies them; `vwegolf.dbc` marks as inferred.

**Resolve:** any capture from Southern or Western hemisphere location.

### Charging current frame not identified

`ms_v_charge_current` and `ms_v_charge_power` never set. `0x0191` is motor/inverter only (zero during charging). OBC charge current on different KCAN frame not yet found.

**Resolve:** dedicated charging session capture (Capture 8). Filter for frames only active when `ChargeInProgress` bit in `0x0594` set.

### UDS diagnostic access via can1 — not working yet

Can1 (OBD port CAN on J533 harness) receives traffic but no UDS responses (tested 2026-04-09 during active AC charging). Bus speed likely 1 Mbps (VW internal diagnostic CAN), though 500 kbps also received frames without errors — needs confirmation.

Tested at 500 kbps:
- Standard ISO-TP to BMS (7E5/7ED), motor (7E0/7E8), charger (744/7AE): timeout
- VW TP2.0 (`-v` flag) to BMS: timeout
- OBD2 broadcast (7DF): timeout
- Gateway diagnostic session (200/200): timeout

**Hypotheses:**
1. Wrong bitrate — 1 Mbps may be required for UDS transmit
2. J533 only routes UDS with ignition on (KL15 active), not during charging
3. OBD port CAN on J533 requires different protocol/session setup

**Resolve:** retry all UDS requests at 1 Mbps with ignition on and laptop for live debug (Capture 11). e-Up module uses ISOTP_STD on bus 1 with same ECU addresses — if those work with ignition on, gateway routing hypothesis confirmed.

### Direct charge start/stop command — unknown

VW e-Golf charging managed by OBC via e-Manager. Available controls:
- **Min SoC**: car charges to this level when plugged in (no schedule needed)
- **Schedule slots**: 3 departure-time profiles with max SoC and location
- **Direct start/stop**: unknown if CAN command exists

People built charge stop buttons on direct CAN interfaces but haven't published protocol. If no direct command exists, writing min SoC via BAP is charge control mechanism (set high to charge, set to current SoC to stop).

**Resolve:** Capture 13 — charge start/stop investigation.

---

## Capture quick-reference — `feat/vwegolf-climate-control`

| ID | Sequence to perform | Done |
|---|---|---|
| 5 | `climatecontrol on` → wait for blower → capture KCAN 2–3 min. Note cabin temp with thermometer. | ✓ 2026-04-10 |
| 6 | `climatecontrol on` → do nothing → wait for 10-min auto-stop → capture entire session. | ✓ 2026-04-10 |
| 7 | `climatecontrol on` at `xvg cc-temp 18` → stop → change to `xvg cc-temp 22` → `climatecontrol on` again. Compare Frame 2 byte 3. | ✓ 2026-04-10 |
| 8 | Plug in Type 2 AC charger → capture KCAN during active charging. Filter for frames only present when `ChargeInProgress` in 0x594 is set. | partial (2 captures taken 2026-04-09) |
| 12 | Set departure schedule 15 min ahead in head unit → capture KCAN from before schedule fires until clima stops. Compare against OVMS-triggered capture. | ✓ 2026-04-10 |

---

## Remaining captures needed

### ~~Capture 5 — Cabin temp in remote mode~~ — resolved: not available

Cabin temp **not broadcast** during remote clima (ignition off). All sources dead:

| Source | Result |
|---|---|
| `0x5EA` (clima ECU) | 2 frames in 304s, temp = sentinel (0x3FE), remote_mode = 0 |
| `0x66E` (interior sensor) | 2 frames, d[4] = 0xFE (not ready) |
| `0x17330110` port 0x1B | absent |

Bus extremely sparse in remote mode (~59 frames max for busiest ID over 304s). Sensor ECU sleeps with ignition off even though HVAC runs. `v.e.cabintemp` goes stale until ignition on. Ground truth: cabin rose 7→9°C per thermometer but no CAN frame reflected this.

Capture file: `can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-223732.crtd`

### ~~Capture 6 — Natural timer expiry~~ — resolved

Clima ran 15.2 min then auto-stopped. Stop indicators on `0x17332510`:

| Signal | Active | Stopped |
|---|---|---|
| Port 0x12 payload[0] | `0x05` | `0x00` |
| Port 0x19 payload[2] | `0x46` | `0x40` (bits 2:1 cleared) |
| Port 0x18 | `49 58 01` (ON) | no explicit OFF from ECU |

Port 0x12 broadcasts `05` for first ~3 min then goes silent. At stop: single `00` frame + big status dump (79-byte port 0x01). Port 0x1A goes `c0→c2` at stop (opposite of original prediction). Keepalives on `17332501` start ~16s after stop at ~6s intervals.

**Bug found:** `v.e.hvac` never resets after stop — driven by `0x5EA` remote_mode which doesn't work in remote mode (Capture 5). Fix: decode port 0x12 on `0x17332510` for authoritative hvac state.

Capture file: `can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-224459.crtd`

### ~~Capture 7 — Port 0x19 temperature encoding confirmation~~ — resolved

Encoding **confirmed on the wire**: `raw = (celsius - 10) * 10`.
- 18°C → 0x50 ✓ (Frame 2 byte 3)
- 22°C → 0x78 ✓ (Frame 2 byte 3)

**Bug found:** second `climatecontrol on` sent 18°C despite `config set xvg cc-temp 22` 4s earlier. `m_climate_temp` only updates in Ticker10 (every 10s). Fix: read config directly in `CommandClimateControl`.

Capture file: `can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-231401.crtd`

### Capture 8 — Charging current (Type 2 AC only)

**Goal:** find KCAN frame carrying OBC charge current during AC charging.

Must be **Type 2 AC** — CCS DC fast charging keeps KCAN completely silent. Filter for frames only present when `ChargeInProgress` in `0x0594` set.

### Capture 9 — Lock / unlock auth bytes

See open item above.

### Capture 10 — Charge SoC limit (min SoC)

**Goal:** ID BAP frame for read/write charge SoC limit.

**Sequence:** change min SoC in head unit e-Manager while capturing KCAN. Diff against idle baseline.

**Look for:** BAP frame on `0x17332501` or similar changing when limit adjusted. Cross-ref thomasakarlsen/e-golf-comfort-can charge profile docs. Note: Carnet/We Connect defunct — must use head unit.

### Capture 11 — Can1 (OBD port CAN) with ignition on

**Goal:** determine if UDS diagnostic access works with ignition on.

**Sequence:** laptop on OVMS hotspot, ignition on, `can can1 start active 500000`. Capture can1 traffic to ID present frames. Retry UDS requests:
```
obdii can1 request device 7e0 7e8 22f446    (ambient temp, ECU 01)
obdii can1 request device 7e5 7ed 2274cb    (SoH/CAC, BMS ECU 8C)
obdii can1 request device 744 7ae 221da9    (charger AC voltage, ECU C6)
obdii can1 request device -v 7e5 7ed 2274cb (same with VW TP2.0)
```

**Look for:** any positive UDS response. If standard ISO-TP works, e-Up PID list (`vweup_obd.h`) usable almost directly. If only VW TP2.0 works, need `-v` flag and potentially different addressing.

### ~~Capture 12 — Schedule-triggered clima start~~ — resolved

Schedule-triggered clima uses different BAP signaling than OVMS-triggered:

| Signal | OVMS-triggered (Cap 6) | Schedule-triggered (Cap 12) |
|---|---|---|
| Port 0x18 trigger (`49 58 01`) | yes | absent |
| Port 0x19 ACK | yes | absent |
| Port 0x1A burst at stop | 6 frames | 1 frame at startup only |
| 17332501 keepalives after stop | 23 frames ~6s | 5 frames total |

**Same across both modes** (universal stop detection):
- Port 0x12 payload[0]: `05`=active, `00`=idle
- Port 0x13: `04 04`=active, `04 00`=idle
- Big port 0x01 status dump (79 bytes) at stop
- 1s status cycle runs continuously
- Clima duration: ~15 min both modes

**Note:** CRTD frames prefixed `2R29` despite can3 capture — J533 gateway routing artifact. BAP data valid.

**Bug confirmed:** `v.e.hvac` remains true after schedule-triggered stop too. Fix: drive hvac from port 0x12 on `0x17332510`.

Capture file: `can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-233616.crtd`

### Capture 13 — Charge start/stop command

**Goal:** determine if direct CAN command exists to start/stop AC charging.

**Sequence:** plug in car, capture KCAN. Change min SoC in head unit from 0% to 100% (should trigger charge start) and back (should stop). Look for command frames vs just SoC limit change. Also capture moment OBC naturally stops at target SoC to see "charge complete" sequence in reverse.

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

## Metrics support status

Legend: ✓ confirmed · ~ active-only · ✗ not decoded · ? unverified

Overall: **51 / 85 (60%)**

### Battery & power (12 / 20)

| Metric | Status | Source |
|---|---|---|
| `v.b.soc` | ✓ | 0x131 |
| `v.b.temp` | ✓ | 0x59E |
| `v.b.capacity` | ✓ | 0x5CA |
| `v.b.range.est` | ✓ | 0x5F5 |
| `v.b.range.ideal` | ✓ | 0x5F5 |
| `v.b.energy.recd` | ✓ | 0x2AF |
| `v.b.energy.used` | ✓ | 0x2AF |
| `v.b.current` | ~ | 0x191 |
| `v.b.voltage` | ~ | 0x191 |
| `v.b.power` | ~ | 0x191 |
| `v.b.12v.voltage` | ✓ | OVMS ADC |
| `v.b.12v.voltage.ref` | ✓ | OVMS ADC |
| `v.b.consumption` | ? | Shows 0 — likely framework default, not decoded |
| `v.b.soh` | ✗ | Needs UDS on BMS ECU 8C — Phase 3 |
| `v.b.cac` | ✗ | Needs UDS on BMS ECU 8C — Phase 3 |
| `v.b.range.full` | ✗ | Could compute from capacity × range/kWh |
| `v.b.c.voltage.*` | ✗ | Cell voltages — needs UDS Phase 3 |
| `v.b.c.temp.*` | ✗ | Cell temps — needs UDS Phase 3 |
| `v.b.p.voltage.*` | ✗ | Pack aggregates — not decoded |
| `v.b.coulomb.*` | ✗ | Coulomb counters — not decoded |

### Charging (9 / 14)

| Metric | Status | Source |
|---|---|---|
| `v.c.charging` | ✓ | 0x594 |
| `v.c.timermode` | ✓ | 0x594 |
| `v.c.type` | ✓ | 0x594 (AC Type 2 confirmed; CCS not detected) |
| `v.c.duration.full` | ✓ | 0x594 |
| `v.c.time` | ✓ | framework |
| `v.c.state` | ✓ | 0x594 charge state transitions |
| `v.c.substate` | ✓ | 0x594 |
| `v.c.voltage` | ✓ | mirrored from `v.b.voltage` during AC session |
| `v.d.cp` | ✓ | 0x594 charge port door |
| `v.c.current` | ✗ | OBC frame not yet identified — Capture 8 |
| `v.c.power` | ✗ | blocked on `v.c.current` |
| `v.c.kwh` | ✗ | Not decoded |
| `v.c.limit.soc` | ✗ | BAP frame not yet captured — Capture 10 |
| `v.c.mode` | ✗ | Not decoded |

### Environment & climate (9 / 18)

| Metric | Status | Source |
|---|---|---|
| `v.e.on` | ✓ | framework |
| `v.e.locked` | ✓ | 0x583 |
| `v.e.gear` | ✓ | 0x187 |
| `v.e.drivemode` | ✓ | 0x187 |
| `v.e.parktime` | ✓ | 0x6B7 (encoding not fully confirmed) |
| `v.e.drivetime` | ✓ | framework |
| `v.e.cabinsetpoint` | ✓ | 0x594 |
| `v.e.hvac` | ~ | 0x05EA remote_mode field; 3=active, 0=idle |
| `v.e.temp` | ✗ | Not broadcast in remote mode (Capture 5 confirmed) |
| `v.e.cabintemp` | ~ | 0x5EA/0x66E ignition-on only; stale during remote clima (Capture 5) |
| `v.e.heating` | ✗ | Derivable from hvac state + setpoint vs ambient |
| `v.e.cooling` | ✗ | Not decoded |
| `v.e.cabinfan` | ✗ | Not decoded |
| `v.e.handbrake` | ✗ | Frame unknown |
| `v.e.headlights` | ✗ | Frame unknown |
| `v.e.footbrake` | ✗ | Frame unknown |
| `v.e.regenbrake` | ✗ | Not decoded |
| `v.e.throttle` | ✗ | Not decoded |

### Doors & body (7 / 7) — complete

| Metric | Status | Source |
|---|---|---|
| `v.d.fl` | ✓ | 0x583 |
| `v.d.fr` | ✓ | 0x583 |
| `v.d.rl` | ✓ | 0x583 |
| `v.d.rr` | ✓ | 0x583 |
| `v.d.trunk` | ✓ | 0x65A |
| `v.d.hood` | ✓ | 0x65A |
| `v.d.cp` | ✓ | 0x594 |

### Position (13 / 15)

| Metric | Status | Source |
|---|---|---|
| `v.p.speed` | ✓ | 0xFD |
| `v.p.latitude` | ✓ | 0x486; sign bit inferred, needs S hemisphere confirm |
| `v.p.longitude` | ✓ | 0x486; sign bit inferred, needs W hemisphere confirm |
| `v.p.odometer` | ✓ | 0x6B7 |
| `v.p.gpslock` | ✓ | GPS hardware |
| `v.p.altitude` | ✓ | GPS hardware |
| `v.p.direction` | ✓ | GPS hardware |
| `v.p.gpshdop` | ✓ | GPS hardware |
| `v.p.gpsmode` | ✓ | GPS hardware |
| `v.p.gpsspeed` | ✓ | GPS hardware |
| `v.p.gpssq` | ✓ | GPS hardware |
| `v.p.satcount` | ✓ | GPS hardware |
| `v.p.location` | ✓ | framework lookup |
| `v.p.trip` | ✗ | No trip odometer source identified |
| `v.p.acceleration` | ✗ | Not decoded |

### Identity (2 / 2) — complete

### Motor / inverter (0 / 5) — needs UDS Phase 3

### TPMS (0 / 4) — frame not yet identified

### Generator (v.g.*) — N/A, e-Golf has no V2G

---

## High-value gaps (worth implementing next)

1. **Charge start/stop** — direct command or min SoC write — Captures 10, 13
2. `v.c.current` / `v.c.power` — OBC charge current — Capture 8
3. UDS on can1 — unlocks SoH, cell data, ABRP — Capture 11
4. ~~`v.e.cabintemp` / `v.e.temp` — remote-mode cabin/ambient temp~~ — not available (Capture 5)
5. `v.b.soh` — state of health via UDS — Phase 3
6. `v.t.pressure` — TPMS alerts — frame not yet identified