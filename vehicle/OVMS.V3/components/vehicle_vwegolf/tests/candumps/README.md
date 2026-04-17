# candumps/

This directory holds CRTD-format CAN log captures used by `test_crtd_replay.cpp`
to validate the decode pipeline against real car data.

Capture files are **not committed** to this repository because they contain
personally identifying information (VIN, and implicitly vehicle location/time).

## Creating a capture

Use `tests/capture.sh` — it handles SSH log start/stop, names the file after the
running firmware version, and prompts for a one-line description:

```bash
bash tests/capture.sh        # capture can3 (default)
bash tests/capture.sh can2   # capture can2 (FCAN, rarely needed)
```

Requires the laptop to be on the OVMS WiFi hotspot (`192.168.4.1`) with an SSH
key for `ovms@192.168.4.1`.

The `test_crtd_replay` test looks for `candumps/kcan-capture.crtd` as its fixed
input. New baseline captures have timestamped filenames and are used for manual
analysis; rename or symlink to `kcan-capture.crtd` to use them in the replay test.

## CCS DC fast charging — KCAN stays silent

CCS fast charging bypasses the OBC entirely. The car manages DC charging via the CCS
protocol (PLC on the charging cable) and keeps the KCAN bus sleeping throughout the
session. No KCAN frames appear during a CCS session — only the OVMS 12V ADC reflects
that something is happening (aux voltage rises as the DC/DC converter comes up).

`v.c.charging`, `v.c.type`, `v.c.current`, and `v.c.state` do not update during CCS.
Finding OBC charge current requires a **Type 2 AC** session (see Capture 8 in PROJECT.md).

## J533 gateway — bus topology (confirmed from baseline captures)

The OVMS module connects to the car via the J533 gateway harness, which provides
access to two CAN buses:

| CRTD tag | OVMS bus | Physical bus | Role |
|---|---|---|---|
| `2R11` / `2R29` | can2 | FCAN (powertrain) | sparse — ECU-level frames, some bridged KCAN at startup |
| `3R11` / `3R29` | can3 | KCAN (convenience) | dominant — all real metric data lives here |

**Bridging direction:** J533 bridges FCAN frames (gear 0x187, VIN 0x6B4, etc.)
onto KCAN. The bulk of metric traffic — including SOC (0x131), speed (0x0FD),
charging, clima, etc. — arrives on can3 (`3R11`). The `IncomingFrameCan3` handler
is the primary decode path. `IncomingFrameCan2` handles the few frames that arrive
on can2 and forwards everything to `IncomingFrameCan3` as a fallback.

The CRTD bus number encodes the physical OVMS bus: `busnumber = m_busnumber + '1'`,
so `2R11` = can2, `3R11` = can3.

## Pre-fix bus-filter audit (files whose name lies about bus contents)

Before commit `9fdbf8b7`, `tests/capture.sh` passed the bus filter to `can log start`
as a string (`"can2"`) instead of an integer, so the filter was silently dropped
and the log captured every active bus. As a result, several older `.crtd` files
have a `can2-*` or `can3-*` prefix that does **not** match what's actually inside.
Files listed here must be loaded with the multi-bus-aware `crtd.py` and an
explicit `bus=N` filter — see `tests/analysis/crtd.py` `load()` / `load_multi()`.

**Mislabeled single-bus files** — the name is wrong, but the contents are pure.
Load with `load(path)` and trust the auto-picked bus; the prefix in the name lies:

| Name prefix | Actual bus | File |
|---|---|---|
| can2 | 3 | `can2-3.3.005-778-g7404ab27_ota_1_edge-20260404-233246.crtd` |
| can2 | 3 | `can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-182723.crtd` |
| can2 | 3 | `can2-3.3.005-802-gc4a679ec-dirty_ota_1_edge-20260407-093156.crtd` |
| can3 | 2 | `can3-3.3.005-792-gea41f218-dirty_ota_1_edge-20260405-005724.crtd` |
| can3 | 2 | `can3-3.3.005-798-g8242a608-dirty_ota_1_edge-20260409-221040.crtd` |
| can3 | 2 | `can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-233616.crtd` |
| can3 | 2 | `can3-3.3.005-817-gecdd91a1-dirty_ota_0_edge-20260407-101513.crtd` |
| can3 | 2 | `can3-3.3.005-817-gecdd91a1-dirty_ota_0_edge-20260407-132857.crtd` |
| can3 | 2 | `can3-3.3.005-817-gecdd91a1-dirty_ota_1_edge-20260407-134245.crtd` |
| can3 | 2 | `can3-3.3.005-817-gecdd91a1-dirty_ota_1_edge-20260407-144841.crtd` |
| can3 | 2 | `can3-3.3.005-817-gecdd91a1-dirty_ota_1_edge-20260407-214739.crtd` |

**Multi-bus files** — actually contain frames from two buses. `load(path)` will
refuse with a `ValueError` listing the buses present; pass `bus=N` explicitly or
use `load_multi(path)`:

| Buses | File |
|---|---|
| 2+3 | `can3-3.3.005-778-g7404ab27_ota_1_edge-20260404-235013.crtd` |
| 2+3 | `can3-3.3.005-797-g4afb8c2c-dirty_ota_0_edge-20260405-091701.crtd` |
| 2+3 | `can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-223732.crtd` |
| 2+3 | `can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-224459.crtd` |
| 2+3 | `can3-3.3.005-817-gecdd91a1-dirty_ota_0_edge-20260407-151019.crtd` |
| 2+3 | `can3-3.3.005-817-gecdd91a1-dirty_ota_0_edge-20260407-220545.crtd` |
| 2+3 | `can3-3.3.005-830-g2393b7b5-dirty_ota_0_edge-20260408-151419.crtd` |
| 2+3 | `can3-3.3.005-830-g2393b7b5-dirty_ota_1_edge-20260408-115558.crtd` |

The two `kcan-*` test fixtures (`kcan-can3-clima_on_off.crtd`, `kcan-synthetic.crtd`)
are also technically multi-bus but are test inputs referenced by path in the C++
test suite — do **not** rename them, and load them with an explicit `bus=3` filter.

Files not listed here are clean single-bus captures that match their name prefix.
The audit was regenerated 2026-04-12 by `tests/analysis/scratch/apr7_charge_audit.py`.
Post-`9fdbf8b7` captures don't need this table — the bus filter works correctly now.

## Running the replay test

```
cd tests/
make test
```

## File format

CRTD (CAN Recording Text Data) lines look like:

```
<unix_timestamp> <bus><dir><bits> <id_hex> <byte0> <byte1> ...
```

Examples:
```
1775036991.851551 3R11 464 08 9a 01 00 00 00 00 00
1775036948.298901 3R29 1B000010 10 00 01 02 05 00 00 00
```

- `3R11` — bus 3 (KCAN), receive, 11-bit standard frame
- `3R29` — bus 3 (KCAN), receive, 29-bit extended frame
- `2R11` — bus 2 (FCAN), receive, 11-bit standard frame
- `3CER` — bus 3, CAN error (skipped by the parser)
