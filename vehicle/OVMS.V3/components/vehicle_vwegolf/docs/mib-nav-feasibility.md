# MIB Navigation ↔ OVMS — Feasibility Spike

Status: **H1 CONFIRMED — 2026-07-01.** Nav destination/guidance IS readable on KCAN.
Phase 1 capture analysed (`can3-…-20260701-113544.crtd`). H2 (inject) still untested.

> **PII WARNING.** The 20260701-113544 capture contains a real navigation destination
> (name + street + postcode + locality/county) as cleartext ASCII on BAP node 0x32, and a
> VIN fragment on `0x6B4`. **Do not push this `.crtd` (or any verbatim payload) to the
> public upstream repo.** Redact/regenerate before sharing. See `feedback_no_pii_in_repo`.

## Question

Can OVMS interact with the e-Golf's integrated MIB navigation — either **read** its
route/guidance state, or **inject** a destination — using only OVMS + a self-hosted
backend, to reduce app sprawl and make use of the built-in nav display?

Two hypotheses, ranked by value:

- **H1 (read nav state)** — nav guidance (active route / next-turn / distance / ETA /
  destination) is visible on a bus OVMS already taps. Value: backend learns where the
  driver is going and advises charge stops for the car's own route.
- **H2 (inject destination)** — OVMS writes a destination into MIB nav over a reachable
  bus. Strong negative prior.

## Phase 0 — desk RE findings

- **Bus reach.** OVMS taps only `can1` OBD/diag (active), `can2` FCAN (listen-only),
  `can3` KCAN (active, only writable convenience bus). The MIB head unit lives on the
  infotainment CAN **behind** the J533 gateway — not directly tapped. DBC node list
  (`vwegolf.dbc:21`) has no radio/MIB/nav/amplifier node.
- **Hardware ceiling.** Stock OVMS3 = 1× ESP32CAN + 2× MCP2515, all three already used
  (`main/ovms_peripherals.cpp:147-173`). A 4th bus (`can4`) needs an *external*
  MCP2515+transceiver add-on (ref pattern `vehicle_voltampera.cpp:167-171`).
- **The cluster is on KCAN and OVMS reads it.** `Instruments` node broadcasts
  `Instruments_Range` (ID 1525 / `0x5F5`), decoded at `IncomingFrameCan3` case `0x05F5`
  (`src/vehicle_vwegolf.cpp:673`). KCAN and FCAN are gateway-bridged (`docs/index.rst`).
- **VW pushes nav to the cluster MFA over BAP.** On MQB, the instrument cluster (Kombi)
  renders turn-by-turn / map hints fed from the nav unit over **BAP** (nav→cluster,
  display direction; toggled by the cluster's "BAP communication" coding bit). Since the
  cluster receiving these is the same node OVMS already sees on KCAN, **BAP nav-display
  frames may traverse KCAN** — this is the crux Phase 1 tests.
- **No public RE ref documents a nav LSG/frame** for the e-Golf comfort CAN
  (`thomasakarlsen/e-golf-comfort-can` covers OCU/battery/clima only; `revag-bap` and the
  MIB2-PQ RE project don't map a comfort-CAN nav frame). Only the clima LSG `0x25` is
  reversed here (`docs/clima-control-bap.md`).
- **No online injection path.** VW "send to car" is cloud→car via We Connect; Car-Net/We
  Connect is discontinued on this car (`PROJECT.md`). Dead.

### Gate 0 verdict: **PROCEED to Phase 1.**
H1 is *not* killed — a live cluster on KCAN plus BAP nav-display architecture make it
plausible that nav guidance is observable on a bus we read. H2 remains a strong negative
prior; it is only worth testing if Phase 1 makes nav traffic visible.

## Phase 1 result — H1 CONFIRMED (capture 20260701-113544, KCAN)

The comfort bus is dense with BAP: `0x1733{NN}10` = response/broadcast from BAP node `NN`
(clima `0x25` = `17332510`). Active nodes seen:
`01 05 07 08 09 0A 0C 0D 0E 0F 11 12 13 1B 1C 1F 23 25 28 29 31 32 33 37 3C 46`.

**Navigation = BAP node `0x32`** (extended IDs `0x1733321X`). Confirmed by reassembling
multi-frame BAP:

- `0x17333210` (suffix `10`) — continuous ~1 Hz status stream (port `0x16`); the live
  guidance channel (short 1–12 B frames, updated while nav active).
- `0x17333211` (suffix `11`) — event-driven, larger multi-frame messages. On destination
  entry (t≈47 s, the route-active window; frame present almost only in the middle capture
  third, seg `[1,22,8]`) it emits: a ~51-byte frame = **destination name + street + two
  5-digit groups** (postcode/coords), and a ~25-byte frame = **"Locality, County"**.
  Both are **cleartext ASCII**.
- BAP header decode of the text frames: opcode `0x4`, node `0x32`, text on port ~`0x2E`
  (see `docs/vw-bap-protocol.md` for the node/port bit layout).

Node `0x33` (`0x1733331X`) carries the **telephony/SIM operator** name ("TELIA S"), not
nav — secondary. VIN fragments appear on standard ID `0x6B4`.

**Conclusion:** OVMS can *read* the driver's entered destination + address, plus a live
1 Hz guidance status stream, directly off KCAN. This defeats the prior assumption that MIB
nav was unreachable. → the read-side of the ABRP-style goal is feasible **without new
hardware**: backend gets origin (GPS `0x486`) + destination (node `0x32`) + SoC and can
advise charge stops for the car's own chosen route.

Not yet characterised (needs a driving capture with active turn-by-turn): whether the
port-`0x16` status stream encodes next-turn / distance-to-turn / ETA as decodable numeric
fields. H2 (injecting a destination) remains untested and is still expected to be hard —
node `0x32` here is observed emitting *status*, not receiving a set-destination command.

## Phase 1 — car capture procedure (execute on the car)

Goal: determine whether any KCAN frame's payload tracks a nav quantity. KCAN (`can3`)
captures raw — no filter to disable. Use the standard capture tooling
(`docs/dev-guide.md`, "CAN Capture & Replay").

1. Car awake, ign-on, MIB nav operational. Laptop on OVMS hotspot.
2. Start capture:
   ```
   bash vehicle/OVMS.V3/components/vehicle_vwegolf/tests/capture.sh        # can3 (KCAN)
   ```
   (Optionally a second run with `can2` for FCAN — lower priority, listen-only.)
3. Run this **stimulus timeline**, noting wall-clock seconds for each event (for
   `diff_window`):
   - t0: idle baseline, no route (~20 s).
   - Enter a destination in MIB nav, **do not** start guidance yet — hold ~15 s.
   - Start guidance — hold ~20 s.
   - Trigger/approach a turn so the cluster shows a turn arrow + shrinking distance.
   - Let ETA/distance tick for ~30 s.
   - Cancel the route — hold ~15 s.
4. Ctrl-C to stop; the script SCPs the `.crtd` and writes a companion `.md` in
   `tests/candumps/`. Record the stimulus timeline in that `.md`.

## Phase 1b — drive capture to decode the guidance status stream (next)

Goal: characterise the node-0x32 `0x17333210` status stream — decode next-turn,
distance-to-turn, remaining-distance/ETA as numeric fields — before writing the firmware
handler, so the metric set covers live guidance, not just the static destination string.

On the car, with active turn-by-turn guidance to a real destination:

1. `bash …/tests/capture.sh` (can3 / KCAN). Run ≥5 min covering **several turns**.
2. Drive a route with a few close-together turns (roundabout / town grid is ideal) so
   distance-to-turn visibly counts down then resets. Let it run long enough that
   remaining-distance/ETA changes meaningfully.
3. Jot approximate wall-clock seconds at each **turn moment** (optional but sharpens the
   correlation). Put them in the companion `.md` "Sequence" block.

Expected fingerprints in the analysis:
- **distance-to-next-turn** → sawtooth (decreasing, resets to a large value at each turn).
- **remaining distance / ETA** → roughly monotone decreasing across the whole drive.
- **next-turn maneuver** → a small enum byte that changes discretely at each turn.

Decoder is pre-staged: `tests/analysis/scratch/nav_status_decode.py` (point it at the new
`.crtd`; prints per-port trajectories + flags monotone / sawtooth candidates). PII rule
still applies — keep literal text and the raw `.crtd` out of upstream.

## Analysis recipe (`tests/analysis/crtd.py`, in `.venv`)

Drop a one-shot in `tests/analysis/scratch/`. Reuse existing helpers — do not re-parse.

- `load(path, bus=3)` → `Capture`.
- `diff_window(cap, t_event, pre, post)` around each stimulus edge (route-start,
  turn, cancel) → ranked changed IDs. This is the primary discriminator.
- For candidate IDs: `transitions(records, off, mask, shift)` to see whether a
  byte/nibble tracks distance-to-turn (monotone decreasing) or arrow direction
  (discrete); `mux_split()` for muxed frames; `frame_rate()` to spot nav frames that
  only appear while a route is active.
- Watch specifically for **any BAP node ≠ `0x25`** (BAP frames are extended-ID; decode
  the node/port per `docs/vw-bap-protocol.md`) and for IDs present *only* during active
  guidance.

## Decision criteria

- **H1 CONFIRMED** iff a frame's payload reproducibly tracks a nav quantity across ≥2
  independent captures. Then: add the signal to `vwegolf.dbc`, pin ground truth in
  `tests/analysis/test_dbc_decode.py`, expose a metric (e.g. `xvg`-prefixed), and verify
  it lands in Influx via the existing `ovms_server_v3` MQTT feed
  (`ovms/<user>/<vehicleid>/metric/#`, `src/ovms_server_v3.cpp:448-481`).
- **H1 REJECTED** iff no nav-correlated frame appears on KCAN/FCAN across the timeline.
  Record here as a dated negative so it is not re-explored blind.
- **H2** tested only if H1 confirms nav traffic is present: diff a destination-entry
  capture for a route-set opcode. Expectation: none exists on comfort CAN.

## Phase 2 (only if H1 rejected but still wanted) — infotainment-CAN tap

Assess whether the infotainment CAN pair is present at the J533 connector OVMS mates
with, and whether adding an external MCP2515 as `can4` is worth it. Output = decision
memo only; hardware work is a separate, explicitly-authorized follow-up.

## Fallback (out of scope for this spike)

Self-hosted ABRP replacement needs no MIB access and is unblocked: SoC + GPS (from
`0x486`) already flow over MQTT to Influx; backend does route planning with charge stops;
guidance shown on phone/web. Documented here as the default if the MIB path dies.
