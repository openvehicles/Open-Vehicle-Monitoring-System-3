# can2-3.3.005-778-g7404ab27_ota_1_edge-20260404-233246.crtd

**Contaminated — capture script bus filter bug.** All 82,203 frames tagged `3R11` (can3/KCAN).
The can2 filter was not applied; this is KCAN data mislabeled as FCAN.

**Key fact preserved:** FCAN (can2) was completely silent during ignition-on/off with car
parked. J533 does not bridge FCAN onto KCAN in this direction. Signals 0x131/0x0FD/0x187
confirmed on KCAN only. `IncomingFrameCan3` is the correct decode path.

All signals confirmed in `vwegolf.dbc` — no pending analysis.
