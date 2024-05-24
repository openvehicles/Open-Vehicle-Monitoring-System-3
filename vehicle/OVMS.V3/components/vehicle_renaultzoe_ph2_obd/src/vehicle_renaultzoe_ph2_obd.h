/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th Apr 2022
;
;    (C) 2022       Carsten Schmiemann
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

#ifndef __VEHICLE_RENAULTZOE_PH2_OBD_H__
#define __VEHICLE_RENAULTZOE_PH2_OBD_H__

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

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     data[b]
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b+1) << 16)  | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (data[b] & 0x0f)
#define CAN_NIBH(b)     (data[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))

#define POLLSTATE_OFF					PollSetState(0);
#define POLLSTATE_ON					PollSetState(1);
#define POLLSTATE_DRIVING			PollSetState(2);
#define POLLSTATE_CHARGING		PollSetState(3);

using namespace std;

#define TAG (OvmsVehicleRenaultZoePh2OBD::s_tag)

class OvmsVehicleRenaultZoePh2OBD : public OvmsVehicle {
  public:
    static const char *s_tag;

  public:
    OvmsVehicleRenaultZoePh2OBD();
    ~OvmsVehicleRenaultZoePh2OBD();
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    static void WebCfgCommon(PageEntry_t& p, PageContext_t& c);
#endif
    void ConfigChanged(OvmsConfigParam* param) override;
    void ZoeWakeUp();
    void IncomingFrameCan1(CAN_frame_t* p_frame) override;
    void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    void WebInit();
    void WebDeInit();
#endif
    bool CarIsCharging = false;
    bool CarPluggedIn = false;
    bool CarLastCharging = false;
    bool CarIsDriving = false;
    bool CarIsDrivingInit = false;
    float ACInputPowerFactor = 0.0;
    float Bat_cell_capacity = 0.0;

	protected:
    int m_range_ideal;
    int m_battery_capacity;
    bool m_UseCarTrip = false;
    bool m_UseBMScalculation = false;
    char zoe_vin[18] = "";
		void IncomingINV(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void IncomingEVC(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void IncomingBCM(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void IncomingLBC(uint16_t type, uint16_t pid, const char* data, uint16_t len);
    void IncomingHVAC(uint16_t type, uint16_t pid, const char* data, uint16_t len);
    void IncomingUCM(uint16_t type, uint16_t pid, const char* data, uint16_t len);
    //void IncomingCLUSTER(uint16_t type, uint16_t pid, const char* data, uint16_t len);
    int calcMinutesRemaining(float charge_voltage, float charge_current);
    void ChargeStatistics();
    void EnergyStatisticsOVMS();
    void EnergyStatisticsBMS();
    void Ticker10(uint32_t ticker) override;//Handle charge, energy statistics
    void Ticker1(uint32_t ticker) override; //Handle trip counter
    
    		
    // Renault ZOE specific metrics
    OvmsMetricBool   *mt_bus_awake;               //CAN bus awake status
    OvmsMetricFloat  *mt_pos_odometer_start;      //ODOmeter at trip start
    OvmsMetricFloat  *mt_bat_used_start;          //Used battery kWh at trip start
    OvmsMetricFloat  *mt_bat_recd_start;          //Recd battery kWh at trip start
    OvmsMetricFloat  *mt_bat_chg_start;           //Charge battery kWh at charge start
    OvmsMetricFloat  *mt_bat_available_energy;    //Available energy in battery
    OvmsMetricFloat  *mt_bat_aux_power_consumer;  //Power usage by consumer
    OvmsMetricFloat  *mt_bat_aux_power_ptc;       //Power usage by PTCs
    OvmsMetricFloat  *mt_bat_max_charge_power;    //Battery max allowed charge, recd power
    OvmsMetricFloat  *mt_bat_cycles;              //Battery full charge cycles
    OvmsMetricFloat  *mt_main_power_available;    //Mains power available
    OvmsMetricString *mt_main_phases;             //Mains phases used
    OvmsMetricFloat  *mt_main_phases_num;         //Mains phases used
    OvmsMetricString *mt_inv_status;              //Inverter status string
    OvmsMetricFloat  *mt_inv_hv_voltage;          //Inverter battery voltage sense, used for motor power calc
    OvmsMetricFloat  *mt_inv_hv_current;          //Inverter battery current sense, used for motor power calc
    OvmsMetricFloat  *mt_mot_temp_stator1;        //Temp of motor stator 1
    OvmsMetricFloat  *mt_mot_temp_stator2;        //Temp of motor stator 2
    OvmsMetricFloat  *mt_hvac_compressor_speed;   //Compressor speed
    OvmsMetricFloat  *mt_hvac_compressor_pressure;//Compressor high-side pressure in BAR
    OvmsMetricFloat  *mt_hvac_compressor_power;   //Compressor power usage
    OvmsMetricString *mt_hvac_compressor_mode;    //Compressor mode
    OvmsMetricFloat  *mt_v_env_pressure;          //Ambient air pressure
    
        
  protected:
    string zoe_obd_rxbuf;
};

#endif //#ifndef __VEHICLE_RENAULTZOE_PH2_OBD_H__
