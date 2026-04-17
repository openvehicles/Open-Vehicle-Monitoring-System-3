# can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-231401.crtd

50,825 frames — **Capture 7** (clima at 18°C then 22°C, temp encoding confirm).
Temperature encoding confirmed: `raw = (celsius - 10) * 10`. 18°C → 0x50, 22°C → 0x78.
Bug fixed: read cc-temp config at command time, not from Ticker10 cache.
All findings in `vwegolf.dbc` and PROJECT.md.
