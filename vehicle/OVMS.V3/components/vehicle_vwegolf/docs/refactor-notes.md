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

### 3. All KCAN TX frames sent on wrong bus (critical)

**Symptom:** Climate control, OCU keepalive, wake ping, and wakeup command had no
effect on the car. Commands appeared to succeed in firmware but the car never responded.

**Root cause:** Every TX call used `m_can2` (FCAN, powertrain bus). The clima ECU,
OCU keepalive target, and all BAP frame recipients live on KCAN (can3). Frames sent
on FCAN are not forwarded to KCAN by J533 — only the RX direction is bridged.

**Evidence:** The working manual injection captures (`kcan-can3-clima_on_off.crtd`,
`clima-start-test2.md`) show TX frames as `3T29 17332501` (can3 transmit). The
baseline captures confirmed can3 = KCAN and can2 = FCAN.

**Fix:** Changed all seven TX callsites from `m_can2` to `m_can3`:
`SendOcuHeartbeat`, `SendBapWakePing`, `CommandWakeup` (×2), `CommandClimateControl` (×3).

### 4. OCU keepalive too slow for VW OSEK NM (critical)

**Symptom:** After flashing and pressing clima toggle, the car reacted briefly (relay
click from the OCU heartbeat starting) but the clima ECU never entered remote mode and
never ACKed any BAP command. `17332510` had zero frames in the capture.

**Root cause:** `SendOcuHeartbeat()` was called only from `Ticker1` (1Hz). VW OSEK NM
requires keepalives at ~200ms intervals (~5Hz). At 1Hz the network manager considers
the OCU node absent, and the clima ECU ignores all commands.

**Evidence:** Working capture (`kcan-can3-clima_on_off.crtd`) shows `3T11 5A7` TX
frames at ~10Hz — consistent with the heartbeat being triggered by incoming frames.
New build capture shows exactly 1.0s gaps between heartbeat frames and zero ECU
response on `17332510`.

**Fix:** Added `m_ocu_heartbeat_counter` incremented on every incoming KCAN frame.
`SendOcuHeartbeat()` fires every 75 frames (~5Hz on the ~375fps active bus).
`Ticker1` is retained as a low-activity fallback.

### 5. SOC sentinel written to persistent metric (0x131)

**Symptom:** After a cold boot the SOC metric shows 127.5% until the first real
frame arrives and overwrites it. If OVMS reboots before the ECU sends a real value
(e.g., the car goes back to sleep), the 127.5% value is persisted via metric
persistence and survives the reboot.

**Root cause:** The BMS broadcasts `d[3] = 0xFE` on 0x131 for roughly the first 90
frames (~3 s) after wake, before it has a valid state of charge. The decoder did not
filter this sentinel. `0xFE * 0.5 = 127.0%` was written unconditionally to
`ms_v_bat_soc`.

**Fix:** Added `if (d[3] == 0xFE) break;` before the decode, matching the pattern
already used for 0x066E (cabin temperature). The metric stays undefined until the
first real value arrives.

**Confirmed from:** baseline captures `can2-*-233246.crtd` (93 sentinel frames out
of 4,097 total) and `can3-*-235013.crtd` (5 sentinel frames out of 883 total).

### 6. 0x05EA StandklimaStatus_02 shift error (cosmetic)

`(d[3] & 0x38) >> 6` — masking bits 5:3 then shifting right 6 produces zero always.
Should be `>> 3`. Fixed. Only affects a verbose-level log line; no functional impact.

### 7. Optimistic `ms_v_env_hvac` set caused app toggle to invert on TX failure

**Symptom:** pressing the clima start button appeared to do nothing. Pressing it a second
time sent a STOP command instead of START, so clima never started.

**Root cause:** `CommandClimateControl` called `ms_v_env_hvac->SetValue(enable)` before
any ECU confirmation. If the TX failed silently (CAN bus error during wake-up burst,
bus-off recovery), the metric was set to `true` anyway. The app then read HVAC as ON and
sent `enable=false` on the next press.

**Evidence:** `can3-*-004737.crtd` — bap_counter=0x01 (start) command was silently
dropped after the wake ping's error interrupt. bap_counter=0x02 command was `29 58 00 00`
(stop byte) because the app saw HVAC=true. The ECU never received a valid start command.

**Fix:** Removed `ms_v_env_hvac->SetValue(enable)` from `CommandClimateControl`. The
metric stays undefined until a real ECU ACK is decoded from `0x17332510`. Now repeated
presses always send START until the ECU confirms.

### 8. Charge state never set (`ms_v_charge_state` always "stopped")

**Symptom:** OVMS Connect showed "stopped" during an active charging session.
`v.c.current`, `v.c.power`, and `v.c.voltage` were all empty.

**Root cause:** `CommandClimateControl` only set `ms_v_charge_inprogress` (boolean). The
app reads `ms_v_charge_state` (string: "charging"/"stopped") and the separate charge-side
metrics (`ms_v_charge_voltage` etc.). None of these were ever set; `NotifyChargeStart`
and `NotifyChargeStop` were never called.

**Fix:** In the 0x0594 decode, added: transition detection for NotifyChargeStart/Stop,
`ms_v_charge_state` set to "charging"/"stopped", and `ms_v_charge_voltage` mirrored from
`ms_v_bat_voltage` while charging (same HV bus during AC session).

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

## Operational constraint: 0x5A7 conflict window

When the car's ignition is on or was just turned off (within the OSEK NM ring-down
period, ~10–15s), the OEM OCU sends non-zero `0x5A7` frames. Our firmware also sends
all-zeros `0x5A7`. The different data values cause arbitration loss → TX_Fail → can3
bus-off → all KCAN TX fails for ~110ms per cycle.

When the car is fully sleeping, the OEM OCU is off and our `0x5A7` transmits without
conflict (confirmed from `kcan-can3-clima_on_off.crtd` which shows both `3T11 5A7` TX
successes and `3R11 5A7` RX when the car is sleeping — both all-zeros, no conflict).

**User guide must document:** "wait 15 seconds after turning off the car before using
remote climate control." No code fix is needed; this is a protocol constraint.

A future improvement could monitor incoming `0x5A7` bytes and suppress our heartbeat
while non-zero data is seen. Not required for initial validation.

---

## What is NOT yet done (before upstream PRs)

### Charging current frame not yet identified

`ms_v_charge_current` and `ms_v_charge_power` are never set. During a live AC charging
session (328.5V confirmed from `ms_v_bat_voltage`), `v.c.current` showed empty and
`v.c.power` showed empty.

**Root cause:** 0x0191 carries motor/inverter current only (signed, zero while parked and
charging). The OBC (on-board charger) charging current is on a different KCAN frame that
has not yet been identified.

**To resolve:** capture a dedicated charging session with `capture.sh`, filter for frames
that are only active during charging (compare to a parked-not-charging baseline). Candidate
frames to check: anything that only appears when ChargeInProgress bit in 0x0594 is set.
Once the frame and signal encoding are confirmed:
- Decode it in `IncomingFrameCan3`
- Set `ms_v_charge_current->SetValue(amps)`
- Set `ms_v_charge_power->SetValue(ms_v_charge_voltage * amps / 1000.0f)` in kW
- Add the signal to `vwegolf.dbc`

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
