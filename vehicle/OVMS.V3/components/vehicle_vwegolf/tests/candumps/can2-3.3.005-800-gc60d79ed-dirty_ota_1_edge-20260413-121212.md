# can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260413-121212.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_1/edge` |
| Bus | can2 |
| Captured | 20260413-121212 |
| Duration | ~64s |
| Frames | 15 |

## Sequence

capture 14 — wrong bus. AC Type 2 (CTEK Chargestorm, RFID start) captured on
can2 (FCAN) by mistake; FCAN is silent with powertrain asleep during AC charge.
Only 15 OVMS internal event-log lines (`1CEV`), zero CAN data frames. See
can3-…-20260413-121648 for the usable session.

---

## Notes

Useful only for the internal-event timeline (OVMS saw the charge via *other*
buses it was listening to — presumably can3):

- T+ 7s  `vehicle.aux.12v.normal`
- T+22s  `vehicle.charge.start` → `vehicle.charge.state`
- T+24s  `vehicle.charge.type`  → `vehicle.charge.prepare`
- T+27s  `vehicle.aux.12v.blip`
- T+41s  `vehicle.aux.12v.charging.blip`
- T+43s  `vehicle.charge.stop`  → `vehicle.charge.state`
- T+45s  `vehicle.charge.finish`
- T+49s  `vehicle.aux.12v.charging`
- T+51s  `vehicle.aux.12v.charging.dip`
- T+56s  `vehicle.aux.12v.dip`

Confirms OVMS correctly decodes the start→prepare→stop→finish sequence from
0x594 (ChargeManagement) during short AC sessions. 12V blip pattern = DC-DC
converter cycling on/off around the charge window. Actual frame decode lives
in the companion can3 capture's notes.
