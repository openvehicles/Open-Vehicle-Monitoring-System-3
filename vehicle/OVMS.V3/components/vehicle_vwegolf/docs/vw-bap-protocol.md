# VW BAP Protocol Reference

BAP = *Bedien- und Anzeigeprotokoll*. Runs on raw CAN. Used by VAG vehicles for comfort ECUs (climate, media, etc.). Not ISO-TP/UDS — cannot use OVMS poller.

RE source: https://github.com/norly/revag-bap

## Concepts

| BAP concept | Analogue | Description |
|---|---|---|
| Node (LSG) | IP address | 6-bit logical control unit (0–63) |
| Port | UDP port | RPC port / status register, 6-bit (0–63) |
| Opcode | Method | Request/reply type, 3-bit (0–7) |

One CAN ID carries all BAP traffic for one direction between two nodes.

## Frame Encoding

### Single-frame (payload ≤ 6 bytes, byte 0 bit 7 = 0)
```
Byte 0:  [0 | opcode(3) | node[5:2](4)]
Byte 1:  [node[1:0](2) | port(6)]
Byte 2…: payload
```
DLC = 2 + payload_len.

### Multi-frame start (byte 0 bit 7 = 1, bit 6 = 0)
```
Byte 0:  [1 | 0 | channel(2) | len[11:8](4)]
Byte 1:  len[7:0]
Byte 2:  [0 | opcode(3) | node[5:2](4)]
Byte 3:  [node[1:0](2) | port(6)]
Byte 4…7: first 4 bytes of payload
```

### Multi-frame continuation (byte 0 bit 7 = 1, bit 6 = 1)
```
Byte 0:  [1 | 1 | channel(2) | 0(4)]
Byte 1…7: next 7 bytes of payload
```

## Opcodes

| Opcode | Dir | Meaning |
|---|---|---|
| 0x0 | → ECU | **Get** — request current value |
| 0x1 | → ECU | **SetGet** — write + request new status |
| 0x2 | ← ECU | **Status** — ECU pushes / replies |
| 0x3 | ← ECU | **Error** — negative response |
| 0x4 | ← ECU | **Ack** — write acknowledged |
| 0x6 | → ECU | **Set** — write, no reply |

## Validity Rules

```
opcode ≤ 7 · node ≤ 63 · port ≤ 63 · len ≤ 4095
single-frame: len ≤ 6
```

e-Golf specific CAN IDs, port map, commands → `clima-control-bap.md`.
