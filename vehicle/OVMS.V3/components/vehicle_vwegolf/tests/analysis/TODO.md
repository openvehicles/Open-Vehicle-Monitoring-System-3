# tests/analysis — TODO

Remaining items from the crtd.py improvement plan. Ranked by ROI; each
sized for one focused branch.

## High value

### Synthetic fixtures for `test_dbc_decode.py` (unblock CI gate)

Today the pytest suite asserts against three real `.crtd` captures
(`can2-…-201238`, `all-…-143106`, `can3-…-121648`) — all gitignored
because they contain real VINs. CI can't run them (see reverted commit
23d69a19 / 168388a82). Until synthetic fixtures exist, pytest stays
dev-only and silent DBC drift can ship.

Plan: extend the `kcan-synthetic.crtd` pattern (fictional Berlin
e-Golf, no PII) with per-test fixtures covering the asserted signals
— wheelspeed @ rest + 30 km/h plateau, gear D/B nibble transitions,
SoC monotonic decrease, BMS current/voltage/peak-power envelope,
mux split on 0x1A5554A8. Either hand-fabricated frame streams or
a `write_crtd()` helper (see Low-value entry) fed by a fixture
builder. Mark capture-dependent tests with `@pytest.mark.skipif`
so the synthetic subset runs everywhere and the capture-backed
subset still runs locally.

Pin assertion: green pytest job in CI with no `.crtd` files
present in the workspace. Re-enable the workflow snippet from
the reverted commit once this lands.

### Correlation finder
`correlate(cap, ref_id, off, window=0.2)` → IDs whose bytes also
transition within ±window of `ref_id`'s transitions. Answers the
"what frame caused this metric to change" question — currently
eyeballed per session. Building blocks (`transitions`, `frame_rate`)
already exist.

Pin assertion: at the gear D→B transition in cap 21 (t≈6.33 s),
`correlate(cap, "187", 2)` must list 0x191 BMS_PowerBus and 0x2AF
BMS_TripEnergy as co-firing within ±200 ms.

## Medium

### Cross-capture diff
`diff_captures(a, b)` → IDs only in A, only in B, common-but-different
bytes. Idle-vs-charge comparison is the standing case (cap 14 charge
vs a same-bus idle baseline).

Pin assertion: against cap 14 (AC charge) vs a matching idle KCAN
capture, 0x594 ChargeManagement bytes carrying ChargeInProgress /
ChargeType must show as different.

### Fold bus_provenance into crtd.py
`provenance(path) → dict[hex_id, set[int]]` as an importable function
(currently CLI-only in `bus_provenance.py`). Keep the CLI wrapper.

## Low

### CRTD writer
`write_crtd(path, frames)` — for trimming a 60k-line capture into a
small reproducible test fixture. Not urgent: we haven't needed
fixture-style slices yet.

## Future / nice-to-have

- Cap-index linter: verify every `tests/candumps/*.md` is listed in
  `docs/captures.tsv` and dates match filenames.
- DBC-aware `hexdump_window`: annotate each byte column with the
  signal it belongs to.
- `Capture.find(pred)` — iter frames matching an arbitrary predicate
  on bytes (current pattern is a list comp per script).
