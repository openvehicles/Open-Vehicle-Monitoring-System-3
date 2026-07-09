# MIB Nav + OVMS + Backend — Proposed Architecture

> **DECISION RECORD — 2026-07-01. Read this before the architecture below.**
> The original goal was to *replace* the €5/mo ABRP Premium subscription with native MIB nav +
> a self-hosted EV brain. Spike concluded: **don't. Keep ABRP Premium (€50/yr annual); re-scope
> the project.** The architecture below is retained but the routing/EV-brain half is PARKED — see
> § Decision record for what we build instead and why.

Goal: ABRP-style EV routing (charge stops) using OVMS + a self-hosted backend, making the
car's *integrated* MIB nav more useful — **without** injecting a destination over CAN (H2,
unproven/hard). Achieved with two independent channels, each on a path we've confirmed.

## Two channels

	Channel	Direction	Transport	Confirmed?	Needs
A	Read nav + telemetry → backend "EV brain"	car → backend	KCAN (BAP node 0x32) + MQTT	YES (H1, 2026-07-01)	OVMS firmware handler
B	Keep MIB charger POIs fresh	backend → car	USB/SD PersonalPOI	YES (stock MIB feature)	POI generator + manual import

Neither channel needs new car wiring. Channel A rides the existing OVMS KCAN tap; Channel B
rides the MIB's stock USB/SD PersonalPOI import.

## Channel A — live read / EV brain

```
MIB nav ──BAP node 0x32──► KCAN ──► OVMS ──MQTT──► backend ──► phone/web + push
 (dest + 1Hz guidance)         (reassemble        (routing:      ("charge at X in Y km";
 SoC, GPS 0x486                 multiframe →        SoC + consumption   highlight a Channel-B
 also on bus)                   metrics)            + charger DB → plan) POI to pick in car)
```

- **OVMS firmware** (new, `vehicle_vwegolf`): reassemble node-0x32 multi-frame BAP in
  `IncomingFrameCan3`; expose destination (string) + guidance status (distance-to-turn / ETA /
  maneuver, pending Phase-1b drive capture) as `xvg`-prefixed metrics. Publish via the existing
  `ovms_server_v3` MQTT feed. (State machine mirrors the clima burst-gating pattern already in
  `SendClimaBapBurst`, but read-only.)
- **Backend routing**: consumes destination + SoC + GPS from MQTT/Influx; computes charge-stop
  plan for the car's *own* chosen route. Output = phone/web + push notification. Does NOT try to
  steer the car's nav — the MIB does turn-by-turn; backend is the range/charging advisor.

## Channel B — charger POI freshness

The MIB's onboard charger POIs come from the map database (updated rarely → stale). Overlay
fresh chargers as **PersonalPOI** from the live charger-network API.

```
charger-network API ──► backend charger-sync ──► PersonalPOI bundle ──► USB/SD ──► MIB import
   (user's API key)      (normalize: name,        (metainfo2.txt +        (manual menu:
                          lat, lon, power,         PersonalPOI/<variant>/) "Import special
                          connector, network)      per jimmyH/mypois fmt)   destinations")
```

- **Format**: `PersonalPOI/` + `metainfo2.txt`; variant subfolder **`MIB2HIGH`** (this car).
  Reference generator: `github.com/jimmyH/mypois`. Charger metadata (kW, connector, network)
  goes in the POI name/description so it's visible on-screen.
- **Import is manual + destructive**: each import overwrites the previous PersonalPOI set, and
  requires the on-screen "Import special destinations" menu action. → refresh cadence is low
  (pre-trip / weekly), not real-time.
- **Delivery options**:
	Option	Refresh	Effort	Note
	Manual USB stick	re-image stick, plug in, import	low setup	simplest
	Permanent USB-gadget (Pi Zero W)	backend OTA-updates files; driver still presses Import	more setup	no stick swapping; files always current

## Backend (co-located with existing mosquitto + Telegraf + Influx on the gitea box)

	Service	Role	Reuse
	MQTT ingest	nav dest + guidance + SoC + GPS	EXISTING (server_v3 → Telegraf → Influx)
	charger-sync	poll charger API → normalized charger store	NEW (API key)
	routing engine	route + consumption + chargers → plan	NEW (self-host: OSRM/Valhalla/GraphHopper + EV model)
	POI generator	charger store → PersonalPOI bundle	NEW (mypois-based)
	UI / notify	web plan view + charge-stop push	push infra EXISTING (project_selfhosted_telemetry)

## What this deliberately does NOT do

- No CAN destination injection into the MIB (H2) — unproven, high-risk; the driver still enters
  the destination in the MIB (or picks a Channel-B charger POI).
- No dependency on mib2-toolbox (MIB2-High-only green-menu mods) — PersonalPOI is stock on STD +
  High, so the charger channel works regardless of head-unit variant.

## Decisions (locked 2026-07-01)

	Decision	Choice	Impact
	MIB variant	Discover Pro (MIB2 **High**)	PersonalPOI variant folder = `MIB2HIGH`; mib2-toolbox mods available if ever needed
	Charger API	**Nobil** (Nordic DB)	charger-sync polls Nobil REST/JSON (apikey); good SE/NO/DK coverage
	POI delivery	Manual USB stick	backend emits a bundle; re-image stick + import pre-trip; no extra hardware
	Routing	Self-host engine	own routing (OSRM/Valhalla/GraphHopper) + EV consumption model + Nobil charger DB

### Nobil integration notes
- REST/JSON API (`nobil.no`), `apikey` query param; `search`/`rectangle`/`chargerid` endpoints.
- Per station: position `(lat, lon)`, operator/name, and per-connector attributes (connector
  type, power kW) under `Connectors`. Map → PersonalPOI: name = operator + max kW; description =
  connector types/power; category = charging. Cache locally; poll on a low cadence.
- Rate-limited — sync to a local charger store (feeds both routing and the POI generator); do
  not hit the API per route.

## Build order (once questions answered)

1. Phase-1b drive capture → decode node-0x32 guidance fields (distance/ETA/turn).
2. OVMS firmware handler → destination + guidance metrics over MQTT (Channel A).
3. Backend charger-sync + PersonalPOI generator (Channel B) — independent of firmware, can start
   in parallel.
4. Backend routing engine + UI/notify tying A's live state to B's charger set.

## Decision record — 2026-07-01: keep ABRP Premium, re-scope

### What we actually wanted
Not "native nav for its own sake" — the real want was **EV-aware turn-by-turn (charge stops) on
the car's main screen, without the €5/mo ABRP Premium.** Current daily reality = naive native car
nav **or** EV-unaware Apple Maps on CarPlay. ABRP Premium fixes it but costs money.

### Why we're NOT replacing it (the walls are not ours to move)
The €5/mo buys **EV nav on the CarPlay screen.** Every free route to that surface is blocked by a
wall no amount of OVMS/backend work can move:

	path to EV nav on the car screen	blocker	movable by us?
	Apple Maps EV routing on CarPlay	needs VW↔Apple ECU integration; e-Golf unsupported, never will be (discontinued car)	NO — VW's call
	DIY router app on CarPlay	Apple CarPlay navigation entitlement (MFi) — not granted to hobbyists	NO — Apple's gate
	ABRP free tier on CarPlay	CarPlay is Premium-only; free tier now caps at 1-stop trips, no in-car nav	NO — ABRP's paywall
	ABRP Premium on CarPlay	€50/yr (annual, ≈€4/mo)	— works today

No free EV *planner* exists on CarPlay (Google Maps / PlugShare / Chargemap are finders, not
charge-aware planners). Feasibility spike (`mib-nav-feasibility.md`) already showed native MIB nav
**cannot plan** (Q1: PersonalPOI is a cosmetic pin, not an EV entity) and its dead online brain
**cannot be revived** (Q3: authenticated VW backend + undocumented protocol, no public prior art —
`mib2-toolbox#203` still open). So "native nav replaces ABRP" was never reachable.

### Decision
**Pay ABRP Premium (€50/yr).** It's the cheap answer to a structurally unbuildable problem — not a
sign the project failed. Then re-scope to the pieces that pay off independently of that subscription:

	item	status	rationale
	OVMS → ABRP live telemetry	BUILD (cheap win)	plugin already exists (`~/projects/ovms_plugin-abrp`); feeds real SoC/consumption so the paid ABRP is accurate for THIS car. Highest value/effort.
	ChargeFinder (`ev-chargers`)	OPTIONAL / hobby	free live SE charger availability + map + native-nav POI pins (Channel B). NOT a planner; never competed with ABRP. Keep if enjoyable; drop if ABRP already scratches it. Docs: `ev-chargers/docs/poi-exporter.md`.
	Channel A nav-read (node 0x32)	PARK	only mattered if OUR backend did the planning. ABRP plans → not needed. Drops the hardest firmware work.
	Self-host routing engine (router repo)	PARK	only unique value was free multi-stop planning; a big build to undercut ≈€4/mo.

### If this is ever revisited
Re-open only if one of the walls moves: ABRP kills/hikes the sub, VW back-ports Apple Maps EV to the
e-Golf (won't happen), or a CarPlay entitlement path opens for personal apps. Absent that, the €50/yr
stands and Channel A/router stay parked. Channel B (`poi-exporter.md`) remains independently viable
as a charger layer regardless.
