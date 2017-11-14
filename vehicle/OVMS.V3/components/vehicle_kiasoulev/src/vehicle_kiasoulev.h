/*
;    Project:       Open Vehicle Monitor System
;    Date:          22th October 2017
;
;    Changes:
;    1.0  Initial stub
;
;    (C) 2017        Geir Øyvind Vælidalo <geir@validalo.net>
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

#ifndef __VEHICLE_KIASOULEV_H__
#define __VEHICLE_KIASOULEV_H__

#include "vehicle.h"

using namespace std;

typedef union {
  struct { //TODO Is this the correct order, or should it be swapped?
    unsigned char Park : 1;
    unsigned char Reverse : 1;
    unsigned char Neutral : 1;
    unsigned char Drive : 1;
    unsigned char Break : 1;
    unsigned char ECOOff : 1;
    unsigned char : 1;
    unsigned char : 1;
  };
  unsigned char value;
} KsShiftBits;

class OvmsVehicleKiaSoulEv : public OvmsVehicle
  {
  public:
    OvmsVehicleKiaSoulEv();
    ~OvmsVehicleKiaSoulEv();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    void Ticker1(std::string event, void* data);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);

  protected:
    void vehicle_kiasoulev_car_on(bool isOn);
    float vehicle_kiasoulev_get_maxrange(void);
    uint16_t calcMinutesRemaining(float target);

    float ks_battery_capacity = 27000; //TODO Detect battery capacity from VIN
    float ks_maxRange = 160; //Real world range at current temperature

    uint32_t ks_tpms_id[4];
    float ks_obc_volt;
    uint8_t ks_chargepower; // in [1/256 kW]
    char m_vin[18];
    KsShiftBits ks_shift_bits;

    float ks_trip_start_odo;
    float ks_last_soc;
    float ks_last_ideal_range;
    uint32_t ks_start_cdc; 			// Used to calculate trip power use (Cumulated discharge)
    uint32_t ks_start_cc;  			// Used to calculate trip recuperation (Cumulated charge)
    uint32_t ks_cum_charge_start; 	// Used to calculate charged power.
    uint16_t ks_chargeduration = 0;  // charge duration in seconds

    uint8_t ks_battery_module_temp[8];

    UINT ks_battery_DC_voltage; 				//DC voltage                    02 21 01 -> 22 2+3
    INT ks_battery_current; 					//Battery current               02 21 01 -> 21 7+22 1
    UINT ks_battery_avail_charge; 			//Available charge              02 21 01 -> 21 2+3
    uint8_t ks_battery_max_cell_voltage; 	//Max cell voltage              02 21 01 -> 23 6
    uint8_t ks_battery_max_cell_voltage_no; 	//Max cell voltage no           02 21 01 -> 23 7
    uint8_t ks_battery_min_cell_voltage; 	//Min cell voltage              02 21 01 -> 24 1
    uint8_t ks_battery_min_cell_voltage_no; 	//Min cell voltage no           02 21 01 -> 24 2

    uint32_t ks_battery_cum_charge_current; 		//Cumulated charge current    02 21 01 -> 24 6+7 & 25 1+2
    uint32_t ks_battery_cum_discharge_current; 	//Cumulated discharge current 02 21 01 -> 25 3-6
    uint32_t ks_battery_cum_charge; 				//Cumulated charge power      02 21 01 -> 25 7 + 26 1-3
    uint32_t ks_battery_cum_discharge; 			//Cumulated discharge power   02 21 01 -> 26 4-7
    uint8_t ks_battery_cum_op_time[3]; 			//Cumulated operating time    02 21 01 -> 27 1-4

    uint8_t ks_battery_cell_voltage[32];

    uint8_t ks_battery_min_temperature; 		//02 21 05 -> 21 7
    uint8_t ks_battery_inlet_temperature; 	//02 21 05 -> 21 6
    uint8_t ks_battery_max_temperature; 		//02 21 05 -> 22 1
    uint8_t ks_battery_heat_1_temperature; 	//02 21 05 -> 23 6
    uint8_t ks_battery_heat_2_temperature; 	//02 21 05 -> 23 7

    UINT ks_battery_max_detoriation; 			//02 21 05 -> 24 1+2
    uint8_t ks_battery_max_detoriation_cell_no; 	//02 21 05 -> 24 3
    UINT ks_battery_min_detoriation; 			//02 21 05 -> 24 4+5
    uint8_t ks_battery_min_detoriation_cell_no; 	//02 21 05 -> 24 6

    uint8_t ks_heatsink_temperature; //TODO Remove?
    uint8_t ks_battery_fan_feedback;

    uint8_t ks_auxVoltage; // Voltage 12V aux battery [1/10 V]

    struct {
      unsigned char ChargingChademo : 1;
      unsigned char ChargingJ1772 : 1;
      unsigned char : 1;
      unsigned char : 1;
      unsigned char FanStatus : 4;
    } ks_charge_bits;
  };

#define SQR(n) ((n)*(n))
#define ABS(n) (((n) < 0) ? -(n) : (n))
#define MIN(n,m) ((n) < (m) ? (n) : (m))
#define MAX(n,m) ((n) > (m) ? (n) : (m))
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     data[b]
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b+1) << 16)  | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (can_databuffer[b] & 0x0f)
#define CAN_NIBH(b)     (can_databuffer[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))

#endif //#ifndef __VEHICLE_KIASOULEV_H__
