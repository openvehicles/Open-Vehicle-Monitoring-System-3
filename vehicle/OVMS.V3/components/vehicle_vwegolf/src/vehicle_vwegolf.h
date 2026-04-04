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

 protected:
    void Ticker1(uint32_t ticker) override;
    void Ticker10(uint32_t ticker) override;

 private:
    // Seconds since the last KCAN frame arrived. Reset to 0 in IncomingFrameCan3,
    // incremented each second in Ticker1. Bus is alive while this is < VWEGOLF_BUS_TIMEOUT_SECS.
    // Initialized to timeout so we treat the bus as offline at cold boot.
    uint8_t m_bus_idle_ticks = VWEGOLF_BUS_TIMEOUT_SECS;

    // OVMS must send the 0x5A7 OCU keepalive every second while it is an active node.
    // We only start sending it after deliberately taking an action (wakeup or command)
    // to avoid asserting an unexpected node presence when the car is idle.
    bool m_ocu_active = false;

    // Target temperature and battery-allow flag for clima; refreshed from config in Ticker10.
    uint8_t m_climate_temp = 21;
    bool m_climate_on_battery = false;

    // Rolling BAP counter included in each command frame. The ECU echoes it (| 0x80) in
    // its ACK so we can match responses to commands. Must never be zero; wraps 0xFF → 0x01.
    uint8_t m_bap_counter = 0;

    // Pending one-shot control actions. Set by the Command* methods, consumed and cleared
    // in the next SendOcuHeartbeat() call.
    bool m_mirror_fold_requested = false;
    bool m_horn_requested = false;
    bool m_indicators_requested = false;
    bool m_panic_requested = false;
    bool m_unlock_requested = false;
    bool m_lock_requested = false;

    // VIN assembly state. Frame 0x6B4 carries the 17-char VIN split across three frames
    // identified by data[0]. We collect all three before committing to the metric.
    uint8_t m_vin_parts_received = 0;
    char m_vin_buf[18] = {};

    void SendOcuHeartbeat();
    void SendBapWakePing();
};

#endif  // __VEHICLE_VWEG_H__
