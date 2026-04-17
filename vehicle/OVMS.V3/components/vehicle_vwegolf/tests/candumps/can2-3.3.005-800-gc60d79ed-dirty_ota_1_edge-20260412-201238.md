# can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-201238.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_1/edge` |
| Bus | can2 (FCAN) |
| Captured | 20260412-201238 |
| Duration | ~154 s; ~26 s of gear=B motion |
| Frames | 10322 (9830 standard, 95 unique IDs) |
| Capture ref | **Capture 17b** — short drive in B, regen stop |

## Sequence

Drive-mode = B (one-pedal regen). Accelerate to ~30 km/h, hold for ~20
count, lift accelerator and let regen decelerate to a stop. Companion
D-mode capture: 20260412-195958.

## Gear timeline (0x187 b2 low nibble)

| State | Window | Notes |
|---|---|---|
| B (6) | 0.0 – 26.1 s | Single window, full motion phase |

## Key decode results

### 0x0B2 — vehicle wheel speeds (4 wheels) — CONFIRMED

**Primary outcome of Capture 17.** Frame is four 16-bit little-endian
wheel speeds at byte offsets 0/1, 2/3, 4/5, 6/7. Scale
**0.0078125 km/h per bit (= 1/128)**.

Trajectory of wheel 0 (b0:1 LE) along the run:

| t/s | raw | km/h | phase |
|---:|---:|---:|---|
| 0–1.5 | 0 | 0.0 | stationary |
| 2.7 | 634 | 4.95 | accel start |
| 5.2 | 4122 | 32.2 | accel peak (overshoot) |
| 6.7 | 4618 | 36.0 | peak |
| 11.0 | 3970 | 31.0 | settling |
| 14–17 | ~3850 | **30.05** | cruise plateau (target) |
| 18.1 | 3421 | 26.7 | regen brake start |
| 19.0 | 2790 | 21.8 | |
| 20.1 | 2000 | 15.6 | |
| 21.2 | 1295 | 10.1 | |
| 22.7 | 731 | 5.7 | |
| 23.5 | 265 | 2.1 | almost stopped |
| 24.5 | 0 | 0.0 | stopped |

The 30 km/h cruise plateau matches the driver's target exactly,
confirming the 1/128 km/h scale. All four wheel pairs trace within
±0.2 km/h. This is the FCAN counterpart to KCAN ESP_Speed (0xFD).

### 0x0B4 — regen brake torque / state — PARTIAL (revised by Cap 17c)

> **REVISION (Cap 17c, 20260412-205418):** the b3-as-flag interpretation
> below is wrong. Cap 17c (varied B drive 0-75 km/h) showed:
> - **(b3,b4) is a 16-bit LE pair**, not a single-byte flag. b3 takes
>   many values across forward cruise and regen; b4 is its high byte.
> - **(b6,b7) is also a 16-bit LE pair.** Both pairs fire during
>   forward propulsion too — they are not regen-only. The b3≈0xDA
>   "latch" was just one short regen event happening to land in that
>   numeric range; the wider envelope of 17c rules out the latch theory.
> - 0x0B4 carries general motor/inverter state, not just regen state.
>
> See `205418.md` for the corrected interpretation. Original Cap 17b
> text below is preserved for the byte-by-byte raw observations, which
> are still valid evidence — only the interpretation changed.

**Primary outcome of Capture 17.** Bytes 0/1 are a rolling counter
present even at rest. Bytes 2/3/4/6/7 are 0 in D capture (brake-pedal
stop), but here in B capture they activate the moment regen starts and
ramp through the deceleration:

| t/s | speed (km/h) | b3 | b6 |
|---:|---:|---:|---:|
| 17.0 | 30.1 | 0 | 0 |
| 17.5 | 29.4 | 48–53 | 64 |
| 17.7 | 28.7 | 89–114 | 80 |
| 18.1 | 26.7 | 198–221 | 96–112 |
| 18.5 | 24.5 | 86–109 | 128 |
| 19.0 | 21.7 | 218 | 160 |
| 19.4 | 19.1 | 218 | 176 |
| 20.1 | 15.5 | 218 | 176 |
| 20.3 | 14.4 | 218 | 64–144 |
| 21.2 | 10.1 | 218 | 176–224 |

Pattern: b3 latches to ~0xDA (218) for the duration of regen, with brief
modulation early. b6 ramps with decel magnitude (0→224, 32-step
quantization visible). b3 looks like an active-flag / max-torque value;
b6 looks like instantaneous regen torque.

In the D capture (brake-pedal stop) **all of b2/b3/b4/b6/b7 stay at 0
through the entire deceleration**, providing a clean negative control.
The contrast is unambiguous: 0x0B4 high bytes carry regen state.

Open question: which byte(s) are negative-torque magnitude vs status
flag, and the units/scale. Needs a capture with several distinct regen
intensities (mild B coast vs. hard B brake into a near-stop).

### 0x057 — accelerator / motor torque request

DLC 4. Bytes 0/1 rolling counter; bytes 2/3 motion-correlated. b3
ramps 0→5→16→34→38 during accel, plateaus ~32–34 during cruise, decays
0 during regen stop. Same trajectory in D capture, so **not regen-related**.
b3 max 0x26 (38).

Interpretation: motor torque request from the accelerator pedal. Note
that b3 winds down smoothly during the regen stop in B (driver lifted
foot) — consistent with "accelerator request → 0", separate from the
regen torque on 0x0B4.

### 0x0FD bytes 4/5 — secondary motion signal

b4/b5 active during motion, 0x00 at rest. Same general behavior as in
the D capture. Unidentified scalar.

### 0x101 bytes 3/5 — motion-correlated

b3 ramps with motion (0→81→218→0), b5 likewise. Larger dynamic range
than 0x0FD. Candidate for motor rpm or DC current.

## ID diff vs D capture

5 IDs appear here that did not in the D capture:

| ID | Frames | Behaviour | Note |
|---|---:|---|---|
| 0x3FE | 5 | all-zero | sparse keep-alive |
| 0x5F7 | 6 | static (`80 24 24 a1 ff fd e7 ff`) | low-rate broadcast |
| 0x643 | 9 | static (`00 02 0b 00 00 00 00 00`) | low-rate node ping |
| 0x6B7 | 3 | static | rare broadcast |
| 0x6B8 | 3 | static | rare broadcast |

None of these correlate with B-mode specifically — they are
low-rate/sporadic broadcasters that fired during this longer capture
but not the 56 s D capture. No B-mode-only signal found among them.

## Notes

Capture 17 (D + B pair) was the queued follow-up from Cap 15's 0x0B2 /
0x0B4 hypotheses. Both decoded:

- **0x0B2 = wheel speeds, 1/128 km/h** — ready for `ms_v_pos_speed` and
  per-wheel diagnostic metrics. Promote 0x0B2 from "candidate" to
  "confirmed" in the dbc.
- **0x0B4 = regen brake state**, partial decode. Byte roles need one
  more capture with varied regen intensity to lock magnitude/units.

Promoted candidates from Cap 15 follow-up list: 0x0B2 (done), 0x0B4
(partial). Still pending: 0x057 (motor request, partial), 0x0FD
(secondary motor scalar), 0x101 (motor rpm? candidate).
