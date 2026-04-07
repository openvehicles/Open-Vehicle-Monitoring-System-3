# VW e-Golf OVMS — Project Tracker

Living document. Update as items are resolved or new ones are found.
Signal definitions → `docs/vwegolf.dbc`. Protocol details → `docs/clima-control-bap.md`.

---

## Upstream PR queue

Break the `climate-control` branch into small focused PRs in this order (each depends on the previous being merged):

| # | PR | Status | Blocker |
|---|---|---|---|
| 1 | Decode-only baseline — all `IncomingFrameCan3`/`Can2` metric decodes, no bus writes | ready | — |
| 2 | `CommandWakeup` — KCAN NM wake sequence | ready | PR 1 merged |
| 3 | `CommandClimateControl` — clima start/stop via BAP | blocked | port 0x19 temp encoding (see below) |
| 4 | OCU heartbeat one-shot actions — horn, indicators, panic, mirror fold | ready | PR 2 merged |
| 5 | `CommandLock` / `CommandUnlock` | blocked | auth bytes not captured (see below) |
| 6 | User guide (`docs/index.rst`) — required by maintainer before any PR merges | not started | — |

Each PR must: pass native test suite · reference relevant `docs/` RE notes · English user-facing strings only · single log tag `v-vwegolf` · no metric defaults in constructor.

---

## Open items

### ~~Startup sentinel filters missing~~ — fixed

All startup sentinel filters implemented and tested:

| Frame | Sentinel check | Fixed |
|---|---|---|
| `0x0191` | `d[2]==0xFF` → ~2047A / ~1023V | ✓ |
| `0x059E` | `d[2]==0xFE` → 87°C bat temp | ✓ |
| `0x05CA` | `d[2]==0xFF` → ~102 kWh capacity | ✓ |
| `0x05EA` | `u16>=1020` → 62+°C cabin temp | ✓ (prior) |
| `0x06B5` | `d[6]==0xFE` → max-range solar/air | ✓ |

### CCS charge type not detected

During active CCS DC fast charging, bits [3:2] of `0x0594` d[5] = 0x00 (no connector type).
`v.c.type` stays undefined. The CCS type indicator is elsewhere in the frame — d[3] and d[5]
raw bytes are now logged at VERBOSE to identify it on the next CCS session.

**Also noted:** `0x0594` default case previously wrote `"undefined"` string to the metric,
overwriting any previously-set type on every idle frame. Fixed to `break` instead.

**To resolve:** capture verbose log during active CCS charging and look for `d[3]=xx d[5]=xx`
values that differ from the non-charging baseline (`d[3]=03 d[5]=00`).

### ~~Odometer log bug~~ — fixed

`0x06B7` reused `u32` for park time after setting the odometer metric, then printed `u32`
(now park time) as `odo=5936 km` in the log. The metric itself was always set correctly.
Fixed by using a dedicated `odo` variable; log now prints the right value.

### Temperature encoding for BAP port 0x19 (blocks clima PR)

Frame 2 byte 6 is hardcoded to `0x20` (thomasakarlsen default). Port 0x16 (schedule) uses
`byte = celsius + 35` (20°C → `0x37`). Port 0x19 may differ — `0x20` decodes as 32°C under
that formula, which is wrong.

**To resolve:** two back-to-back `climatecontrol on` commands with different `xvg cc_temp`
values (e.g. 18°C and 22°C). Diff Frame 2 byte 6 between captures. If the ECU echoes the
setpoint in `0x17330110` port 0x1B, that confirms the encoding.

### Lock / unlock auth bytes (blocks lock PR)

`CommandLock` / `CommandUnlock` set the action bits in `0x5A7` but the gateway likely requires
additional authentication bytes alongside the action bit. Without them the command is ignored.

**To resolve:** capture the OEM app or key fob locking the car while OVMS is in monitor mode.
Diff the `0x5A7` frame against the idle keepalive to identify the auth pattern.

### GPS sign bits — S/W hemisphere unconfirmed

Sign bits inferred from bit layout: bit 55 (`d[6]` MSB) = lat sign, bit 56 (`d[7]` bit 0) = lon sign.
Confirmed 0 for known N/E location (Norway). Firmware applies them; `vwegolf.dbc` marks them
as inferred.

**To resolve:** any capture from a Southern or Western hemisphere location.

### `CommandWakeup` second frame necessity unclear

The `0x1B000067` NM presence frame is sent as part of `CommandWakeup` but it is not confirmed
whether it is actually required. A controlled test with only the first frame would clarify.

### `m_climate_on_battery` flag not sent

The "allow climatisation on battery" config (`xvg cc_on_battery`) is read but never included
in the BAP command frames. The bit/byte in the port 0x19 payload is not yet identified.

**To resolve:** capture a start command with the flag toggled. Diff against a standard start.

### Charging current frame not identified

`ms_v_charge_current` and `ms_v_charge_power` are never set. `0x0191` is motor/inverter only
(zero during charging). The OBC charge current lives on a different KCAN frame not yet found.

**To resolve:** dedicated charging session capture. Filter for frames only active when the
`ChargeInProgress` bit in `0x0594` is set. Candidates will be frames absent in parked-not-charging
baseline. Once identified, decode in `IncomingFrameCan3`, set `ms_v_charge_current` and
`ms_v_charge_power`, add signal to `vwegolf.dbc`.

---

## Remaining captures needed

### Capture 5 — Cabin temp in remote mode

**Goal:** identify cabin temp source when car is parked and clima is running via OVMS with
ignition off. In ignition-on mode it comes from `0x17330110` node 0x01 port 0x1B. May be
the same frame remotely or may differ.

**Sequence:** `climatecontrol on` → wait for blower → capture KCAN 2–3 min. Note actual
cabin temp with a thermometer for ground truth.

**What to look for:** any frame with a value that warms from ambient toward setpoint over
5–10 min; whether `0x17330110` port 0x1B is present and tracking when ignition is off.

### Capture 6 — Natural timer expiry

**Goal:** see what the ECU sends when the 10-minute clima timer expires on its own.

**Sequence:** start clima, wait for auto-stop.

**What to look for:** port 0x1a header byte reverting from `c2` to `c0`; final stop
notification; whether status payloads (`00 00 04`, `02 00 04`) change.

### Capture 7 — Port 0x19 temperature encoding

See open item above. Needed to unblock the clima PR.

### Capture 8 — Charging current (Type 2 AC only)

See open item above. Must be **Type 2 AC** — CCS DC fast charging keeps KCAN completely
silent (no frames visible to OVMS during the session). AC charging goes through the
internal OBC which does appear on KCAN.

### Capture 9 — Lock / unlock auth bytes

See open item above.

### Capture 10 — Charge SoC limit

**Goal:** identify the BAP frame used to read/write the charge SoC limit (`v.c.limit.soc`).

**Sequence:** change the charge limit in the VW app (or car MMI) while OVMS monitors KCAN.
Diff against idle baseline to isolate the frame carrying the new limit value.

**What to look for:** a BAP frame on `0x17332501` or similar charge management CAN ID that
changes when the SoC limit is adjusted. Cross-reference with thomasakarlsen/e-golf-comfort-can
charge profile docs.

---

## Metrics support status

Legend: ✓ confirmed · ~ active-only (parked snapshot can't exercise) · ✗ not decoded · ? unverified

Overall: **49 / 85 (58%)**

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
| `v.b.soh` | ✗ | May be available via UDS on BMS ECU (0x7E7/0x7EF) |
| `v.b.cac` | ✗ | Not decoded |
| `v.b.range.full` | ✗ | Could compute from capacity × range/kWh |
| `v.b.c.voltage.*` | ✗ | Cell voltages — needs BMS UDS poll |
| `v.b.c.temp.*` | ✗ | Cell temps — needs BMS UDS poll |
| `v.b.p.voltage.*` | ✗ | Pack aggregates — not decoded |
| `v.b.coulomb.*` | ✗ | Coulomb counters — not decoded |

### Charging (7 / 14)

| Metric | Status | Source |
|---|---|---|
| `v.c.charging` | ✓ | 0x594 |
| `v.c.timermode` | ✓ | 0x594 |
| `v.c.type` | ✓ | 0x594 |
| `v.c.duration.full` | ✓ | 0x594 |
| `v.c.time` | ✓ | framework |
| `v.c.state` | ~ | 0x594 ChargeInProgress transition |
| `v.c.voltage` | ~ | mirrored from `v.b.voltage` during AC session |
| `v.c.current` | ✗ | OBC frame not yet identified — Capture 8 |
| `v.c.power` | ✗ | blocked on `v.c.current` |
| `v.c.kwh` | ✗ | Not decoded |
| `v.c.limit.soc` | ✗ | BAP frame not yet captured — Capture 10 |
| `v.c.mode` | ✗ | Not decoded |
| `v.c.timerstart` | ✗ | Schedule format partially known — see clima-control-bap.md |
| `v.d.cp` | ✗ | Charge port door — frame unknown |

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
| `v.e.temp` | ✗ | ECU sentinel suppressed; remote-mode source unknown — Capture 5 |
| `v.e.cabintemp` | ✗ | Same — Capture 5 |
| `v.e.heating` | ✗ | Derivable from hvac state + setpoint vs ambient |
| `v.e.cooling` | ✗ | Not decoded |
| `v.e.cabinfan` | ✗ | Not decoded |
| `v.e.handbrake` | ✗ | Frame unknown |
| `v.e.headlights` | ✗ | Frame unknown |
| `v.e.footbrake` | ✗ | Frame unknown |
| `v.e.regenbrake` | ✗ | Not decoded |
| `v.e.throttle` | ✗ | Not decoded |

### Doors & body (6 / 7)

| Metric | Status | Source |
|---|---|---|
| `v.d.fl` | ✓ | 0x583 |
| `v.d.fr` | ✓ | 0x583 |
| `v.d.rl` | ✓ | 0x583 |
| `v.d.rr` | ✓ | 0x583 |
| `v.d.trunk` | ✓ | 0x65A |
| `v.d.hood` | ✓ | 0x65A |
| `v.d.cp` | ✗ | Charge port door — see Charging above |

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

### Motor / inverter (0 / 5) — needs powertrain UDS polling

### TPMS (0 / 4) — frame not yet identified

### Generator (v.g.*) — not applicable, e-Golf has no V2G

---

## High-value gaps (worth implementing next)

1. `v.c.current` / `v.c.power` — OBC charge current — Capture 8
2. `v.e.cabintemp` / `v.e.temp` — remote-mode cabin/ambient temp — Capture 5
3. `v.e.heating` / `v.e.cooling` — derivable once cabintemp is known
4. `v.c.limit.soc` — BAP frame not yet captured; needs Capture 10
5. `v.b.soh` — state of health — probably accessible via UDS on BMS ECU
6. `v.t.pressure` — TPMS alerts — frame not yet identified
