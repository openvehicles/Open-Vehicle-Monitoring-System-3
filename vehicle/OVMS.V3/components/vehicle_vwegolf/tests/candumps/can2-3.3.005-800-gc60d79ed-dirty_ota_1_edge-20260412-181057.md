# can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-181057.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_1/edge` |
| Bus | can2 |
| Captured | 20260412-181057 |
| Duration | ~272s wall clock; ~34s of actual frames before OVMS crashed |
| Frames | 20471 (incl. 18 `2CER` error records) |
| Capture ref | **Capture 15** — gear selector stimulation |

## Sequence

Parked, ignition on, brake held. can2 capture running:
P → R → N → D → B → (back through N/R to) P.

OVMS crashed mid-capture. Frame stream stopped ~5 s after returning to P.
Full P→R→N→D→B→P sequence still present in the data before crash.

## Gear timeline (from 0x187 byte 2 low nibble, confirmed decode)

| State | Enter at (rel s) | Duration |
|---|---|---|
| P (2)   | 0.0  | 9.4 s |
| R (3)   | 9.4  | 2.4 s |
| N (4)   | 11.8 | 2.8 s |
| D (5)   | 14.6 | 4.3 s |
| B (6)   | 18.9 | 3.0 s |
| N (4) + R (3) + P (2) | 21.9 | back to P at 22.3 s; stream dies ~6 s later |

## Candidate ID diff results

Analysis method: for each candidate, compute set of unique byte values
seen during the mid-state window of each gear (avoiding transition edges).
"Stable" = single value throughout window. Gear-state signal = stable in
each state AND value differs between states.

### Null result on primary question

**No candidate ID carries gear state as a stable byte that changes with
gear.** 0x187 remains the sole gear source on FCAN among the observed
high-rate broadcasts. No further decode needed for `v.e.gear`.

### Secondary findings

#### 0x0B3 — "drive-armed" latch

Bytes 3, 4, 7 transition on first shift out of P and stay latched through
R/N/D/B and the return to P.

| State | b3 | b4 | b7 |
|---|---|---|---|
| P (initial)   | 00 | 08 | 20 |
| R / N / D / B / P2 | 3e | 12 | 90 |

Hypothesis: powertrain-armed / ready-to-drive status. Needs ignition-off
cycle to confirm reset behaviour. Possibly useful for a "ready to move"
derived metric.

#### 0x0B4 — active-drive torque/speed candidate

Bytes 3, 4 stable at `00 00` in P/R/N. In D/B they become non-stable and
take non-zero values (sample seen: `d5 06`). Consistent with electric
motor creeping against held brake in forward/reverse drive modes. Needs
Capture 17 (short drive) to correlate scale with speed / torque.

#### 0x057 — pedal candidate (DLC 4)

Only 4 data bytes. Bytes 0 and 1 roll through 16 unique values per window
in every gear state (no gear correlation). Brake was held throughout the
capture, so brake-stable behaviour is unsurprising. Best-bet candidate for
throttle/brake diff in Capture 16.

### Nulls — no gear-related signal observed

- 0x086, 0x120, 0x121 — bytes 0/1 rolling counters; remaining bytes static
  within this window. No gear reaction.
- 0x0A7, 0x0A8 — rolling counter heads, stable tails. Tail bytes did not
  differ between gear states. Need different stimulation to interpret.
- 0x0B2 — all zeros throughout. Likely motion/drive-only. Wait for Cap 17.
- 0x107 — byte 2 fixed at `fe`, rest zero. No gear reaction.

## CAN error records

18 `2CER` records scattered through the capture (errflags 0x81001001,
0xa3401c01, 0x83001001, 0x23401c01, 0x22401c02, 0x22401c02). Present from
the very first frame onwards — not correlated with gear transitions.
May be related to the OVMS crash root cause (separate investigation).

## Notes

Capture 15 is **usable despite the crash** — the full gear sequence fits
well within the ~34 s that streamed before the stream died. Primary
question (does any candidate carry gear?) answered negative; 0x187 is
authoritative. Two secondary decode leads (0x0B3 latch, 0x0B4 torque)
handed off to Captures 16 and 17 for follow-up.
