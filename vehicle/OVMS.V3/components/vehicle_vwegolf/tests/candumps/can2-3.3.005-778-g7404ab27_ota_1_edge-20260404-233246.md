# can2-3.3.005-778-g7404ab27_ota_1_edge-20260404-233246.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-778-g7404ab27/ota_1/edge` |
| Bus | can2 |
| Captured | 20260404-233246 |
| Duration | ~124s |
| Frames | 82203 |

## Sequence

sleeping car, ignition on, wait 30 count, ignition off, wait for car to sleep

---

## Notes

All 82,203 frames are tagged `3R11` (can3/KCAN) — FCAN (can2) was inactive during
this capture. This confirms that J533 bridges FCAN frames onto KCAN, not the reverse.

Key frame findings:

| Frame | Count | d[3] values | Decoded value |
|---|---|---|---|
| 0x131 (SOC) | 4,097 | 0xFE ×93 (sentinel), 0x73 ×4004 | 57.5% real SOC |
| 0x0FD (speed) | 4,058 | — | 0 km/h (parked) |
| 0x187 (gear) | ~1,200 | nibble=2 | Park |

The 0xFE sentinel in 0x131 decodes to 127.5% and was being persisted via metric
persistence on every boot. Fixed by adding a sentinel guard (`if (d[3] == 0xFE) break`)
matching the pattern used for 0x066E cabin temperature.
