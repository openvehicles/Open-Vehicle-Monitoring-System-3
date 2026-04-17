# can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-205418.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_1/edge` |
| Bus | can2 (FCAN) |
| Captured | 20260412-205418 |
| Duration | ~68 s wall, 46.5 s of B-mode motion |
| Frames | 23186 (22088 standard, 96 unique IDs) |
| Capture ref | **Capture 17c** — varied drive in B mode (queued from 17b) |

## Sequence

Drive-mode = B (one-pedal regen) throughout. Wider speed envelope than
17a/17b: gentle accel to 23 km/h, lift-off coast to 14 km/h, hard accel
to **75 km/h peak**, regen brake down to 60 km/h, then steady cruise
56-58 km/h until end of capture. Multiple regen events of varied
intensity — exactly the data 17b wanted for locking down 0x0B4 byte
roles.

## Speed profile (0x0B2 b0:1 LE × 1/128 km/h)

| t/s | km/h | phase |
|---:|---:|---|
| 0.0 – 3.8 | 0.0 | stationary |
| 4.5 – 8.2 | 3.5 → 23.5 | gentle accel |
| 8.7 – 11.5 | 23.4 → 16.3 | lift-off coast |
| 12.5 – 22.6 | 15.0 → 75.4 | hard accel to peak |
| 22.6 – 26.0 | 75.4 → 62.2 | **regen brake event** |
| 27.0 – 35.5 | 59.8 → 55.4 | settling cruise |
| 36.0 – 40.5 | 55.5 → 56.5 | flat cruise (with brief 0x0B4 pulse — see below) |
| 41.0 – 46.5 | 56.4 → 58.6 | steady cruise tail |

## Key result — 0x0B4 reinterpreted

Cap 17b decoded 0x0B4 as `b3 = regen-active flag, b6 = regen torque`.
**Cap 17c invalidates that hypothesis.** The wider speed envelope and
multiple distinct regen events show:

- **(b3, b4) is a 16-bit LE pair**, not a single-byte flag. b4 is the
  small high byte (0x00–0x04 across the whole capture). Combined LE16
  varies smoothly across drive and regen.
- **(b6, b7) is also a 16-bit LE pair.** b7 is the small high byte
  (0x00–0x16).
- **b5 is always 0** across all 1195 0x0B4 frames in this capture —
  not a signal.
- **Both pairs fire during forward propulsion**, not only during regen.
  At t=41–46 the car is in steady forward cruise at 56–58 km/h with
  `(b3,b4) = 53` and `(b6,b7) = 0` — a forward-cruise idle state.
- **b2 is a separate small byte** (0–25 range) that climbs during regen
  and idles low during forward cruise, but can be non-zero with both
  pairs at 0. Independent quantity.

### Regen window evidence (t=22.95 → 26.05 s, raw 0x0B4 frames)

| t/s | b2 | (b3,b4) LE | (b6,b7) LE | speed km/h |
|---:|---:|---:|---:|---:|
| 22.95 | 0 | 53 | 848 | 75.55 |
| 23.09 | 2 | 486 | 5600 | 75.38 |
| 23.29 | 21 | 498 | 5632 | 74.45 |
| 23.55 | 25 | 504 | 5632 | 73.05 |
| 23.71 | 23 | 509 | 5648 | 72.31 |
| 23.96 | 24 | 522 | 5664 | 71.14 |
| 24.17 | 24 | 528 | 5680 | 70.11 |
| 24.48 | 24 | 539 | 5696 | 68.80 |
| 24.80 | 24 | 552 | 5712 | 67.27 |
| 25.08 | 24 | 564 | 5728 | 66.13 |
| 25.29 | 24 | 570 | 5744 | 65.08 |
| 25.49 | 24 | 576 | 5744 | 64.05 |
| 25.53 | 24 | 582 | 5760 | 63.95 |
| 26.00 | 24 | 600 | 5776 | 62.19 |

Both LE16 pairs ramp **monotonically and smoothly** while the car
decelerates. A single-byte flag would latch; a 16-bit field can do this.
Decel rate ≈ 4.3 km/h/s (~1.2 m/s²) — moderate B-mode brake.

### Regen release (t=26.31 → 26.66 s)

| t/s | b2 | (b3,b4) LE | (b6,b7) LE | speed km/h |
|---:|---:|---:|---:|---:|
| 26.31 | 22 | 406 | 3664 | 60.60 |
| 26.32 | 21 | 386 | 2656 | 60.72 |
| 26.33 | 20 | 365 | 2224 | 60.62 |
| 26.34 | 20 | 346 | 1872 | 60.74 |
| 26.35 | 19 | 326 | 1488 | 60.55 |
| 26.37 | 17 | 285 | 928 | 60.55 |
| 26.60 | 6 | 0 | 0 | 60.16 |

Clean trailing edge — both pairs and b2 decay together over ~250 ms as
the driver returns to neutral throttle.

### Forward cruise tail (t=41.0 → 46.5 s)

| t/s | b2 | (b3,b4) LE | (b6,b7) LE | speed km/h |
|---:|---:|---:|---:|---:|
| 41.09 | 0 | 53 | 0 | 56.40 |
| 42.47 | 0 | 53 | 0 | 56.64 |
| 43.60 | 0 | 53 | 0 | 57.52 |
| 44.89 | 0 | 53 | 0 | 58.14 |
| 46.01 | 0 | 53 | 0 | 58.59 |

`(b3,b4) = 53` is the **forward-cruise idle** value of MotorTorqueA.
b6,b7 stay at 0 throughout — they are *not* present during pure
forward propulsion. b2 is also 0 here. This is the cleanest negative
control we have for distinguishing the two pairs: pair A is the always-on
torque-like quantity, pair B activates during high-load events
(strong regen, cruise pulses).

### Cruise pulse (t=36.81 → 40.59 s)

A brief active window during what otherwise looks like steady cruise.

| t/s | (b3,b4) LE | (b6,b7) LE | speed km/h |
|---:|---:|---:|---:|
| 36.55 (before) | 0 | 0 | 55.98 |
| 36.81 | 653 | 1200 | 56.09 |
| 37.51 | 653 | 1216 | 56.25 |
| 37.99 | 648 | 688 | 56.42 |
| 38.50 | 648 | 352 | 56.66 |
| 39.89 | 648 | 224 | 56.79 |
| 40.16 | 648 | 0 | 56.48 |
| 40.88 | 53 | 0 | 56.47 |
| 41.09 (after) | 53 | 0 | 56.40 |

Speed barely changes (55.98 → 56.66 → 56.40) so this is not regen.
Hypothesis: brief torque request to maintain cruise on a slight
gradient, then settle back to the 53 idle value. Pair B (b6,b7) drops
out before pair A returns to idle, suggesting pair B reacts only to
above-idle load.

## What this tells us

1. **0x0B4 carries general motor/inverter state, not just regen state.**
   The "regen brake torque" name from 17b was wrong.
2. **Two independent 16-bit motor quantities** at offsets 24 and 48,
   plus a separate small byte at offset 16, plus a rolling counter at
   offset 0 unrelated to motor state.
3. **Units, sign convention, and which pair is torque vs power vs
   current still unknown.** Both pairs are always non-negative in this
   capture (no negative-torque sign bit observed) and both are present
   during forward drive — so neither pair is signed regen-only.
4. **MotorTorqueA cruise idle = 53.** That gives us a baseline for
   future captures: any value above 53 means "above-idle load" of some
   kind.

## Other partials

### 0x057 byte 3 (motor torque request) — wider envelope

Cap 17b had b3 max ≈ 38 at 30 km/h cruise. Cap 17c reaches **b3 = 80
at 75 km/h peak**, with a near-linear ratio between b3 and km/h across
the whole envelope:

| speed km/h | b3 |
|---:|---:|
| 3.5 | 5 |
| 10.1 | 11 |
| 19.3 | 19 |
| 22.9 | 23 |
| 50.3 | 54 |
| 75.4 | 80 |

The ratio is slightly above 1 at high speed and slightly above 1 at low
speed too — not a constant scale, more like a power request that scales
with cruise drag (linear-ish in velocity in this range). Not literally
the speed reading from 0x0B2 (which we already have at perfect scale).
Likely the inverter / motor torque request from the accelerator,
matching the Cap 17 name `MotorTorqueRequest`.

## DBC update

- Replaced placeholder `RegenStateFlag` (b3 single byte) and `RegenTorque`
  (b6 single byte) signals on 0x0B4 with:
  - `MotorState_b2` — byte 2
  - `MotorTorqueA` — bytes 3/4 LE16
  - `MotorTorqueB` — bytes 6/7 LE16
- Frame name kept as `Unknown_0B4` because units/identity still TBD.
- Comments updated with the Cap 17c interpretation and the explicit
  note that the Cap 17b flag hypothesis was overturned.

## Open questions / next captures

- What is the exact identity of MotorTorqueA vs MotorTorqueB? Likely
  candidates: motor torque (Nm), motor mechanical power, inverter DC
  current, inverter DC power. Two captures could disambiguate:
  - **17d — sustained regen at constant decel**, e.g. coast down a long
    grade. If the pair is power, it should drop with speed at constant
    F. If it is torque or current, it should stay flat.
  - **17e — high-power forward acceleration WOT**. If pair B activates
    on heavy forward load (not just regen), that confirms it is a
    bidirectional load metric, not regen-only.
- What is byte 2? Hypothesis: regen pedal demand percent / brake-by-wire
  request. Could correlate with how hard the lift-off was.
- Is there a sign byte we missed? All pairs were unsigned in this
  capture; need a hard regen vs hard accel comparison to look for a
  flipped MSB or a separate sign bit.
- 0x0FD bytes 4/5 and 0x101 bytes 3/5 are still unidentified
  motion-correlated scalars. Not touched in this capture's analysis.

## Notes

This capture is the **most data-dense FCAN motion capture so far**:
46.5 s of continuous B-mode driving across a wide speed envelope, with
multiple distinct regen and cruise phases. Worth keeping as the
canonical reference for any future motor/torque decode work — even
without UDS ground truth, the timeline is rich enough to disprove
single-byte hypotheses and to provide a baseline for unit calibration
once a ground-truth source is available.
