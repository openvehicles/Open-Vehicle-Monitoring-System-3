# Climate Control via BAP — RE Notes

Reference: https://github.com/thomasakarlsen/e-golf-comfort-can

## CAN IDs

| Direction | CAN ID (29-bit) | Description |
|---|---|---|
| OVMS → Clima ECU | `0x17332501` | Commands |
| Clima ECU → bus | `0x17332501` | Keepalive polls, 5s status bursts |
| Clima ECU → bus | `0x17332510` | Command ACKs, full state broadcasts |

BAP node address of clima ECU: **0x25**

BAP frame encoding (single/multi-start/multi-cont), opcodes, validity rules → `vw-bap-protocol.md`.

---

## Port Map (node 0x25)

| Port | Dir | Description |
|---|---|---|
| 0x01 | → ECU | Status poll (empty SetGet, 2-byte) |
| 0x02 | → ECU | Status poll (empty SetGet, 2-byte) |
| 0x12 | ← ECU | Status broadcast (7 bytes, ~1 s) · payload[0]: `05`=active `00`=idle |
| 0x13 | ← ECU | Status · `04 04`=active `04 00`=idle |
| 0x14 | ← ECU | Schedule slot 0 (8 bytes) |
| 0x15 | ← ECU | Schedule slot 1 (8 bytes) |
| 0x16 | ← / ↔ | Schedule slot 2 (8 bytes); Ack confirms writes |
| 0x18 | → ECU | Start/stop trigger (2-byte payload) |
| 0x19 | → ECU | Clima parameters (8-byte payload) |
| 0x1A | ← ECU | Schedule slot Ack / status |

**Authoritative stop detection:** port 0x12 payload[0] `05`→`00` (works for both OVMS-triggered and schedule-triggered stops). Do NOT rely on `0x5EA` remote_mode — stale in remote mode.

---

## Climate Start/Stop — 3-Frame Sequence

Must send all 3 frames in order on `0x17332501`.

### Frame 1 — Parameters (multi-frame, port 0x19)
```
DLC=8: 80 08 29 59  [counter]  06 00 01
```
`29 59` = opcode=2, node=0x25, port=0x19. `[counter]` = rolling byte, increment each call.

### Frame 2 — Continuation
```
DLC=5: C0  06 00 [temp] 00
```
Full 8-byte port 0x19 payload: `[counter] 06 00 01 06 00 [temp] 00`

### Frame 3 — Trigger (port 0x18)
```
DLC=4: 29 58  00 01     (START)
DLC=4: 29 58  00 00     (STOP)
```

---

## Port 0x19 Payload Encoding

`[counter] 06 00 01 06 00 [temp] 00`

Temperature byte 6: `raw = (celsius - 10) * 10`
- 18°C → `0x50`, 20°C → `0x64`, 21°C → `0x6E`, 22°C → `0x78`

Bytes 1–5 unknown; `06 00 01 06 00` constant in all captures.

---

## Schedule Slot Encoding (port 0x14–0x16)

8-byte ECU status: `ff ff ff [hour] [temp_byte] fe [slot_id] 00`

- Byte 3: departure hour (direct decimal, e.g. `0x11` = 17:00)
- Byte 4: `celsius + 35` (e.g. 20°C → `0x37`)
- Byte 6: slot ID (0–2)

---

## KCAN Bus Wake-Up

### NM Alive Frame (CRITICAL — required from deep sleep)

Clima ECU rejects BAP from nodes not in OSEK NM ring. Send before any BAP command when bus was sleeping:

```
CAN ID:  0x1B000067  (29-bit extended)
DLC:     8
Data:    67 10 41 84 14 00 00 00
```

- `0x67` = OVMS KCAN NM node ID
- Byte 1 = `0x10` = NM alive with ring participation bit
- Wait ≥ 1 s for ring to settle before sending BAP

Implemented in `CommandWakeup()`, called by `CommandClimateControl()` when `m_bus_idle_ticks >= VWEGOLF_BUS_TIMEOUT_SECS`.

### Full Wake Sequence (bus sleeping)

1. Send `0x17330301` (dominant-bit wake, TX_Fail expected — wakes transceivers)
2. Send `0x1B000067` NM alive (joins ring)
3. Wait 1 s
4. Send 3-frame clima sequence

**Warning:** do not use the start frame as wake ping — if TX_Fail, continuation arrives orphaned (~110 ms later), ECU discards partial BAP message, clima doesn't start.

### Wake Ping (bus already active)
```
can send can3 17332501 09 41
```
Sufficient only when KCAN already live (e.g. just after ignition-off). Not for deep sleep.

---

## Test Commands (OVMS shell)

Start:
```
can can3 tx extended 17332501 80 08 29 59 01 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 01
```

Stop:
```
can can3 tx extended 17332501 80 08 29 59 02 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 00
```

Bus effects: `0x3E9` transitions from sentinel to live values; `0x3B5`/`0x530` follow. Blower activates within ~10 s.

---

## ECU ACK Pattern (0x17332510)

Immediate ACK (~1 s after command):
```
80 0a 49 59  {ectr} 04 46 00   [+ continuation]
```
`{ectr}` = our counter | 0x80. Confirms which command was ACKed.

After ACK: ECU sends ~4 keepalive cycles on `17332501` at 5 s intervals (~16 s delay), then silent until next command.
