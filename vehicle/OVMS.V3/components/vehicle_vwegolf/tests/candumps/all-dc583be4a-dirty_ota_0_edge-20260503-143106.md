# all-dc583be4a-dirty_ota_0_edge-20260503-143106.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `dc583be4a-dirty/ota_0/edge` |
| Bus tag in file | `2` only (FCAN) |
| Captured | 20260503-143106 |
| Capture duration | 607.1 s of data, ~643 s wall (matches MD header) |
| Frames | 57 459 |
| Logger stats | Messages:57380 Dropped:199 Filtered:0 |

## Sequence

Driving — Hälla shopping center (Västerås) → E18 westbound → road 66 → OKQ8.

Started just after a top-up DC charge (session ended at 92 % real / 99 % bus-raw, see [`…-132333.md`](all-dc583be4a-dirty_ota_0_edge-20260503-132333.md)).

Trip envelope from 0x187 GearSelector:
- t=  5.8 s: P → R → N → D → **B** (regenerative drive mode)
- t=583.1 s: N → R → P (parked at OKQ8)
- Driving duration: ≈ 9 m 37 s

---

## Findings

### "all-bus" capture mode is inconsistent

This capture file is tagged `all-…` (same firmware build as the DC capture 4 h earlier) but the CRTD only contains `2…` records — **bus 2 (FCAN) only**, with **just 5 unique IDs** and zero KCAN traffic. The morning DC capture under the same firmware build produced 190 IDs tagged bus 3.

Same `dc583be4a-dirty` build, same option `all`, two completely different shapes. The all-bus mode is unstable / not actually all-bus.

### 0x131 BMS_SoC works correctly during driving

99.0 % → 95.0 % over 607 s, monotonic-ish. ≈ 4 pp drop matches 1.7 kWh consumed on a 35-kWh-class pack. The DC-charging "metric pinned" issue from the earlier capture is therefore **not a general 0x131 decode bug — it is DC-charging-specific**.

Note: bus reports raw 99 % at start, but real displayed SoC after the top-up charge was 92 %. **The bus carries gross SoC including pack buffer**; the 7 pp gap is consistent with VW's standard top buffer.

### 0x191 BMS_PowerBus decode is fully correct on FCAN-driving

| | Value |
|---|---|
| Current range | −307 A draw / +63 A regen |
| Voltage range | 343.0 V (full draw sag) / 360.8 V (idle) |
| Power range | **−105.5 kW peak draw / +22.5 kW peak regen** |
| Net energy integral | **−1.706 kWh** consumed |

Peak −105.5 kW matches the e-Golf's nominal 100 kW motor spec. Trip in B-mode shows realistic regen events.

### 0x2AF BMS_TripEnergy — current DBC decode is wrong

Current DBC (pre-2026): EnergyRecd at bytes 4-5, EnergyUsed at bytes 6-7, factor 0.002778 Wh/LSB.

Observed:
- **Bytes 0, 1, 4, 5, 6, 7 are static zero** across all 12 089 frames (entire 607 s trip).
- **Bytes 2-3 carry a single 16-bit LE varying signal**, range 0..65 504.
- Signal is 0 at gear-P with ignition just on, climbs immediately when gear engages, oscillates wildly mid-trip, and resets near 0 (last value 32) when shifted to P at trip end.

Hypothesis: cumulative net Wh since key-on at 1 mWh/raw, modulo 65 536, signed wraparound on regen.

Trip-total cross-check vs 0x191 integral:
- Pre-park value: 58 496 raw at t=583 s
- 0x191 integral over full trip: −1 706 Wh = −1 706 000 mWh
- 26 wraparounds × 65 536 + 58 496 = 1 762 432 mWh (1 762 Wh)
- Match to 1 706 Wh within ~3 %.

The 20 s-sampled mid-trip "dips" are aliased rapid wraparounds — at −100 kW, the counter wraps every ~0.6 s.

DBC updated to reflect bytes 2-3 layout with the mWh hypothesis flagged for follow-up.

**Action:** capture single-bus FCAN with finer time resolution and parallel 0x191 sampling to lock the scale and confirm regen-sign behaviour before relying on this for OVMS metrics.

### 0x6B4 VIN_Broadcast confirms VIN assembly

Three frames (idx 0/1/2) assemble to a complete 17-character VIN consistent with the e-Golf platform. Value redacted — confirms the existing 3-frame indexed decode in firmware.

### Missing IDs

Driving capture is missing 0x057 motor-torque-request, 0x0B2 wheel-speeds, 0x0B4 motor/inverter, 0x107/120/121 BMS candidates, 0x365/3BE pedal frames, and **all KCAN** (no 0x0FD speed, no 0x594, no 0x1A5554A8, none of the gateway broadcasts). Whatever is filtering frames in the all-bus VFS logger this run is far more aggressive than the morning DC capture under the same build.

## Notes

- All-bus capture mode needs investigating — produced two radically different shapes today on the same firmware build.
- 0x2AF mWh hypothesis is the highest-value finding here: with a clean confirmation it gives OVMS a real net-trip-energy metric for free, without computing the integral in firmware.
