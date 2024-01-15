/*
;    Project:       Open Vehicle Monitor System
;    Date:          23 November 2022
;
;    Duplicated from source code for BMW I3 vehicle.
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

#ifndef __VEHICLE_MINISE_H__
#define __VEHICLE_MINISE_H__

#include "vehicle.h"

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;

// RX buffer access macros: b=byte# 0..7 / n=nibble# 0..15
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

#define POLLSTATE_SHUTDOWN  0
#define POLLSTATE_ALIVE     1
#define POLLSTATE_READY     2
#define POLLSTATE_CHARGING  3

class OvmsVehicleMiniSE : public OvmsVehicle {
  public:
  OvmsVehicleMiniSE();
  ~OvmsVehicleMiniSE() override;

  protected:
  void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
  void Ticker1(uint32_t ticker) override;
  void Ticker60(uint32_t ticker) override;

  protected:
  string obd_rxbuf;                                     // CAN messages unpacked into here
  float hv_volts{};                                     // Traction battery voltage - used to calculate power from current
  float soc = 0.0f;                                     // Remember SOC for derivative calcs
  int framecount = 0, tickercount = 0, replycount = 0;  // Keep track of when the car is talking or schtum.
  int eps_messages = 0;                                 // Is the EPS (power steering) alive?  If so we are "on"
  int pollerstate;                                      // What pollerstate we are in
  int last_obd_data_seen;                               // "monotonic" value last time we saw data

  // Local metrics
  // Wheel speeds
  OvmsMetricFloat *mt_se_wheel1_speed;
  OvmsMetricFloat *mt_se_wheel2_speed;
  OvmsMetricFloat *mt_se_wheel3_speed;
  OvmsMetricFloat *mt_se_wheel4_speed;
  OvmsMetricFloat *mt_se_wheel_speed;   // "Combined" the car calls it.  Not sure how its combined
  // Charge limits
  OvmsMetricFloat *mt_se_charge_actual;
  OvmsMetricFloat *mt_se_charge_max;
  OvmsMetricFloat *mt_se_charge_min;
  // Cell OCV (Open circuit voltage) stats
  OvmsMetricFloat *mt_se_batt_pack_ocv_avg;
  OvmsMetricFloat *mt_se_batt_pack_ocv_min;
  OvmsMetricFloat *mt_se_batt_pack_ocv_max;
  // Ranges
  OvmsMetricInt *mt_se_range_bc;
  OvmsMetricInt *mt_se_range_comfort;
  OvmsMetricInt *mt_se_range_ecopro;
  OvmsMetricInt *mt_se_range_ecoproplus;
  // Charging
  OvmsMetricInt *mt_se_v_charge_voltage_phase1;
  OvmsMetricInt *mt_se_v_charge_voltage_phase2;
  OvmsMetricInt *mt_se_v_charge_voltage_phase3;
  OvmsMetricFloat *mt_se_v_charge_voltage_dc;
  OvmsMetricFloat *mt_se_v_charge_voltage_dc_limit;
  OvmsMetricFloat *mt_se_v_charge_current_phase1;
  OvmsMetricFloat *mt_se_v_charge_current_phase2;
  OvmsMetricFloat *mt_se_v_charge_current_phase3;
  OvmsMetricFloat *mt_se_v_charge_current_dc;
  OvmsMetricFloat *mt_se_v_charge_current_dc_limit;
  OvmsMetricFloat *mt_se_v_charge_current_dc_maxlimit;
  OvmsMetricInt *mt_se_v_charge_deratingreasons;
  OvmsMetricInt *mt_se_v_charge_faults;
  OvmsMetricInt *mt_se_v_charge_failsafetriggers;
  OvmsMetricInt *mt_se_v_charge_interruptionreasons;
  OvmsMetricInt *mt_se_v_charge_errors;
  OvmsMetricBool *mt_se_v_charge_readytocharge;
  OvmsMetricString *mt_se_v_charge_plugstatus;
  OvmsMetricInt *mt_se_v_charge_pilotsignal;
  OvmsMetricInt *mt_se_v_charge_cablecapacity;
  OvmsMetricBool *mt_se_v_charge_dc_plugconnected;
  OvmsMetricInt *mt_se_v_charge_chargeledstate;
  OvmsMetricInt *mt_se_v_charge_dc_voltage;
  OvmsMetricInt *mt_se_v_charge_dc_controlsignals;
  OvmsMetricBool *mt_se_v_door_dc_chargeport;
  OvmsMetricBool *mt_se_v_charge_dc_inprogress;
  OvmsMetricString *mt_se_v_charge_dc_contactorstatus;
  OvmsMetricInt *mt_se_v_charge_temp_gatedriver;
  // Trip consumption
  OvmsMetricInt *mt_se_v_pos_tripconsumption;
  // Cabin aircon
  OvmsMetricBool *mt_se_v_env_autorecirc;
  // State
  OvmsMetricBool *mt_se_obdisalive;
  OvmsMetricInt *mt_se_pollermode;
  OvmsMetricInt *mt_se_age;

  protected:
  void CanResponder(const CAN_frame_t *p_frame);
  static OvmsMetricFloat *MetricFloat(const char *name, uint16_t autostale, metric_unit_t units);
  static OvmsMetricInt *MetricInt(const char *name, uint16_t autostale, metric_unit_t units);
  static OvmsMetricBool *MetricBool(const char *name, uint16_t autostale, metric_unit_t units);
  static OvmsMetricString *MetricString(const char *name, uint16_t autostale, metric_unit_t units);
  static void HexDump(string &rxbuf, uint16_t type, uint16_t pid);
};

#endif // #ifndef __VEHICLE_MINISE_H__
