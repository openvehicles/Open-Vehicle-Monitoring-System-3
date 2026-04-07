# VW BAP Protocol Reference

**BAP** = *Bedien- und Anzeigeprotokoll* (Control and Display Protocol)

This document describes the BAP protocol as reverse-engineered from wire traces on a VW Golf Mk 6, specifically between an RCD 310 head unit and an MDI device. The original reverse-engineering work is by Max Staudt: https://github.com/norly/revag-bap

BAP runs on top of raw CAN frames and is used by VW/Audi group vehicles to control subsystems such as climate control, media interfaces, and other comfort ECUs. It is distinct from ISO-TP/UDS and is not routed through the standard OBD2 diagnostic path.

---

## Concepts

BAP is analogous to a small network stack sitting on CAN:

| BAP concept | Network analogue | Description |
|---|---|---|
| Node (LSG) | IP address | Logical control unit (*Logisches Steuergerät*), 6-bit (0–63) |
| Port | UDP port | RPC port or status register ID, 6-bit (0–63) |
| Opcode | Method | Request/reply type, 3-bit (0–7) |
| Frame | Packet | Carries one BAP message, single or multi-CAN-frame |

A single CAN ID carries all BAP traffic for one direction between two nodes. Multiple logical nodes and ports are multiplexed over that one CAN ID.

---

## CAN Frame Encoding

### Single-Frame (payload ≤ 6 bytes)

Bit 7 of byte 0 is **0**.

```
Byte 0:  [0 | opcode(3) | node[5:2](4)]
Byte 1:  [node[1:0](2) | port(6)]
Byte 2…: payload (0–6 bytes)
```

Decoded as a 16-bit big-endian header:

```
Bits 15:    0           → single-frame marker
Bits 14–12: opcode      → 3-bit operation code
Bits 11–6:  node        → 6-bit LSG address
Bits  5–0:  port        → 6-bit port/register
```

CAN DLC = 2 + payload length.

**Example** — opcode=2, node=0x0F, port=0x01, payload `AA BB`:
```
header = (2 << 12) | (0x0F << 6) | 0x01 = 0x23C1
Byte 0 = 0x23, Byte 1 = 0xC1, Byte 2 = 0xAA, Byte 3 = 0xBB
DLC = 4
```

---

### Multi-Frame Start Frame (payload > 6 bytes, first CAN frame)

Bit 7 of byte 0 is **1**, bit 6 is **0**.

```
Byte 0:  [1 | 0 | channel(2) | len[11:8](4)]
Byte 1:  [len[7:0](8)]
Byte 2:  [0 | opcode(3) | node[5:2](4)]   ← same header format as single-frame
Byte 3:  [node[1:0](2) | port(6)]
Byte 4…7: first 4 bytes of payload
```

- **channel**: slot 0–3, identifies which concurrent multi-frame stream this belongs to
- **len**: total payload length (12-bit, 1–4095)

CAN DLC = 4 + min(payload_len, 4).

---

### Multi-Frame Continuation Frame

Bit 7 of byte 0 is **1**, bit 6 is **1**.

```
Byte 0:  [1 | 1 | channel(2) | don't care(4)]
Byte 1…7: next up to 7 bytes of payload
```

CAN DLC = 1 + bytes_in_this_frame.

Continuation frames are sent until `len` total bytes have been delivered. Up to 8 concurrent incomplete multi-frame receptions are tracked (channels 0–3 on TX, 0–7 on RX).

---

## Opcodes

The opcode field is 3 bits (values 0–7). Based on the protocol context and the tmbinc talk (see Acknowledgements in the source repo), the known values are:

| Opcode | Direction | Meaning |
|---|---|---|
| 0x0 | → ECU | **Get** — request current value of a port |
| 0x1 | → ECU | **SetGet** — write a value and request the new status |
| 0x2 | ← ECU | **Status** — ECU pushes or replies with current value |
| 0x3 | ← ECU | **Error** — negative response |
| 0x4 | ← ECU | **Ack** — acknowledgement of a write |
| 0x6 | → ECU | **Set** — write a value (no reply expected) |

Note: not all opcodes may be used by all ECUs. Opcodes not listed here have not been observed in the available traces.

---

## Frame Validity Rules

From `vag_bap_frame_is_valid()`:

```
opcode ≤ 7          (3-bit field)
node   ≤ 63         (6-bit field)
port   ≤ 63         (6-bit field)
len    ≤ 4095       (12-bit field)
if single-frame: len ≤ 6
```

---

## CAN IDs

BAP does not define CAN IDs itself — they are vehicle-specific. Each ECU pair uses a fixed CAN ID for each direction. The CAN ID carries all BAP traffic between those two nodes; the node and port fields inside the frame distinguish individual services.

On a VW Golf Mk 6 (and likely e-Golf), the relevant CAN IDs must be determined from bus traces. The KCAN (convenience CAN, bus 3 in the OVMS e-Golf config) carries most comfort function traffic including climate control.

---

## Relevance to VW e-Golf Climate Control

Climate control on the e-Golf is managed by an ECU that communicates via BAP on the KCAN, not via standard ISO-TP UDS requests. To command the climate system (e.g. start pre-conditioning), the OVMS must:

1. Identify the correct CAN ID for the climate ECU on KCAN
2. Know the ECU's BAP node address and the relevant port numbers
3. Construct and transmit correctly encoded BAP frames (use opcode `SetGet` or `Set`)
4. Parse Status responses (opcode `Status`) to read back the current state

Because BAP is not ISO-TP, it cannot be handled by the OVMS `PollSetPidList` / `IncomingPollReply` poller. It must be implemented by directly constructing CAN frames in `IncomingFrameCan3()` and sending via the CAN bus handle.

e-Golf specific CAN IDs, port map, and confirmed command sequences: see `clima-control-bap.md`.

---

## Reference

- Original reverse-engineering: https://github.com/norly/revag-bap (GPL v2)
- tmbinc's Golf Mk 6 hacking talk (30C3): https://media.ccc.de/v/30C3_-_5360_-_en_-_saal_2_-_201312281600_-_script_your_car_-_felix_tmbinc_domke
- Related tool/teardown: https://github.com/tmbinc/car

> **Disclaimer**: The revag-bap code was tested only on a lab bench driving an RCD 310 and MDI device, not inside a real car. All CAN IDs, node addresses, and port numbers for the e-Golf must be confirmed from live bus traces before use.
