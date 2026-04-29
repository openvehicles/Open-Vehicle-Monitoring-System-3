# can2-3.3.005-917-g5ecab373-dirty_ota_0_main-20260429-022911.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-917-g5ecab373-dirty/ota_0/main` |
| Bus | can2 (FCAN) |
| Captured | 20260429-022911 |
| Duration | ~84 s |
| Frames | 98810 |
| Capture ref | **Capture 24** — AC charge stop sequence (Type 1 J1772) |

## Sequence

Session was charging at 9 A when capture started. Covers the ramp-down and
post-charge hold. Companion captures: cap 22 (full session start-to-finish)
and cap 23 (mid-session steady-state), same firmware, different sessions same night.

---

## Notes

### 0x569 OBC_AcInlet — charge stop ramp-down

170 frames over 84.1 s. Three distinct payload states, two transitions:

| t/s | d0 | d1 | d2 | d3 | d4 | d5 | d6 | d7 | V | A | charging? | phase |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| 0.21 | 00 | 40 | 01 | 02 | f8 | 09 | 00 | 00 | 248 | 9 | yes | initial: 9 A charging |
| 19.41 | 00 | 40 | 01 | 02 | f8 | 08 | 00 | 00 | 248 | 8 | yes | ramp: 9->8 A |
| 21.51 | 00 | 00 | 01 | 02 | f8 | 00 | 00 | 00 | 248 | 0 | no | stop: flag clears, 0 A |

Key observations:
- **t=19.41**: current steps 9->8 A while d[1] (charging flag) remains 0x40.
  Brief intermediate step before OBC disengages.
- **t=21.51**: d[1] clears from 0x40 to 0x00 simultaneously with current
  reaching 0 A. Single-frame transition — no intermediate states observed.
- **Voltage 248 V persists** throughout (EVSE line voltage still on pilot;
  cable physically still connected after charge stops).
- 148 frames remain after t=21.51 at `00 00 01 02 f8 00 00 00` (cable
  still plugged, OBC idle) until end of capture at t=84.1 s.

### 0x594 ChargeManagement — stop sequence

167 frames. State machine transitions from active charge through post-charge
hold back to idle:

| t/s | b0 | b1 | b2 | b3 | b4 | b5 | b6 | b7 | phase |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 0.36 | 00 | 10 | 07 | a3 | 01 | 34 | 30 | 0d | charging (Phase 2, b4=0x01) |
| 19.41 | 00 | 10 | 07 | 03 | 00 | 14 | 30 | 0d | stop: b4 clears 0x01->0x00, b5 drops |
| 19.86 | 00 | 10 | 07 | 03 | 00 | 04 | 30 | 0d | b5 continues stepping down |
| 20.86 | 00 | 00 | 00 | 03 | 00 | 00 | 30 | 0d | idle: b1/b2/b4/b5 all clear |
| 68.85 | 00 | e0 | 1f | 83 | 7f | 00 | 00 | 1e | cable handshake pattern (brief) |
| 69.35 | 00 | 00 | 00 | 03 | 00 | 00 | 30 | 0d | returns to idle |

b4 clearing from 0x01->0x00 at t=19.41 is simultaneous with the 0x569 current
drop -- both frame types agree on the stop event timing. b5=0x34 during Phase 2,
steps down through 0x14->0x04->0x00 as the OBC winds down. The 0xe0/1f/83/7f
pattern at t=68.85 is the same cable-handshake sentinel seen at session start
in cap 22; transient with no charge-active flag set.
