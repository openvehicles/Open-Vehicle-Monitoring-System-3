# Refactor Notes — climate-control branch

Design decisions and operational constraints for the `climate-control` implementation.
Bug history and maintainer rule violations fixed are recorded in the commit log.

Upstream PR plan → `PROJECT.md`. Signal definitions → `docs/vwegolf.dbc`.

---

## Design decisions

### `m_ocu_active`

OVMS only sends the OCU keepalive (0x5A7) after it has deliberately taken an action
(wakeup, climate start, etc.). It does not assert OCU presence at cold boot. This is
intentional: an unexpected node on KCAN at startup could confuse the gateway, and the
keepalive suppression avoids TX error accumulation when the bus is sleeping.

The `xvg offline` shell command sets this flag back to false and is retained for
field testing — useful when you want to stop OVMS broadcasting on KCAN without
restarting the module.

### `m_bus_idle_ticks` initialized to `VWEGOLF_BUS_TIMEOUT_SECS`

At cold boot we have no evidence the bus is live. Initializing to the timeout value
means OVMS treats the bus as offline until it receives a frame. Without this,
`CommandClimateControl` called immediately after boot would skip the wake sequence.

### Wake ping vs CommandWakeup

`CommandWakeup` and the internal wake ping do different things:

- **`CommandWakeup`**: sends `0x17330301` (dominant wake bits) then `0x1B000067` (OSEK
  NM alive for node 0x67). Required when waking from deep sleep — joins the NM ring so
  the clima ECU accepts subsequent BAP commands.
- **Wake ping** (`09 41`): a 2-byte BAP Get on port 0x01. Used only when the bus is
  already live — harmless if it TX_Fails, just primes the TX queue.

`CommandClimateControl` calls `CommandWakeup` when `m_bus_idle_ticks >= VWEGOLF_BUS_TIMEOUT_SECS`.
On an active bus it sends the wake ping instead.

### `SendOcuHeartbeat` one-shot flag pattern

Each action flag (`m_horn_requested`, etc.) is set by its `Command*` method, then
consumed and cleared on the very next `SendOcuHeartbeat` tick. The action appears in
exactly one 0x5A7 frame and is not repeated if the ticker fires again before the next
command.

### Optimistic `ms_v_env_hvac` update

`CommandClimateControl` sets `ms_v_env_hvac` immediately after Frame 3 TX succeeds.
This gives the app immediate feedback (~200ms) rather than waiting 4–5s for the ECU ACK.
The 0x17332510 BAP ACK (`49 58 XX`) overwrites with ground truth when it arrives.

The guard is Frame 3 TX success (not `ESP_FAIL`) — if transmission fails the metric is
not touched and repeated presses will correctly retry START rather than flipping to STOP.

---

## Operational constraint: 0x5A7 conflict window

When the car's ignition is on or was just turned off (within the OSEK NM ring-down
period, ~10–15s), the OEM OCU sends non-zero `0x5A7` frames. Our firmware also sends
all-zeros `0x5A7`. The different data values cause arbitration loss → TX_Fail → can3
bus-off → all KCAN TX fails for ~110ms per cycle.

When the car is fully sleeping, the OEM OCU is off and our `0x5A7` transmits without
conflict (confirmed from `kcan-can3-clima_on_off.crtd` — both `3T11 5A7` TX successes
and `3R11 5A7` RX seen; all-zeros, no conflict).

**User guide must document:** "wait 15 seconds after turning off the car before using
remote climate control." No code fix needed; this is a protocol constraint.

A future improvement could monitor incoming `0x5A7` bytes and suppress our heartbeat
while non-zero data is seen.
