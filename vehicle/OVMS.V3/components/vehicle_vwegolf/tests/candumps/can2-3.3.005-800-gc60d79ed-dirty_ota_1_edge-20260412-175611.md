# can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-175611.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_1/edge` |
| Bus | can2 |
| Captured | 20260412-175611 |
| Duration | ~67s |
| Frames | 26127 |

## Sequence

ccs charging

---

## Notes

<!-- Add analysis notes here -->

### opendbc cross-check (2026-07-09)

Cross-referenced against commaai opendbc `vw_mqb.dbc`. Frame names adopted for
0x086 (`LWI_01`), 0x0A7/0x0A8 (`Motor_11`/`Motor_12`), 0x107 (`Motor_04`),
0x120 (`TSK_06`), 0x121 (`Motor_20`) — see `docs/vwegolf.dbc` comments.

Plausibility results on this cap (car parked, CCS idle, wheel untouched):

- `LWI_01` `SteeringAngleAbs` constant **5.1 deg**, `SteeringRotSpeed` 0 —
  static as expected. (In 20260611-195720 the same decode shows 10.1 deg with
  0.1-deg jitter and rotation-speed blips — live sensor.) Signals added to DBC;
  dynamic scale/sign confirmation still pending an unfiltered drive capture.
- `Motor_20` pedal field 0 % throughout — consistent, not probative.
- `Motor_11` torque field sits at raw 1022 (= 513 Nm after offset) = VW SNA
  sentinel while stationary; occasional 0/-1. Signals NOT added yet.
- `Motor_12` motor speed 0 rpm throughout — consistent, not probative.

Pytest: `test_lwi_steering_angle_static_when_parked` pins the 5.1 deg value.
