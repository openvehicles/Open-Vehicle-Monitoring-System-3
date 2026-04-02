# Clima BAP Capture TODO

Goal: identify which BAP port/byte encodes each clima function by diffing
captures across distinct state changes.

## Setup

On laptop (connected to OVMS hotspot):
```bash
nc 192.168.4.1 3000 > vehicle/OVMS.V3/components/vehicle_vwegolf/tests/candumps/clima-sequence.crtd
```

On OVMS shell:
```
can log start tcpserver transmit crtd 3000 can3
```

## Capture sequence

Perform each step slowly, leaving ~15 seconds between each action so the
port values have time to settle and are clearly separated in the log.

1. **Start with clima OFF, car awake** — wait 15s (baseline: all ports in idle state)
2. **Turn clima ON from dash** — wait 20s
3. **Change temperature DOWN** (e.g. 18°C) — wait 15s
4. **Change temperature UP** (e.g. 24°C) — wait 15s
5. **Turn clima OFF from dash** — wait 20s (observe which ports change)
6. **Turn clima ON again** — wait 15s (confirm on/off port flips)
7. **Turn clima OFF** — wait 10s

Stop capture:
```
can log stop
```
Ctrl-C the `nc`.

## What to look for in analysis

- **Port 0x16**: already observed to change between on/off states.
  Compare byte[3] (0x37 vs 0x00) — likely remaining runtime or on/off flag.
- **Temperature port**: find which port's payload changes when setpoint is
  adjusted. Candidate range: ports 0x11–0x16. The value 0x15 (=21) seen in
  port 0x16 byte[3] may be the setpoint — confirm by setting 18°C (0x12)
  and 24°C (0x18).
- **On/off state port**: one port should flip a single bit or byte cleanly
  between clima running and stopped.
- **Duration**: port 0x11 payload `01 3c ...` — 0x3c = 60, could be minutes.

## After capture

Run the BAP decoder script against the new file:
```bash
python3 - << 'EOF'
# (copy decoder from session history or re-derive)
EOF
```

Then diff port payloads between each phase to isolate which port encodes what.

## CAN IDs confirmed on this car

| Direction        | CAN ID       | Notes                          |
|------------------|--------------|--------------------------------|
| OVMS → clima ECU | `0x17330301` | Wake / BAP init                |
| OVMS → bus       | `0x1B000067` | Bus wake broadcast             |
| OVMS → bus       | `0x5A7`      | OCU heartbeat (repeated)       |
| Clima ECU → OVMS | `0x17332510` | Status responses (node 0x25)   |

## Known BAP values (to confirm)

- Node: **0x25** (climate ECU LSG address)
- Opcode for status push from ECU: **3** (MQB uses 3, not 2 as in PQ docs)
- Port **0x02**: BAP config response (constant, ignore)
- Port **0x16**: clima state — changes on on/off toggle
