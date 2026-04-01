# candumps/

This directory holds CRTD-format CAN log captures used by `test_crtd_replay.cpp`
to validate the decode pipeline against real car data.

Capture files are **not committed** to this repository because they contain
personally identifying information (VIN, and implicitly vehicle location/time).

## Creating a capture

The `test_crtd_replay` test looks for `candumps/kcan-capture.crtd`.

**Hardware:** Connect your laptop to the OVMS WiFi hotspot (`192.168.4.1`).

**On the laptop** — start receiving:
```
nc 192.168.4.1 3000 > candumps/kcan-capture.crtd
```

**On the OVMS shell** (web UI at `http://192.168.4.1` or SSH) — start streaming:
```
can log start tcpserver transmit crtd :3000 can3
```

Drive or trigger the state you want to capture (charging, driving, etc.), then stop:
```
can log stop
```

Press Ctrl-C on the `nc` command. The `.crtd` file is now ready for use.

## Running the replay test

```
cd tests/
make test
```

The `test_crtd_replay` test will parse the file, dispatch all frames through the
vehicle module, and assert that key metrics (`ms_v_pos_speed`, `ms_v_bat_soc`,
`ms_v_env_gear`) contain plausible values for the state captured.

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

- `3R11` — bus 3, receive, 11-bit standard frame
- `3R29` — bus 3, receive, 29-bit extended frame
- `3CER` — bus 3, CAN error (skipped by the parser)
