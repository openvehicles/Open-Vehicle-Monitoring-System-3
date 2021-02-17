/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th September 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Tom Parker
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

#ifndef __VEHICLE_NISSANLEAF_H__
#define __VEHICLE_NISSANLEAF_H__

#include <iostream>
#include <algorithm>
#include "freertos/timers.h"
#include "vehicle.h"
#include "ovms_webserver.h"
#include "ovms_semaphore.h"
#include "ovms_mutex.h"
#include "nl_types.h"

#define DEFAULT_MODEL_YEAR 2012
#define DEFAULT_CABINTEMP_OFFSET .0
#define GEN_1_NEW_CAR_GIDS 281
#define GEN_1_NEW_CAR_AH 66
#define GEN_1_KM_PER_KWH 7.1
#define GEN_1_WH_PER_GID 80
#define GEN_1_30_NEW_CAR_GIDS 356
#define GEN_1_30_NEW_CAR_AH 79
#define GEN_2_40_NEW_CAR_GIDS 502
#define GEN_2_40_NEW_CAR_AH 115
#define GEN_2_62_NEW_CAR_GIDS 775
#define GEN_2_62_NEW_CAR_AH 176
#define REMOTE_COMMAND_REPEAT_COUNT 24 // number of times to send the remote command after the first time
#define ACTIVATION_REQUEST_TIME 10 // tenths of a second to hold activation request signal
#define DEFAULT_AC_VOLTAGE_MULTIPLIER 1.12 // scales from 179 to 200V see pg EVC-88 of 2013 Leaf EVC.pdf where AC voltage is 100/200V

using namespace std;

typedef enum
  {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  START_CHARGING,
  AUTO_DISABLE_CLIMATE_CONTROL,
  UNLOCK_DOORS,
  LOCK_DOORS
  } RemoteCommand;

typedef enum
  {
  CAN_READ_ONLY,
  INVALID_SYNTAX,
  COMMAND_RESULT_OK
  } CommandResult;

typedef enum
  {
  CHARGER_STATUS_IDLE,
  CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT,
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_QUICK_CHARGING,
  CHARGER_STATUS_FINISHED
  } ChargerStatus;

class OvmsVehicleNissanLeaf : public OvmsVehicle
  {
  public:
    OvmsVehicleNissanLeaf();
    ~OvmsVehicleNissanLeaf();

  public:
    static OvmsVehicleNissanLeaf* GetInstance(OvmsWriter* writer=NULL);
    void ConfigChanged(OvmsConfigParam* param);
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);

  public:
    bool ObdRequest(uint16_t txid, uint16_t rxid, uint32_t request, string& response, int timeout_ms /*=3000*/, uint8_t bus);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    vehicle_command_t CommandHomelink(int button, int durationms=1000);
    vehicle_command_t CommandClimateControl(bool enable);
    vehicle_command_t CommandLock(const char* pin);
    vehicle_command_t CommandUnlock(const char* pin);
    vehicle_command_t CommandWakeup();
    void RemoteCommandTimer();
    void CcDisableTimer();

  // --------------------------------------------------------------------------
  // Webserver subsystem
  //  - implementation: nl_web.(h,cpp)
  //

  public:
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
    static void shell_obd_request(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  public:
    void GetDashboardConfig(DashboardConfig& cfg);

  private:
    void CommandInit(); // initialise shell commands specific to Leaf
    void vehicle_nissanleaf_car_on(bool isOn);
    void vehicle_nissanleaf_charger_status(ChargerStatus status);
    void SendCanMessage(uint16_t id, uint8_t length, uint8_t *data);
    void Ticker1(uint32_t ticker);
    void Ticker10(uint32_t ticker);
    void HandleEnergy();
    void HandleCharging();
    void HandleRange();
    int  calcMinutesRemaining(float target, float charge_power_w);
    void SendCommand(RemoteCommand);
    OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);
    OvmsVehicle::vehicle_command_t CommandStartCharge();
    virtual int GetNotifyChargeStateDelay(const char* state);
    RemoteCommand nl_remote_command; // command to send, see RemoteCommandTimer()
    uint8_t nl_remote_command_ticker; // number remaining remote command frames to send
    void PollReply_Battery(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_QC(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_L0L1L2(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_VIN(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_BMS_Volt(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_BMS_Shunt(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_BMS_Temp(uint8_t reply_data[], uint16_t reply_len);

    TimerHandle_t m_remoteCommandTimer;
    TimerHandle_t m_ccDisableTimer;
    metric_unit_t m_odometer_units = Other;
    OvmsMetricInt *m_gids;
    OvmsMetricFloat *m_hx;
    OvmsMetricFloat *m_soc_new_car;
    OvmsMetricFloat *m_soc_instrument;
    OvmsMetricInt *m_range_instrument;
    OvmsMetricVector<int> *m_bms_thermistor;
    OvmsMetricVector<int> *m_bms_temp_int;
    OvmsMetricBitset<96> *m_bms_balancing;
    OvmsMetricFloat *m_soh_new_car;
    OvmsMetricInt *m_soh_instrument;
    OvmsMetricFloat *m_battery_energy_capacity;
    OvmsMetricFloat *m_battery_energy_available;
    OvmsMetricInt *m_battery_type;
    OvmsMetricBool *m_battery_heaterpresent;
    OvmsMetricVector<int> *m_charge_duration;
    OvmsMetricVector<string> *m_charge_duration_label;
    OvmsMetricInt *m_quick_charge;
    OvmsMetricFloat *m_soc_nominal;
    OvmsMetricInt *m_charge_count_qc;
    OvmsMetricInt *m_charge_count_l0l1l2;
    OvmsMetricBool *m_climate_fan_only;
    OvmsMetricBool *m_climate_remoteheat;
    OvmsMetricBool *m_climate_remotecool;
    OvmsMetricString *m_climate_vent;
    OvmsMetricString *m_climate_intake;
    OvmsMetricInt *m_climate_fan_speed;
    OvmsMetricInt *m_climate_fan_speed_limit;
    OvmsMetricFloat *m_climate_setpoint;
    OvmsMetricBool *m_climate_auto;


    float m_cum_energy_used_wh;				    // Cumulated energy (in wh) used within 1 second ticker interval
    float m_cum_energy_recd_wh; 					// Cumulated energy (in wh) recovered  within 1 second ticker interval
    float m_cum_energy_charge_wh;					// Cumulated energy (in wh) charged within 10 second ticker interval
    bool  m_gen1_charger;					        // True if using original charger and 0x5bf messages, false if using 0x390 messages
    bool  m_enable_write;                 // Enable/disable can write (polling and commands

  protected:
    OvmsCommand*        cmd_xnl;
    string              nl_obd_rxbuf;
    OvmsMutex           nl_obd_request;
    OvmsSemaphore       nl_obd_rxwait;
  };

#endif //#ifndef __VEHICLE_NISSANLEAF_H__
