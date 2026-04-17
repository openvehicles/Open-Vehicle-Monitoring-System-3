# can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-195958.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_1/edge` |
| Bus | can2 (FCAN) |
| Captured | 20260412-195958 |
| Duration | ~56 s; ~23 s of gear=D motion |
| Frames | 3629 (3452 standard, 90 unique IDs) |
| Capture ref | **Capture 17a** — short drive in D, brake-pedal stop |

## Sequence

Drive-mode = D. Accelerate to ~30 km/h, hold for ~30 count, brake to a stop
using the brake pedal (no regen). Companion B-mode capture: 20260412-201238.

## Gear timeline (0x187 b2 low nibble)

| State | Window | Notes |
|---|---|---|
| D (5) | 0.3 – 23.0 s | Single window, full motion phase |

## Key decode results

### 0x0B2 — vehicle wheel speeds (4 wheels)

**Confirmed.** Frame is four 16-bit little-endian wheel speeds packed at
byte offsets 0/1, 2/3, 4/5, 6/7. Scale **0.0078125 km/h per bit (= 1/128)**.

Evidence: cruise plateau at raw ≈ 3850 → 30.05 km/h, matching the
"accelerate to 30 km/h" target. Peak 4509 → 35.2 km/h (overshoot during
acceleration). Brake-pedal deceleration to 0 at end of window.

All four pair offsets trace identical curves within ±0.2 km/h —
consistent with wheel-speed broadcast in low-slip cruise. Frame is
all-zeros while stationary; wakes once any wheel turns.

This is the FCAN counterpart to KCAN ESP_Speed (0xFD on bus 3). Either
can source `ms_v_pos_speed` when ignition on.

### 0x0B4 — motor torque / regen-active state

**Partial decode.** Bytes 0/1 are an unrelated rolling field (active even
at rest). Bytes 2,3,4,6,7 stay at 0x00 throughout the **entire D capture**
because brake-pedal stop produces no regen torque. Compare against the B
capture (201238) where these bytes light up during the regen
deceleration phase.

Conclusion (cross-capture): 0x0B4 carries regen brake state.
- b3 = regen-active flag / torque magnitude (0 in D capture)
- b6 = regen torque magnitude (ramps with decel intensity in B)

D capture serves as the **negative control** that confirms the bytes are
regen-specific, not generic motor torque.

### 0x057 — accelerator / motor torque request

**Partial decode.** DLC 4. Bytes 0/1 are a rolling counter (active even
at rest). Bytes 2 and 3 are zero while stationary, ramp during
acceleration, plateau during cruise, decay during stop.

Sample trace (b3): 0 → 3 → 16 → 18 → 29 → 36 → 37 → 35 → 33 → 0

b3 max ~0x25 (37) at peak acceleration. Tracks the accelerator pedal /
motor torque request. Same behavior in both D and B captures (cruise
phase identical), so this is **not** regen-related.

### 0x0FD bytes 4/5 — secondary motion-correlated signal

b4/b5 are 0x00 at rest, ramp during motion. b5 max 0x0d in D capture.
Slow ramp suggests filtered quantity — possibly motor speed in rpm/100
or similar. Worth confirming in a longer capture with varied speed.

### 0x101 bytes 3/5 — motion-correlated, larger swing

b3 ramps 0→90→0 over the motion window. b5 jumps 0→215 then back.
Larger dynamic range than 0x0FD. Candidate for motor rpm or inverter
DC current. Needs varied-speed capture to lock down.

## Nulls / unchanged from Cap 15

- 0x0B3: latched to "drive-armed" (b3=0x3e, b4=0x12, b7=0x90) for
  the entire D motion window — consistent with the Cap 15 hypothesis.
- 0x187: gear=5 throughout, confirms D-mode only.

## Notes

D capture's value is as the **regen-negative reference** for 0x0B4. With
brake-pedal stop the regen channels stay quiescent, isolating the b3/b6
ramp seen in the B capture as unambiguous regen torque evidence.
