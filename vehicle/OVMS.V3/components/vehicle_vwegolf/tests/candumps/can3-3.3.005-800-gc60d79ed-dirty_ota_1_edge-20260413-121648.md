# can3-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260413-121648.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_1/edge` |
| Bus | can3 |
| Captured | 20260413-121648 |
| Duration | ~81s |
| Frames | 57975 |

## Sequence

capture 14, ac type 2 ctek chargestorm, RFID start. Charger delivered ~9 s of
active power before the session self-stopped. Companion file
`can2-…-20260413-121212.md` holds the OVMS event-log timeline.

---

## Notes

### 0x594 ChargeManagement — state-machine trace (confirms existing decode)

Clean transitions through a short AC Type 2 session:

```
t       b0 b1 b2 b3 b4 b5 b6 b7   phase
 0.17   00 00 20 03 00 00 18 09   idle, no cable
23.11   00 00 23 03 00 30 38 09   b2 20→23, b5 00→30 (plug + handshake)
23.67   00 f0 27 83 02 30 38 09   b1=f0, b2=27, b3 high nibble 8 — CP ok
25.67   00 f0 27 c3 06 30 38 09   b3 →c3, b4 →06 — delivering power
26.17   00 f0 27 e3 06 30 38 09   b3 →e3
27.67   00 f0 27 e3 06 34 38 09
32.17   00 c0 06 23 00 34 38 09   b1 f0→c0, b2 27→06 — charge stop
32.67   00 c0 06 03 00 34 38 09   post-charge hold
58.27   00 c0 06 03 00 14 28 09
59.67   00 00 00 03 00 00 18 09   fully idle
```

Active-charge window: **T+23.1 → T+32.2 (~9 s)**. Post-charge hold (b1=0xc0)
continues to T+59.7. Matches existing dbc decode of ChargeInProgress, ChargeType,
ChargeDurationFull.

### 0x1A5554A8 — OBC live-telemetry, CONFIRMED multiplexed (was dbc candidate)

Prior captures flagged this as OBC live-telemetry without a confirmed frame
structure. Capture 14 locks it down.

- DLC 8, sparse (~1.4 Hz avg, 2 Hz during charge).
- Idle sentinel: `03 fe ff ff e7 ff ff ff` (present while bus alive, no charge).
- Zeros frame: `00 00 00 00 00 00 00 00` emitted once during teardown gap.
- **Alive during charge T+21.7 → T+37.2** — wraps the 0x594 active window,
  leads it by ~1.5 s, lingers ~5 s into the 0xc0 post-charge hold.

Multiplexed by byte 0. Two mux IDs interleave at 1 Hz each:

```
Mux A   b0 ∈ {0x68, 0x6c}   c9 aa 9c 83 b6 ed 0e   (static payload)
Mux B   b0 ∈ {0x51, 0x55}   5d 00 80 <E> c2 dc 69  (b4 = live counter)
```

- b0 bit 2 toggles every frame (0x68↔0x6c, 0x51↔0x55) — rolling alive bit.
- b0 upper nibble selects mux (0x6X = A, 0x5X = B).
- **Mux B byte 4 increments +1 per second while charging** (observed values
  0x83, 0x9e, 0xaa..0xb8 across the window — monotonic aside from the
  pre-charge handshake gap). Matches prior capture's 164→170→171→172→173→174
  observation. Consistent with a charge-energy / charge-time totalizer with
  ~1-second ticks. Exact unit (Wh? decawatt-seconds? minutes of charge?)
  still TBD — need a longer session with a known energy delivery to calibrate.
- Mux A payload was static across the whole session — probably a fixed
  identifier / pack hash / firmware stamp, not live telemetry.
- Other Mux B bytes (b1=0x5d, b2=0x00, b3=0x80, b5=0xc2, b6=0xdc, b7=0x69)
  were static this session. Need higher-SoC or higher-power charge to see
  whether they carry current/voltage/power.

### 0x3C0 — ruled out as charge signal

Dense trajectory across the whole capture shows 0x3C0 is a 4-byte AUTOSAR-style
counter + signature pair, not charge data:
- b1 is a monotonic 4-bit counter (0→f, wraps every 1.6 s at 10 Hz).
- b0 is a lookup-table scramble of b1 (CRC/signature).
- b2, b3 always 0x00.
- Values identical inside and outside the charge window.

Earlier "looks charge-correlated" read was a sampling artifact (3 widely-spaced
samples happened to land on different phases of the counter). Rule out.

### Other candidates, not yet characterised

Flagged during the pre/mid/post sweep (see scratch/ for the sweep script)
but not yet decoded:

- `0x5EB` b5 `0x83↔0xc3` — correlated to charge window.
- `0x663` b4–b6 change across charge phases.
- `0x485` b7 flips to `0x3f` post-charge — end-of-charge flag candidate.
- `0x584`, `0x569`, `0x556` — transitions at charge start.
- `0x12DD546F`, `0x12DD5472` — new BAP app-layer frames, not yet in dbc
  (0x12DD prefix = BAP channel; 0x12DD5471 is the already-known
  `BAP_ClimaStatus`).

### Not present

- `0x191 BMS_PowerBus` — absent for the entire capture. Expected to carry
  HV bus current/voltage during charge. Either the BMS holds this frame off
  the bus during short AC sessions or the publishing ECU was still asleep.
  Worth confirming against a longer AC charge capture.

### TP 2.0 background traffic

The `0x17330XXX` / `0x17F0XXXX` / `0x1B0000XX` / `0x16A954XX` 29-bit clusters
were all active during the session (hundreds of frames each). Consistent with
a diagnostic tester session + AUTOSAR NM activity during charge handshake —
not of immediate interest for charge decode but recorded for future BAP /
diagnostic work.
