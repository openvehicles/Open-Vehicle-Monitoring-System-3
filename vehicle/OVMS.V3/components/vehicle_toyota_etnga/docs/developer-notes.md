# Toyota e-TNGA platform — developer notes

Implementation notes for the e-TNGA base module (`vehicle_toyota_etnga`) and the vehicles built on
it (`vehicle_subaru_solterra`, `vehicle_toyota_bz4x`). The user guide is `index.rst` in this
directory; this file is not part of the published documentation.

A PID number is only meaningful together with the ECU it is polled on: `0x1004` is drive-mode
select on the Hybrid Control ECU and tyre temperatures on the TPMS gateway, and `0x106E` is read
from two different ECUs depending on the state. Every PID below is therefore named with its ECU.

## Validation status

e-TNGA is developed against a single available vehicle — a 96-cell Subaru Solterra. Much of the
decode logic is therefore derived from CAN reverse engineering rather than confirmed against an
independent reference, and several pack and charging variants have no hardware behind them at all.
This table records which behaviours have actually been observed on a car, so a measured behaviour
can be told apart from an inferred one. The vehicles built on this base inherit these statuses;
nothing has been confirmed on bZ4X hardware.

| Status | Meaning |
|---|---|
| Vehicle-validated | Observed on a real vehicle, with the date of the session that confirmed it. |
| Vehicle-validated (partial) | Confirmed on some code paths but not all; the unexercised path is named. |
| Log-inferred | Decoded from captured CAN traffic or module logs and self-consistent, but never cross-checked against an independent ground truth — a scan tool, a published specification, or a physically known value. |
| Unvalidated | Reasoned from a specification, an analogous DID, or another platform. Never exercised on hardware. |

Last reviewed: 2026-08-22.

| Behaviour | Status | Evidence / gap |
|---|---|---|
| `SLEEP` / `AWAKE` / `DRIVING` transitions | Vehicle-validated | Continuous daily-driver use since 2026-06. |
| `v.e.awake` decoupled from CAN2 bus-liveness | Vehicle-validated | Zero spurious "Vehicle is idling" alerts over a 14.6 kWh charge (2026-07-13); the true-positive path fired twice in the following week, both genuine. |
| Adaptive parked-sleep cooldown backoff | Unvalidated | Merged, but the escalating cooldown has never been confirmed on a vehicle. |
| AC charge path (handshake, wait, charging) | Vehicle-validated | Many sessions, including multi-phase pause/resume within one plug-in (2026-06-24). |
| DC charge path | Vehicle-validated | Repeated DC fast charges over the 2026-07-16→19 road trip, up to 50.58 kWh (6%→84%). |
| Charge port `v.d.cp` latched at handshake | Vehicle-validated (partial) | Confirmed for plug-in while already `AWAKE`. Plug-in that *wakes the module from* `SLEEP` — the case the fix was written for — is unvalidated. |
| `v.c.type` AC (`type1` / `type2`) and DC (`ccs`) | Vehicle-validated | AC read correctly across sessions; 27 grid-log records over the 2026-07-16→19 road trip carry `ccs`. |
| Charge power derived from pack V×I | Vehicle-validated | Energy reconciliation on 2026-06-24: 88% efficiency, station 0.17 kWh vs battery 0.15 kWh, replacing earlier 52%/158% garbage. |
| Charge-fault diagnostic DID dump | Vehicle-validated (partial) | A real fault on 2026-06-24 fired the dump and rendered decoded values. It also fired on *healthy* scheduled AC charges by reading a retained value; fixed 2026-08-22, the fix awaits one scheduled charge to confirm. |
| CAN-stale logging/accounting suspend | Vehicle-validated | A real lock produced an 85 s CSV gap with no stale rows, 2026-06-24. |
| DC limiting-side attribution (car vs station) | Vehicle-validated | 15 classifications on real DC sessions over 2026-07-16→19, exercising both branches. The thresholds remain untuned against ground truth — there is no station nameplate data to check against. |
| Charge-power DID scale factors | Log-inferred | Units inferred by analogy to the grid-power DID `0x161D` on the Plug-In Control ECU (`0x745`); flagged `unit inferred` in the source. |
| HLC handshake (`0x1666`) and AC-Op (`0x1684`) enum labels, both on the Plug-In Control ECU (`0x745`) | Log-inferred | Labels come from CAN reverse engineering; they render correctly in the 2026-06-24 fault dump, but the enum semantics have not been cross-checked against a scan tool. |
| Ambient temperature during charge (`0x1F46` on the Hybrid Control ECU `0x7D2`) | Log-inferred | Selected because the A/C ECU `0x7C4` sleeps while charging. Returns a plausible value; never compared against a known ambient reading. |
| 96-cell pack arrangement and per-cell decode | Vehicle-validated | `bms status` on 2026-06-23 reported exactly 96 cells in 4 modules of 24, 24 temperature sensors at 6 per module, 0 warnings and 0 alerts. |
| 78-cell and 104-cell pack variants | Unvalidated | No such hardware available; both are reasoned from published pack specs. The runtime auto-arrange is deliberately **grow-only** — it cannot shrink, because a short reply is indistinguishable from a truncated one. |
| Per-cell voltage polling during AC charge | Unvalidated | `0x182E` on the HV Battery ECU (`0x747`) now polls during AC charging, where it previously did not. Needs one AC session to confirm the cell data updates and that the added array poll does not disturb the charge. |
| `v.b.cac` / `v.b.soh` / `v.b.capacity` from `0x1D3E` on the HV Battery ECU (`0x747`) | Vehicle-validated | Confirmed on-module 2026-08-22: the live metrics reproduce exactly from the eight raw per-module slots (197.656 Ah / 98.2875 % / 62.96 kWh). Only the 96-cell pack has an established nominal. |
| `v.e.charging12v` union rule | Vehicle-validated | The 2026-07-16→20 road trip closed the last two gaps: the DC-charge term (16 of 16 fast charges) and the 12V rising-edge wake trigger (4 clean fires). |
| 12V current from `0x15F7` on the Hybrid Control ECU (`0x7D2`) | Vehicle-validated (partial) | Direct read confirmed on-module 2026-06-21. The under-load swing is still unexercised. |
| TPMS pressures and temperatures | Vehicle-validated | Two drive sessions on 2026-06-04 polled cleanly via the gateway relay: ~280→310 kPa, 34–38 °C, no timeouts. Zeros only before the sensors wake at drive start. |
| Throttle, foot brake and park brake (Brake/EPB ECU `0x7B0`) | Vehicle-validated | Throttle logged 90,752 change events spanning 0–100%, foot brake 11,239 events spanning 0–100%, park brake 163 Applied / 151 Released. |
| Drive mode and AWD mode (Hybrid Control ECU `0x7D2`) | Unvalidated | Neither metric has produced a change event in any captured log, while sibling metrics from the same ECU logged tens of thousands. Cheapest check: change drive mode once and watch for the log line. |

## Poll states

The module keeps the vehicle in one of seven poll states. The state selects which PIDs are polled
and how often. The current state is logged on every transition under the `v-etnga` tag
("Transitioning from the *X* to the *Y* state"), and is the first thing to look at when diagnosing
behaviour.

| State | Entered when | Polling |
|---|---|---|
| `SLEEP` | At boot; when CAN2 has been silent for about 120 s; or when a watchdog forces it | Nothing is transmitted. The module only listens for CAN2 traffic and watches the 12V rail. A wake is deliberately edge-triggered on 12V rising past the calibrated reference, and is held off briefly after a forced sleep by an escalating cooldown, so a car that never wakes properly cannot be polled awake repeatedly. |
| `AWAKE` | Any CAN2 frame, or a 12V rising edge | A small keep-alive set: control mode and charge lid, park-brake status, the cable-seated signal and the pack capacity arrays. Two watchdogs bound how long this can last — 5 minutes if the charge door never opens, 15 minutes if it opened but no cable follows. |
| `DRIVING` | The vehicle reports its control mode as driving (`0x10D1` on the Plug-In Control ECU `0x745`) | The full driving set at 1 to 120 s: speed, gear, odometer, throttle and brakes, pack voltage and current, per-cell arrays, cabin climate, 12V telemetry and TPMS. |
| `CHARGE_HANDSHAKE` | The charge cable is seated and no session is already open | Negotiation signals at 1 s, charge history and voltage type at 5 s, ambient at 30 s. |
| `CHARGE_WAIT` | Handshake did not engage within 60 s (a scheduled or delayed charge), a charge phase ended, or the cable is still seated after a wait-sleep | The same signals slowed to 10-30 s, then nothing: after 10 minutes of waiting the module sleeps so the bus idles and 12V recovers, resuming the open session on the next wake. A wait re-entered after such a sleep sleeps again after 15 s, which keeps the duty cycle low over a long delay. A charge starting during a waking window is caught within 10 s. |
| `CHARGE_AC` | The charger reports AC operation running | Live power and SOC at 1 s, the charger, grid and cabin channels at 1-5 s, 12V telemetry at 10-120 s, the heavy per-cell and capacity arrays at 30-60 s. |
| `CHARGE_DC` | The DC high-level communication sequence is active | Live power and SOC at 1 s, station present voltage and current at 1 s, station caps at 5 s, 12V telemetry at 10-120 s, the arrays at 20-60 s. |

There is no direct edge between `DRIVING` and any charge state, and no direct edge from `DRIVING`
to `SLEEP` — those paths pass through `AWAKE`, which clears trip metrics on the way. Removing the
cable ends the session and returns to `AWAKE` — from `CHARGE_AC` or `CHARGE_DC` by way of
`CHARGE_WAIT`. If the cable is pulled while the module is asleep mid-wait, the removal is
reconciled on the next wake, and the session is closed then.

`v.e.awake` reads `false` in every charge state, which is what stops the base class's periodic
"Vehicle is idling" notification from firing during a charge.

### Charge-stopped notifications

`ms_v_charge_state = "stopped"` is written whenever a charge phase ends, and the `OvmsVehicle` base
turns that into a `charge.stopped` notification. The module does not set `ms_v_charge_substate`,
so the base raises it at **alert** priority rather than **info**, including at an ordinary phase
boundary.

## Poll targets

Six ECUs are polled, all on CAN2.

| ECU | Request / reply | Polled in | Provides |
|---|---|---|---|
| Plug-In Control System | `0x745` / `0x74D` | AWAKE, DRIVING and all four charge states | 23 PIDs: control mode, charge lid, the PISW cable-seated signal, the AC and DC handshake signals, SOC, charge power, station and car charge limits, charge history and outcome, My Room, and cabin power while charging. |
| HV Battery | `0x747` / `0x74F` | AWAKE, DRIVING, AC and DC charge | 4 PIDs: BMS SOC, the per-cell voltage and temperature arrays, and the per-module capacity array behind `v.b.cac`. |
| Hybrid Control System (EV ECU) | `0x7D2` / `0x7DA` | DRIVING and all four charge states | 15 PIDs: speed, gear, odometer, ready signal, throttle, drive and AWD mode, pack voltage and current, cabin power while driving, the 12V auxiliary telemetry, and ambient temperature during a charge. |
| Brake / EPB | `0x7B0` / `0x7B8` | AWAKE, DRIVING | 2 PIDs: brake pedal stroke and electric park brake status. The EPB stays alive in the parked body tail, which is why park brake is polled in AWAKE as well. |
| A/C | `0x7C4` / `0x7CC` | DRIVING | 5 PIDs: cabin and ambient temperature, HVAC setpoint, HV heater power and blower level. This ECU sleeps while charging, which is why in-charge ambient is read from the Hybrid Control ECU instead. |
| TPMS, via gateway relay | `0x750` / `0x758`, sub-target `0x2A` | DRIVING | 3 PIDs: pressures, temperatures and the slot-to-corner map. Mixed (`ISOTP_EXTADR`) addressing. The sub-target answers only while the car is driving or in My Room, so it is not polled at any other time. |

The exhaustive per-PID cadences are the two poll arrays `obdii_polls_base` and `obdii_polls_charge`
in `vehicle_toyota_etnga.cpp`, where every row carries a comment naming its DID, its meaning and
its cadence in each state. They are split in two because a poll list supports only
`VEHICLE_POLL_NSTATES` (4) states: the first array covers `SLEEP` / `AWAKE` / `DRIVING` and the
second is registered at an offset so its four columns land on the four charge states. A PID polled
on both sides therefore appears in both arrays.

## BMS arrangement

The base class declares the HV pack arrangement (Toyota EM "Type B" chemistry, CATL cells, shared
across the platform) and derives the actual cell and temperature-sensor counts from the reply
length of `0x182E` / `0x1814` on the HV Battery ECU (`0x747`) at runtime. The 96-cell pack is
arranged as 96 voltages in 4 modules of 24, with 24 temperature sensors at 6 per module. The pack
variant follows the model year and drivetrain, not the badge — 96-cell for 2022-24, 78-cell FWD or
104-cell AWD for the 2025/26 refresh — which is why the count is derived at runtime rather than
declared per vehicle.

## Web pages

Registered in `etnga_web.cpp`:

| URL | Menu title | Handler |
|---|---|---|
| `/bms/cellmon` | BMS cell monitor | framework `OvmsWebServer::HandleBmsCellMonitor` |
| `/xte/charge` | Charging monitor | `WebDispChgMetrics` |
| `/xte/reports` | Charge reports | `WebChargeReports` |
| `/xte/report` | (no menu entry) | `WebChargeReport`, serves one stored report or CSV by filename |
| `/xte/config` | Configuration | `WebCfgFeatures` |

## Charge session CSV

The CSV header row is authoritative and is written at the top of every file. The column set has
grown across releases and one column was inserted at the front rather than appended, so a
positional parser written against an older file will silently misread every field.

Columns that are easy to misread:

- `batt_temp_c` is the **mean** of the pack sensors, while `batt_tmin_c` / `batt_tmax_c` are the
  extremes. The spread is roughly 3 °C even during a gentle AC charge, and the BMS derates on its
  hottest sensor, so a charging curve should be keyed to `batt_tmax_c`.
- `obc_kw` is diagnostic only and under-reads on DC charging; `battery_kw` is the real power.
- `ambient_c` is empty rather than `0.0` until the first in-charge reading arrives, so an empty
  field is distinguishable from a genuine 0 °C.

## Metric sources

Standard metrics:

| Metric | Source |
|---|---|
| `v.e.cooling` | `0x106E` on the Hybrid Control ECU `0x7D2` while driving and on the Plug-In Control ECU `0x745` while charging — the dedicated A/C consumption channel, so any draw means cooling is on |
| `v.e.heating` | A/C ECU `0x7C4`, `0x1086` |
| `v.e.cabinfan` | A/C ECU `0x7C4`, `0x2801` (levels 1-7) |
| `v.b.consumption`, `v.b.coulomb.used` / `.recd` (+ `.total`), `v.b.energy.used` / `.recd` (+ `.total`) | derived on-module |
| `v.c.power` | Plug-In Control ECU `0x745`, `0x10D4` |
| `v.c.kwh.grid` / `.total` | Plug-In Control ECU `0x745`, `0x161D` |
| `v.b.cac` | HV Battery ECU `0x747`, `0x1D3E` — mean of eight per-module slots |
| `v.b.soh` / `v.b.capacity` | derived from `v.b.cac` |
| `v.c.voltage` / `v.c.current` | Plug-In Control ECU `0x745`, `0x166B` / `0x166C` |
| `v.e.throttle` / `v.e.drivemode` | Hybrid Control ECU `0x7D2`, `0x1060` / `0x1004` |
| `v.e.footbrake` / `v.e.handbrake` | Brake/EPB ECU `0x7B0`, `0x104C` / `0x1045` |
| `v.t.pressure` / `v.t.temp` / `v.t.alert` | TPMS gateway `0x750` sub-target `0x2A`, `0x1005` / `0x1004` / derived |

Custom `xte` metrics:

| Metric | Source |
|---|---|
| `xte.v.c.hlcstate` / `xte.v.c.piswraw` | Plug-In Control ECU `0x745`, `0x1666` / `0x1669` |
| `xte.v.c.ac.opstatus` / `xte.v.c.chargerstate` | Plug-In Control ECU `0x745`, `0x1684` / `0x1619` |
| `xte.v.c.ac.tgtpower` / `xte.v.c.ac.ilimit` | Plug-In Control ECU `0x745`, `0x1619` |
| `xte.v.c.output` / `xte.v.c.outputtarget` | Plug-In Control ECU `0x745`, `0x161E` (kW unit inferred) |
| `xte.v.c.ac.usable` | Plug-In Control ECU `0x745`, `0x1665` (unit inferred) |
| `xte.v.c.gridpower` | Plug-In Control ECU `0x745`, `0x161D` |
| `xte.v.c.permpower` / `xte.v.c.tgtcurrent` | Plug-In Control ECU `0x745`, `0x16A1` / `0x166D` |
| `xte.v.c.dc.maxpower` / `.maxcurrent` / `.maxvoltage` | Plug-In Control ECU `0x745`, `0x166A` / `0x1679` / `0x1681` |
| `xte.v.c.outcome` | Plug-In Control ECU `0x745`, `0x1688` — retained by the vehicle between sessions, so it must be scoped per session |
| `xte.v.c.stoprequest` | Plug-In Control ECU `0x745`, `0x1667` (partial decode) |
| `xte.v.c.myroom` | Plug-In Control ECU `0x745`, `0x1692` — the only live My Room signal on the bus |
| `xte.s.controlstate` | Plug-In Control ECU `0x745`, `0x10D1` |
| `xte.v.e.hvac.power` | `0x106E` on the Plug-In Control ECU `0x745` while charging, and on the Hybrid Control ECU `0x7D2` while driving |
| `xte.v.e.hvac.kwh` / `xte.v.e.hvac.kwh.drive` | derived on-module |
| `xte.v.e.awd` | Hybrid Control ECU `0x7D2`, `0x1087` |
| `xte.v.b.soc.bms` | HV Battery ECU `0x747`, `0x1F5B` |
| `xte.v.b.12v.voltage` | Hybrid Control ECU `0x7D2`, `0x15EE` |
| `xte.v.b.12v.temp` / `xte.v.b.12v.cac` | Hybrid Control ECU `0x7D2`, `0x15F8` / `0x15E5` |
| `xte.v.b.12v.charge.ah` / `.discharge.ah` / `.readyon.h` | Hybrid Control ECU `0x7D2`, `0x15E8` |
