# all-3.3.006-269-gab4f52853_ota_0_edge-20260712-124542.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.006-269-gab4f52853/ota_0/edge` |
| Bus | all |
| Captured | 20260712-124542 |
| Duration | ~578s |
| Frames | 223969 raw (224035 lines incl. events) |
| Logger stats | Messages:225756 Dropped:1992 Filtered:0 |
| Metrics | all-3.3.006-269-gab4f52853_ota_0_edge-20260712-124542.metrics.tsv (668 samples) |

## Sequence

Flash-regression hunt (awake/on flap, charging flip on clima, CCS charge detect). CRTD t ≈ metrics-TSV rel-t + 9 s.

| t (crtd) | Action / observation |
|---|---|
| 0.18 | Remote clima start via BAP TX 0x17332501 |
| 45.0 | Car unlocked/started — 0x3C0 KL15 bit rises (v.e.awake=yes) |
| 46.2 | 0x391 READY bit rises (v.e.on=yes) |
| ~46–146 | Short drive |
| 146.6 | KL15 off, car parked (v.e.awake/on=no); READY bit off 164.2 |
| ~205 | CCS DC plug-in — KCAN wakes (was silent 183–204) |
| 206.6 | 0x594 d[3] bit5 rises (v.c.charging=yes) |
| 213.6 | 0x594 ChargeType=2 (DC_CCS) — first-ever CCS observation on this frame |
| 222–277 | KCAN silent (asleep) mid-charge; ~0.5 kWh delivered |
| 278.6 | Charge stopped — KCAN wakes, bit5 off, ChargeType→0 (~280.6) |

---

## Notes

- **No metric flap reproduced.** v.e.awake / v.e.on / v.c.charging each transitioned exactly
  twice, and every flip matches frame content and real events. No mid-drive dropout: bus-wide
  silent windows (13–21, 53, 183–204, 222–277 s) all fall outside the ignition-on window
  46–146 s. Backstop never fired spuriously here.
- **H2 not reproduced:** clima attempt at t=0.18 did NOT raise 0x594 d[3] bit5; charging
  stayed `no` until real CCS plug-in.
- **ChargeType=2 confirmed** during CCS DC (see `ChargeType` in vwegolf.dbc). KCAN sleeps
  mid-charge, wakes at plug-in and charge-stop — that wake window is the only visibility.
- **Tooling gap:** `cap.decode(dbc, "594", …)` returns empty — ChargeType/ChargePort bit
  overlap makes cantools raise DecodeError on every frame; decode() silently skips. Use raw
  bytes for 0x594.
- Logger: 1992 dropped frames; .md duration ~578 s but bus3 frame span 301 s.
