# can2-3.3.005-917-g5ecab373-dirty_ota_0_main-20260429-020735.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-917-g5ecab373-dirty/ota_0/main` |
| Bus | can2 (FCAN) |
| Captured | 20260429-020735 |
| Duration | ~180 s |
| Frames | 171401 |
| Capture ref | **Capture 23** — mid-session steady-state AC charge (Type 1 J1772) |

## Sequence

Session was already in progress when capture started — joined at sustained 9 A.
No transitions observed. Companion captures: cap 22 (full session start-to-finish)
and cap 24 (stop sequence), same firmware, different sessions same night.

---

## Notes

### 0x569 OBC_AcInlet — steady-state charging

356 frames over 180.1 s. Payload is identical for every frame in the capture:

| t/s | d0 | d1 | d2 | d3 | d4 | d5 | d6 | d7 | V | A | charging? |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 0.35 | 00 | 40 | 01 | 02 | f8 | 09 | 00 | 00 | 248 | 9 | yes |
| 0.85 | 00 | 40 | 01 | 02 | f8 | 09 | 00 | 00 | 248 | 9 | yes |
| ... | | | | | | | | | | | (all 356 frames identical) |
| 179.84 | 00 | 40 | 01 | 02 | f8 | 09 | 00 | 00 | 248 | 9 | yes |

Single payload `00 40 01 02 f8 09 00 00` throughout. Confirms steady-state
Phase 2 charging: 248 V x 9 A = 2232 W, d[1]=0x40 (bit 6 set throughout).
Zero current fluctuation at sustained load.

### 0x594 ChargeManagement — steady-state

355 frames. Two payload variants (b1 oscillates 0x30/0x50, OBC thermal/PWM
cycling). All other bytes stable throughout:

| t/s | b0 | b1 | b2 | b3 | b4 | b5 | b6 | b7 | note |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 0.48 | 00 | 30 | 07 | e3 | 01 | 34 | 30 | 0d | b1=0x30 variant |
| (varies) | 00 | 50 | 07 | e3 | 01 | 34 | 30 | 0d | b1=0x50 variant |
| 179.98 | 00 | 50 | 07 | a3 | 01 | 34 | 30 | 0d | b3 shifts 0xe3->0xa3 near end |

b4=0x01, b5=0x34 stable throughout -- same Phase 2 pattern as cap 22 from
t=132 s onward. b1 oscillation is OBC internal state, not a charging-active
indicator. b3 shift at end is OBC thermal management, still actively charging.
