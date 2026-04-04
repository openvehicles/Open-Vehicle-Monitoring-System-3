# Refactor Notes — climate-control branch

This document captures the decisions made during the refactor of `vehicle_vwegolf.cpp`
on the `climate-control` branch, and what needs to happen before any of this goes
upstream. The intent is to break it into small, reviewable PRs once the implementation
is validated on the car.

---

## Why a refactor before upstream submission

The initial implementation accumulated several categories of debt:

- **Real bugs** that caused regressions (see below)
- **Maintainer rule violations** that would cause a PR to be sent back immediately
- **Code patterns** that obscure intent, making it harder to debug from captures

None of this is upstream-ready as-is. The refactor cleans the slate so each feature
PR is focused on one thing and easy to review.

---

## Bugs fixed

### 1. OCU heartbeat data always zero (critical)

**Symptom:** horn, mirror fold, lock, unlock, indicators, panic never had any effect
on the bus. Clima "worked then stopped" depending on which code path was exercised.

**Root cause:** `SendOcuHeartbeat` built the correct bit pattern into `data[]`, then
re-initialized the entire array to zeros with a TODO comment before the `WriteStandard`
call. The TODO was never resolved. The frame always transmitted all zeros.

**Fix:** removed the re-initialization block. Array is zero-initialized once at
declaration (`uint8_t data[8] = {}`); action bits are OR'd in; sent as-is.

**Why this is non-obvious:** the action bits ARE set correctly before the overwrite,
so the logic looks right on a quick read. The bug is the second initialization block
at the bottom of the function.

### 2. BAP multi-frame start used as wake stimulus

**Symptom:** clima start intermittently did nothing when sent from a sleeping bus.

**Root cause:** when the bus was sleeping, `CommandClimateControl` called
`CommandWakeup` (two NM-style extended frames) then waited 300 ms. If the NM frames
TX_Failed, the wait was sometimes insufficient: Frame 1 of the BAP sequence also
TX_Failed, the ESP32 entered a ~110 ms bus-off recovery, and by the time Frame 2
arrived the ECU had no record of a preceding start — orphaned continuation, command
discarded, Frame 3 fires with no parameters.

**Fix:** `CommandClimateControl` now sends a standalone BAP Get on port 0x01 (`09 41`)
as the wake ping. A Get has no multi-frame state, so a TX_Fail loses nothing. After
200 ms the 3-frame command sequence is sent with the bus live.

**See also:** `clima-control-bap.md` § "Why the multi-frame start frame must not be
used as a wake ping" for the full timing analysis.

### 3. 0x05EA StandklimaStatus_02 shift error (cosmetic)

`(d[3] & 0x38) >> 6` — masking bits 5:3 then shifting right 6 produces zero always.
Should be `>> 3`. Fixed. Only affects a verbose-level log line; no functional impact.

---

## Maintainer rule violations fixed

These would have caused any upstream PR to be returned immediately.

| Violation | Rule | Fix |
|---|---|---|
| `TAG "-583"`, `TAG "-0FD"` etc. in every log call | Single log tag per component | Changed to `TAG` throughout |
| `Spiegelanklappen`, `Hupen`, `Warnblinken` in log strings | English only in user-facing text | Changed to English |
| Static rate-limit counters (`cnt`, `cnt5`, `cnt59`…) inside switch cases | No module-level state in local statics | Removed; noisy messages moved to `LOGV` (already gated by log level) |
| `m_is_car_online` as a separate bool derived from `m_last_message_received` | State belongs in class members, no redundancy | Merged into `m_bus_idle_ticks`; bool is now computed inline |

---

## Design decisions

### `m_ocu_active` (was `m_is_control_active`)

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
`CommandClimateControl` called immediately after boot would skip the wake ping.

### Wake ping vs CommandWakeup

`SendBapWakePing` (used by `CommandClimateControl`) and `CommandWakeup` do different
things:

- `SendBapWakePing`: a 2-byte BAP read-only query to the clima ECU. Lightweight,
  safe to lose (TX_Fail is fine), used only to put dominant bits on the bus before
  a BAP command sequence.
- `CommandWakeup`: sends two extended NM-style frames that trigger broader KCAN wake
  (observed: 0x5F5 and other ECUs respond). Used when the full car needs to wake up,
  not just the clima ECU.

`CommandClimateControl` does not call `CommandWakeup` because clima can be started
without waking every ECU on KCAN — the clima ECU wakes independently.

### `SendOcuHeartbeat` one-shot flag pattern

Each action flag (`m_horn_requested`, etc.) is set by its `Command*` method, then
consumed and cleared on the very next `SendOcuHeartbeat` tick. This means commands
fire once and are not repeated if the ticker fires again before the next command. The
action appears in exactly one 0x5A7 frame.

---

## What is NOT yet done (before upstream PRs)

### Temperature encoding for port 0x19 (Frame 2, byte index 6)

`0x20` is the thomasakarlsen default and is currently hardcoded. The encoding for
port 0x19 is not confirmed — port 0x16 (schedule) uses `byte = celsius + 35` but
port 0x19 may differ.

**To resolve:** capture two clima starts back-to-back with different target temperatures
set via `xvg cc_temp`. Diff Frame 2 byte 6 between the two captures. Once confirmed,
replace `0x20` with the encoded `m_climate_temp`.

### `m_climate_on_battery` is read from config but never sent

The "allow climatisation on battery" flag exists in the BAP protocol but is not yet
included in the command frames. Needs a capture of a battery-allowed vs not-allowed
start to identify which byte/bit carries it.

### Lock and unlock require authentication bytes

`CommandLock` and `CommandUnlock` set the bits in 0x5A7 but have TODO notes indicating
that vehicle-specific authentication bytes are required alongside the action bit. Without
them the gateway likely ignores the command. Needs a capture of the OEM app locking
the car to identify the auth pattern.

### GPS sign bits not decoded

Latitude and longitude from 0x0486 are unsigned. Sign bits for southern/western
hemispheres not yet identified. Not relevant for the-module's current location but should
be fixed before upstream submission.

### `CommandWakeup` second frame necessity unclear

The second frame (`0x1B000067`) in `CommandWakeup` is an NM presence frame for node
0x67. It was observed in wake captures but it is not confirmed whether it is actually
necessary or a coincidence. A controlled test (wake with only the first frame) would
clarify.

---

## Planned upstream PRs (after car validation)

Break the feature into small PRs so each one is easy to review and revert:

1. **Decode-only baseline** — all `IncomingFrameCan3` metrics, no control. This is
   the lowest-risk PR and should go first.
2. **CommandWakeup** — KCAN wake sequence only, no clima.
3. **CommandClimateControl** — clima start/stop, depends on wakeup PR.
4. **OCU heartbeat actions** — horn, indicators, panic, mirror fold.
5. **Lock/Unlock** — only after auth byte pattern is confirmed.
6. **User guide** — required for merge; covers hardware wiring, J533 adapter,
   all config parameters and commands.

Each PR should reference the relevant `docs/` RE notes and include a test capture
replay result.

---

## Branch strategy reminder

- `investigation` — RE notes and captures only, pushes freely
- `climate-control` — this implementation branch; pre-push hook requires tests to pass
- upstream PRs are branched from `master` as `feat/*` or `fix/*`

Do not squash the commits on `climate-control` before extracting upstream PRs —
the commit history is useful context for reviewing what changed and why.
