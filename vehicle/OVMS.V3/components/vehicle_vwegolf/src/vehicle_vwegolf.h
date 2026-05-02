/*
;    Project:       Open Vehicle Monitor System
;	 Subproject:    Integrate VW e-Golf
;
;    (C) 2026  Erick Fuentes <fuentes.erick@gmail.com>
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#ifndef __VEHICLE_VWEG_H__
#define __VEHICLE_VWEG_H__

#include "can.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_log.h"
#include "ovms_metrics.h"
#include "vehicle.h"

// Forward declarations — full definitions are in ovms_webserver.h, included only
// in vwegolf_web.cpp to avoid pulling webserver transitive deps into this header.
struct PageEntry;
struct PageContext;
typedef struct PageEntry PageEntry_t;
typedef struct PageContext PageContext_t;

// Poll states — index matches the timing array in poll_pid_t entries.
#define VWEGOLF_OFF 0       // All systems sleeping
#define VWEGOLF_AWAKE 1     // Base systems online
#define VWEGOLF_CHARGING 2  // Base systems online and charging
#define VWEGOLF_ON 3        // All systems online, drivable

// KCAN goes silent within a few seconds of the car sleeping. After this many
// consecutive Ticker1 ticks with no incoming frames, the bus is treated as offline
// and we suppress OCU keepalive transmission to avoid accumulating TX errors.
#define VWEGOLF_BUS_TIMEOUT_SECS 10

// Shorter threshold for the clima wake-before-send decision. KCAN NM runs at 200ms
// intervals; 3 seconds of silence means the bus is going to sleep or already asleep,
// even if m_bus_idle_ticks hasn't reached BUS_TIMEOUT yet. Used together with the
// OEM OCU idle counter to avoid waking during the 0x5A7 conflict window.
#define VWEGOLF_CLIMA_WAKE_SECS 3

// Settle delay between WakeKcanBus and the BAP burst. The NM-join flood needs time to
// subside before the clima ECU will accept the multi-frame command. Applied from
// Ticker1 (non-blocking) when CommandClimateControl had to wake the bus.
#define VWEGOLF_CLIMA_SETTLE_MS 1000

// Session length limits after WakeKcanBus. OCU keepalive + NM alive stop when the
// ring quiesces post-ACK (grace) or if no ACK arrives (hard cap). Without these,
// we talk alone on a sleeping ring, every TX fails ACK, TEC storms for minutes.
#define VWEGOLF_OCU_ACK_GRACE_SECS 5
#define VWEGOLF_OCU_SESSION_CAP_SECS 30

class OvmsVehicleVWeGolf : public OvmsVehicle {
 public:
    OvmsVehicleVWeGolf();
    ~OvmsVehicleVWeGolf();

    void WebInit();
    void WebDeInit();
    static void WebCfgSettings(PageEntry_t& p, PageContext_t& c);
    static void WebDispMetrics(PageEntry_t& p, PageContext_t& c);

    void IncomingFrameCan2(CAN_frame_t* p_frame) override;
    void IncomingFrameCan3(CAN_frame_t* p_frame) override;

    vehicle_command_t CommandHorn();
    vehicle_command_t CommandPanic();
    vehicle_command_t CommandIndicators();
    vehicle_command_t CommandMirrorFoldIn();
    vehicle_command_t CommandLock(const char* pin) override;
    vehicle_command_t CommandUnlock(const char* pin) override;
    vehicle_command_t CommandWakeup() override;
    vehicle_command_t CommandClimateControl(bool enable) override;
    void SendOcuHeartbeat();
    void SendNmAlive();
    void WakeKcanBus();
    vehicle_command_t SendClimaBapBurst(bool enable);

 protected:
    void Ticker1(uint32_t ticker) override;

 private:
    // Seconds since the last genuine KCAN (can3) frame arrived. Reset to 0 only for
    // frames with origin==m_can3; FCAN frames forwarded via IncomingFrameCan2 are excluded.
    // Incremented each second in Ticker1. Bus is alive while < VWEGOLF_BUS_TIMEOUT_SECS.
    // Initialized to timeout so we treat the bus as offline at cold boot.
    uint8_t m_bus_idle_ticks = VWEGOLF_BUS_TIMEOUT_SECS;

    // Seconds since the last non-zero 0x5A7 from the OEM OCU. While the car is on or
    // just turned off, the OEM OCU sends non-zero 0x5A7 frames that conflict with our
    // all-zeros heartbeat (arbitration loss → bus-off). Must not wake while this is < CLIMA_WAKE_SECS.
    // Initialized to timeout so cold boot treats the OEM OCU as absent.
    uint8_t m_oem_ocu_idle_ticks = VWEGOLF_BUS_TIMEOUT_SECS;

    // OVMS must send the 0x5A7 OCU keepalive while it is an active node.
    // VW OSEK NM requires keepalives at ~200ms intervals — Ticker1 alone (1Hz) is
    // insufficient. We enforce a 180ms minimum interval via a FreeRTOS tick timestamp
    // checked on every incoming KCAN frame, giving ~5Hz without storm risk when the
    // bus gets a sudden traffic burst (e.g. from an NM wake).
    // We only start sending after deliberately taking an action (wakeup or command)
    // to avoid asserting an unexpected node presence when the car is idle.
    bool m_ocu_active = false;
    uint32_t m_last_heartbeat_tick = 0;

    // Session bounds (seconds, incremented in Ticker1 while m_ocu_active).
    // Without a bounded session, NM alive + heartbeat keep firing after the ring
    // quiesces; every TX fails ACK, TEC storms for minutes. See cap 20260422-173035.
    // Session cap ends it if no ACK arrives; grace ends it shortly after an ACK.
    uint8_t m_ocu_session_secs = 0;
    uint8_t m_ocu_grace_secs = 255;  // 255 = no ACK seen yet

    // Rolling BAP counter included in each command frame. The ECU echoes it (| 0x80) in
    // its ACK so we can match responses to commands. Must never be zero; wraps 0xFF → 0x01.
    uint8_t m_bap_counter = 0;

    // Set while a multi-frame BAP command burst is in flight (CommandClimateControl).
    // SendOcuHeartbeat skips if set — a 0x5A7 queued between BAP frames blocks the
    // continuation and the ECU discards the message. The 180 ms throttle isn't enough
    // when the 3×WriteExtended calls can take up to 600 ms worst-case.
    bool m_bap_burst_active = false;

    // Deferred clima burst. When CommandClimateControl has to wake the bus, it kicks
    // WakeKcanBus, records the tick, and returns Success immediately so the command
    // dispatch task isn't blocked for the 1 s NM-join settle. Ticker1 fires the 3-frame
    // BAP burst once VWEGOLF_CLIMA_SETTLE_MS has elapsed.
    bool m_clima_pending = false;
    bool m_clima_pending_enable = false;
    uint32_t m_clima_pending_tick = 0;

    bool m_mirror_fold_in_requested = false;
    bool m_horn_requested = false;
    bool m_indicators_requested = false;
    bool m_panic_requested = false;
    bool m_unlock_requested = false;
    bool m_lock_requested = false;

    // VIN assembly state. Frame 0x6B4 carries the 17-char VIN split across three frames
    // identified by data[0]. We collect all three before committing to the metric.
    uint8_t m_vin_parts_received = 0;
    char m_vin_buf[18] = {};

#ifdef VWEGOLF_NATIVE_TEST
 public:
    uint8_t test_bus_idle_ticks() const { return m_bus_idle_ticks; }
#endif
};

#endif  // __VEHICLE_VWEG_H__
