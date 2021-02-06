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

    static void ZombieMode(TimerHandle_t timer);
    void ZombieMode();

    static void SoftwareVersions(
            int, OvmsWriter* writer, OvmsCommand*, int, const char* const*);
    void SoftwareVersions(OvmsWriter* writer);

    static void WakeUp(void* self);

    void IncomingPollFrame(CAN_frame_t* frame);
    bool SendPollMessage(canbus* bus, uint16_t id, uint8_t type, uint16_t pid);

    bool HasWoken(canbus* currentBus, uint32_t ticker);
    void DeterminePollState(canbus* currentBus, uint32_t ticker);
    void SendAlarmSensitive(canbus* currentBus);
    // Signal to an ECU that an OBD tester is connected
    bool SendTesterPresentTo(canbus* currentBus, uint16_t id); 
    // Signal to an ECU to enter Diagnostic session defined by mode
    bool SendDiagSessionTo(canbus* currentBus, uint16_t id, uint8_t mode); 

  

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
    enum GwmState
    {
        AllowToSleep,  // Stop Sending any control to GWM
        SendTester,  // Send Tester Present to GWM
        SendDiagnostic,  // Send Diagnostic Session Override to GWM
        Undefined    // We are not controlling GWM
    };

    /// A temporary store for the VIN
    char m_vin[18];
    /// The last ticker that we saw RX packets on
    uint32_t m_rxPacketTicker;
    /// The last number of packets received on the CAN
    uint32_t m_rxPackets;
    /// The number of ticks that no packets have been recieved
    uint32_t m_noRxCount;
    /// The current state of control commands we are sending to the gateway
    GwmState m_gwmState;
    /// The ticker time for after-run of charging and running sessions
    uint32_t m_afterRunTicker;
    /// The ticker time for Zombie Mode Sleep Timeout before attempting to wake.
    uint32_t m_preZombieOverrideTicker;
    /// Boolean for Car State
    bool carIsResponsiveToQueries;
    /// A count of the number of times we've woken the car to find it wasn't charging
    uint16_t m_diagCount;
    /// The command registered when the car is made to query the software versions of the
    /// different ECUs
    OvmsCommand* m_cmdSoftver;
    /// The responses from the software version queries
    std::vector<std::pair<uint32_t, std::vector<char>>> m_versions;
    float mg_cum_energy_charge_wh;
    
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //  - implementation: mi_web.(h,cpp)
    //
  public:
    void WebInit();
    //static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    void GetDashboardConfig(DashboardConfig& cfg);
#endif //CONFIG_OVMS_COMP_WEBSERVER
    
};

#endif  // __VEHICLE_MGEV_H__
