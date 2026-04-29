# can2-3.3.005-917-g5ecab373-dirty_ota_0_main-20260429-202450.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-917-g5ecab373-dirty/ota_0/main` |
| Bus | can2 (FCAN) |
| Captured | 20260429-202450 |
| Duration | ~316 s |
| Frames | 403977 |
| Capture ref | **Capture 22** — full AC charge session start-to-finish (Type 1 J1772) |

## Sequence

Full AC charge sequence: cable plug-in through sustained charging to capture stop.
Companion captures cap 23 (mid-session) and cap 24 (stop sequence) were taken
during a separate session the same night, same firmware.

---

## Notes

### Bus topology — OBC frames on FCAN

All frames are `2R11` (FCAN, bus 2). IDs 0x569, 0x594, 0x5EA etc. are OBC /
ChargeCtrl ECU frames that **originate on FCAN**; the J533 gateway bridges them
onto KCAN for the head unit. The `IncomingFrameCan2` handler forwards them to
`IncomingFrameCan3` so the decode path is shared.

### 0x569 OBC_AcInlet — decode confirmed

687 frames over 315.5 s. Byte layout validated:

| Byte | Field | Encoding | Observed |
|---|---|---|---|
| d[4] | AC line voltage | 1 V/bit | 248 V (0xf8), 250 V (0xfa) at start |
| d[5] | AC charge current | 1 A/bit | 0, 1, 8, 9 A |
| d[1] bit 6 | charging-active flag | 1 = charging | 0x40 (bit6 only), 0xf0 (bits 4–7) |

**AC power during active charging:**
- 248 V × 8 A = 1984 W (89 frames, early phase with d[1]=0xf0)
- 248 V × 9 A = 2232 W (421 frames, main phase with d[1]=0x40)

Charging flag `d[1] & 0x40` reliably distinguishes idle-with-cable-plugged from
active delivery. Note d[1]=0xf0 also has bit 6 set; the upper nibble may encode
OBC control-pilot state or negotiation phase.

### 0x569 trajectory (payload transitions)

```
t(s)    d0 d1 d2 d3 d4 d5 d6 d7   V    A   chg?  phase
  0.49  00 00 80 0e fa 00 00 00   250   0   no    cable plugged, EVSE reports 250V
  1.04  00 00 00 0e fa 00 00 00   250   0   no
  5.09  00 00 01 0e f8 00 00 00   248   0   no    voltage settles to 248V
  6.74  00 00 01 0a f8 00 00 00   248   0   no
 13.09  00 f0 01 0a f8 00 00 00   248   0   yes   charging-active set, no current yet
 13.19  00 f0 2d 0a f8 08 00 00   248   8   yes   Phase 1: 8A, d[1]=0xf0
 59.49  00 f0 01 0a f8 00 00 00   248   0   yes   Phase 1 end: current drops to 0
 59.54  00 00 01 0a f8 00 00 00   248   0   no    charging flag clears
104.99  00 00 01 02 f8 00 00 00   248   0   no    d[3] change (state machine)
128.04  00 00 01 02 f8 01 00 00   248   1   no    1A transient before charging-active
131.69  00 40 03 02 f8 09 00 00   248   9   yes   Phase 2: 9A, d[1]=0x40 (sustained)
  ...   (248V/9A continues to end of capture at ~316s)
```

### 0x594 ChargeManagement — state-machine trace

Confirms two distinct charging phases in this session.

```
t(s)    b0 b1 b2 b3 b4 b5 b6 b7   phase
  0.19  00 e0 1f 83 7f 00 00 1e   cable handshake (OBC startup)
  0.68  00 00 00 03 00 00 10 0d   idle
125.68  00 d0 01 03 00 00 30 0d   ramp up (b1=d0, CP negotiation)
128.01  00 d0 01 03 00 30 30 0d   b5→30 (EVSE ready)
128.68  00 60 07 c3 01 30 30 0d   b3=c3 — CP ok
131.68  00 60 07 e3 01 30 30 0d   b3→e3, b4=01 — delivering power
132.68  00 60 07 e3 01 34 30 0d   b5→34 (energy counter incrementing)
141.68  00 70 07 e3 01 34 30 0d   b1 oscillates 0x60/0x70/0x80/0x90 (OBC PWM/thermal)
  ...   (sustained charge; b1 oscillates, rest stable until end of capture)
```

b4=0x01 during Phase 2 (cf. b4=0x06 in cap 14 cap13 short-session). Appears to
reflect a different OBC operating mode or negotiated current level.
