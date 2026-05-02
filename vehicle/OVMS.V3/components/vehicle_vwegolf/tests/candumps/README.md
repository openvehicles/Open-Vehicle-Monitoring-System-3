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

## J533 gateway — bus topology (confirmed from Captures 15–17c, April 2026)

The OVMS module connects to the car via the J533 gateway harness:

| CRTD tag | OVMS bus | Physical bus | Role |
|---|---|---|---|
| `2R11` / `2R29` | can2 | FCAN (powertrain) | BMS SoC, BMS power, trip energy, gear, VIN |
| `3R11` / `3R29` | can3 | KCAN (convenience) | speed, charge management, clima, GPS, body |

**Bridging:** J533 bridges BMS SoC (0x131) from FCAN onto KCAN. Both buses carry it;
`IncomingFrameCan2` is the primary decode path for FCAN frames, `IncomingFrameCan3`
for KCAN. There is no cross-forwarding between the two handlers.

The CRTD bus number encodes the physical OVMS bus: `2R11` = can2 (FCAN), `3R11` = can3 (KCAN).

All captures in this directory are clean single-bus files. Pre-cleanup captures
(firmware before 3.3.005-800/ota_1, April 12 2026) suffered from a logger bug where
the bus filter was dropped, mixing FCAN and KCAN frames into one file. Those captures
were deleted; see `CAPTURES_TODO.md` for what they contained and what needs to be
redone with clean firmware.

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
