# can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-184855.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_1/edge` |
| Bus | can2 (confirmed — 14548/14548 frames on bus 2; filter fix in capture.sh) |
| Captured | 20260412-184855 |
| Duration | ~46 s of frames |
| Frames | 14548 |
| Capture ref | **Capture 16** — throttle / brake stimulation (retry, first attempt was can3-only due to filter bug) |

## Sequence

Parked in N, ignition ON, FCAN awake. Approximate user sequence:
5 s idle → brake pumps x3 (1 s apart) → ~3 s idle → throttle blips x3 (1 s apart) → 5 s idle.
Precise pedal timestamps not recorded by the user — event windows inferred from temporal clustering.

## Primary result — 0x057 is NOT pedal

Byte 1 is a low nibble (0–15), byte 0 is full 8-bit. Combined LE value
ranges 5 to 3953 = 0x005 to 0xF71 — a 12-bit quantity cycling at ~30 Hz
continuously, with no stable dwell. This is inconsistent with a pedal
position signal (which would hold value between events). More consistent
with motor rotor angle / wheel-speed tick / free-running time base.

**0x057 dropped as pedal candidate.** Keep it as a candidate for drive
dynamics — revisit with Capture 17.

## Bimodal scan — pedal candidates found

Scanned all can2 11-bit IDs for bytes where one value dominates ≥80 % of
frames with the remainder concentrated in a few excursion values. Three
IDs showed the expected pattern, and two of them correlated cleanly in
time with the expected brake→idle→throttle sequence.

### 0x365 — brake torque / pressure request (strong)

| State | Frame | Count |
|---|---|---|
| Idle    | `a0 06 00 00 82 04 64 7a` | 127 |
| Active  | `a0 06 00 00 83 04 64 7a` | 11  |

Single bit flip: byte 4 bit 0 (0x82 → 0x83). All 11 activations cluster
in **t = 13.68 – 25.38 s**. No activations outside that window.

Hypothesis: brake torque or hydraulic pressure request from ESP/brake
controller. Expected to be binary on this stimulus (3 clean brake pumps);
11 frames fits 3 pumps with ~3-4 frames captured per sustained press.

### 0x3BE — brake light switch + throttle active (two signals in one frame)

| State | Frame | Count |
|---|---|---|
| Idle       | `00 00 e7 04 c0 a0 0c 00` | 65 |
| Brake A    | `00 00 e7 54 c0 a0 0c 00` | 19 |
| Throttle   | `00 00 e7 04 c0 a1 0c 00` | 12 |
| Brake B    | `00 00 e7 44 c0 a0 0c 00` | 2  |

**Byte 3 transitions (04 → 44/54)** — 21 frames in window **t = 13.70 – 31.43 s**.
Co-fires with 0x365 during the brake window and extends ~6 s past the
last 0x365 activation. Suggests brake light switch / redundant pedal
switch with different release timing from the ESP torque request.
Bit 4 (0x10) is sometimes set alongside bit 6 (0x40) and sometimes not —
possibly two-stage pressure threshold or ABS-related.

**Byte 5 bit 0 (a0 → a1)** — 12 frames in window **t = 33.71 – 43.71 s**.
Separate, later window. Matches "throttle blips x3" phase of the sequence.
Hypothesis: accelerator pedal active flag.

### 0x101 — heartbeat / counter, not pedal

Byte 6 bit 6 (0x00 → 0x40) flips 11 times but events are **spread across
the whole capture** (2.3 s, 2.8 s, 4.1 s, 8.8 s, 14.5 s, 20.4 s, 24.9 s,
26.5 s, 27.0 s, 31.2 s, 35.5 s). No temporal clustering. Discarded as
pedal candidate.

### 0x0A7, 0x121, 0x3B1 — bimodal but not correlated

Appeared in the bimodal scan but either too few excursions (0x3B1 = 1) or
time distribution does not match the stimulus sequence. No useful pedal
signal from this capture.

## Findings summary

| ID | Byte/bit | State | Confidence |
|---|---|---|---|
| 0x365 | b4 bit 0 | brake torque/pressure active | strong — clean bimodal + time cluster |
| 0x3BE | b3 bits 4,6 | brake pedal switch | strong — co-fires with 0x365 |
| 0x3BE | b5 bit 0 | throttle active | strong — distinct time cluster |
| 0x057 | — | not pedal, likely motor angle / time base | strong negative |
| 0x101 | b6 bit 6 | heartbeat, not pedal | negative |

## Open questions / next steps

1. **Timestamped confirmation capture.** Repeat with user noting wall-clock
   times for each brake pump and throttle blip. Verify edge counts match
   pump counts exactly.
2. **Brake threshold test.** Gentle vs firm brake — does 0x3BE byte 3 cycle
   between 0x44 (bit 6 only) and 0x54 (bits 4+6) by pressure?
3. **Steering / ESP isolation.** 0x3BE byte 3 extends past the brake window.
   Rule out steering wheel movement or ABS self-test contamination by a
   brake-only capture with wheel strictly centred.
4. **Standard metric wiring.** Once confirmed, wire 0x3BE b5 bit 0 →
   `v.e.throttle` (bool) and 0x3BE b3 bit 4 / 0x365 b4 bit 0 → `v.e.footbrake`
   with hysteresis.
