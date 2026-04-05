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
