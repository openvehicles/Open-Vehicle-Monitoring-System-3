# MCP2515: SetAcceptanceFilter() has no effect — RXB0CTRL RXMODE overrides filters

## Summary

`mcp2515::SetAcceptanceFilter()` silently does nothing. The filter/mask registers are
written correctly, but `Start()` hardcodes `RXB0CTRL = 0b01100100` (RXMODE=3, receive
all messages) which bypasses the acceptance filter hardware entirely on RX buffer 0.

## Affected versions

Present since `4302400b0` (ESP32CAN, MCP2515: add hardware acceptance filter APIs &
shell commands). The `RXB0CTRL` initialisation predates that commit — it has been
RXMODE=3 since the original 2017 driver.

## Root cause

MCP2515 datasheet §4.2: RXBnCTRL bits [6:5] (RXMODE) control whether filters are
applied:

| RXMODE | Behaviour |
|--------|-----------|
| 00     | Receive valid messages matching acceptance filter |
| 11     | Receive all messages (filters ignored)           |

`Start()` sets RXB0CTRL to `0b01100100`:

```cpp
// vehicle/OVMS.V3/components/mcp2515/src/mcp2515.cpp
WriteRegAndVerify(REG_RXB0CTRL, 0b01100100, 0b01101101);
//                                ^^                     bits [6:5] = 0b11 = RXMODE=3
```

`SetAcceptanceFilter()` writes the six filter registers and two mask registers (correct),
but never touches RXMODE. RXB0 therefore catches every frame on the bus regardless of
what filters are programmed. With the BUKT (rollover) bit also set, RXB0 spillover fills
RXB1 before RXB1's own filters ever get a chance to act.

RXB1CTRL is not explicitly initialised and resets to 0x00 (RXMODE=0, filtered) — so
filter[2]–filter[5] would work on RXB1 in isolation, but they never get to, because
RXB0 in promiscuous mode captures everything first.

## Impact

On a high-traffic CAN bus (e.g. VW e-Golf FCAN during OBC charging, ~2000 frames/s)
the intent is to use `SetAcceptanceFilter()` to admit only the handful of IDs the
vehicle module decodes, reducing the ISR rate. With RXMODE=3 the MCP2515 receives every
frame, overflows its RX buffers, floods the ISR, and starves the OVMS Events task →
task watchdog reset.

No existing vehicle module calls `SetAcceptanceFilter()` in production (NIU GTevo has
the call commented out), so the bug went undetected.

## Proposed fix

Inside `SetAcceptanceFilter()`, after writing the filter/mask registers and while still
in CONFIG mode, clear the RXMODE bits on both RX buffers:

```cpp
// Set RXMODE=0 (filter-matched) on both RX buffers.
// RXB0CTRL is initialised with RXMODE=3 (receive all) in Start(); switch to
// filtered mode now that filters are configured.  BUKT bit is preserved via mask.
// Callers passing all-zero masks get functionally equivalent promiscuous behaviour.
m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_RXB0CTRL, 0b01100000, 0b00000000);
m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_RXB1CTRL, 0b01100000, 0b00000000);
```

`REG_RXB1CTRL` (0x70) also needs adding to `mcp2515_regdef.h`.

Backwards compatibility: callers that pass all-zero masks (the shell command default)
will still receive every frame because mask=0 means no bits are required to match.
RXMODE=0 with mask=0 is functionally identical to RXMODE=3.

## Reproducing

1. Configure filters via `SetAcceptanceFilter()` or the `setaccfilter` shell command.
2. Monitor `can canX status` — `rxpkt` rate will be unchanged; filtered IDs are not
   being dropped in hardware.
3. Alternatively: read RXB0CTRL via SPI after calling `SetAcceptanceFilter()` — bits
   [6:5] will still read 0b11.
