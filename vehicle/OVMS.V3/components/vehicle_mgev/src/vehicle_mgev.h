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
#include "metrics_standard.h"
#include "ovms_webserver.h"
#include "mg_obd_pids.h"
#include "mg_poll_states.h"

#include "freertos/timers.h"

#include <vector>

#define MANUAL_COMMAND_TIMEOUT 10 //ticks. Number of times to check result of manual frame commands before timing out.
#define MANUAL_COMMAND_WAIT_TICK_PERIOD 100 //ms. Number of milliseconds to wait before rechecking result of manual frame commands.
#define portTICK_PERIOD_US (portTICK_PERIOD_MS*1000) //portTICK_PERIOD_MS converted to microseconds.
#define SYNC_REQUEST_TIMEOUT 100 //ms. Number of milliseconds to wait for the synchronous PollSingleRequest() calls.
#define GWM_RETRY_CHECK_STATE_TIMEOUT 5 //seconds. Number of seconds to wait before retry state check.
#define CAR_UNRESPONSIVE_THRESHOLD 3 //seconds. If reaches this threshold, GWM state will change back to Unknown.
#define CHARGING_THRESHOLD 12.9 //Volts. If voltage is lower than this, we say 1. 12V battery is not charging and 2. we should sleep OVMS to avoid draining battery too low
#define DEFAULT_BMS_VERSION 1 //Corresponding to the BMSDoDLimits array element
#define WLTP_RANGE 263.0 //km
#define TRANSITION_TIMEOUT 50 //s. Number of seconds after 12V goes below CHARGING_THRESHOLD to stay in current state before going to sleep.

namespace
{

typedef struct
{
    float Lower;
    float Upper;
} BMSDoDLimits_t;

const BMSDoDLimits_t BMSDoDLimits[] =
{
    {6, 97}, //Pre Jan 2021 BMS firmware DoD range 6 - 97
    {2.5, 94} //Jan 2021 BMS firmware DoD range 2.5 - 94
};

typedef struct{
       int fromPercent;
       int toPercent;
       float maxChargeSpeed;
}charging_profile;

}

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
    OvmsMetricFloat* m_bat_voltage_vcu;
    OvmsMetricFloat* m_bat_coolant_temp;
    OvmsMetricFloat* m_bat_resistance;
    OvmsMetricFloat *m_bms_max_cell_voltage, *m_bms_min_cell_voltage;
    OvmsMetricBool *m_bms_main_relay_b, *m_bms_main_relay_g, *m_bms_main_relay_p;
    OvmsMetricString *m_bms_time;
    OvmsMetricInt *m_bat_error;
    OvmsMetricFloat* m_env_face_outlet_temp;
    OvmsMetricFloat* m_dcdc_load;
    OvmsMetricInt* m_vcu_dcdc_mode;
    OvmsMetricFloat *m_vcu_dcdc_input_current, *m_vcu_dcdc_input_voltage, *m_vcu_dcdc_output_current, *m_vcu_dcdc_output_voltage;
    OvmsMetricFloat *m_vcu_dcdc_temp;
    OvmsMetricFloat* m_soc_raw;
    OvmsMetricFloat* m_motor_coolant_temp;
    OvmsMetricFloat* m_motor_torque;
    OvmsMetricBool* m_radiator_fan;
    OvmsMetricInt *m_poll_state_metric; // Kept equal to m_poll_state for debugging purposes
    OvmsMetricInt *m_gwm_state; // Kept equal to m_gwmState for variant A for debugging purposes and used to store actual GWM state for variant B
    OvmsMetricBool *m_bcm_auth; // True if BCM is authenticated, false if not
    OvmsMetricInt *m_gwm_task, *m_bcm_task; // Current ECU tasks that we are awaiting response for manual frame handling so we know which function to use to handle the responses.
    OvmsMetricInt *m_ignition_state; // For storing state of start switch

  protected:
    void ConfigChanged(OvmsConfigParam* param) override;

    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) override;

    void IncomingFrameCan1(CAN_frame_t* p_frame) override;
    void IncomingFrameCan2(CAN_frame_t* p_frame) override;
    void IncomingFrameCan3(CAN_frame_t* p_frame) override;
    void IncomingFrameCan4(CAN_frame_t* p_frame) override;

    int GetNotifyChargeStateDelay(const char* state) override;

	void processEnergy();

    static void WakeUp(void* self);
    void NotifyVehicleIdling() override;
    canbus* IdToBus(int id);
    void ConfigurePollInterface(int bus);

    /**
     * Form the poll list for OVMS to use by combining the common and variant specific lists.
     * @param SpecificPollData Variant specific poll list to add to common list
     * @param DataSize sizeof(SpecificPollData)
     */
    void ConfigurePollData(const OvmsVehicle::poll_pid_t *SpecificPollData, size_t DataSize);
    // Form the poll list for OVMS to use by using only the common list
    void ConfigurePollData();

    // Integer to string without padding
    static string IntToString(int x);
    /**
     * Integer to string with padding
     * @param length Desired length. If string is shorter than length, padding will be added.
     * @param padding The padding character to use.
     */
    static string IntToString(int x, uint8_t length, string padding);

    // Signal to an ECU that an OBD tester is connected
    bool SendTesterPresentTo(canbus* currentBus, uint16_t id);
    // Signal to an ECU to enter Diagnostic session defined by mode
    bool SendDiagSessionTo(canbus* currentBus, uint16_t id, uint8_t mode);
    bool SendPollMessage(canbus* bus, uint16_t id, uint8_t type, uint16_t pid);

    // typedef struct
    // {
    //     uint32_t TXID;
    //     uint16_t PID;
    //     array<uint16_t, VEHICLE_POLL_NSTATES> PollInterval;
    //     uint32_t SentTime;
    // } ManualPollList_t;
    // std::vector<ManualPollList_t> m_ManualPollList;
    // /**
    //  * Setup manual poll list by copying required data (see ManualPollList_t) from the declared table (poll_pid_t so can use same exact format as normal poll table) and adding a sent time property to each item
    //  * @param ManualPolls Poll items to manually poll
    //  * @param ManualPollSize sizeof(ManualPolls)
    //  */
    // void SetupManualPolls(const OvmsVehicle::poll_pid_t *ManualPolls, size_t ManualPollSize);
    // // Loop through manual poll list and send a request one by one
    // void SendManualPolls(canbus* currentBus, uint32_t ticker);

    static OvmsVehicleMgEv* GetActiveVehicle(OvmsWriter* writer);
    static void SoftwareVersions(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void AuthenticateECUShell(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void DRLCommandShell(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void DRLCommandWithAuthShell(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

    enum class GWMTasks
    {
        None,
        SoftwareVersion,
        Authentication,
        AuthenticationDone,
        AuthenticationError
    };
    enum class BCMTasks
    {
        None,
        Poll,
        SoftwareVersion,
        Authentication,
        AuthenticationDone,
        AuthenticationError,
        DRL,
        DRLDone,
        DRLError
    };
    // List of ECUs that can be authenticated
    enum class ECUAuth
    {
        GWM,
        BCM
    };
    bool AuthenticateECU(vector<ECUAuth> ECUsToAuth);

    // The polling structure, this is stored on external RAM which should be no slower
    // than accessing a const data structure as the Flash is stored externally on the
    // same interface and will be cached in the same way
    OvmsVehicle::poll_pid_t* m_pollData = nullptr;
    // A temporary store for the VIN
    char m_vin[18];
	  // Store cumulative energy charged
    float mg_cum_energy_charge_wh;
    // Set to true when send tester present to GWM and set to false when response is received
    bool m_WaitingGWMTesterPresentResponse = false;
    // Count number of times a tester present message is not responded by the GWM
    uint8_t m_GWMUnresponsiveCount = 0;
    // The ticker time for after-run of charging and running sessions
    uint32_t m_afterRunTicker = 0;

  private:
    // OVMS shell commands
    OvmsCommand *m_cmdSoftver, *m_cmdAuth, *m_cmdDRL, *m_cmdDRLNoAuth;
    // The responses from the software version queries. First element of tuple = ECU ID. Second element of tuple = software version character vector response. Third element of tuple = response data bytes remaining, should be 0 after finished.
    std::vector<std::tuple<uint32_t, std::vector<char>, uint16_t>> m_versions;
    // True when getting software versions from ECUs
    bool m_GettingSoftwareVersions = false;
    // Number of full software version responses obtained
    uint8_t m_SoftwareVersionResponsesReceived = 0;
    // Timer for measuring how much time has elapsed since sending first frame
    int64_t m_DRLConsecutiveFrameTimer;
    // Response from BCM after sending DRL command
    uint8_t m_DRLResponse[5];

    int calcMinutesRemaining(int target_soc, charging_profile charge_steps[]);
    bool soc_limit_reached;
    bool range_limit_reached;

    // mg_configuration.cpp
    int CanInterface();

	// mg_can_handler.cpp
    void IncomingPollFrame(CAN_frame_t* frame);

    // software_versions.cpp
    void SoftwareVersions(OvmsWriter* writer);

    // mg_poll_bms.cpp
    void IncomingBmsPoll(uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain);
    void SetBmsStatus(uint8_t status);
    void ProcessBatteryStats(int index, uint8_t* data, uint16_t remain);
    float calculateSoc(uint16_t value);
    // A cache of the last byte in the first message of the BMS cell voltage message
    uint8_t m_bmsCache;
    string m_bmsTimeTemp;

    // mg_poll_dcdc.cpp
    void IncomingDcdcPoll(uint16_t pid, uint8_t* data, uint8_t length);

    // mg_poll_vcu.cpp
    void IncomingVcuPoll(uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain);
    void HandleVinMessage(uint8_t* data, uint8_t length, uint16_t remain);
    void SetEnvOn();

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

    // mg_gwm.cpp
    bool AuthenticateGWM(canbus* currentBus);
    void IncomingGWMAuthFrame(CAN_frame_t* frame, uint8_t serviceId, uint8_t* data);
	void IncomingGWMFrame(CAN_frame_t* frame, uint8_t frameType, uint16_t frameLength, uint8_t serviceId, uint16_t responsePid, uint8_t* data);

    // mg_bcm.cpp
    bool AuthenticateBCM(canbus* currentBus);
    void IncomingBCMAuthFrame(CAN_frame_t* frame, uint8_t serviceId, uint8_t* data);
    void DRLCommand(OvmsWriter* writer, canbus* currentBus, bool TurnOn);
    void IncomingBCMDRLFrame(CAN_frame_t* frame, uint8_t frameType, uint8_t serviceId, uint16_t responsePid, uint8_t* data);
    void DRLFirstFrameSent(const CAN_frame_t* p_frame, bool success);
    CanFrameCallback DRLFirstFrameSentCallback;
	void IncomingBCMFrame(CAN_frame_t* frame, uint8_t frameType, uint16_t frameLength, uint8_t serviceId, uint16_t responsePid, uint8_t* data);

    // software_versions.cpp
    void IncomingSoftwareVersionFrame(CAN_frame_t* frame, uint8_t frameType, uint16_t frameLength, uint16_t responsePid, uint8_t* data);
    uint32_t m_ECUCountToQuerySoftwareVersion = 0;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //  - implementation: mi_web.(h,cpp)
    //
  public:
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
    void GetDashboardConfig(DashboardConfig& cfg);
    static void WebDispChgMetrics(PageEntry_t &p, PageContext_t &c);
#endif //CONFIG_OVMS_COMP_WEBSERVER

};

#endif  // __VEHICLE_MGEV_H__
