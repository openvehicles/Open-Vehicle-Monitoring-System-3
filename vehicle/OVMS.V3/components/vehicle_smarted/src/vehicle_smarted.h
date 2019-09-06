/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 */

#ifndef __VEHICLE_SMARTED_H__
#define __VEHICLE_SMARTED_H__

#include <atomic>

#include "can.h"
#include "vehicle.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "freertos/timers.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;

void xse_recu(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xse_chargetimer(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

class OvmsVehicleSmartED : public OvmsVehicle
{
  public:
    OvmsVehicleSmartED();
    ~OvmsVehicleSmartED();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    char m_vin[18];

  public:
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
    static void WebCfgCommands(PageEntry_t& p, PageContext_t& c);
    void ConfigChanged(OvmsConfigParam* param);
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);
    bool CommandSetRecu(bool on);

  public:
    virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandWakeup();
#ifdef CONFIG_OVMS_COMP_MAX7317
    virtual vehicle_command_t CommandSetChargeTimer(bool timeron, int hours, int minutes);
    virtual vehicle_command_t CommandClimateControl(bool enable);
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandHomelink(int button, int durationms=1000);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
#endif
    
  protected:
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);
    virtual void Ticker60(uint32_t ticker);
    void GetDashboardConfig(DashboardConfig& cfg);
    void vehicle_smarted_car_on(bool isOn);
    TimerHandle_t m_locking_timer;
    
    void SaveStatus();
    void RestoreStatus();
    void HandleCharging();
    void HandleEnergy();
    int  calcMinutesRemaining(float target, float charge_voltage, float charge_current);
    void calcBusAktivity(bool state, uint8_t pos);
    void HandleChargingStatus();
    void PollReply_BMS_BattVolts(uint8_t* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattTemp(uint8_t* reply_data, uint16_t reply_len);
    void PollReply_BMS_ModuleTemp(uint8_t* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerPN_HW(uint8_t* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerVoltages(uint8_t* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerAmps(uint8_t* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerSelCurrent(uint8_t* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerTemperatures(uint8_t* reply_data, uint16_t reply_len);

    OvmsCommand *cmd_xse;
    
    OvmsMetricInt *mt_vehicle_time;             // vehicle time
    OvmsMetricInt *mt_trip_start;               // trip since start
    OvmsMetricInt *mt_trip_reset;               // trip since Reset
    OvmsMetricBool *mt_hv_active;               // HV active
    OvmsMetricBool *mt_c_active;                // charge active
    OvmsMetricFloat *mt_bat_energy_used_start;  // display enery used/100km
    OvmsMetricFloat *mt_bat_energy_used_reset;  // display enery used/100km
    OvmsMetricFloat *mt_pos_odometer_start;     // ODOmeter at Start

  private:
    unsigned int m_candata_timer;
    unsigned int m_candata_poll;
    unsigned int m_egpio_timer;

  protected:
    int m_doorlock_port;                    // … MAX7317 output port number (3…9, default 9 = EGPIO_8)
    int m_doorunlock_port;                  // … MAX7317 output port number (3…9, default 8 = EGPIO_7)
    int m_ignition_port;                    // … MAX7317 output port number (3…9, default 7 = EGPIO_6)
    int m_doorstatus_port;                  // … MAX7317 output port number (3…9, default 6 = EGPIO_5)
    int m_range_ideal;                      // … Range Ideal (default 135 km)
    int m_egpio_timout;                     // … EGPIO Ignition Timout (default 5 min)
    bool m_soc_rsoc;                        // Display SOC=SOC or rSOC=SOC
    bool m_enable_write;                    // canwrite
    bool m_lock_state;                      // Door lock/unlock state

  protected:
    char NLG6_PN_HW[11] = "4519822221";           //!< Part number for NLG6 fast charging hardware
    OvmsMetricBool *mt_nlg6_present;              //!< Flag to show NLG6 detected in system
    OvmsMetricVector<float> *mt_nlg6_main_amps;   //!< AC current of L1, L2, L3
    OvmsMetricVector<float> *mt_nlg6_main_volts;  //!< AC voltage of L1, L2, L3
    OvmsMetricFloat *mt_nlg6_amps_setpoint;       //!< AC charging current set by user in BC (Board Computer)
    OvmsMetricFloat *mt_nlg6_amps_cablecode;      //!< Maximum current cable (resistor coded)
    OvmsMetricFloat *mt_nlg6_amps_chargingpoint;  //!< Maxiumum current of chargingpoint
    OvmsMetricFloat *mt_nlg6_dc_current;          //!< DC current measured by charger
    OvmsMetricFloat *mt_nlg6_dc_hv;               //!< DC HV measured by charger
    OvmsMetricFloat *mt_nlg6_dc_lv;               //!< 12V onboard voltage of Charger DC/DC
    OvmsMetricVector<float> *mt_nlg6_temps;       //!< internal temperatures in charger unit and heat exchanger
    OvmsMetricFloat *mt_nlg6_temp_reported;       //!< mean temperature, reported by charger
    OvmsMetricFloat *mt_nlg6_temp_socket;         //!< temperature of mains socket charger
    OvmsMetricFloat *mt_nlg6_temp_coolingplate;   //!< temperature of cooling plate 
    OvmsMetricString *mt_nlg6_pn_hw;              //!< Part number of base hardware (wo revisioning)
    
    #define DEFAULT_BATTERY_CAPACITY 17600
    #define DEFAULT_BATTERY_AMPHOURS 53
    #define MAX_POLL_DATA_LEN 238
    #define CELLCOUNT 93
    #define SE_CANDATA_TIMEOUT 10
    
};

#endif //#ifndef __VEHICLE_SMARTED_H__
