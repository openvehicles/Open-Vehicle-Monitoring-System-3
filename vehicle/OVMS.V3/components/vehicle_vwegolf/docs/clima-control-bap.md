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
| Controller → Clima ECU | `0x17332501` | Commands and periodic keepalives |
| Clima ECU → bus | `0x17332510` | Status broadcasts and Acks |

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

Expected ECU response on `17332510`:
- Port 0x19 Ack (confirmed from schedule capture): `80 82 49 59 [counter] 04`
  - `49 59` = opcode=4 (Ack), node=0x25, port=0x19
  - Payload is 2 bytes: echoed counter + `0x04` status byte
- Port 0x18 Ack: not yet observed (port 0x18 only activates in remote mode, not ignition-on)

Observable bus effects: `0x3E9` transitions from `fe f8 df ff ...` (ECU idle/invalid)
to live values, and `0x3B5`/`0x530` follow. Blower should activate within ~10 s.

---

## Capture Log for This Investigation

| File | What it captured |
|---|---|
| `kcan-can3-clima_schedule.crtd` | Setting a schedule (departure 17:00, 20°C, slot 2) |
| `kcan-can3-clima_range.crtd` | Adjusting clima temp dial (ignition on, 20°C → LO → HI → off) |
| `kcan-can3-clima_inject.crtd` | First injection attempt (failed — wrong node/port/no continuation) |
| `clima-sequence.crtd` | Bus while clima actively running (pre-existing active state) |
