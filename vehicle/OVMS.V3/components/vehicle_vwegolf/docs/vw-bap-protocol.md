# VW BAP Protocol Reference

BAP = *Bedien- und Anzeigeprotokoll*. Request/response application protocol on raw CAN — property get/set and function calls between control units. Not ISO-TP/UDS — cannot use the OVMS poller. Not broadcast-style CAN: a channel connects client(s) to one server.

RE sources:
- https://github.com/karlsen-technologies/smartkar-cano-new — `docs/canbus-reverse-engineering/BAP_PROTOCOL.md` (primary; opcode table and array format below)
- https://github.com/norly/revag-bap (earlier; its opcode table is inconsistent with observed e-Golf traffic — superseded)

## Concepts

| Term | Description |
|---|---|
| **FSG** | Functional control unit — the server; provides data, accepts commands |
| **ASG** | Display control unit — the client (OVMS acts as an ASG) |
| **LSG ID** | 6-bit logical control unit ID (0–63); one physical module can host several |
| **Function ID** | 6-bit function within an LSG (0–63); standard functions ≤ 0x0F, device-specific ≥ 0x10 |
| **OpCode** | 3-bit operation type |

Each ASG sends on its own CAN ID; the FSG replies to all ASGs on one shared CAN ID.

## OpCodes

| OpCode | Dir | Meaning |
|---|---|---|
| 0x00 | FSG→ASG | **Reset** — init, version info |
| 0x01 | ASG→FSG | **Get** — request data |
| 0x02 | ASG→FSG | **SetGet** — write + request confirmation |
| 0x03 | FSG→ASG | **HeartbeatStatus** — periodic status |
| 0x04 | FSG→ASG | **Status** — response to Get/SetGet |
| 0x05 | ASG→FSG | **StatusAck** — acknowledge status |
| 0x06 | FSG→ASG | **Ack** — simple acknowledgment |
| 0x07 | FSG→ASG | **Error** — negative response |

## Frame Encoding

### Short message (payload ≤ 6 bytes, byte 0 bit 7 = 0)
```
Byte 0:  [0 | opcode(3) | lsg[5:2](4)]
Byte 1:  [lsg[1:0](2) | function(6)]
Byte 2…: payload
```
DLC = 2 + payload_len.

### Long message start (byte 0 = [1|0|group(2)|0000])
```
Byte 0:  0x80/0x90/0xA0/0xB0 (group 0–3, index always 0)
Byte 1:  total payload length (excl. BAP header)
Byte 2:  [0 | opcode(3) | lsg[5:2](4)]
Byte 3:  [lsg[1:0](2) | function(6)]
Byte 4…7: first 4 bytes of payload
```

### Long message continuation (byte 0 = [1|1|group(2)|index(4)])
```
Byte 0:  e.g. 0xC0…0xCF for group 0
Byte 1…7: next 7 bytes of payload
```
First continuation has index 0 (same as start); index increments per frame, wraps 15→0. Up to 4 concurrent long messages per CAN ID, keyed by group — reassemble on `(can_id, group)`.

## Arrays

Functions returning lists (profiles, providers, destinations) use BAP arrays: a 4–5 byte array header then records. SetGet header: `[ASG-ID:4|Transaction-ID:4]`, `[LargeIdx|PosTransmit|Backward|Shift|RecordAddr:4]`, startIndex, elementCount. Status responses echo the Transaction-ID. **RecordAddr** selects the record format for the same function (full vs compact records, partial updates) — same offset means different fields under different RecordAddr values. Full spec: smartkar `BAP_PROTOCOL.md`.

## Validity Rules

```
opcode ≤ 7 · lsg ≤ 63 · function ≤ 63
short message: payload ≤ 6
```

e-Golf Battery Control channel (LSG 0x25: climate/charge functions, profiles, commands) → `clima-control-bap.md`.
