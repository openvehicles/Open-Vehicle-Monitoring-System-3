# all-dc583be4a-dirty_ota_0_edge-20260503-132333.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `dc583be4a-dirty/ota_0/edge` |
| Bus | all (FCAN+KCAN merged under tag 3 in this mode) |
| Captured | 20260503-132333 |
| Capture duration | ~533 s of data (truncated; module reset before download completed) |
| Logger stats | Messages:541038 Dropped:2320 Filtered:0 (≈68% of total recovered) |

## Session info (out-of-band)

| Field | Value |
|---|---|
| Charge type | CCS DC |
| Session length | 23 m 46 s (ended 13:36) |
| Capture start vs session start | +5 min into session → covers session 5:00–~13:54 |
| Energy delivered (whole session) | 13.238 kWh |
| Average power (whole session) | 33.55 kW |
| Real SoC (whole session) | 49 % → 92 % (Δ = 43 pp) |
| Real SoC during capture window | ≈ 58 % → ≈ 74 % (linear est.) |
| Pack effective capacity (derived) | ~30.8 kWh → 35.8 kWh-class e-Golf (~86 % usable) |

## Sequence

CCS DC charging mid-session through to module reset.

---

## Findings

### 0x594 ChargeManagement — broadcasts on KCAN during CCS DC

1068 frames @ 2 Hz across whole capture. **Contradicts** earlier note in `vwegolf.dbc` ("CCS DC keeps KCAN silent so ChargeType=2 never observed").

4 unique payloads:
```
t=  0.4 s   00 40 00 23 28 28 38 09   ChargeType=2 (DC_CCS)
t=  1.7 s   00 40 00 23 28 e8 38 09   ChargeType=2
t=  1.9 s   00 40 00 63 7f ec 34 09   ChargeType=3 + ChargeInProgress=1 (contradiction)
t=199.4 s   00 30 00 63 7f ec 34 09   same, CDF byte changed 0x40→0x30
```

`ChargeType=2 (DC_CCS)` was observed for the first 1.9 s, then payload locked into `CType=3 + CIP=1` for the remaining ~530 s of charging. That state is `CableConnected_NotCharging` per current DBC VAL_, which is impossible during an active 33.55 kW charge.

**Implication:** 0x594 bit layout under DC differs from AC, or the AC-confirmed bit positions for ChargeType (42|2) / ChargeInProgress (29|1) need a re-decode. Byte 3 transitions 0x23 → 0x63 (bit 6 set) at t=1.9 s and stays — strong DC-charging-active candidate flag.

### 0x131 BMS_SoC pinned in OVMS UI during DC

Frame present at 50 Hz (26 677 frames over 533 s). `byte[3]` monotonically rises 0x95 → 0xb2 (decodes 74.5 % → 89.0 % using current DBC scale).

But: real SoC during the same window was ~58 % → ~74 % (out-of-band sanity), and the user reports the OVMS SoC metric was **pinned** for the entire session.

So one of:
1. DBC scale/offset for `0x131 byte[3]` is wrong on DC (delta-rate matches at ~1.6 pp/min vs real 1.8 pp/min, absolute is off by ~16 pp);
2. byte[3] under DC encodes a different quantity (target SoC, net-of-buffer SoC, …);
3. The all-bus merge collapses two different 0x131 frames (FCAN BMS + a KCAN 0x131) under one tag — needs single-bus DC capture to disambiguate.

In any of those cases, the existing OVMS read path on this frame doesn't surface live DC SoC. Action: re-capture single-bus FCAN during a DC session to confirm, then audit BMS_SoC decode.

### 0x191 BMS_PowerBus absent during CCS DC

**Zero frames** in 533 s. Confirms the TODO at `vehicle_vwegolf.cpp:147`: 0x191 carries inverter/motor current and is silent during DC charging. DC charge current/voltage/power must come from another frame.

### 0x1A5554A8 OBC telemetry — different shape under DC

| | AC (cap14, 20260413) | DC (this capture) |
|---|---|---|
| Frame rate | ~2 Hz | ~0.2 Hz |
| Mux values seen | 0x5, 0x6 | 0x0, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0xD, 0xE, 0xF |
| Mux 0x5 b4 (totalizer) | +1/s | +1/s (only 3 frames) |

Charge totalizer at +1/s under both AC and DC — at 33.55 kW DC that would have to encode ~36 Wh/tick to be energy, far above the observed +1; **the totalizer is a wall-clock seconds counter, not an energy/Ah counter.**

Multiple new mux IDs (0x3, 0x4, 0x7, 0x8, 0xD, 0xE, 0xF) appear only on DC and likely contain DC-side current/voltage/power. Each has its own static `b1, b2, b3, b6, b7` skeleton with `b4` varying — needs longer DC capture for byte-level decode (this capture only sampled 2–9 frames per mux).

### 0x31E (20 Hz, undecoded)

Bytes 0/1 form a varying 16-bit LE pair across the capture: `0xfb20 → 0xf1ef → 0xf744 → 0xfe5f → 0xf4ba → 0xfa06`. Range ~0xf000–0xff00 with byte 0 sweeping the full 0x06–0xef range. Plausible HV current or voltage candidate (sign-magnitude or offset-encoded). Worth dedicated trajectory analysis next round.

### Bus-tagging note

In this firmware (`dc583be4a-dirty`), the new "all-bus" capture mode merges all bus traffic under tag `3` in the CRTD file — bus origin is **not** preserved. Frames whose DBC home is FCAN (0x131, 0x191 candidate, 0x2AF, 0x187, 0x6B4) appear here next to KCAN frames, all marked bus 3. For decode work that needs to disambiguate FCAN vs KCAN, fall back to single-bus capture mode.

## Notes

- 14 `3CER` bus-error events fire in the first ~3 ms of capture (txerr climbing 8 → 47, errflags 0x80001080 → 0x23401c01) then settle. `txdelay=0 wdgreset=0` throughout — the mcp2515 ISR-storm regression is gone, this is just normal bus-bringup noise.
- Module reset truncated the file: 67 % of logged messages recovered, last 7+ minutes of session not captured.
