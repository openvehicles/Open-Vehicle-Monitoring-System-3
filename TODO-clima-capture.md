# Clima BAP Capture TODO

Goal: identify which BAP port/byte encodes each clima function by diffing
captures across distinct state changes.

---

## Capture 1 — `clima-sequence.crtd` (2026-04-02, completed)

**Conditions:** car charging, ignition key in radio/accessory slot (no full ignition).
**Duration:** 154.9s, 153,762 frames, 182 unique 11-bit CAN IDs, 17,755 29-bit BAP frames.

**Sequence performed:**
1. Clima OFF, car awake — baseline
2. Clima ON from dash (setpoint 20°C)
3. Temp knob DOWN to 18°C
4. Temp knob UP to 24°C
5. Clima OFF from dash
6. Clima ON again
7. Clima OFF

---

## Findings from Capture 1

### Clima ON/OFF state — CONFIRMED

The running state is visible in three standard 11-bit KCAN frames. All three flip
cleanly on every ON/OFF transition:

| CAN ID | Byte | OFF value | ON value | Note |
|--------|------|-----------|----------|------|
| `0x66E` | byte[2] | `0x38` | `0x3a` | Single bit (bit 1) — cleanest indicator |
| `0x5F5` | byte[1] | `0x01` | `0x00` | Also carries temperature-related data |
| `0x3B5` | byte[0] | `0x00` | `0x80` | Bit 7 |
| `0x5EC` | byte[5] | `0x04` | `0x07` | Secondary indicator |

**Recommended for OVMS decode:** use `0x66E` byte[2] as the primary on/off flag.

`0x5F5` full payload for reference:

| State | Payload |
|-------|---------|
| OFF | `08 01 34 c0 20 6d c8 20` |
| ON 20°C | `f6 00 34 80 1e 75 88 1e` |
| ON 18°C | `f9 00 34 c0 1e 73 c8 1e` |
| ON 24°C | `f2 00 34 e0 1d 77 e8 1d` |

Notable structure in `0x5F5`:
- byte[2] = `0x34` is constant (message type ID)
- byte[3] and byte[6] always differ by exactly 8 — paired field
- byte[4] and byte[7] always identical — likely sensor readback (not setpoint)

### Temperature setpoint — NOT CONFIRMED

The 20→18→24°C range was too narrow to establish a clear encoding formula:

- `0x5F5` bytes[4,7]: only changed by 1 unit over the entire 6°C range — likely a
  slow-moving sensor readback (cabin/vent temp), **not** the setpoint.
- `0x5F7` bytes[1,2] change with each temp adjustment but with inconsistent scaling
  (4 half-steps down produced +264 as 16-bit; 12 half-steps up produced -1052).

No confirmed temperature field yet.

### BAP cyclic broadcasts (29-bit) — insight

`0x17332510` (node 0x25, the clima ECU) broadcasts a repeating 13-port cycle every
~13 seconds throughout. The port 0x16 payload was completely static:

```
80 08 39 56 ff ff ff 15  (start, 8 bytes)
c0 37 fe 02 00           (continuation)
```

Full port 0x16 payload: `ff ff ff 15 37 fe 02 00`

**Interpretation:** port 0x16 on node 0x25 reflects the **configured profile**
(0x15=21°C setpoint, 0x37=55 min duration), not the actively running state.
The running state lives in the standard 11-bit frames above.

---

## Capture 2 — `kcan-can3-clima_range.crtd` (2026-04-02, completed)

**Conditions:** ignition on (radio slot), KCAN alive.

**Sequence performed:**
1. Ignition on, clima panel off — baseline
2. Clima ON, setpoint 20°C
3. Temp knob all the way DOWN to **LO** (16°C minimum)
4. Temp knob all the way UP to **HI** (29.5°C maximum)
5. Clima OFF

---

## Findings from Capture 2

### Temperature setpoint encoding — CONFIRMED

**CAN ID `0x17330110`**, BAP node 0x01 (J533 gateway), port **0x1B** (driver zone)
and port **0x21** (passenger zone / zone 2, mirrors driver on this car).

These are opcode-4 Ack frames — the gateway acknowledges each half-degree step
as the user turns the dial. They appear once every ~50 ms while the dial is held.

| Setpoint | Raw value (LE uint16) | hex byte[0] |
|----------|-----------------------|-------------|
| LO (16.0°C) | 60 | `3C` |
| 16.5°C | 65 | `41` |
| 17.0°C | 70 | `46` |
| 18.0°C | 80 | `50` |
| 19.0°C | 90 | `5A` |
| **20.0°C** | **100** | **`64`** |
| 22.0°C | 120 | `78` |
| 25.0°C | 150 | `96` |
| 28.5°C | 185 | `B9` |
| 29.0°C | 190 | `BE` |
| HI (≥29.5°C) | 255 | `FF` |
| Off / below-LO | 0 | `00` |

**Encoding formula:**

```
raw_value = (setpoint_C - 10.0) * 10        # tenths-of-°C above 10°C
setpoint_C = raw_value / 10.0 + 10.0
```

Range observed: `3C 00` (60 = 16.0°C) → `BE 00` (190 = 29.0°C) → `FF 00` (HI sentinel).
`00 00` appears one step below LO and is treated as "LO" by the display.

Full sweep confirmed step-by-step at 0.5°C increments: **no gaps, clean linear encoding**.

### Clima ON/OFF state — CONFIRMED (same as Capture 1)

`0x66E` byte[2]: `0x38` = OFF, `0x3A` = ON.

The clima turned ON at t+55s and OFF at t+76s. The full temperature sweep (LO→HI)
happened within that 21-second window and is fully captured in `17330110` port 0x1B.

### Mode flags — node 0x01 ports 0x1C and 0x1D

These two ports track the same value and appear to encode operational mode / dial state:

| Value | Condition |
|-------|-----------|
| `00` | Clima off |
| `01` | Clima on, normal (including while turning dial) |
| `02` | Running, moderate demand |
| `03` | Running, higher demand |
| `04` | Running, high demand |
| `07` | At or below LO setpoint |

### Observation: no BAP Set/SetGet commands visible

No opcode-1 or opcode-6 BAP commands with non-empty payloads were found on KCAN
during the temperature dial events. The `17330110` Ack frames appear as **cyclic
status updates from the J533 gateway** reflecting the current setpoint — the actual
dial-to-gateway commands happen on a different bus (instrument CAN) not captured here.

For OVMS remote control, the command path is directly to node 0x25 via `17332501`
(not through node 0x01). See Capture 3.

---

## Capture 3 — `kcan-can3-clima_schedule.crtd` (2026-04-02, completed)

**Conditions:** ignition on (radio slot), KCAN alive.

**Sequence performed:**
1. Opened e-Manager in infotainment
2. Set departure timer: **17:00**, clima pre-heat start **16:50**
3. Temperature confirmed as 20°C (displayed), confirmed/saved

---

## Findings from Capture 3

### Stored clima profile — node 0x25 port 0x16 format confirmed

`17332510` opcode-4 Ack, port 0x16, payload structure:

```
FF FF FF [temp_C] [duration_min] FE 02 00
```

- `byte[3]` = setpoint temperature in **°C, integer** (e.g. `0x14` = 20°C, `0x15` = 21°C)
- `byte[4]` = pre-conditioning duration in **minutes** (e.g. `0x0A` = 10 min, `0x37` = 55 min)
- `FF FF FF` prefix and `FE 02 00` suffix are constant framing

Observed changes:
- Initial: `FF FF FF 11 37 FE 02 00` → 17°C, 55 min
- After schedule set: `FF FF FF 11 00 FE 02 00` → 17°C, 0 min (duration cleared)

**Note:** the temperature stored here reflects the profile last written, which may
differ from what the e-Manager UI displays if the user adjusts it via the dial while
clima is running. This port is the STORED profile, not the active running setpoint.

### Schedule / departure timer — node 0x25 port 0x12

`17332510` opcode-4 Ack, port 0x12 payload changes when the departure timer is modified:

```
[flags] [00] [00] [0A] [00] [time_lo] [time_hi]
```

Observed values:
- `00 00 00 0A 00 2B 00`
- `00 00 00 0A 00 BB 00`  ← schedule active
- `05 00 00 0A 00 CB 00`
- `09 00 00 0A 00 FB 00`

The `[time_lo time_hi]` bytes `2B 00` / `BB 00` etc. encode the departure time.
Exact mapping needs one more targeted capture (known time → known raw bytes).

Port 0x13 value `07 04` (vs normal `07 00`) appears when a schedule is active.

### Command channel to clima ECU

`17332501` was the only 29-bit CAN ID observed sending commands to node 0x25.
All observed commands were `SetGet` with **empty payload** (= poll/refresh requests):
- port 0x01: request full status dump
- port 0x02: request ECU info

The actual WRITE commands to SET temperature, duration, and ON/OFF state are
**not yet observed in passive capture**. They likely originate from the infotainment
unit on a bus segment not visible to the OVMS (or via the 0x17330301 TP 2.0
channel setup path already used for wakeup). See TODO for Capture 4.

### e-Manager node (0x23) interaction

`17332300` carries `Set node=0x23 port=0x12 [00 00]` at each schedule change
event (t+35s, t+117s, t+182s). This may be the e-Manager acknowledging a
schedule update or triggering a schedule sync.

---

## Capture 4 — `kcan-can3-clima_on_off.crtd` (2026-04-03, completed)

**Conditions:** car parked, OVMS connected via J533 harness. Commands sent from OVMS
shell and iOS app (OVMS Connect via TurboServ).

**Sequence performed:**
1. `climatecontrol on` from OVMS shell
2. Wait ~60s, then `climatecontrol off` from OVMS shell
3. iOS toggle: AC OFF → ON
4. Wait ~10s, iOS toggle: AC ON → OFF
5. iOS toggle: rapid repeated presses (AC turns on, then spammed while running)

All commands confirmed working — clima started and stopped as expected each time.

---

## Findings from Capture 4

### CommandClimateControl confirmed working

The 3-frame BAP sequence to node 0x25 successfully starts and stops climatisation:

```
Frame 1: 80 08 29 59  [ctr]  06 00 01    (port 0x19 parameters, multi-frame start)
Frame 2: C0  06 00 20 00                 (continuation)
Frame 3: 29 58  00 [01|00]               (port 0x18 trigger: 01=start, 00=stop)
```

### ECU acknowledges each command on 0x17332510 (within ~1s)

```
80 0a 49 59  {ctr | 0x80}  04 46 00  [+ continuation]
```

Counter echoed with bit 7 set — use to match ACK to command.

### ECU sends 5s status bursts on 0x17332501 (~16s after command)

~4 cycles, then silent until next command:

```
19 42  /  19 41                           (keepalive polls, ports 0x02 and 0x01)
90 04 19 59 {ctr} 00 00 04               (ch1, port 0x19, 4-byte status)
80 04 19 5a {ctr} 02 00 04               (ch0, port 0x1a, 4-byte status)
```

### ECU full state dump on 0x17332510 (port 0x1a)

Port 0x1a length and header byte differ by context:
- 34 bytes (`90 22 ... c0`): idle state
- 25 bytes (`90 19 ... c2`): clima running (bit 1 of header byte set)

Not yet confirmed — needs a controlled before/after capture.

See `docs/clima-control-bap.md` for full detail.

---

## Remaining TODO Captures

### Capture 5 — Cabin temperature in remote mode (TODO)

**Goal:** identify which CAN ID carries cabin temperature during remote clima operation
(car parked, HVAC running via OVMS command, no ignition).

In ignition-on mode cabin temp comes from `0x17330110` node 0x01 port 0x1B (confirmed,
Capture 2). But that is the J533 gateway echoing the dash. When parked remotely, the
source may be different — or it may still be the same frame.

**Sequence to capture:**
1. Start `climatecontrol on` from OVMS
2. Once clima is running (blower audible), capture KCAN for 2-3 minutes
3. Note actual cabin temp from a thermometer for ground truth

**What to look for:**
- Any frame with a value that warms from ambient toward setpoint over 5-10 min
- Whether `0x17330110` port 0x1B is present and tracking temp when ignition is off

### Capture 6 — Natural timer expiry (TODO)

**Goal:** see what the ECU sends when the 10-minute clima timer expires on its own.

**Sequence:** start clima, then wait for it to stop automatically.

**What to look for:**
- Does the port 0x1a header byte revert from `c2` to `c0`?
- Does the ECU send a final stop notification on either CAN ID?
- Do the status payloads (`00 00 04`, `02 00 04`) change?

### Capture 7 — Port 0x19 temperature encoding (TODO)

**Goal:** confirm how the target temperature is encoded in byte 6 of the port 0x19
payload (currently hardcoded to `0x20` from thomasakarlsen default).

**Method:** issue `climatecontrol on` with different `cc_temp` config values (18°C,
22°C, 25°C) and check whether anything downstream changes — `0x17330110` setpoint,
`0x5F5`, or ECU state dump. If the ECU echoes it back, that confirms the encoding.

Current candidate: same formula as port 0x16 schedule (`byte = celsius + 35`),
giving 20°C → `0x37`. The hardcoded `0x20` decodes as 32°C in that formula.

---

## CAN IDs confirmed on this car

| Direction | CAN ID | Notes |
|-----------|--------|-------|
| ECU → bus (11-bit) | `0x66E` | Clima running state in byte[2] |
| ECU → bus (11-bit) | `0x5F5` | Clima state + temperature data |
| ECU → bus (11-bit) | `0x3B5` | Clima running flag in byte[0] |
| ECU → bus (11-bit) | `0x5EC` | Clima running flag in byte[5] |
| J533 gateway → bus (29-bit) | `0x17330110` | BAP node 0x01 — real-time setpoint broadcast |
| Clima ECU → bus (29-bit) | `0x17332510` | BAP node 0x25 cyclic status |
| OVMS/GW → clima ECU (29-bit) | `0x17332501` | BAP command channel to node 0x25 |
| OVMS → clima ECU | `0x17330301` | Wake / BAP init |
| Bus wake broadcast | `0x1B000067` | Bus keep-alive |

## Known BAP values (node 0x25, from cyclic broadcast)

- Node: **0x25** (climate ECU LSG address)
- Opcode for status push: **3** (MQB uses 3, not 2 as in PQ docs); Ack: **4**
- Port **0x02**: BAP config response (constant, ignore)
- Port **0x16**: stored clima profile — format `FF FF FF [temp_C] [duration_min] FE 02 00`
  - byte[3] = temperature in °C (integer, e.g. `0x14`=20°C, `0x15`=21°C)
  - byte[4] = duration in minutes (e.g. `0x0A`=10 min)
  - This does NOT change when the user toggles clima on/off from the dash
