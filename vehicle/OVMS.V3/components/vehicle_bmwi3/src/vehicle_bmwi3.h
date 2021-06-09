/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    OVMS Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
;    BMW I3 component:
;    Developed by Stephen Davies <steve@telviva.co.za>
;
;    2020-12-12     0.0       Work started
;    2021-01-04               Ready to merge as first version
;
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

#ifndef __VEHICLE_BMWI3_H__
#define __VEHICLE_BMWI3_H__

#include "vehicle.h"

using namespace std;

// rxbuff access macros: b=byte# 0..7 / n=nibble# 0..15
#define RXBUF_BYTE(b)     (rxbuf[b])
#define RXBUF_UCHAR(b)    ((unsigned char)RXBUF_BYTE(b))
#define RXBUF_SCHAR(b)    ((signed char)RXBUF_BYTE(b))
#define RXBUF_UINT(b)     (((UINT)RXBUF_BYTE(b) << 8) | RXBUF_BYTE(b+1))
#define RXBUF_SINT(b)     ((short)RXBUF_UINT(b))
#define RXBUF_UINT24(b)   (((uint32_t)RXBUF_BYTE(b) << 16) | ((UINT)RXBUF_BYTE(b+1) << 8) | RXBUF_BYTE(b+2))
#define RXBUF_UINT32(b)   (((uint32_t)RXBUF_BYTE(b) << 24) | ((uint32_t)RXBUF_BYTE(b+1) << 16)  | ((UINT)RXBUF_BYTE(b+2) << 8) | RXBUF_BYTE(b+3))
#define RXBUF_SINT32(b)   ((int32_t)RXBUF_UINT32(b))
#define RXBUF_NIBL(b)     (rxbuff[b] & 0x0f)
#define RXBUF_NIBH(b)     (rxbuff[b] >> 4)
#define RXBUF_NIB(n)      (((n)&1) ? RXBUF_NIBL((n)>>1) : RXBUF_NIBH((n)>>1))


#define POLLSTATE_SHUTDOWN   0
#define POLLSTATE_ALIVE	     1
#define POLLSTATE_READY	     2
#define POLLSTATE_CHARGING   3


class OvmsVehicleBMWi3 : public OvmsVehicle
  {
  public:
    OvmsVehicleBMWi3();
    ~OvmsVehicleBMWi3();
    void CanResponder(const CAN_frame_t* p_frame);
    void Ticker1(uint32_t ticker);
    void Ticker10(uint32_t ticker);



  protected:
    string bmwi3_obd_rxbuf;                               // CAN messages unpacked into here
    float hv_volts;                                       // Traction battery voltage - used to calculate power from current
    float soc = 0.0f;                                     // Remember SOC for derivative calcs
    int framecount = 0, tickercount = 0, replycount = 0;  // Keep track of when the car is talking or schtum.
    int eps_messages = 0;                                 // Is the EPS (power steering) alive?  If so we are "on"
    int pollerstate;                                      // What pollerstate we are in
    int last_obd_data_seen;                               // "monotonic" value last time we saw data

    // Local metrics
    // Wheel speeds
    OvmsMetricFloat *mt_i3_wheel1_speed;
    OvmsMetricFloat *mt_i3_wheel2_speed;
    OvmsMetricFloat *mt_i3_wheel3_speed;
    OvmsMetricFloat *mt_i3_wheel4_speed;
    OvmsMetricFloat *mt_i3_wheel_speed;   // "Combined" the car calls it.  Not sure how its combined
    // Charge limits
    OvmsMetricFloat *mt_i3_charge_actual;
    OvmsMetricFloat *mt_i3_charge_max;
    OvmsMetricFloat *mt_i3_charge_min;
    // Cell OCV (Open circuit voltage) stats
    OvmsMetricFloat *mt_i3_batt_pack_ocv_avg;
    OvmsMetricFloat *mt_i3_batt_pack_ocv_min;
    OvmsMetricFloat *mt_i3_batt_pack_ocv_max;
    // Ranges
    OvmsMetricInt *mt_i3_range_bc;
    OvmsMetricInt *mt_i3_range_comfort;
    OvmsMetricInt *mt_i3_range_ecopro;
    OvmsMetricInt *mt_i3_range_ecoproplus;
    // Charging
    OvmsMetricInt *mt_i3_v_charge_voltage_phase1;
    OvmsMetricInt *mt_i3_v_charge_voltage_phase2;
    OvmsMetricInt *mt_i3_v_charge_voltage_phase3;
    OvmsMetricFloat *mt_i3_v_charge_voltage_dc;
    OvmsMetricFloat *mt_i3_v_charge_voltage_dc_limit;
    OvmsMetricFloat *mt_i3_v_charge_current_phase1;
    OvmsMetricFloat *mt_i3_v_charge_current_phase2;
    OvmsMetricFloat *mt_i3_v_charge_current_phase3;
    OvmsMetricFloat *mt_i3_v_charge_current_dc;
    OvmsMetricFloat *mt_i3_v_charge_current_dc_limit;
    OvmsMetricFloat *mt_i3_v_charge_current_dc_maxlimit;
    OvmsMetricInt *mt_i3_v_charge_deratingreasons;
    OvmsMetricInt *mt_i3_v_charge_faults;
    OvmsMetricInt *mt_i3_v_charge_failsafetriggers;
    OvmsMetricInt *mt_i3_v_charge_interruptionreasons;
    OvmsMetricInt *mt_i3_v_charge_errors;
    OvmsMetricBool *mt_i3_v_charge_readytocharge;
    OvmsMetricString *mt_i3_v_charge_plugstatus;
    OvmsMetricInt *mt_i3_v_charge_pilotsignal;
    OvmsMetricInt *mt_i3_v_charge_cablecapacity;
    OvmsMetricBool *mt_i3_v_charge_dc_plugconnected;
    OvmsMetricInt *mt_i3_v_charge_chargeledstate;
    OvmsMetricInt *mt_i3_v_charge_dc_voltage;
    OvmsMetricInt *mt_i3_v_charge_dc_controlsignals;
    OvmsMetricBool *mt_i3_v_door_dc_chargeport;
    OvmsMetricBool *mt_i3_v_charge_dc_inprogress;
    OvmsMetricString *mt_i3_v_charge_dc_contactorstatus;
    OvmsMetricInt *mt_i3_v_charge_temp_gatedriver;
    // Trip consumption
    OvmsMetricInt *mt_i3_v_pos_tripconsumption;
    // Cabin aircon
    OvmsMetricBool *mt_i3_v_env_autorecirc;
    // State
    OvmsMetricBool *mt_i3_obdisalive;
    OvmsMetricInt *mt_i3_pollermode;
    OvmsMetricInt *mt_i3_age;

    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
  };

#endif //#ifndef __VEHICLE_BMWI3_H__
