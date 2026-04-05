# can3-3.3.005-792-gea41f218-dirty_ota_1_edge-20260405-004737.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-792-gea41f218-dirty/ota_1/edge` |
| Bus | can3 |
| Captured | 20260405-004737 |
| Duration | ~101s |
| Frames | 39323 |

## Sequence

Car off and sleeping (no frames until wake ping). Toggle clima in app (start), wait ~30s,
toggle clima in app again (intended stop). No clima activation was observed.

---

## Notes

### What happened

Two calls to `CommandClimateControl` are visible from the BAP counters in the capture:

**First call (bap_counter=0x01, enable=true):**
- Wake ping `09 41` sent at t=3.0s → bus woke immediately (flood of RX frames within 7ms)
- CAN bus error interrupt at t=3.0s: `errflags=0x80001080 rxerr=9` — triggered by the
  wake ping against a waking bus
- Expected BAP 3-frame start sequence (counter=0x01) is **not present in the capture**.
  No `3T29 17332501 80 08 29 59 01 ...` exists. No TX_Fail either.
- Best hypothesis: the BAP frames were silently dropped by the CAN driver during its
  error recovery window. The OVMS CAN driver appears to not log frames that are rejected
  during bus-error recovery.
- `StandardMetrics.ms_v_env_hvac->SetValue(true)` ran (optimistic set). This was the bug.

**Between first and second call:**
- OCU heartbeats (0x5A7 all-zeros) transmit successfully at ~5Hz from t=3.3s onward.
- `1B000010` NM ring frames appear at 200ms intervals — J533 is awake and running OSEK NM.
- No `17332510` ECU ACK at any point — the clima ECU never received a valid command.

**Second call (bap_counter=0x02, enable=false):**
- At t=87s, user pressed the toggle again. Because `ms_v_env_hvac` was set to `true`
  by the first call, the app saw clima as ON and sent `CommandClimateControl(false)`.
- BAP 3-frame STOP sequence transmitted successfully: `80 08 29 59 02 06 00 01` /
  `c0 06 00 20 00` / `29 58 00 00`.
- Frame 3 payload: `00` = stop. Correct for a stop command, but clima was never started.
- No `17332510` ACK — the ECU ignores a stop command when clima is not running.

### Bugs confirmed

**Optimistic ms_v_env_hvac set:** Setting `ms_v_env_hvac` in `CommandClimateControl`
before ECU confirmation causes the app toggle to invert on a retry. If the TX fails
silently, the metric is wrong, and the next press sends the opposite command.
Fixed: removed the optimistic set. Metric now stays undefined until ECU ACK is decoded.

**Silent TX failure after wake ping:** The first BAP command is silently lost during
the CAN bus error recovery window that follows the wake ping. The error interrupt
(`errflags=0x80001080`) is the trigger. This is not yet fixed — we need to understand
whether a retry or longer delay resolves it.

### What we have NOT yet captured

A successful BAP START command from this firmware. Every attempt has either:
- TX_Failed completely (bus-off, ignition scenario)
- Been silently dropped (wake ping error recovery window, this capture)
- Been a STOP command (due to the optimistic metric bug)

### Next test

1. Car off and fully sleeping (wait 15+ seconds after ignition off)
2. Press START in app
3. Verify Frame 3 = `29 58 00 01` (start flag) in capture
4. Look for `17332510` with echoed counter (0x01 | 0x80 = 0x81) confirming ECU received it
5. Look for `39 58 00` status frames at 300ms intervals (clima running)
