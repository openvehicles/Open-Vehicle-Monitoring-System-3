# Clima Start — Test 2 Field Reference

Car parked, ignition off, OVMS powered, laptop on OVMS hotspot (192.168.4.1).

---

## Step 1 — Start capture BEFORE sending commands

On the laptop:
```
nc 192.168.4.1 3000 > kcan-can3-clima_start2.crtd
```

On the OVMS shell:
```
can log start tcpserver transmit crtd 3000 can3
```

---

## Step 2 — Send clima START (3 frames, in order)

```
can can3 tx extended 17332501 80 08 29 59 01 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 01
```

Wait ~10 seconds. Listen for blower.

---

## Step 3 — Check for ECU response

On OVMS shell, look for Ack on `17332510`:
- Port 0x19 Ack: `80 82 49 59 [counter] 04`
- Port 0x18 Ack: not yet observed — first time to see this

Watch these frame IDs for transitions:
```
# Active clima signature (from clima-sequence.crtd):
# 0x3E9: 00 03 00 00 00 00 00 00   (was: fe f8 df ff when idle)
# 0x3B5: 00 fe 12 00 0c 00 20 00
# 0x530: 00 00 00 00 00 00 00 00
```

---

## Step 4 — Send clima STOP

```
can can3 tx extended 17332501 80 08 29 59 02 06 00 01
can can3 tx extended 17332501 C0 06 00 20 00
can can3 tx extended 17332501 29 58 00 00
```

---

## Step 5 — Stop capture

On OVMS shell:
```
can log stop
```

Ctrl-C the `nc` on the laptop.

---

## What to look for in the capture

| Thing to find | How to find it |
|---|---|
| Our TX frames | `3T29 17332501` — should see all 3 start frames |
| Port 0x19 Ack | `3R29 17332510 80 82 49 59 ...` |
| Port 0x18 Ack | `3R29 17332510` with BAP node=0x25 port=0x18 |
| HVAC live state | `3R29 000003E9 00 03 ...` transition |
| Temperature byte | In any `17332510` port 0x19 Status — byte 6 is the target temp |

---

## Outstanding unknowns to observe

- **Port 0x18 Ack**: never seen yet — payload format unknown
- **Temperature encoding** in port 0x19 payload (byte 6 = `0x20`):
  - Is it direct Celsius (`0x20` = 32°C)?
  - Or offset like schedule: `byte = celsius + 35` → `0x20` = -3°C (invalid)
  - If the car starts at a sane temp, `0x20` is probably not +35 offset
  - To confirm: note what temp the car actually heats to

- **Duration** (byte 1 = `0x06`): minutes? 5-min intervals? (6 × 5 = 30 min, 6 min are both plausible)

---

## If nothing happens

- Check TX errors on bus: `can status can3`
- Verify the 3 frames went out: look for `3T29 17332501` lines in capture
- ECU may need ignition off for >30 s to be in remote mode (port 0x18 only works when car is fully asleep)
