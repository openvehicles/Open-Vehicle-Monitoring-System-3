# tests/analysis — TODO

Remaining items from the crtd.py improvement plan. Ranked by ROI; each
sized for one focused branch.

## High value

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
