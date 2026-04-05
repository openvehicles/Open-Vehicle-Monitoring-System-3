# can3-3.3.005-778-g7404ab27_ota_1_edge-20260404-235013.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-778-g7404ab27/ota_1/edge` |
| Bus | can3 |
| Captured | 20260404-235013 |
| Duration | ~85s |
| Frames | 16994 |

## Sequence

car sleepeing, ignitoin turned on, 30 count, ignition off, wait for car to sleep

---

## Notes

Frames split across two buses: 16,613 tagged `3R11` (can3/KCAN, dominant) and 366
tagged `2R11` (can2/FCAN, sparse). This is the clearest evidence of J533's bridging
behaviour: FCAN frames appear on KCAN (`3R11`) at high frequency while FCAN itself
(`2R11`) has only a brief burst at startup.

SOC specifically:

| Bus | Count | d[3] | Meaning |
|---|---|---|---|
| `3R11` (KCAN) | 878 | 0x6B | 53.5% real SOC |
| `3R11` (KCAN) | 3 | 0xFE | startup sentinel |
| `2R11` (FCAN) | 2 | 0xFE | startup sentinel only |

Real SOC data arrives exclusively on KCAN. The `2R11` instances carry only the
sentinel, confirming `IncomingFrameCan3` is the correct and sufficient decode path.
