/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris Staite
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

#ifndef __VEHICLE_MGEV_H__
#define __VEHICLE_MGEV_H__

#include "vehicle.h"
#include "ovms_metrics.h"
#include "mg_poll_states.h"

#include "freertos/timers.h"

#include <vector>

class OvmsCommand;

class OvmsVehicleMgEv : public OvmsVehicle
{
  public:
    OvmsVehicleMgEv();
    ~OvmsVehicleMgEv() override;

    const char* VehicleShortName() override;

    bool SetFeature(int key, const char* value) override;
    const std::string GetFeature(int key) override;

    OvmsMetricFloat* m_bat_pack_voltage;
    OvmsMetricFloat* m_env_face_outlet_temp;
    OvmsMetricFloat* m_dcdc_load;

  protected:
    void ConfigChanged(OvmsConfigParam* param) override;

    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) override;

    void Ticker1(uint32_t ticker) override;

    void IncomingFrameCan1(CAN_frame_t* p_frame) override;
    void IncomingFrameCan2(CAN_frame_t* p_frame) override;
    void IncomingFrameCan3(CAN_frame_t* p_frame) override;
    void IncomingFrameCan4(CAN_frame_t* p_frame) override;

    vehicle_command_t CommandWakeup() override;

  private:
    canbus* IdToBus(int id);
    void ConfigurePollInterface(int bus);

    static void ZombieTimer(TimerHandle_t timer);
    void ZombieTimer();

    static void SoftwareVersions(
            int, OvmsWriter* writer, OvmsCommand*, int, const char* const*);
    void SoftwareVersions(OvmsWriter* writer);

    static void WakeUp(void* self);

    void IncomingPollFrame(CAN_frame_t* frame);
    bool SendPollMessage(canbus* bus, uint16_t id, uint8_t type, uint16_t pid);

    bool HasWoken(canbus* currentBus, uint32_t ticker);
    void DeterminePollState(canbus* currentBus, bool wokenUp, uint32_t ticker);
    void SendAlarmSensitive(canbus* currentBus);
    bool SendKeepAliveTo(canbus* currentBus, uint16_t id);

    void AttemptDiagnostic();

    void NotifyVehicleIdling() override;

    // mg_poll_bms.cpp
    void IncomingBmsPoll(uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain);
    void SetBmsStatus(uint8_t status);
    void ProcessBatteryStats(int index, uint8_t* data, uint16_t remain);
    /// A cache of the last byte in the first message of the BMS cell voltage message
    uint8_t m_bmsCache;

    // mg_poll_dcdc.cpp
    void IncomingDcdcPoll(uint16_t pid, uint8_t* data, uint8_t length);

    // mg_poll_vcu.cpp
    void IncomingVcuPoll(uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain);
    void HandleVinMessage(uint8_t* data, uint8_t length, uint16_t remain);

    // mg_poll_atc.cpp
    void IncomingAtcPoll(uint16_t pid, uint8_t* data, uint8_t length);

    // mg_poll_bcm.cpp
    void IncomingBcmPoll(uint16_t pid, uint8_t* data, uint8_t length);

    // mg_poll_tpms.cpp
    void IncomingTpmsPoll(uint16_t pid, uint8_t* data, uint8_t length);

    // mg_poll_ipk.cpp
    void IncomingIpkPoll(uint16_t pid, uint8_t* data, uint8_t length);

    // mg_poll_peps.cpp
    void IncomingPepsPoll(uint16_t pid, uint8_t* data, uint8_t length);

    // mg_poll_evcc.cpp
    void IncomingEvccPoll(uint16_t pid, uint8_t* data, uint8_t length);

    /// The states that the gateway CAN can be in
    enum WakeState
    {
        Off,     // The CAN is off and we're not trying to wake it
        Waking,  // Still off, but we are trying to wake it up
        Tester,  // The gateway is on, but only because we're keeping it awake with a
                 // tester present message
        Diagnostic,  // The gateway is on, but only because we're keeping it awake with
                     // a diagnostic session.  This causes an error on the IPK, so best
                     // not to keep in this state.
        Awake    // CAN is awake because the car is on or tester present is being sent
    };

    /// A temporary store for the VIN
    char m_vin[18];
    /// The last ticker that we saw RX packets on
    uint32_t m_rxPacketTicker;
    /// The last number of packets received on the CAN
    uint32_t m_rxPackets;
    /// The current state of the CAN to the gateway
    WakeState m_wakeState;
    /// The ticker time the wake state was changed
    uint32_t m_wakeTicker;
    /// A count of the number of times we've woken the car to find it wasn't charging
    uint16_t m_diagCount;
    /// A timer used to send the zombie keep alive frame which is required every 250ms
    TimerHandle_t m_zombieTimer;
    /// The command registered when the car is made to query the software versions of the
    /// different ECUs
    OvmsCommand* m_cmdSoftver;
    /// The responses from the software version queries
    std::vector<std::pair<uint32_t, std::vector<char>>> m_versions;
};

#endif  // __VEHICLE_MGEV_H__
