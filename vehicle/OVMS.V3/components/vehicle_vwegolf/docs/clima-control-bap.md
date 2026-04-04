# Climate Control via BAP — Reverse Engineering Notes

## Overview

Pre-conditioning on the e-Golf is controlled via BAP messages on KCAN (CAN bus 3).
This is entirely separate from the ISO-TP diagnostic path and cannot be handled by the
standard OVMS poller.

Reference: https://github.com/thomasakarlsen/e-golf-comfort-can

---

## CAN IDs

| Direction | CAN ID (extended, 29-bit) | Description |
|---|---|---|
| Controller → Clima ECU | `0x17332501` | Commands (our 3-frame sequences) |
| Clima ECU → Controller | `0x17332501` | Periodic keepalive polls and 5s status bursts |
| Clima ECU → bus | `0x17332510` | Command ACKs and full state broadcasts |

`0x17332501` is **bidirectional**: OVMS writes to it and the clima ECU also sends on it
(keepalives and 4-byte status frames). `0x17332510` carries larger broadcasts.

BAP node address of the clima ECU: **0x25** (decimal 37)

---

## BAP Message Encoding (recap)

Single-frame (DLC ≤ 8, payload ≤ 6 bytes):
```
Byte 0:  [0 | opcode(3) | node[5:2](4)]
Byte 1:  [node[1:0](2) | port(6)]
Byte 2+: payload
```

Multi-frame start (len > 4 bytes of payload):
```
Byte 0:  [1 | 0 | channel(2) | 0(4)]   — channel is always 0 in practice
Byte 1:  total payload length (8-bit in this implementation)
Byte 2:  [0 | opcode(3) | node[5:2](4)]
Byte 3:  [node[1:0](2) | port(6)]
Byte 4+: first 4 bytes of payload
```

Multi-frame continuation:
```
Byte 0:  [1 | 1 | channel(2) | 0(4)]   — 0xC0 for channel 0
Byte 1+: next 7 bytes of payload
```

Note: the BAP spec documents a 12-bit length field, but this implementation uses the
lower 8 bits only (byte 1 alone). Bits 3:0 of byte 0 are observed to be 0 in all
captured frames.

---

## Observed Port Map (node 0x25)

| Port | Direction | Description |
|---|---|---|
| 0x01 | → ECU | Status poll (empty SetGet, 2-byte frame) |
| 0x02 | → ECU | Status poll (empty SetGet, 2-byte frame) |
| 0x12 | ← ECU | Status broadcast (7 bytes, periodic ~1 s) |
| 0x14 | ← ECU | Schedule slot 0 settings (8 bytes) |
| 0x15 | ← ECU | Schedule slot 1 settings (8 bytes) |
| 0x16 | ← ECU / ↔ | Schedule slot 2 settings (8 bytes); Ack confirms writes |
| 0x18 | → ECU | Immediate start/stop trigger (2-byte payload) |
| 0x19 | → ECU | Immediate clima parameters (8-byte payload) |
| 0x1A | ← ECU | Schedule slot Ack / status (observed in schedule captures) |

---

## Periodic Keepalive Frames (head unit → ECU, opcode=1 SetGet)

The car's head unit sends these every ~80 s when schedules are configured:

```
17332501  90 04 19 59  [counter] 00 00 04    (port 0x19, len=4)
17332501  80 04 19 5A  [counter] 02 00 04    (port 0x1A, len=4)
```

`[counter]` is a rolling byte, observed starting around 0x2A, incrementing each cycle.
These carry schedule sync data, NOT start commands.

---

## Climate Start/Stop Sequence

Confirmed from thomasakarlsen's capture (cross-referenced with our KCAN traces).
**Three frames must be sent in order:**

### Frame 1 — Parameters (multi-frame to port 0x19)

```
DLC=8: 80 08 29 59  [counter]  06 00 01
```

`29 59` = BAP: opcode=2 (Status push), node=0x25, port=0x19
`[counter]` = any rolling byte value; increment between calls.

### Frame 2 — Continuation (4 remaining payload bytes)

```
DLC=5: C0  06 00 20 00
```

`C0` = multi-frame continuation, channel 0.
Full 8-byte port 0x19 payload: `[counter] 06 00 01 06 00 20 00`

### Frame 3 — Trigger (single-frame to port 0x18)

```
DLC=4: 29 58  00 01     (START)
DLC=4: 29 58  00 00     (STOP)
```

`29 58` = BAP: opcode=2, node=0x25, port=0x18.
Byte 1 of payload is the on/off flag: `0x01` = start, `0x00` = stop.

---

## Port 0x19 Payload Encoding (partial — needs more captures)

8-byte payload: `[counter] 06 00 01 06 00 20 00`

| Byte | Value | Known meaning |
|---|---|---|
| 0 | rolling | Counter, increments with each call |
| 1 | `0x06` | Unknown — possibly duration (units TBD) |
| 2 | `0x00` | Unknown |
| 3 | `0x01` | Unknown — possibly mode/profile flag |
| 4 | `0x06` | Unknown |
| 5 | `0x00` | Unknown |
| 6 | `0x20` | **Suspected temperature** (see below) |
| 7 | `0x00` | Unknown / padding |

Temperature encoding (byte 6) is not yet confirmed. Candidate formulas:
- Direct Celsius: `0x20` = 32°C
- Offset encoding used by port 0x16 schedule: `temp = (byte - 35)` → -3°C (invalid)

**TODO**: Capture two clima starts with different target temperatures to confirm encoding.
The thomasakarlsen defaults (`06 00 01 06 00 20 00`) are usable until this is resolved.

---

## Schedule Slot Encoding (port 0x14–0x16 status)

8-byte payload from ECU status broadcasts on `17332510`:

```
ff ff ff  [hour]  [temp_or_min]  fe  [slot_id]  00
```

From confirmed captures (schedule capture, temp=20°C, departure=17:00, slot=2):
- Byte 3: `0x11` = 17 = departure hour (direct decimal)
- Byte 4: `0x37` = 55, formula `temp = byte - 35` → 20°C ✓
- Byte 6: `0x02` = schedule slot ID (0–2)

Temperature encoding for schedule writes: **`byte = celsius + 35`**
- 16°C → `0x33`, 20°C → `0x37`, 22°C → `0x39`, 30°C → `0x41`

---

## Why the First Injection Failed

The initial injection attempt used:
```
can can3 tx extended 17332501  91 08  19 96  FF FF FF 14
```

Errors:
1. **Wrong node**: BAP header encoded node=0x26; correct is 0x25 (one bit error in second BAP header byte: `0x96` vs `0x59`)
2. **Wrong port**: encoded port=0x16; start command uses port 0x19
3. **Missing continuation**: payload length byte said 8 bytes but only 4 were delivered — ECU was waiting for the continuation frame
4. **Missing Frame 3**: the port 0x18 start trigger (`29 58 00 01`) was never sent

---

## Confirmed ECU Response Pattern

From `kcan-can3-clima_on_off.crtd` (clima started and stopped via both OVMS shell and
iOS app, all confirmed working).

### Immediate ACK on 0x17332510 (within ~1s)

```
80 0a 49 59  {ectr} 04 46 00   [+ continuation frame with remaining bytes]
```

`49 59` = BAP: opcode=2 (Status), node=0x25, port=0x19. Multi-frame, length=10.
`{ectr}` = our rolling counter | 0x80. Use this to confirm which command was ACKed.

### Periodic status on 0x17332501 (~16s delay, then every 5s for ~20s)

After the ACK, the ECU sends ~4 cycles at 5-second intervals, then goes quiet:

```
3R29 17332501  19 42                          (keepalive, port 0x02, node 0x25)
3R29 17332501  19 41                          (keepalive, port 0x01, node 0x25)
3R29 17332501  90 04 19 59 {ctr} 00 00 04    (ch1, port 0x19, 4-byte status)
3R29 17332501  80 04 19 5a {ctr} 02 00 04    (ch0, port 0x1a, 4-byte status)
```

`{ctr}` is a 3-bit rolling counter cycling 0x28–0x2F. The ECU goes silent after the
burst and only resumes when the next command is issued. Status payload bytes (`00 00 04`,
`02 00 04`) are constant across start and stop in current captures — full decode pending
a capture where the 10-minute timer expires.

### Full state dump on 0x17332510 (alongside ACK)

The ECU broadcasts a complete state on `0x17332510`:

- **Port 0x19** (130 bytes, channel 0, `80 82 49 59 ...`): appears static across calls;
  contains schedule data and component identification strings.
- **Port 0x1a** (channel 1): length and a header byte change with clima state:
  - 34 bytes (`90 22`), header byte `c0`: seen in idle/no-active state
  - 25 bytes (`90 19`), header byte `c2`: seen when clima is running (bit 1 of header byte differs)

The `c0` → `c2` transition (bit 1) is a candidate clima-active flag. Not yet confirmed —
needs a controlled before/after capture.

### Note on port 0x18 trigger frame

One observed start command had Frame 3 transmitted as `29 58 00 00` (stop byte) due to
a stale TX queue entry, yet clima started normally ~16s later. This suggests port 0x19
parameters (Frame 1+2) may alone trigger climatisation, with port 0x18 Frame 3 playing
a confirmatory or redundant role. The code still sends all 3 frames.

## Test Commands

To test clima start (from OVMS shell):

```
can can3 tx extended 17332501 80 08 29 59 01 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 01
```

To stop:

```
can can3 tx extended 17332501 80 08 29 59 02 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 00
```

Observable bus effects: `0x3E9` transitions from `fe f8 df ff ...` (ECU idle/invalid)
to live values, and `0x3B5`/`0x530` follow. Blower should activate within ~10 s.

---

## KCAN Bus Wake-Up

The KCAN bus enters a low-power sleep state when the car is parked. CAN transceivers
wake on detection of dominant bits (any CAN frame). The first frames to appear after
wake are Network Management (NM) frames from each node.

### NM Frame Format (OSEK NM, observed on KCAN)

| Field | Value |
|---|---|
| CAN ID | `0x1B000000 \| node_id` (29-bit extended) |
| Payload byte 0 | node_id (echoes lower byte of CAN ID) |
| Payload byte 1 | NM control (0x00 = no sleep request, 0x10 seen on some nodes) |
| Payload bytes 2–7 | NM state data (varies by node) |

Example observed after wake (`kcan-can3-clima_start2.crtd`):
```
3R29 1B00000E  0e 00 01 01 04 08 04 00   node 0x0E
3R29 1B000067  67 10 01 01 04 00 00 00   node 0x67
3R29 1B0000A9  a9 00 01 01 04 00 00 00   node 0xA9
3R29 1B000046  46 00 01 01 04 00 00 00   node 0x46
```

Clima ECU (node 0x25) NM ID would be `0x1B000025`. Observing this frame in RX
confirms the clima ECU is alive on the bus.

### Wake Ping

To wake the bus before sending a command sequence, send a short harmless frame.
Even if it TX_Fails (no ACK — nodes still asleep), the dominant bits wake the
transceivers. Subsequent frames ~150 ms later will succeed.

Recommended wake ping — BAP Get on port 0x01 (2-byte read query, non-destructive):
```
can can3 tx extended 17332501 09 41
```
`09 41` = opcode=0 (Get), node=0x25, port=0x01. Safe to send; ECU may or may not reply.

### Why the multi-frame start frame must not be used as a wake ping

When the start frame TX_Fails, the continuation frame is delayed ~110 ms (bus waking).
The ECU receives an orphan continuation without a valid preceding start and discards the
partial BAP message. The trigger frame then fires with no parameters set → no HVAC.

Correct sequence when bus state is unknown:
1. Send wake ping (TX_Fail is fine — bus wakes from dominant bits)
2. Wait 150–200 ms for RX traffic to confirm bus is live
3. Send the 3-frame clima sequence (all frames will be ACK'd)

---

## Implementation Bugs Found During Development

### Bug 1: OCU heartbeat data overwrite (critical — caused clima to stop working)

`SendOcuHeartbeat()` built the correct bit pattern for each pending action (horn, mirror
fold, lock, etc.) into a local `data[]` array, then unconditionally re-zeroed the entire
array before calling `WriteStandard`. The frame always transmitted all zeros; no one-shot
action ever reached the bus.

This is why clima "worked then stopped": it worked when tested via the raw shell commands
(`can can3 tx extended ...`) but once routed through `CommandClimateControl` any
subsequent state that depended on the heartbeat being correct (e.g. OCU presence
acknowledged by the gateway) was broken.

**Fix:** removed the re-initialization block at the bottom of `SendOcuHeartbeat`. The
array is zero-initialized once at the top (`uint8_t data[8] = {};`); action bits are OR'd
in as needed; the array is sent as-is.

### Bug 2: Multi-frame start used as implicit wake stimulus

When `CommandClimateControl` was called with the bus sleeping, it called `CommandWakeup`
(which sends two NM-style extended frames) then waited 300 ms before sending the BAP
sequence. If the NM frames TX_Failed (no ACK from sleeping nodes), the wait was sometimes
not long enough: Frame 1 of the BAP sequence also TX_Failed, the ESP32 introduced a
~110 ms bus-off recovery gap, and Frame 2 arrived at the ECU as an orphaned continuation.
The ECU discarded the incomplete BAP message and Frame 3 fired with no parameters.

**Fix:** `CommandClimateControl` now sends a harmless BAP Get on port 0x01 (`09 41`) as
the wake stimulus before the command sequence. A Get can fail safely — it carries no
multi-frame state. After 200 ms the BAP command sequence is sent when the bus is live.
`CommandWakeup` is retained as a standalone full-system wake command.

### Bug 3: 0x05EA StandklimaStatus_02 shift error

The original decode of `StandklimaStatus_02` from 0x05EA used `(d[3] & 0x38) >> 6`.
`0x38` masks bits 5:3; shifting right by 6 pushes all of them below bit 0 → result is
always 0. The correct shift is `>> 3`. Fixed in the refactor. This field is only logged
at verbose level for observability, so it had no functional impact, but the logged value
was always wrong.

---

## Capture Log for This Investigation

| File | What it captured |
|---|---|
| `kcan-can3-clima_schedule.crtd` | Setting a schedule (departure 17:00, 20°C, slot 2) |
| `kcan-can3-clima_range.crtd` | Adjusting clima temp dial (ignition on, 20°C → LO → HI → off) |
| `kcan-can3-clima_inject.crtd` | First injection attempt (failed — wrong node/port/no continuation) |
| `clima-sequence.crtd` | Bus while clima actively running (pre-existing active state) |
| `kcan-can3-clima_start2.crtd` | Start/stop injection attempt from sleeping bus — TX_Fail on start frame, confirmed wake behaviour |
| `kcan-can3-clima_on_off.crtd` | Confirmed working start/stop from OVMS shell and iOS app (TurboServ); ECU ACK and status burst pattern captured |
