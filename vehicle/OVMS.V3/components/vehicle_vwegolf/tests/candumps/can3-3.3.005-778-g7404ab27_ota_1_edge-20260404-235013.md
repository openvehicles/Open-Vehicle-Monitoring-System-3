# can3-3.3.005-778-g7404ab27_ota_1_edge-20260404-235013.crtd

16,994 frames — ignition on/off, car parked. 16,613 `3R11` (KCAN), 366 `2R11` (FCAN via J533 bridge).

**Key fact:** J533 bridges FCAN frames onto KCAN at high frequency; FCAN itself has only a
brief burst at startup. SoC real data arrives exclusively on KCAN (`3R11`). FCAN carries
only startup sentinels (0xFE). `IncomingFrameCan3` confirmed sufficient.

All signals confirmed in `vwegolf.dbc` — no pending analysis.
