# can3-3.3.005-792-gea41f218-dirty_ota_1_edge-20260405-005724.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-792-gea41f218-dirty/ota_1/edge` |
| Bus | can3 |
| Captured | 20260405-005724 |
| Duration | ~39s |
| Frames | 42785 |

## Sequence

ignition on, turn ignition off, press activate clima in app, if clima starts count 30 then press clima stop in app

---

## Notes

### What happened

**All receive frames on bus 2 (`2R11`/`2R29`), zero `3R` frames:**
can3 entered bus-off/error-passive due to TX collisions. During recovery it was deaf to
KCAN. The KCAN frames the car emitted were received via J533's FCAN bridge (can2).

**576× `3CER TX_Fail T11 5A7` — OCU heartbeat collision:**
The car's ignition was just turned off. The OEM OCU was still active on KCAN sending
non-zero `0x5A7` (status bits set during active running state). Our all-zeros `0x5A7`
conflicted → TX_Fail → bus-off cascade.

When ignition is on or recently off (within OSEK NM ring timeout ~10s), the OEM OCU
sends non-zero `0x5A7`. Our all-zeros `0x5A7` loses arbitration every time. Once the
car is fully sleeping, the OEM OCU is off and our TX succeeds.

**BAP commands TX_Failed:**
```
3CER TX_Fail T29 17332501 80 08 29 59 03 06 00 01
3CER TX_Fail T29 17332501 c0 06 00 20 00
3CER TX_Fail T29 17332501 29 58 00 01   ← START flag (0x01) was correct
```
The commands had the right content — they were just lost due to bus-off.

**44× `2R29 17332510` — periodic ECU status broadcasts (not command ACKs):**
The clima ECU broadcasts status on KCAN at regular intervals regardless. These frames
arriving on can2 (via J533 FCAN bridge) are not responses to our failed commands.

### Key finding: 0x5A7 conflict window

The user must wait ~15 seconds after turning off the ignition before pressing the
clima button. During the OSEK NM ring-down period, the OEM OCU is still active and
collides with our heartbeat. There is no code fix — this is a protocol-level constraint.

The user guide must document: "wait 15 seconds after turning off the car before
issuing remote climate control commands."

### Implication

This capture proves Frame 3 has the correct start flag (`00 01`) when the app sends
a genuine start command. The BAP command content is correct — the only issue was the
0x5A7 collision making can3 bus-off before any frame could be delivered.
