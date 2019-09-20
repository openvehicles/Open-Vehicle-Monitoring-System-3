CANopen Shell Commands
======================

.. highlight:: none

Show status report
  ::

    copen status

Start / stop workers
  ::

    copen <bus> start
    copen <bus> stop

  - stop will fail if clients are still running.

Send NMT commands
  ::

    copen <bus> nmt <start|stop|preop|reset|commreset> [id=0] [timeout_ms=0]

  - id 0 = broadcast
  - timeout > 0: wait for state change (heartbeat), 3 tries
    Note: state change response is not a mandatory CANopen feature.

Read SDO
  ::

    copen <bus> readsdo <id> <index_hex> <subindex_hex> [timeout_ms=50]

  - index & subindex: hexadecimal without "0x" or "h"
  - defaults to 3 tries on timeout

Write SDO
  ::

    copen <bus> writesdo <id> <index_hex> <subindex_hex> <value> [timeout_ms=50]

  - index & subindex: hexadecimal without "0x" or "h"
  - value: prefix "0x" = hex, else decimal, string if no decimal
  - defaults to 3 tries on timeout

Show node core attributes
  ::

    copen <bus> info <id> [timeout_ms=50]

  - prints device type, error register, device name etc.
  - Note: some attributes read are optional and may be empty/zero.

Scan bus for nodes
  ::

    copen <bus> scan [[startid=1][-][endid=127]] [timeout_ms=50]

  - loops "info" over multiple node ids (default: all)
  - Note: a full scan with default timeout takes ~20 seconds

