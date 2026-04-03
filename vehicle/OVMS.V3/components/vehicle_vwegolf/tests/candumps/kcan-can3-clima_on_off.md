# kcan-can3-clima_on_off.crtd — Capture Notes

## Capture sequence

CAN3 (KCAN) capture while issuing climate start/stop commands from both the OVMS shell
and the OVMS Connect iOS app via TurboServ. All commands confirmed working — clima
started and stopped as expected in every case.

| Time (approx.) | Action |
|---|---|
| T+72s | OVMS shell: `climatecontrol on` |
| T+109s | iOS app: toggle (showing "AC OFF") → pressed → changed to "AC ON" |
| T+151s | OVMS shell: `climatecontrol off` |
| T+176–183s | iOS app: toggle pressed repeatedly while clima was turning on, then while running |

Rolling BAP counters issued by OVMS during this capture: 0x02 (shell on), 0x03 (iOS on),
0x04 (shell off), 0x05–0x18 (rapid iOS presses).

---

## Key findings

### Command ACK on 0x17332510 (within ~1s of command)

Each climate command triggers an immediate short response from the ECU on `0x17332510`:

```
80 0a 49 59  {ectr} 04 46 00   (+ continuation with remaining payload)
```

`49 59` = BAP: opcode=2 (Status), node=0x25, port=0x19, multi-frame length=10.
`{ectr}` = our rolling counter **with bit 7 set** — e.g. our 0x02 → ECU echoes 0x82,
our 0x03 → 0x83, our 0x04 → 0x84. This is how to correlate ACK to command.

### Periodic status bursts on 0x17332501 (~16s after command)

About 16 seconds after each command, the ECU sends ~4 cycles of status frames on
`0x17332501` at 5-second intervals, then goes quiet until the next command.

Each cycle consists of:

```
3R29 17332501  19 42                          (keepalive poll, port 0x02, node 0x25)
3R29 17332501  19 41                          (keepalive poll, port 0x01, node 0x25)
3R29 17332501  90 04 19 59 {ctr} 00 00 04    (ch1 multi-frame, port 0x19, 4-byte payload)
3R29 17332501  80 04 19 5a {ctr} 02 00 04    (ch0 multi-frame, port 0x1a, 4-byte payload)
```

`{ctr}` is a 3-bit rolling counter cycling through 0x28–0x2F (8 values).

The status payloads (`00 00 04` and `02 00 04`) did not visibly change between start and
stop commands in this capture. A capture that allows the 10-minute timer to expire
naturally is needed to see what the ECU sends when clima finishes.

### ECU full state dump on 0x17332510 (triggered by each command)

Alongside the ACK, the ECU sends a full state broadcast on `0x17332510`:

- **Port 0x19** (channel 0): 130 bytes (`80 82 49 59 ...`), 18 continuation frames.
  Content appears static — schedule configuration and installed component strings.

- **Port 0x1a** (channel 1): length and header byte vary by state:
  - **34 bytes (`90 22`), header byte `c0`**: seen at initial bus connect and after one
    of the iOS toggle presses. Likely the idle/no-active-request state.
  - **25 bytes (`90 19`), header byte `c2`**: seen during cycles where clima was
    actively running. Bit 1 of the header byte (`c0` → `c2`) appears to be the
    clima-active flag — not yet confirmed.

### TX queue CER entries are normal

When a new command fires while previous frames are still queued, the stale frames are
flushed and logged as `3CER TX_Queue`. This is expected housekeeping — the new command
frames follow immediately and transmit successfully.

During the rapid iOS press sequence (~10 presses within 2 seconds), TX_Queue CERs
appear on every press, but each press still transmits all 3 BAP frames successfully.

### Note on first start attempt

The CLI `climatecontrol on` at T+72s had an anomaly: the Frame 3 trigger (`29 58 00 XX`)
was transmitted as `29 58 00 00` (stop byte) instead of the expected `29 58 00 01`
(start byte). The likely cause is a stale stop frame from a prior command that was still
in the TX queue and slipped ahead of the new frame 3. Despite this, clima started and the
ECU sent status normally ~16s later. This suggests that the port 0x19 parameters frame
(Frame 1 + 2) may be sufficient to trigger climatisation on its own, with the port 0x18
trigger frame having a secondary or confirmatory role.
