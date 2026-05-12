# tests/analysis — CRTD capture analysis helpers

Python tooling for ad-hoc decode work against `.crtd` files in
`tests/candumps/`. Keep one-shot exploration scripts short by importing
`crtd.py` instead of re-writing the parser each time.

## Layout

```
analysis/
├── README.md            — this file
├── crtd.py              — parser + DBC decode helpers
├── bus_provenance.py    — per-ID bus map (detect routing-bug mislabels)
├── test_dbc_decode.py   — pytest regression suite: DBC ↔ ground-truth .md
└── scratch/             — gitignored. Local one-shot exploration scripts.
    └── .gitignore       — excludes everything in scratch/ except itself
```

## Dependencies

- Python ≥ 3.10 (`Capture.decode` uses `str | None` union syntax).
- [`cantools`](https://github.com/cantools/cantools) — DBC parsing/decode.
- [`pytest`](https://docs.pytest.org/) — regression suite runner.

Install into the project `.venv`:

```bash
.venv/bin/pip install cantools pytest
```

Drop new exploratory scripts in `scratch/`. They survive across sessions
but are never committed, so you don't have to keep rewriting the same
parser scaffolding from `/tmp` each time.

Scripts that produce reusable findings should fold those findings into
`docs/vwegolf.dbc` (signals/comments) and the relevant
`tests/candumps/*.md` notes — the script itself is disposable, the
decoded knowledge is not.

From a script in `scratch/`, import `crtd` like this:

```python
import sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))   # tests/analysis
from crtd import load, byte_stats, trajectory_le16, gear_timeline, gear_windows
```

## Quick start

```python
from crtd import load, load_multi, load_dbc, byte_stats, trajectory_le16, gear_timeline, gear_windows, GEAR_NAMES

# Single-bus file: auto-picks the bus
cap = load("../candumps/can2-…-201238.crtd")

# Multi-bus file (e.g. pre-fix capture where the bus filter was dropped):
# must specify which bus you want, or use load_multi for a dict.
cap = load("../candumps/can3-…-multibus.crtd", bus=3)
buses = load_multi("../candumps/can3-…-multibus.crtd")   # {2: Capture, 3: Capture}

print(f"{cap.frame_count} frames on bus {cap.bus}, {len(cap.by_id)} IDs, "
      f"{cap.duration:.1f}s (file had buses {sorted(cap.buses_seen)})")

# Gear timeline (e-Golf 0x187 b2 low nibble)
for g, wins in gear_windows(gear_timeline(cap)).items():
    total = sum(e - s for s, e in wins)
    print(f"  {GEAR_NAMES.get(g, g)}={g}: {len(wins)}w, {total:.1f}s")

# Per-byte stats — find which bytes vary
for i, s in enumerate(byte_stats(cap.by_id["0B4"])):
    if s.varying:
        print(f"  b{i}: 0x{s.min:02x}–0x{s.max:02x} ({s.unique} unique)")

# Trajectory of a 16-bit LE pair (e.g. wheel speed)
for t, raw in trajectory_le16(cap.by_id["0B2"], 0)[::20]:
    print(f"  {t:5.1f}s  {raw*0.0078125:6.2f} km/h")

# DBC-driven decode — auto-finds docs/vwegolf.dbc as the source of truth
dbc = load_dbc()
for t, kmh in cap.decode(dbc, "0B2", "WheelSpeedFL")[::20]:
    print(f"  {t:5.1f}s  FL={kmh:6.2f} km/h")

# Whole-message decode: pass signal=None
for t, signals in cap.decode(dbc, "0B2")[::100]:
    print(f"  {t:5.1f}s  {signals}")
```

## DBC decode helpers

`crtd.py` integrates [`cantools`](https://github.com/cantools/cantools) so
analysis scripts can pull signal values directly from `docs/vwegolf.dbc`
instead of hand-coding bit layouts in every script. The DBC is then the
*actual* single source of truth — every consumer follows when it changes.

| Symbol | Purpose |
|---|---|
| `load_dbc(path=None)` | Load a DBC (defaults to `docs/vwegolf.dbc`). Loads with `strict=False` so the intentional `ChargeType`/`ChargePort` bit overlap is permitted. |
| `default_dbc_path()` | Absolute path of the bundled DBC, useful for scripts. |
| `Capture.decode(dbc, frame_id, signal=None)` | Decode every matching frame. `frame_id` accepts hex string (`"0B2"`, `"0x131"`, `"1A5554A8"`) or int. Returns `[(t, value), ...]` if `signal=` is set, else `[(t, {sig: val, ...}), ...]`. Short / mal-decoded frames are silently skipped. |
| `decode_choices=False` | Bypass DBC `VAL_` tables, return raw integers for enum signals (`GearPosition: 6` instead of `'B_mode'`). |

## Regression test suite

`test_dbc_decode.py` pins each per-capture `.md` ground-truth claim
(wheel-speed scale, gear nibble, SoC range, peak motor power, …) as a
pytest assertion against the bundled DBC. A DBC edit that silently
breaks a previously-confirmed signal fails here with the capture name
and signal already in the message.

Run from the component root with the project venv active:

```bash
source .venv/bin/activate
make -C vehicle/OVMS.V3/components/vehicle_vwegolf/tests pytest
```

Or without venv activation:

```bash
PYTEST=$PWD/.venv/bin/pytest make -C vehicle/OVMS.V3/components/vehicle_vwegolf/tests pytest
```

The native C++ suite (`make test`) is unchanged — pytest is a separate
target so the hook that gates `fix/*`/`feat/*` pushes on the C++ tests
still works without requiring cantools+pytest on every developer's
machine.

When you add a new ground-truth `.md` assertion that should be guarded,
add a test in `test_dbc_decode.py` and cite the `.md` section in the
docstring so future readers can find the source of evidence.

## API summary

| Symbol | Purpose |
|---|---|
| `load(path, bus=None)` → `Capture` | Read one bus. Auto-picks if single-bus; raises `ValueError` if multi-bus and `bus` not given. `.by_id[hex_id] = [(t, [b0..b7]), ...]`, `.bus`, `.buses_seen` |
| `load_multi(path)` → `dict[int, Capture]` | Read every bus present in a multi-bus file at once |
| `scan_buses(path)` → `set[int]` | Cheap: list buses present without parsing frames |
| `iter_frames(path)` | Stream `(rel_t, bus, hex_id, bytes)` without buffering |
| `iter_crtd(path)` | Back-compat: `(rel_t, hex_id, bytes)`, all buses merged |
| `le16(b, off)` / `be16(b, off)` | 16-bit pair extraction |
| `trajectory_byte(records, off)` | List of `(t, byte_value)` |
| `trajectory_le16` / `trajectory_be16` | Same for 16-bit pairs |
| `byte_stats(records, dlc=8)` | Per-byte `ByteStat(min, max, unique)` |
| `downsample(seq, n=25)` | Even-stride sample for compact dumps |
| `hexdump_window(records, dlc, n)` | Pretty multi-row dump |
| `gear_timeline(cap)` / `gear_windows(timeline)` | e-Golf gear decode helpers |
| `GEAR_NAMES` | `{2:"P", 3:"R", 4:"N", 5:"D", 6:"B"}` |
| `in_window(t, windows)` | Test if a timestamp falls in any `(s,e)` slice |
| `load_dbc(path=None)` / `default_dbc_path()` | Load `docs/vwegolf.dbc` via cantools |
| `Capture.decode(dbc, frame_id, signal=None)` | Decode-via-DBC; see "DBC decode helpers" below |

## Conventions

- Hex IDs are **uppercase strings** (`"0B2"`, `"187"`) — same as the
  CRTD file column. Don't pass ints.
- Timestamps are seconds, **relative to the first 2R11 frame** in the file.
  Absolute Unix time is on `Capture.t0_unix` if needed.
- Both 11-bit and 29-bit data frames on buses 1/2/3 are parsed. `<N>CER`
  error records and comment lines are silently skipped.
- Some older captures (pre commit `9fdbf8b7`) silently dropped the bus
  filter at logging time, so a file named `can3-*.crtd` may actually
  contain bus 1+2+3 frames. `load()` will refuse to auto-pick in that
  case — pass `bus=N` explicitly or use `load_multi`.

## Why not SQLite?

Considered. Skipped: the CRTD file never enters the LLM context window
(Python parses it locally), so token cost is identical to SQLite. The
real win is short reusable analysis scripts, which is what `crtd.py`
delivers without a conversion step.

## Why cantools + DBC over hand-rolled byte arithmetic?

Same single-source-of-truth argument: every scratch script that
re-implements bit/byte extraction is a drift hazard. Bugs we have hit
because of this — `0x2AF BMS_TripEnergy` was decoded against the wrong
byte offsets in firmware for months before a capture exposed it. Wiring
the DBC into every decode path means a fix in `vwegolf.dbc` propagates
to every consumer, and the regression test suite catches silent breaks.
