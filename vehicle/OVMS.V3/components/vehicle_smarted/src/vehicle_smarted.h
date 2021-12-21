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
#include "ovms_mutex.h"
#include "ovms_semaphore.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

#define CAN_BYTE(b)     d[b]
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b))

using namespace std;

class OvmsVehicleSmartED : public OvmsVehicle
{
  public:
    OvmsVehicleSmartED();
    ~OvmsVehicleSmartED();
    static OvmsVehicleSmartED* GetInstance(OvmsWriter* writer=NULL);

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingPollError(canbus* bus, uint16_t type, uint16_t pid, uint16_t code);
    char m_vin[18];

  public:
    void WebInit();
    void WebDeInit();
    void ObdInitPoll();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
    static void WebCfgCommands(PageEntry_t& p, PageContext_t& c);
    static void WebCfgNotify(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBmsCellMonitor(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBmsCellCapacity(PageEntry_t& p, PageContext_t& c);
    static void WebCfgEco(PageEntry_t& p, PageContext_t& c);
    void ConfigChanged(OvmsConfigParam* param);
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);
    bool CommandSetRecu(bool on);
    bool SetRecu(int mode);

  public:
    virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandSetChargeTimer(bool timeron, int hours, int minutes);
    virtual vehicle_command_t CommandClimateControl(bool enable);
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandHomelink(int button, int durationms=1000);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    virtual vehicle_command_t CommandTrip(int verbosity, OvmsWriter* writer);
    void BmsDiag(int verbosity, OvmsWriter* writer);
    void printRPTdata(int verbosity, OvmsWriter* writer);

  public:
    static void xse_recu(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xse_drivemode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xse_chargetimer(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xse_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xse_bmsdiag(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void xse_RPTdata(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  protected:
    int m_reboot_ticker;
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);
    virtual void Ticker60(uint32_t ticker);
    void GetDashboardConfig(DashboardConfig& cfg);
    virtual void CalculateEfficiency();
    void vehicle_smarted_car_on(bool isOn);    
    void NotifyTrip();
    void NotifyValetEnabled();
    void NotifyValetDisabled();
    void NotifyValetHood();
    void NotifyValetTrunk();
    void SaveStatus();
    void RestoreStatus();
    void HandleCharging();
    void HandleEnergy();
    int  calcMinutesRemaining(float target, float charge_voltage, float charge_current);
    void calcBusAktivity(bool state, uint8_t pos);
    void HandleChargingStatus();
    void PollReply_BMS_BattAmps(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattHVstatus(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattHVContactorCyclesLeft(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattHVContactorMax(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattHVContactorState(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattADCref(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattVolts(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattCapacity(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattTemp(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_ModuleTemp(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattDate(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattProdDate(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattHWrev(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattSWrev(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattIsolation(const char* reply_data, uint16_t reply_len);
    void PollReply_BMS_BattVIN(const char* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerPN_HW(const char* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerVoltages(const char* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerAmps(const char* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerSelCurrent(const char* reply_data, uint16_t reply_len);
    void PollReply_NLG6_ChargerTemperatures(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_VC(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_CoolingTemp(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_CoolingPumpTemp(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_CoolingPumpLV(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_CoolingPumpAmps(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_CoolingPumpRPM(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_CoolingPumpOTR(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_CoolingFanRPM(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_CoolingFanOTR(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_BatteryHeaterOTR(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_BatteryHeaterON(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_VacuumPumpOTR(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_VacuumPumpPress1(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_VacuumPumpPress2(const char* reply_data, uint16_t reply_len);
    void PollReply_CEPC_BatteryAgeCondition(const char* reply_data, uint16_t reply_len);

    OvmsCommand *cmd_xse;
    
    OvmsMetricInt *mt_vehicle_time;             // vehicle time
    OvmsMetricFloat *mt_trip_start;             // trip since start
    OvmsMetricFloat *mt_trip_reset;             // trip since Reset
    OvmsMetricBool *mt_hv_active;               // HV active
    OvmsMetricBool *mt_c_active;                // charge active
    OvmsMetricBool *mt_bus_awake;               // can-bus aktiv
    OvmsMetricFloat *mt_bat_energy_used_start;  // display enery used/100km
    OvmsMetricFloat *mt_bat_energy_used_reset;  // display enery used/100km
    OvmsMetricFloat *mt_pos_odometer_start;     // ODOmeter at Start
    OvmsMetricFloat *mt_real_soc;               // real state of charge

    OvmsMetricInt *mt_ed_eco_accel;             // eco score on acceleration over last 6 hours
    OvmsMetricInt *mt_ed_eco_const;             // eco score on constant driving over last 6 hours
    OvmsMetricInt *mt_ed_eco_coast;             // eco score on coasting over last 6 hours
    OvmsMetricInt *mt_ed_eco_score;             // eco score shown on dashboard over last 6 hours
    
    OvmsMetricInt *mt_ed_hv_off_time;           // HV off Timer
    
    OvmsMetricFloat *mt_12v_batt_voltage;       // 12V Batt Voltage from can

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
    bool m_reset_trip;                      // Reset trip when charging else when env on
    bool m_notify_trip;                     // Notify Trip values after driving end (default=true)
    int m_preclima_time;                    // pre clima time (default=15 minutes)
    int m_reboot_time;                      // Reboot time
    bool m_reboot;                          // Reboot Module or Restart Network
    bool m_gpio_highlow;                    // EGPIO direction
    
    uint16_t HVcontactState;                // contactor state: 0 := OFF, 2 := ON
    uint16_t myBMS_Year;                    // year of battery final testing
    uint16_t myBMS_Month;                   // month of battery final testing
    uint16_t myBMS_Day;                     // day of battery final testing
    uint16_t myBMS_ProdYear;                // year of battery production
    uint16_t myBMS_ProdMonth;               // month of battery production
    uint16_t myBMS_ProdDay;                 // day of battery production
    uint16_t myBMS_SOH;                     // Flag showing if degraded cells are found, or battery failure present
    int16_t myBMS_Temps[13];                // three temperatures per battery unit (1 to 3)

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
    
    OvmsMetricFloat* mt_myBMS_Amps;               //!< battery current in ampere (x/32) reported by by BMS
    OvmsMetricFloat* mt_myBMS_Amps2;              //!< battery current in ampere read by live data on CAN or from BMS
    OvmsMetricFloat* mt_myBMS_Power;              //!< power as product of voltage and amps in kW
    
    OvmsMetricFloat* mt_myBMS_HV;                 //!< total voltage of HV system in V
    OvmsMetricInt* mt_myBMS_HVcontactCyclesLeft;  //!< counter related to ON/OFF cyles of the car
    OvmsMetricInt* mt_myBMS_HVcontactCyclesMax;   //!< static, seems to be maxiumum of contactor cycles 
    
    OvmsMetricInt* mt_myBMS_ADCCvolts_mean;       //!< average cell voltage in mV, no offset
    OvmsMetricInt* mt_myBMS_ADCCvolts_min;        //!< minimum cell voltages in mV, add offset +1500
    OvmsMetricInt* mt_myBMS_ADCCvolts_max;        //!< maximum cell voltages in mV, add offset +1500
    OvmsMetricInt* mt_myBMS_ADCvoltsOffset;       //!< calculated offset between RAW cell voltages and ADCref, about 90mV
    
    OvmsMetricInt* mt_myBMS_Isolation;            //!< Isolation in DC path, resistance in kOhm
    OvmsMetricInt* mt_myBMS_DCfault;              //!< Flag to show DC-isolation fault
    
    OvmsMetricString* mt_myBMS_BattVIN;           //!< VIN stored in BMS
    
    OvmsMetricVector<int> *mt_myBMS_HWrev;        //!< hardware-revision
    OvmsMetricVector<int> *mt_myBMS_SWrev;        //!< soft-revision
    
    OvmsMetricBool* mt_CEPC_Wippen;               //!< Recu Wippen installed
    
    OvmsMetricFloat* mt_CEPC_CoolingTemp;         //!< main cooling temperatur measurement / 8
    OvmsMetricFloat* mt_CEPC_CoolingPumpTemp;     //!< temperature at cooling pump, offset 50
    OvmsMetricFloat* mt_CEPC_CoolingPumpLV;       //!< 12V onboard voltage of cooling pump / 10
    OvmsMetricFloat* mt_CEPC_CoolingPumpAmps;     //!< current measured at cooling pump / 5
    OvmsMetricFloat* mt_CEPC_CoolingPumpRPM;      //!< RPM in % of cooling pump (value / 255 * 100%)
    OvmsMetricInt* mt_CEPC_CoolingPumpOTR;        //!< operating time record of cooling pump in hours
    OvmsMetricFloat* mt_CEPC_CoolingFanRPM;       //!< RPM in % of cooling fan (value / 255 * 100%)
    OvmsMetricInt* mt_CEPC_CoolingFanOTR;         //!< operating time record of cooling fan in hours
    OvmsMetricInt* mt_CEPC_BatteryHeaterOTR;      //!< operating time record of PTC heater of battery
    OvmsMetricInt* mt_CEPC_BatteryHeaterON;       //!< Status of the PTC heater of the battery
    OvmsMetricFloat* mt_CEPC_VaccumPumpOTR;       //!< operating time record of vaccum pump
    OvmsMetricInt* mt_CEPC_VaccumPumpPress1;      //!< pressure of vaccum pump measuerement #1 in mbar?
    OvmsMetricInt* mt_CEPC_VaccumPumpPress2;      //!< pressure of vaccum pump measuerement #2 in mbar?
    OvmsMetricFloat* mt_CEPC_BatteryAgeCondition; //!< DT_Batterie_Alterszustand: PRES_EBAP_BattAge_CapaLoss [%]

    #define DEFAULT_BATTERY_CAPACITY 17600
    #define DEFAULT_BATTERY_AMPHOURS 52
    #define CELLCOUNT 93
    #define RAW_VOLTAGES 0                        //!< Use RAW values or calc ADC offset voltage
    #define SE_CANDATA_TIMEOUT 10

  // BMS helpers
  protected:
    float* m_bms_capacitys;                   // BMS Capacity (current value)
    float* m_bms_cmins;                       // BMS minimum Capacity seen (since reset)
    float* m_bms_cmaxs;                       // BMS maximum Capacity seen (since reset)
    float* m_bms_cdevmaxs;                    // BMS maximum capacity deviations seen (since reset)
    short* m_bms_calerts;                     // BMS capacity deviation alerts (since reset)
    int m_bms_calerts_new;                    // BMS new capacity alerts since last notification
    bool m_bms_has_capacitys;                 // True if BMS has a complete set of capacity values
    std::vector<bool> m_bms_bitset_c;         // BMS tracking: true if corresponding capacity set
    int m_bms_bitset_cc;                      // BMS tracking: count of unique capacity values set
    int m_bms_readings_c;                     // Number of BMS capacity readings expected
    int m_bms_readingspermodule_c;            // Number of BMS capacity readings per module
    float m_bms_limit_cmin;                   // Minimum capacity limit (for sanity checking)
    float m_bms_limit_cmax;                   // Maximum capacity limit (for sanity checking)

  protected:
    OvmsMetricFloat*  mt_v_bat_pack_cmin;                 // Cell capacity - weakest cell in pack [As]
    OvmsMetricFloat*  mt_v_bat_pack_cmax;                 // Cell capacity - strongest cell in pack [As]
    OvmsMetricFloat*  mt_v_bat_pack_cavg;                 // Cell capacity - pack average [As]
    OvmsMetricFloat*  mt_v_bat_pack_cstddev;              // Cell capacity - current standard deviation [As]
    OvmsMetricFloat*  mt_v_bat_pack_cstddev_max;          // Cell capacity - maximum standard deviation observed [As]
    OvmsMetricInt*  mt_v_bat_pack_cmin_cell;              // Cell capacity - number of weakest cell in pack
    OvmsMetricInt*  mt_v_bat_pack_cmax_cell;              // Cell capacity - number of strongest cell in pack
    OvmsMetricInt*  mt_v_bat_pack_cmin_cell_volt;         // Cell volatage - number of weakest cell in pack
    OvmsMetricInt*  mt_v_bat_pack_cmax_cell_volt;         // Cell volatage - number of strongest cell in pack
    
    OvmsMetricVector<float>* mt_v_bat_cell_capacity;      // Cell capacity [As]
    OvmsMetricVector<float>* mt_v_bat_cell_cmin;          // Cell minimum capacity [As]
    OvmsMetricVector<float>* mt_v_bat_cell_cmax;          // Cell maximum capacity [As]
    OvmsMetricVector<float>* mt_v_bat_cell_cdevmax;       // Cell maximum capacity deviation observed [As]
    
    OvmsMetricInt* mt_v_bat_HVoff_time;                // HighVoltage contactor off time in seconds
    OvmsMetricInt* mt_v_bat_HV_lowcurrent;             // counter time of no current, reset e.g. with PLC heater or driving
    OvmsMetricInt* mt_v_bat_OCVtimer;                  // counter time in seconds to reach OCV state
    OvmsMetricInt* mt_v_bat_Cap_As_min;                // cell capacity statistics from BMS measurement cycle
    OvmsMetricInt* mt_v_bat_Cap_As_max;                // cell capacity statistics from BMS measurement cycle
    OvmsMetricInt* mt_v_bat_Cap_As_avg;                // cell capacity statistics from BMS measurement cycle
    OvmsMetricInt* mt_v_bat_LastMeas_days;             // days elapsed since last successful measurement
    OvmsMetricFloat* mt_v_bat_Cap_meas_quality;        // some sort of estimation factor??? after measurement cycle
    OvmsMetricFloat* mt_v_bat_Cap_combined_quality;    // some sort of estimation factor??? constantly updated

  protected:
    void BmsSetCellArrangementCapacity(int readings, int readingspermodule);
    void BmsSetCellCapacity(int index, float value);
    void BmsRestartCellCapacitys();
    void BmsResetCellStats();

  public:
    void BmsResetCellCapacitys();
  
  private:
    void AutoSetRecu();
    int m_auto_set_recu;
    bool recuSet = false;
  
  protected:
    void RestartNetwork();
    void ShutDown();
    int m_shutdown_ticker;
  
  public:
    int ObdRequest(uint16_t txid, uint16_t rxid, string request, string& response, int timeout_ms=3000);
    static void shell_obd_request(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
  
  protected:
    string              smarted_obd_rxbuf;
    uint16_t            smarted_obd_rxerr;
    OvmsMutex           smarted_obd_request;
    OvmsSemaphore       smarted_obd_rxwait;
  
  protected:
    void TempPoll();
  
  // charging 12V
  protected:
    void HandleCharging12v();
    unsigned int m_charging_timer;
};

#endif //#ifndef __VEHICLE_SMARTED_H__
