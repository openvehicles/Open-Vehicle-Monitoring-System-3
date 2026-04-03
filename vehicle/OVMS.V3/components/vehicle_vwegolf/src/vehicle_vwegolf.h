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

class OvmsVehicleVWeGolf : public OvmsVehicle {
 public:
    OvmsVehicleVWeGolf();
    ~OvmsVehicleVWeGolf();

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
    void WakeKcanBus();

 protected:
    void Ticker1(uint32_t ticker) override;
    void Ticker10(uint32_t ticker) override;

 private:
    // Seconds since the last genuine KCAN (can3) frame arrived. Reset to 0 only for
    // frames with origin==m_can3; FCAN frames forwarded via IncomingFrameCan2 are excluded.
    // Incremented each second in Ticker1. Bus is alive while < VWEGOLF_BUS_TIMEOUT_SECS.
    // Initialized to timeout so we treat the bus as offline at cold boot.
    uint8_t m_bus_idle_ticks = VWEGOLF_BUS_TIMEOUT_SECS;

    // Seconds since the last non-zero 0x5A7 frame was received from the OEM OCU. When
    // the car's ignition is on or was just turned off, the OEM OCU sends non-zero 0x5A7
    // which conflicts with our all-zeros heartbeat (arbitration loss → bus-off). We must
    // not send the full NM wake sequence while the OEM OCU is still active.
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

    // Target temperature and battery-allow flag for clima; refreshed from config in Ticker10.
    uint8_t m_climate_temp = 21;
    bool m_climate_on_battery = false;

    // Rolling BAP counter included in each command frame. The ECU echoes it (| 0x80) in
    // its ACK so we can match responses to commands. Must never be zero; wraps 0xFF → 0x01.
    uint8_t m_bap_counter = 0;

    bool m_mirror_fold_in_requested = false;
    bool m_horn_requested = false;
    bool m_indicators_requested = false;
    bool m_panic_mode_requested = false;
    bool m_unlock_requested = false;
    bool m_lock_requested = false;
    uint8_t m_vin_parts_received = 0;
    char m_vin_buf[18] = {};
    uint8_t m_bap_counter = 0;
};

#endif  // #ifndef __VEHICLE_VWEG_H__
