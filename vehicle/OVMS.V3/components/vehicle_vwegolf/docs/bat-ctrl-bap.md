# Battery Control (Climate + Charge) via BAP — RE Notes

References:
- https://github.com/karlsen-technologies/smartkar-cano-new/tree/2b191b8e9066d494125b3c0338787a347ec8d205/docs/canbus-reverse-engineering — `BAP_PROTOCOL.md` and `BAP_BATTERY_CONTROL.md` (thomasakarlsen; supersedes e-golf-comfort-can for BAP). Pinned to the commit this reference was validated against.
- MIB2 firmware RE — message/field names below marked *(MIB2 FW)*
- Local BAP framing notes: `vw-bap-protocol.md` (being aligned to standard terminology)

Terminology (standard BAP terms; the left column lists informal terms sometimes seen elsewhere):

| Informal term | Standard BAP | Meaning |
|---|---|---|
| "node" 0x25 | **LSG ID** 0x25 | Logical control unit: Battery Control |
| "port" | **Function ID** | Function within the LSG |
| "command"/"status push" opcode 2 | **OpCode** 0x02 SetGet | Set value, request confirmation |
| "rolling counter" | **Transaction-ID** (array header) | Echoed in FSG response |

## Channel

Battery Control channel, LSG 0x25. OVMS acts as an **ASG** (client); the Battery Control Unit is the **FSG** (server).

| Direction | CAN ID (29-bit) | Description |
|---|---|---|
| ASG → FSG | `0x17332501` | Commands (also carries FSG keepalive polls / 5 s status bursts observed on-car) |
| FSG → ASG | `0x17332510` | Status responses, ACKs, broadcasts |

## Function IDs (LSG 0x25)

Names per smartkar BAP_BATTERY_CONTROL.md; "obs." column = what was observed on-car.

| Func | Name | Dir | On-car observations |
|---|---|---|---|
| 0x01 | GetAllProperties | → FSG | empty SetGet poll seen |
| 0x02 | BAP-Config | → FSG | empty SetGet poll seen |
| 0x10 | PlugState | ← FSG | — |
| 0x11 | ChargeState | ← FSG | — |
| 0x12 | ClimateState | ← FSG | 7-byte status ~1 s; payload[0] `05`=active `00`=idle (observed; not decoded by this driver) |
| 0x13 | TimerState | ← FSG | timer enable state; observed `04 04`/`04 00` |
| 0x14–0x17 | timer/schedule slots 1–4 | ↔ | 8-byte records; Ack confirms writes |
| 0x18 | ClimateOperationModeInstallation | → FSG | start/stop trigger, 2-byte payload |
| 0x19 | ProfilesArray | ↔ | Battery Control Profiles (see below) |
| 0x1A | PowerProvidersArray | ← FSG | slot Ack / status observed |

**Stop / HVAC-state detection (as implemented):** the driver takes climate on/off from two sources — the OperationMode `49 58 <flag>` echo (command confirmation) and the clima-ECU `0x5EA` conditioning bit `d[3] & 0x08` (the actual-running signal). It does **not** decode Function 0x12. Note `0x5EA d[3]` bits 6–7 (remote-session) are unreliable — they stay set between "armed" and "running" — so use bit 3, which clears on stop.

## Battery Control Profiles (Function 0x19)

Charging/climate settings live in 4 stored **profiles** (= "Charge Locations" in infotainment): profile 0 = hidden global/immediate profile used by "start now" operations; 1–3 = departure timers. Commands do not carry one-shot parameters — they **write profile fields**, then trigger via Function 0x18.

Profiles are BAP arrays; the **RecordAddr** in the array header selects the record format:
- RecordAddr 0: full profile, 20+ bytes — includes **temperature** at byte 12, encoding `raw = °C × 10 − 100` (22.0 °C → `0x78`), unit byte, lead/holding times, name…
- RecordAddr 6: compact profile, 4 bytes — `operation, operation2, maxCurrent, targetChargeLevel`. **No temperature field.**

operation flags (byte 0): bit0 charge · bit1 climate · bit2 climateWithoutExternalSupply · bit3 autoDefrost · bits4–7 seat heaters. `0x06` = climate + climate-on-battery.

Full field tables: smartkar `BAP_BATTERY_CONTROL.md`.

## Climate Start/Stop

All on `0x17332501`, built via the vendored bap-lib (`src/bap/`, `src/egolf/`) in
`vehicle_vwegolf_bat_ctrl.cpp`.

**What OVMS sends (ON):** a channel-open handshake, and then — only after the FSG acks it — a
**read-modify-write of profile 0**, then the immediate trigger:

```
19 42                          GET BapConfig  (OpCode 0x01 Get, LSG 0x25, Function 0x02)
19 41                          GetAll         (OpCode 0x01 Get, LSG 0x25, Function 0x01)
   ← wait for the FSG's 49 41 / 49 42 ack (registration complete) before continuing
80 04 19 59 <3x-txn> 00 00 04  GET ProfilesArray, get-all (Fn 0x19) → FSG replies 49 59 <3x-txn> …
29 59 <3x-txn> 00 00 01 …      ARM: write profile 0 back, operation forced to climate (see below)
   ← wait for the FSG's 49 59 bx write-echo (arm ACKED) before continuing
29 58 00 01                    OperationMode START (immediate = profile 0)  /  29 58 00 00 = STOP
```

**Two things are required to get the ProfilesArray read answered** (both established on-car):

1. **Handshake must complete first.** The FSG silently *drops* a Function-0x19 GET that arrives
   before it has acked the channel-open handshake (`49 41` / `49 42`). So OVMS sends the handshake
   once, waits for the ack, then GETs — it does not bundle them. (The bare Function-1/2 handshake
   GETs are themselves answered directly, which confirms registration completed.)
2. **The get-all segmented framing.** Even correctly sequenced, the FSG only answers the GET in the
   form the factory clients use: a `80`-segmented **get-all** request `80 04 19 59 <asgTxn> 00 00 04`
   (start-segment, length 4, body `19 59 <asgTxn> 00`). A bare, ranged `19 59 <asgTxn> 00 00 01`
   request is dropped.

Sent this way the FSG answers the **OVMS ASG-ID** (`49 59 3x`), so the read does not depend on any
ambient MIB/OCU profile poll — it works from sleep *and* while the car is already awake.

Why the arm, and why a read-modify-write: the trigger runs *whatever operation profile 0 currently
holds*, and **profile 0 persists these writes** — it is a shared "immediate" scratchpad that climate
and charge both drive, so a bare trigger could start the wrong operation if a previous command left
the wrong op bits. So ON first reads profile 0 back (`19 59` GET → `49 59` reply), then writes it
back with **only the operation byte changed** and **every other byte preserved exactly as read**:
target temperature, min charge %, maxCurrent, hold-times, name. The operation byte is set for a
KNOWN, deterministic result:

* **Climate**: set `climate`, clear `charge`, and **force-set** `climateWithoutExternalSupply`
  ("on battery"), preserving every other byte. So climate always arms op `0x06`, e.g. `0x01 → 0x06`,
  `0x07 → 0x06`, `0x06 → 0x06`. On-battery is force-set (not preserved) because a charge command
  clears it and that write is persistent — without re-asserting it here, climate would stop working
  on battery power after any charge. It is an *allow* flag, not a force: the car's own profile 0
  carries it (op `0x06`) yet still uses wall power when plugged, so setting it only restores the car's
  default and never forces battery use.
* **Charge**: PURE charge — set `charge`, clear **both** `climate` and `climateWithoutExternalSupply`.
  The on-battery bit is a *standalone climate trigger*, not just a modifier: leaving it set made a
  charge start run climate-on-battery when charging couldn't (unplugged) — confirmed on-car. E.g.
  `0x06 → 0x01`.

Re-encoding the car's own bytes means no stored setting is clobbered; the deterministic operation
means the trigger
cannot accidentally start charging.

**Safety — the trigger is gated on a successful arm.** If profile 0 can't be read (no `49 59`
within the wake window) or the arm write fails, OVMS does **not** send the trigger — the command
fails with a notification rather than triggering an unknown operation.

**The trigger WAITS for the arm-write echo (`49 59 bx`).** The profile-0 write is a ~5-frame
multi-frame message the FSG takes ~1.5 s to reassemble and apply. Firing the trigger straight after
the write is a race the trigger loses — it runs the STALE profile-0 op (starting the previous
operation, a delayed actual-start, or running a leftover charge op on a climate command). So OVMS
holds the trigger until the BCU echoes the write (`49 59 bx`, the OVMS asgId with bit7 set), then
triggers against the settled op. If the arm was SKIPPED (op already correct, see below) there's no
write to wait for and the trigger fires immediately.

**STOP is op-specific — OFF reads and arms too.** `29 58 00 00` stops the operation that profile 0
is currently armed to, **not** everything running (confirmed on-car: a charge stop only takes effect
with profile 0 armed to charge `0x01`; if profile 0 holds the climate op the same stop leaves charging
untouched). So OFF runs the identical handshake → GET → arm gate as ON, arming the matching op before
the stop trigger. Climate and charge can run **at the same time**, and because a charge stop arms the
pure-charge op `0x01` (clearing the climate bits), **stopping charge while climate is running would
kill climate** — so OVMS blocks charge-stop while `ms_v_env_hvac` is set (the alert asks the user to
stop climate first). Climate-stop is unaffected and always allowed.

**Skip-write optimization.** For climate/charge, if profile 0 already holds the target op the arm
writes nothing (the ~5-frame RA0 write is a no-op) and only the trigger is sent. SET_CURRENT always
writes (applying the current is the command, and it confirms on the write echo).

**Errors are terminal.** A BAP `ERROR` (opcode 7) on the OperationMode func — e.g. the BCU refusing
to charge while unplugged — aborts the command immediately: no re-fire, and a later `49 58` echo
cannot flip it to "confirmed". The lost-reply backstop only re-sends a step whose reply was genuinely
lost, never one the BCU explicitly rejected.

**The sequence is event-driven, not tick-driven.** Each step is fired by the BCU's own reply on
`0x17332510`: the first status frame heard opens the channel (handshake + profile GET); the `49 59`
profile reply drives the arm **and** trigger; the `49 58 <flag>` echo confirms. So each step fires
the instant its predecessor is acknowledged, and **profile 0 is written exactly once** — re-sending
the whole arm+trigger every tick would rewrite profile 0 mid-start and reset the BCU's spin-up,
stretching the confirmation. `Ticker1` only (a) holds the NM-wake
bridge ~1 Hz and (b) acts as a **lost-reply backstop**: if a reply doesn't arrive within
`VWEGOLF_BATCTRL_RETRY_SECS` it re-sends just that step (re-GET the profile, or re-fire the trigger —
**never re-arm**). Bounded by the wake window (`VWEGOLF_BATCTRL_WAKE_SECS`); if it expires without a
confirmation, OVMS raises a user notification distinguishing the cause — a CAN transmit error, an
explicit BAP ERROR (rejected — e.g. not plugged in), a "could not read the profile", or
a plain timeout.

### ProfilesArray arm — full RecordAddr-0 read-modify-write (what OVMS sends)

OVMS arms profile 0 the way the factory MIB does (`SetBatteryControlProfileListRA0`): a **full
record** write, so no field is dropped. It takes the pos-0 global "Optionen" record straight from
the FSG's `49 59` reply, changes only `operation`, and writes the whole thing back.

```
Frame 1 (start):    80 <len> 29 59  <3x-txn> 00 00 01
Frames 2..N (cont): C0 <RA0 record: 20 fixed bytes + length-prefixed name …>
```

- `29 59` = OpCode 0x02 SetGet, LSG 0x25, Function 0x19
- 4-byte array header: `<3x-txn>` = [ASG-ID:4 | **Transaction-ID**:4], ASG-ID `3` = OVMS (its own
  client id, distinct from MIB `1` and OCU `2`), TID rolls and is echoed by the FSG (as-is for the
  GET reply `49 59 3x`, with bit7 set `49 59 bx` for the SET/write echo) · RecordAddr `00` (full
  RA0) · startIndex `00` (pos 0) · elementCount `01`
- then the RA0 record, byte-for-byte as read back, with `operation[0]` set per the command
  (climate: `(op | climate) & ~charge`, preserving onBattery as read; charge:
  `(op | charge) & ~(climate | onBattery)`;
  see Charge Control).

Field scales (firmware-confirmed, MIB2 RE): maxCurrent = **raw amperes** from the set
{5,10,13,32}; targetChargeLevel / minChargeLevel = raw %, step 10; temperature (byte 12)
`raw = °C×10 − 100` (22.0 °C → `0x78`); operation bit0 charge · bit1 climate · bit2
climateWithoutExternalSupply.

#### Compact RecordAddr-6 form — NOT used (reference)

A compact 4-field partial write is an alternative to the full-record arm:

```
Frame 1 (start):  80 08 29 59  [tid] 06 00 01
Frame 2 (cont):   C0  06 00 20 00      (operation=0x06, operation2=0, maxCurrent=0x20, targetChargeLevel=0)
```

This driver does not use it: it force-writes `operation = 0x06` (climate | climateWithoutExternalSupply)
and a fixed maxCurrent regardless of the user's stored settings, and — carrying only 4 fields —
whether the unit preserves or resets the omitted fields (temperature, min charge, name) is **not
determinable from the ASG firmware**. The full-RA0 read-modify-write above avoids both problems. (In
the compact record, byte 2 is `maxCurrent`, not a temperature; an explicit temperature is a
RecordAddr-0 field at byte 12.)

### OperationMode trigger (Function 0x18) — detail

This is the frame OVMS actually sends to start/stop conditioning (short message):

```
29 58  00 01     START (immediate = profile 0)
29 58  00 00     STOP
```

`29 58` = SetGet, LSG 0x25, Function 0x18 (ClimateOperationModeInstallation). The only form this driver uses and has proven on-car is the **global immediate** one: byte 0 = `00` (profile 0), byte 1 = `01` starts / `00` stops. The non-global (timer) byte-1 model is unresolved — two RE sources disagree (OCU firmware reads it as a bitmap: bit0 = profile 0, bits 1–3 = timers; MIB2 firmware reads it as `{profileId, ctl}`) — so only the global form above is documented as fact.

## Charge Control

Charge control **shares the entire climate command path** — same handshake, same get-all
ProfilesArray read, same profile-0 read-modify-write, same OperationMode trigger. Profile 0 is one
shared "immediate" scratchpad and the trigger acts on whatever operation bits it holds. On the car
climate and charge **can run at the same time** (observed: charging the battery while pre-heating,
both drawing from the wall) — but each is driven by a separate arm+trigger, so OVMS **serialises the
command sequences** through the one controller (one arm+trigger in flight at a time) and each differs
only in what the arm writes.

**Scope: profile 0 (the global "Optionen" profile) only.** Every command here reads and writes
profile 0, which is the immediate/manual-charge profile. A **departure timer** charges under its own
per-timer charge profile (a different RA record), which this path never touches — so a `maxCurrent`
set through OVMS governs immediate charging but is **not** applied to a timer-scheduled charge unless
that timer's own profile is edited (in the car).

**Charge start / stop** — identical to climate ON/OFF, except the arm sets the operation to charge
instead of climate. STOP is op-specific: it acts on profile 0's current op, so OFF reads and arms the
pure-charge op before the stop trigger (a stop with profile 0 armed to climate would not stop
charging). Because that arm clears the climate bits, **charge-stop is blocked while climate is on**
(stop climate first):

```
… handshake + get-all GET + read profile 0 (as for climate) …
29 59 <3x-txn> 00 00 01 …   ARM: op = (op | charge) & ~(climate | onBattery)  (pure charge, 0x06 → 0x01)
29 58 00 01                 OperationMode START (after the arm echo)  /  29 58 00 00 = STOP (after the same arm)
```

Confirmed by the same `49 58 <flag>` echo. The result is *not* mapped to `v.e.hvac` (that is climate
only); charge state is read from the existing `0x594` broadcast.

> **Note.** Unlike climate, the *immediate* charge trigger has no factory reference on the e-Golf:
> the car's own e-Manager only schedules charging through departure timers (Functions 0x14–0x17) and
> never emits the Function-0x18 immediate trigger for charging. Start/stop is validated on a 2020
> e-Golf — charge starts, stops, and current changes apply as described. It is gated exactly like
> climate (no successful profile read + arm → no trigger), so a failed arm can never fall through to
> an unintended start.

**Set charge current** — edits the `maxCurrent` field of profile 0. Requested amps are snapped to the
allowed set **{5, 10, 13, 32} A** per phase (`clampMaxCurrent`; the car turns any out-of-set value
into 10 A, so the value is snapped first). Behaviour depends on whether a charge is running:

* **Not charging** — a pure settings edit: writes only `maxCurrent` (operation untouched), **no
  trigger**; confirmed by the FSG's SET echo `49 59 bx`. The car honours it on its next charge.
* **Charging (auto-apply)** — a running charge captured its current at start and *ignores* later
  profile edits, so the write alone wouldn't take effect until the next start. So OVMS also sets the
  op to charge, waits for the write echo, then **re-fires the start** (`29 58 00 01`) — the running
  charge re-reads the profile and picks up the new current. Confirmed on the `49 58` echo.

`maxCurrent` is always ≤ `0x20` (hard cap — a higher value bricks charging until a factory reset).

## Timer/Schedule Slot Encoding (Functions 0x14–0x17)

The codec models four timer slots — Functions `0x14`–`0x17` = timer slots 1–4.

8-byte FSG status record: `ff ff ff [hour] [temp_byte] fe [refId] 00`

- Byte 3: departure hour (direct decimal, e.g. `0x11` = 17:00)
- Byte 4: `celsius + 35` (e.g. 20 °C → `0x37`)
- Byte 6: refId — the profile position this timer runs (`0` = the global "Optionen" profile, `1…` = a charge location), not a slot index.

(On-car observation; not yet reconciled with smartkar's full-profile field layout.)

## KCAN Bus Wake-Up

The Battery Control unit rejects BAP from nodes not in the AUTOSAR/OSEK CAN-NM ring, and the
comfort/EV cluster must be awake. The current climate code wakes it with a CAN-NM message
**from a spare (unused) node id**, deliberately *not* impersonating the factory OCU. On this
car, OCU impersonation (the node-`0x67` NM frame and/or the OCU's `0x5A7` heartbeat) collided
with the live OCU and left OCU DTCs U001100/U120100 in fault memory. This climate path does
neither — spare-node wake, and it never asserts the `0x5A7` heartbeat.

### Spare-node NM wake (current implementation)

```
CAN ID:  0x1B00007D  (29-bit extended; 0x1B000000 | spare node 0x7D)
DLC:     8
Data:    7D 10 49 85 14 00 00 00
```

- Byte 0 = `0x7D` = spare source node id (verified unused in captures)
- Byte 1 = `0x10` = CBV active-wakeup (ring participation)
- Byte 2 = `0x49` = 0x40 charge PNC | 0x08 climate PNC | 0x01 comfort baseline
- Byte 3 = `0x85`, byte 4 = `0x14` = observed remote-service wake bits (Climatronic 0x46 PNC bits firmware/wire-confirmed)

`CommandClimateControl()` arms a wake bridge (`VWEGOLF_BATCTRL_WAKE_SECS`) and sends this
frame; Ticker1 re-sends it ~1 Hz (within the NM timeout) to hold the cluster up, and fires
the BAP handshake+trigger as soon as the Battery Control unit is heard on `0x17332510`. Once
it confirms, OVMS releases the bridge and the cluster sustains its own NM for the session.

**Warning:** do not use the BAP start frame as a wake ping — if the first frame TX_Fails, a
continuation arrives orphaned and the unit discards the partial long message.

(The body commands — mirror fold / lock / horn — use a separate OCU `0x5A7` heartbeat path;
climate is independent of it.)

### Wake Ping (bus already active)
```
can send can3 17332501 19 41
```
(`19 41` = the channel-open GetAll GET, opcode 1; `09` has an invalid opcode nibble.) Sufficient only when KCAN already live (e.g. just after ignition-off). Not for deep sleep.

## Test Commands (OVMS shell)

These use the **compact RecordAddr-6 arm** — a static approximation for manual bus testing. The
module itself sends the full RecordAddr-0 read-modify-write (which needs a live `19 59` GET first,
so it can't be replayed as fixed frames). The compact form still arms + triggers on the car.

Start:
```
can can3 tx extended 17332501 80 08 29 59 01 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 01
```

Stop:
```
can can3 tx extended 17332501 80 08 29 59 02 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 00
```

Bus effects: `0x3E9` transitions from sentinel to live values; `0x3B5`/`0x530` follow. Blower activates within ~10 s.

## FSG ACK Pattern (0x17332510)

Immediate response (~1 s after command):
```
80 0a 49 59  {tid} 04 46 00   [+ continuation]
```
`49 59` = OpCode 0x04 Status, LSG 0x25, Function 0x19 — the SetGet confirmation. `{tid}` = our array-header byte 0 with the FSG's ASG-ID nibble (observed as our value | 0x80), matching command to response via the Transaction-ID.

After ACK: FSG sends ~4 keepalive cycles on `17332501` at 5 s intervals (~16 s), then silent until next command.
