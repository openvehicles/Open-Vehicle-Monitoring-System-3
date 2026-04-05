# Injection attempt 1 — FAILED (wrong node/port, missing frames)

## Command sent

```
can can3 tx extended 17332501 0x91 0x08 0x19 0x96 0xFF 0xFF 0xFF 0x14
```

## Why it failed

1. BAP node 0x26 (should be 0x25) — one-bit error: `0x96` vs `0x59` in second header byte
2. BAP port 0x16 (should be port 0x19 for params + port 0x18 for trigger)
3. Claimed 8-byte payload (len byte = 0x08) but sent no continuation — ECU waited forever
4. Never sent the port 0x18 trigger frame that actually starts clima

## Observed bus effects

- TX completed with initial bus contention (txerr jumped 79→86, then settled)
- Frame `0x3E9` transitioned from `fe f8 df ff` (invalid/sleeping) to live values ~234 ms later
- Frames `0x3B5` and `0x530` also changed state
- These changes were likely ECU waking from idle (unrelated to the wrong command), not clima activation
- Car did NOT start HVAC

## Corrected injection sequence (3 frames required)

See `docs/clima-control-bap.md` for full protocol documentation.

```
can can3 tx extended 17332501 80 08 29 59 01 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 01
```

To stop:

```
can can3 tx extended 17332501 80 08 29 59 02 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 00
```

## Outstanding unknowns

- Temperature encoding in the port 0x19 payload (byte 6 = `0x20` = suspected 22°C; TBD)
  - Port 0x16 schedule uses `byte = celsius + 35` (confirmed: 20°C = `0x37`)
  - Port 0x19 immediate-start payload encoding not yet confirmed — needs capture of
    two starts with different target temps, or comparison with thomasakarlsen's temp setting
- Duration encoding (byte 1 = `0x06`, unit unknown — may be minutes or 5-min intervals)
