# Captures To Redo

Captures taken before the bus-bridging fix (firmware < 3.3.005-800/ota_1, April 2026)
were deleted because the OVMS logger tagged FCAN frames as bus-3 records in KCAN
captures and vice-versa, making ID-to-bus mapping unreliable.

The raw .crtd files are gone. Key findings from their .md notes are preserved below so
the experiments can be repeated with current firmware.

---

## Cap A — KCAN parked / driving census  *(not yet done with clean firmware)*

**Was:** `can3-3.3.005-778-g7404ab27_ota_1_edge-20260404-235013.crtd`
**Status:** BUG (buses [2,3] mixed)
**Sequence:** ignition on/off, car parked.
**Bad note:** The .md concluded "SoC real data arrives exclusively on KCAN" — this was
wrong. 0x131 appeared on bus 3 only because FCAN frames were leaking via the routing
bug. April-12 FCAN driving captures (Captures 17a/b/c) confirm 0x131 is genuine FCAN.

**Redo goal:**
- Clean KCAN capture: car parked + ignition on, then short drive.
- Confirm whether 0x131 (BMS_SoC) appears on KCAN (bus 3) during driving (J533 bridge).
- Also needed: clean FCAN driving capture with current filtered firmware to confirm
  0x131 reaches IncomingFrameCan2 at runtime (no pre-filter driving cap exists yet).

---

## Cap B — Clima start silent TX fail after wake ping  *(analysis preserved, redo optional)*

**Was:** `can3-3.3.005-792-gea41f218-dirty_ota_1_edge-20260405-004737.crtd`
**Status:** OK single-bus but pre-cleanup era; deleted for consistency.
**Sequence:** Car sleeping. Start clima from app. Clima never activated.

**Key findings (preserved):**
- Wake ping (`09 41`) woke the bus in <7 ms.
- CAN bus-error interrupt (`errflags=0x80001080 rxerr=9`) fired immediately after wake.
- BAP 3-frame START sequence was silently dropped during the error-recovery window.
  No TX_Fail logged — the CAN driver discards frames silently while recovering.
- Optimistic `ms_v_env_hvac = true` set before ECU ACK → app showed clima as ON and
  sent STOP on the retry. **Bug fixed:** metric now stays undefined until ECU ACK.
- `17332510` ECU ACK never appeared → ECU never received the command.

**Still open:** whether a post-wake retry delay resolves the silent drop. Later captures
(dff221ec0 era) show clima working from sleep, so the issue is either intermittent or
the retry logic in current firmware handles it.

---

## Cap C — 0x5A7 OCU collision with ignition recently off  *(analysis preserved, no redo needed)*

**Was:** `can3-3.3.005-792-gea41f218-dirty_ota_1_edge-20260405-005724.crtd`
**Status:** MISLABEL (labeled can3, data on bus 2)
**Sequence:** Ignition on → ignition off → press clima in app.

**Key findings (preserved):**
- 576× TX_Fail on 0x5A7 heartbeat — OEM OCU still active for ~10–15 s after ign-off,
  sending non-zero 0x5A7 which collides with our all-zeros frame.
- BAP frames had correct content (`29 58 00 01` = START) but were lost to bus-off.
- **Protocol constraint (not a code bug):** user must wait ~15 s after ignition off
  before issuing remote clima. Document in user guide.
- `17332510` frames arriving on bus 2 during bus-off were J533→FCAN bridge copies, not
  ECU ACKs.

---

## Cap D — Clima timer full cycle with temp measurement  *(redo for clean data)*

**Was:** `can3-3.3.005-797-g4afb8c2c-dirty_ota_0_edge-20260405-091701.crtd`
**Status:** BUG (buses [2,3] mixed), 840K frames / ~16 min
**Sequence:** Car sleeping. Send clima start. Log cabin temp until auto-shutoff.
**Temp readings:** dash panel (plastic, not sensor): 3°C start → 5°C @ 3 min → 6°C @ 5 min
→ 7°C @ 10 min. Clima auto-off at ~16 min.

**Redo goal:** Same sequence with current firmware. Needed for:
- Clean NM ring / BAP frame census during a full clima cycle.
- Cabin temp sensor (0x66E / 0x484?) decode with known reference temps.

---

## Cap E — Deep sleep clima start confirmed working  *(covered by later clean captures)*

**Was:** `can3-3.3.005-799-g8a74fdec-dirty_ota_1_edge-20260410-180648.crtd`
**Status:** OK single-bus but pre-cleanup era.
**Sequence:** `climatecontrol on` from OVMS shell. Car fully asleep.

**Key findings (preserved):**
- Clima started successfully: `BAP clima start sent: counter=0x05 temp=21°C`.
- `TWDT / Crash (abort, core 0)` immediately after capture ended — pre-dates FCAN filter fix.
- `v.e.hvac` remained `true` after `climatecontrol off` — stale metric bug (same root
  cause as Cap B; fixed by removing optimistic set).

Covered by: `can3-dff221ec0-dirty_ota_0_edge-20260422-*.crtd` (clean, same scenario).

---

## Cap F — Cap 8b AC charge KCAN continuation  *(covered by later clean captures)*

**Was:** `can3-3.3.005-798-g8242a608-dirty_ota_1_edge-20260409-221448.crtd`
**Status:** OK single-bus but pre-cleanup era.
**Sequence:** AC Type 2 charge continuation. No analysis performed.

Covered by: `can3-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260413-121648.crtd` (Cap 14)
and `can3-1aec82f33_ota_0_edge-20260428-095821.crtd`.

---

## Cap G — BAP on/off sequence  *(check if covered)*

**Was:** `kcan-can3-clima_on_off.crtd`
**Status:** BUG (buses [2,3] mixed)
**Content:** BAP clima on/off sequence. Referenced in DBC GPS frame comment.

**Check:** `kcan-can3-clima_control.crtd` (OK, buses=[3]) likely covers this.
Redo only if BAP on/off frame details are needed that aren't in the clean captures.

---

## Deleted empty / aborted sessions

| File | Sequence | Note |
|---|---|---|
| `can2-dff221ec0-...-20260427-173425.crtd` | Cap 8 FCAN attempt | 0 frames; metrics TSV kept |
| `can3-dff221ec0-...-20260427-200534.crtd` | Cap 8 KCAN attempt | 0 frames |
| `can3-1aec82f33-...-20260428-172606.crtd` | test capture | 0 frames |

---

## Priority redo list

| Priority | Cap | Scenario | Bus | Why |
|---|---|---|---|---|
| HIGH | A | KCAN driving, ign-on | can3 | Confirm SoC on KCAN during driving (J533 bridge) |
| HIGH | A | FCAN driving, current fw | can2 | Confirm 0x131 reaches IncomingFrameCan2 with filter active |
| MED | D | Clima full cycle | can3 | Clean census + cabin temp decode reference |
| LOW | G | BAP on/off | can3 | Only if kcan-can3-clima_control.crtd is insufficient |
