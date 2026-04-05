# Metrics Support Status — VW e-Golf (VWEG)

Baseline: `metrics-master-parked.txt` snapshot, 2026-04-02, master branch, car parked/ignition off. Unique identifiers (VIN, coordinates, location name, odometer) are omitted from this file.
Investigation branch has the sentinel-value cabin temp fix applied on top.

Legend: ✓ confirmed working · ~ working when active (parked snapshot doesn't exercise it) · ! bug · ? unknown/unverified · – not applicable

Overall support: `[████████████░░░░░░░░]` 49 / 85 (58%)

---

## Battery & Power

| OVMS Metric | Status | Notes |
|---|---|---|
| `v.b.soc` | ✓ | 76.5% in snapshot — from 0x131 |
| `v.b.temp` | ✓ | 15°C in snapshot — from 0x59E |
| `v.b.capacity` | ✓ | 21.45 kWh in snapshot — from 0x5CA |
| `v.b.range.est` | ✓ | 149 km in snapshot — from 0x5F5 |
| `v.b.range.ideal` | ✓ | 200 km in snapshot — from 0x5F5 |
| `v.b.energy.recd` | ✓ | 0 kWh (parked, correct) — from 0x2AF |
| `v.b.energy.used` | ✓ | 0 kWh (parked, correct) — from 0x2AF |
| `v.b.current` | ~ | Empty when parked; decoded from 0x191 |
| `v.b.voltage` | ~ | Empty when parked; decoded from 0x191 |
| `v.b.power` | ~ | Empty when parked; decoded from 0x191 |
| `v.b.consumption` | ? | Shows 0 kWh/100km — likely framework default, not decoded |
| `v.b.12v.voltage` | ✓ | 13.16 V — OVMS hardware (ADC), not CAN |
| `v.b.12v.voltage.ref` | ✓ | 12.6 V — OVMS hardware (ADC), not CAN |
| `v.b.soh` | ✗ | Not decoded — may be available from BMS via UDS (0x7E7/0x7EF) |
| `v.b.cac` | ✗ | Not decoded |
| `v.b.range.full` | ✗ | Not decoded — could compute from capacity × range/kWh |
| `v.b.c.voltage.*` | ✗ | Cell voltages — not decoded; would need BMS UDS poll |
| `v.b.c.temp.*` | ✗ | Cell temperatures — not decoded; would need BMS UDS poll |
| `v.b.p.voltage.*` | ✗ | Pack voltage aggregates — not decoded |
| `v.b.p.temp.*` | ✗ | Pack temperature aggregates — not decoded |
| `v.b.coulomb.*` | ✗ | Coulomb counters — not decoded |

---

## Charging

| OVMS Metric | Status | Notes |
|---|---|---|
| `v.c.charging` | ✓ | no in snapshot — from 0x594 |
| `v.c.timermode` | ✓ | yes in snapshot — from 0x594 |
| `v.c.type` | ✓ | type2 in snapshot — from 0x594 (connector type) |
| `v.c.duration.full` | ✓ | 70 min in snapshot — from 0x594 |
| `v.c.time` | ✓ | 0 sec (not charging, correct) |
| `v.c.state` | ~ | "charging"/"stopped" — from 0x594 ChargeInProgress bit; transitions call NotifyChargeStart/Stop |
| `v.c.current` | ✗ | OBC charge current frame not yet identified on KCAN; 0x191 is motor-only (zero while charging) |
| `v.c.voltage` | ~ | Mirrored from `v.b.voltage` while charging (same HV bus during AC session) — from 0x594 transition |
| `v.c.power` | ✗ | Not decoded — blocked on v.c.current |
| `v.c.kwh` | ✗ | Not decoded |
| `v.c.limit.soc` | ✗ | Minimum charge limit — readable/writable via BAP profile 0x25 (see BAP docs) |
| `v.c.mode` | ✗ | Not decoded |
| `v.c.timerstart` | ✗ | Not decoded — schedule format partially reverse-engineered (see kcan-can3-clima_schedule.md) |
| `v.d.cp` | ✗ | Charge port door status — frame mentioned in code but not implemented |

---

## Environment & Climate

| OVMS Metric | Status | Notes |
|---|---|---|
| `v.e.on` | ✓ | no in snapshot (correct for parked) |
| `v.e.locked` | ✓ | no in snapshot — from 0x583 |
| `v.e.gear` | ✓ | 0 (Park) in snapshot — from 0x187 |
| `v.e.drivemode` | ✓ | 0 in snapshot — from 0x187 |
| `v.e.parktime` | ✓ | 9631 sec in snapshot — from 0x6B7 |
| `v.e.drivetime` | ✓ | 0 sec in snapshot — framework-maintained |
| `v.e.cabinsetpoint` | ✓ | 20°C in snapshot — from 0x594 |
| `v.e.cabintemp` | ✗ | Sentinel 77°C suppressed (no longer shows wrong value) — real cabin temp not yet decoded; needs Capture 5 (parked remote) |
| `v.e.temp` | ✗ | Same as cabintemp — ECU sentinel suppressed, real ambient/cabin temp source not yet identified in remote mode |
| `v.e.hvac` | ~ | Set from `0x05EA` remote_mode field: 3=active, 0=idle. Confirmed from kcan-can3-clima_on_off.crtd. Reflects real ECU state (not commanded state). |
| `v.e.heating` | ✗ | Not decoded — derivable from 0x66E or BAP status |
| `v.e.cooling` | ✗ | Not decoded |
| `v.e.cabinfan` | ✗ | Fan level not decoded |
| `v.e.handbrake` | ✗ | Not decoded — frame unknown |
| `v.e.headlights` | ✗ | Not decoded — frame unknown |
| `v.e.footbrake` | ✗ | Not decoded — frame unknown |
| `v.e.regenbrake` | ✗ | Not decoded |
| `v.e.throttle` | ✗ | Not decoded |

---

## Doors & Body

| OVMS Metric | Status | Notes |
|---|---|---|
| `v.d.fl` | ✓ | no in snapshot — from 0x583 |
| `v.d.fr` | ✓ | no in snapshot — from 0x583 |
| `v.d.rl` | ✓ | no in snapshot — from 0x583 |
| `v.d.rr` | ✓ | no in snapshot — from 0x583 |
| `v.d.trunk` | ✓ | no in snapshot — from 0x65A |
| `v.d.hood` | ✓ | no in snapshot — from 0x65A |
| `v.d.cp` | ✗ | Charge port door — not implemented (see Charging above) |

---

## Position & Navigation

GPS position metrics below are populated by the OVMS GPS hardware (SIM7600), not CAN decoding.

| OVMS Metric | Status | Notes |
|---|---|---|
| `v.p.speed` | ✓ | 0 km/h in snapshot — from 0xFD |
| `v.p.latitude` | ✓ | from 0x486 (verified against GPS) |
| `v.p.longitude` | ✓ | from 0x486 (verified against GPS) |
| `v.p.odometer` | ✓ | confirmed in snapshot — from 0x6B7 |
| `v.p.gpslock` | ✓ | yes — GPS hardware |
| `v.p.altitude` | ✓ | GPS hardware |
| `v.p.direction` | ✓ | 164.7° — GPS hardware |
| `v.p.gpshdop` | ✓ | 0.7 — GPS hardware |
| `v.p.gpsmode` | ✓ | AA — GPS hardware |
| `v.p.gpsspeed` | ✓ | 0 km/h — GPS hardware |
| `v.p.gpssq` | ✓ | 100% — GPS hardware |
| `v.p.satcount` | ✓ | 14 — GPS hardware |
| `v.p.location` | ✓ | framework location lookup |
| `v.p.trip` | ✗ | Not decoded — no trip odometer source identified |
| `v.p.acceleration` | ✗ | Not decoded |

---

## Vehicle Identity

| OVMS Metric | Status | Notes |
|---|---|---|
| `v.vin` | ✓ | `<VIN>` — from 0x6B4 |
| `v.type` | ✓ | VWEG — set at registration |

---

## Motor & Inverter

These require powertrain ECU queries not yet implemented.

| OVMS Metric | Status | Notes |
|---|---|---|
| `v.m.rpm` | ✗ | Not decoded |
| `v.m.temp` | ✗ | Not decoded |
| `v.i.temp` | ✗ | Inverter temp not decoded |
| `v.i.power` | ✗ | Not decoded |
| `v.i.efficiency` | ✗ | Not decoded |

---

## TPMS

| OVMS Metric | Status | Notes |
|---|---|---|
| `v.t.pressure` | ✗ | Not decoded — TPMS data likely on KCAN, frame not identified |
| `v.t.temp` | ✗ | Not decoded |
| `v.t.health` | ✗ | Not decoded |
| `v.t.alert` | ✗ | Not decoded |

---

## Generator (v.g.*) — Not Applicable

All `v.g.*` metrics are for V2G / grid export. The e-Golf does not support V2G. Leave undefined.

---

## Summary

| Category | Progress | Impl / Total |
|---|---|---|
| Battery & power   | `[████████████░░░░░░░░]` | 12 / 20 (60%) |
| Charging          | `[██████████░░░░░░░░░░]` |  7 / 14 (50%) |
| Environment       | `[██████████░░░░░░░░░░]` |  9 / 18 (50%) |
| Doors & body      | `[█████████████████░░░]` |  6 /  7 (86%) |
| Position          | `[█████████████████░░░]` | 13 / 15 (87%) |
| Identity          | `[████████████████████]` |  2 /  2 (100%) |
| Motor / inverter  | `[░░░░░░░░░░░░░░░░░░░░]` |  0 /  5  (0%) |
| TPMS              | `[░░░░░░░░░░░░░░░░░░░░]` |  0 /  4  (0%) |

Counting ✓ and ~ as implemented, ! as implemented-but-buggy, ✗ as not implemented. ? and wildcard rows (`.*`) counted as single items.

### High-value gaps (worth implementing)

1. **`v.c.current` / `v.c.power`** — OBC charge current — KCAN frame not yet identified; needs dedicated charging capture (see refactor-notes.md TODO)
2. **`v.e.cabintemp` / `v.e.temp`** — real cabin/ambient temp in remote mode — sentinel suppressed but metric still empty; needs parked-remote capture
4. **`v.e.heating` / `v.e.cooling`** — derives from hvac state + setpoint vs ambient
5. **`v.c.limit.soc`** — min charge % — BAP profile 0x25 already reverse-engineered
6. **`v.b.soh`** — state of health — probably accessible via UDS on BMS ECU
7. **`v.t.pressure`** — TPMS — useful alert; frame not yet identified, needs capture
8. **`v.c.timerstart`** — departure timer — schedule format partially known from kcan-can3-clima_schedule.md
