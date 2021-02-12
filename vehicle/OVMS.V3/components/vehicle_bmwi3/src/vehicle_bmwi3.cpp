/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Stephen Davies - BMWi3 vehicle type
;
;    BMW I3 vehicle support:
;    Developed by Stephen Davies <steve@telviva.co.za>
;
;    2020-12-12     0.0       Work started
;    2020-12-31     0.1       Workable version - most metrics supported, works with the ABRP plugin.  Open issues:
;                               1) Detection that the car is "on" (ready to drive) is still ropey
;                               2) Don't have individual cell data
;                               3) Read-only - no sending of commands.
;                               4) Detection of DC charging is only slightly tested
;                               5) Would like to find a way to stop the OBD gateway going to sleep on us, eg when charging.
;    2020-01-04               Ready to merge as a first version
;
;
;    Note that if you leave the OVMS box connected to the OBD-II port and lock the car then the alarm will go off.
;       This can be coded off using eg Bimmercode; I'll attempt to add a command to do that to this module at some point.
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

#include "ovms_log.h"
static const char *TAG = "v-bmwi3";

#include <stdio.h>
#include "vehicle_bmwi3.h"

#include "../ecu_definitions/ecu_sme_defines.h"
#include "../ecu_definitions/ecu_kom_defines.h"
#include "../ecu_definitions/ecu_eme_defines.h"
#include "../ecu_definitions/ecu_nbt_defines.h"
#include "../ecu_definitions/ecu_kle_defines.h"
#include "../ecu_definitions/ecu_dsc_defines.h"
#include "../ecu_definitions/ecu_bdc_defines.h"
#include "../ecu_definitions/ecu_bdc_extra_defines.h"
#include "../ecu_definitions/ecu_fzd_defines.h"
#include "../ecu_definitions/ecu_eps_defines.h"
#include "../ecu_definitions/ecu_edm_defines.h"
#include "../ecu_definitions/ecu_edm_extra_defines.h"
#include "../ecu_definitions/ecu_lim_defines.h"
#include "../ecu_definitions/ecu_lim_extra_defines.h"
#include "../ecu_definitions/ecu_ihx_defines.h"



// About the pollstates:
//    POLLSTATE_SHUTDOWN (0)      : We're not receiving anything on the OBD, the car is "off" and has shutdown the D-CAN
//                                    in this mode we are also silent and don't poll.
//    POLLSTATE_ALIVE    (1)      : We are getting frames, car is alive but not "on" as in ready to drive
//    POLLSTATE_READY    (2)      : Car is "READY" to drive or is actually being driven
//    POLLSTATE_CHARGING (3)      : Battery is being charged.
//
// Might be useful to have more states or maybe to treat as bitmap so that we can distinguish
// "connected to power and could charge" and "actually charging right now"


// https://www.bimmerfest.com/threads/can-bus-wake-up-via-obd-connector.789159/
// Hi !
// Do anybody know how to wake-up the CAN-Bus via OBD connector?
// Current situation: The car is locked and in sleep modus - CAN-Bus OFF. Now I want to wake-up the CAN-Bus and sent messages via OBD connector e.g. read vin, ...
// I heared about a methode which should work on Audi cars with gateway:
// In sleep mode PIN 14 (CAN low) should have 11V. Now pull the PIN 14 down (switch to ground) and the wake-up of the CAN-Bus is done.
// Can anybody confirm this on e.g. F10?
// Any other ideas?
// Answers highly wellcome! :)



static const OvmsVehicle::poll_pid_t obdii_polls[] = {
  // TXMODULEID, RXMODULEID, TYPE, PID, { POLLTIMES }, BUS, ADDRESSING
  // SME: Battery management electronics
    { I3_ECU_SME_TX, I3_ECU_SME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_SME_ALTERUNG_KAPAZITAET_TS,                     {  0, 60, 60, 60 }, 0, ISOTP_EXTADR },   // 0x6335 v_bat_soh, v_bat_health
    { I3_ECU_SME_TX, I3_ECU_SME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_SME_HV_SPANNUNG_BERECHNET,                      {  0,  2,  1,  2 }, 0, ISOTP_EXTADR },   // 0xDD68 v_bat_volts
    { I3_ECU_SME_TX, I3_ECU_SME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_SME_HV_STROM,                                   {  0,  2,  1,  2 }, 0, ISOTP_EXTADR },   // 0xDD69 v_bat_current - and v_bat_power by *v_bat_volts
    { I3_ECU_SME_TX, I3_ECU_SME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_SME_ANZEIGE_SOC,                                {  0, 30, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDDBC v_bat_soc, mt_i3_charge_max, mt_i3_charge_min
    { I3_ECU_SME_TX, I3_ECU_SME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_SME_ZUSTAND_SPEICHER,                           {  0,120, 60, 60 }, 0, ISOTP_EXTADR },   // 0xDFA0 v_bat_cac, v_bat_pack_vmax, _vmin, _vavg, v_bat_pack_level_max, _min, avg, mt_i3_batt_pack_ocv_avg, _max, _min
    { I3_ECU_SME_TX, I3_ECU_SME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_SME_TEMPERATUREN,                               {  0, 30, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDDC0 v_bat_pack_temp, _tmin, _tmax and _tavg
#ifdef INVESTIGATIONS
    { I3_ECU_SME_TX, I3_ECU_SME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_SME_PROJEKT_PARAMETER,                          {  0,120,120,120 }, 0, ISOTP_EXTADR },   // 0xDF71 Battery pack info - but we don't use
    { I3_ECU_SME_TX, I3_ECU_SME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_SME_ZELLSPANNUNGEN_MIN_MAX,                     {  0, 30, 30, 30 }, 0, ISOTP_EXTADR },   // 0xDDBF
#endif

  // KOM: (Combo): Instrument panel
    { I3_ECU_KOM_TX, I3_ECU_KOM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KOM_TACHO_WERT,                                 {  0, 10,  1, 10 }, 0, ISOTP_EXTADR },   // 0xD107 v_pos_speed
    { I3_ECU_KOM_TX, I3_ECU_KOM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KOM_GWSZ_ABSOLUT_WERT,                          {  0, 60, 10, 60 }, 0, ISOTP_EXTADR },   // 0xD10D v_pos_odometer
    { I3_ECU_KOM_TX, I3_ECU_KOM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KOM_KOMBI_REICHWEITE_BEV_PHEV,                  {  0,  5,  5,  5 }, 0, ISOTP_EXTADR },   // 0xD111 v_bat_range_est, _ideal, _full
    { I3_ECU_KOM_TX, I3_ECU_KOM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KOM_A_TEMP_WERT,                                {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xD112 v_env_temp
    { I3_ECU_KOM_TX, I3_ECU_KOM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KOM_SEGMENTDATEN_SPEICHER,                      {  0,120,120,120 }, 0, ISOTP_EXTADR },   // 0xD12F edrive stuff I think --> FIXME See what this gives us.
    { I3_ECU_KOM_TX, I3_ECU_KOM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KOM_REICHWEITE_MCV,                             {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0x420C mt_i3_range_bc, _comfort, _ecopro, _ecoproplus
    { I3_ECU_KOM_TX, I3_ECU_KOM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KOM_KOMBI_BC_BCW_KWH_KM,                        {  0, 10, 10, 60 }, 0, ISOTP_EXTADR },   // 0xD129 Averages - but we don't use
    { I3_ECU_KOM_TX, I3_ECU_KOM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KOM_KOMBI_BC_RBC_KWH_KM,                        {  0, 10,  2, 60 }, 0, ISOTP_EXTADR },   // 0xD12A v_pos_trip, v_post_tripconsumption, v_bat_coulomb_used, v_bat_energy_used
  
  // EME:
    { I3_ECU_EME_TX, I3_ECU_EME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EME_EME_HVPM_DCDC_ANSTEUERUNG,                {  0, 30, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDE00 mt_i3_charge_actual
    { I3_ECU_EME_TX, I3_ECU_EME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EME_AE_STROM_EMASCHINE,                       {  0, 30,  2, 30 }, 0, ISOTP_EXTADR },   // 0xDE8A Motor currents - can we calculate an efficiency from this?
    { I3_ECU_EME_TX, I3_ECU_EME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EME_AE_TEMP_LE,                               {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDE8C v_inv_temp, v_charge_temp, mt_i3_v_charge_temp_gatedriver
    { I3_ECU_EME_TX, I3_ECU_EME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EME_AE_TEMP_EMASCHINE,                        {  0, 10,  5, 30 }, 0, ISOTP_EXTADR },   // 0xDEA6 v_mot_temp
    { I3_ECU_EME_TX, I3_ECU_EME_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EME_AE_ELEKTRISCHE_MASCHINE,                  {  0, 60,  1, 60 }, 0, ISOTP_EXTADR },   // 0xDEA7 v_mot_rpm

  //NBT: Headunit high
    { I3_ECU_NBT_TX, I3_ECU_NBT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_NBT_STATUS_SPEED,                             {  0, 30,  2, 30 }, 0, ISOTP_EXTADR },   // 0xD030 Speeds from wheels: mt_i3_wheelX_speed 
    { I3_ECU_NBT_TX, I3_ECU_NBT_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_NBT_STATUS_DIRECTION,                         {  0, 10,  4, 10 }, 0, ISOTP_EXTADR },   // 0xD031 v_env_gear

  // FZD: Roof function centre
    { I3_ECU_FZD_TX, I3_ECU_FZD_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_FZD_DWA_KLAPPENKONTAKTE,                      {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDCDD v_door_fr, _fl. etc.  v_env_locked

  //KLE: Convenience charging electronics
    { I3_ECU_KLE_TX, I3_ECU_KLE_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KLE_BETRIEBSZUSTAND_LADEGERAET,               {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDE84 v_charge_state, c_charge_inprogress, mt_i3_v_charge_dc_inprogress, v_charge_state, etc
    { I3_ECU_KLE_TX, I3_ECU_KLE_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KLE_LADEGERAET_LEISTUNG,                      {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDE85 v_charge_efficiency
    { I3_ECU_KLE_TX, I3_ECU_KLE_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KLE_LADEGERAET_SPANNUNG,                      {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDE86 v_charge_voltage, mt_i3_v_charge_voltage_phaseX, mt_i3_v_charge_voltage_dc etc
    { I3_ECU_KLE_TX, I3_ECU_KLE_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_KLE_LADEGERAET_STROM,                         {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDE87 v_charge_currents in similar way

  // EDM: Electrical digital motor electronics (low voltage ECU)
    { I3_ECU_EDM_TX, I3_ECU_EDM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EDM_PEDALWERTGEBER,                           {  0,  5,  1, 60 }, 0, ISOTP_EXTADR },   // 0xDE9C v_env_throttle
    { I3_ECU_EDM_TX, I3_ECU_EDM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EDM_STATUS_MESSWERTE_IBS,                     {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0x402B v_bat_12v_current, v_env_charging12v

  // BDC: Body domain controller
    { I3_ECU_BDC_TX, I3_ECU_BDC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_BDC_HANDBREMSE_KONTAKT,                       {  0, 60, 60, 60 }, 0, ISOTP_EXTADR },   // 0xD130 v_env_handbrake, but doesn't seem to work
    { I3_ECU_BDC_TX, I3_ECU_BDC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_BDC_VIN,                                      {  0,300,300,300 }, 0, ISOTP_EXTADR },   // 0xF190 v_vin

  // LIM: Charging interface module
    { I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_LADEBEREITSCHAFT_LIM,                     {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDEF2 mt_i3_v_charge_readytocharge
    { I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_PROXIMITY,                                {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDEF5 v_charge_plugstatus
    { I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_PILOTSIGNAL,                              {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDEF6 v_charge_pilot, mt_i3_v_charge_pilotsignal
    { I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_LADESCHNITTSTELLE_DC_TEPCO,               {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDEF7  mt_i3_v_charge_dc_plugconnected, mt_i3_v_charge_dc_controlsignals (but don't seem to work)
    { I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_DC_SCHUETZ_SCHALTER,                      {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDEF8 mt_i3_v_charge_dc_contactorstatus
    { I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_DC_PINABDECKUNG_COMBO,                    {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDEFA mt_i3_v_door_dc_chargeport- (but doesn't seem to work)
    { I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_STATUS_LADEKLAPPE,                        {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDEF1 v_door_chargeport
    { I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_LED_LADESTATUS,                           {  0, 10, 10, 10 }, 0, ISOTP_EXTADR },   // 0xDEF3 mt_i3_v_charge_chargeledstate

  // IHX: Integrated automatic heating/aircon
    { I3_ECU_IHX_TX, I3_ECU_IHX_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_IHX_TEMP_INNEN_UNBELUEFTET,                   {  0, 11, 11, 11 }, 0, ISOTP_EXTADR },   // 0xD85C v_env_cabintemp
    { I3_ECU_IHX_TX, I3_ECU_IHX_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_IHX_EKMV_BETRIEBSZUSTAND_GEN20,               {  0, 11, 11, 11 }, 0, ISOTP_EXTADR },   // 0xD8C5 v_env_cooling
    { I3_ECU_IHX_TX, I3_ECU_IHX_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_IHX_KLIMA_VORN_LUFTVERTEILUNG_LI_RE,          {  0, 11, 11, 11 }, 0, ISOTP_EXTADR },   // 0xD91A v_env_cabinvent
    { I3_ECU_IHX_TX, I3_ECU_IHX_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_IHX_KLIMA_VORN_OFF_EIN,                       {  0, 11, 11, 11 }, 0, ISOTP_EXTADR },   // 0xD92C v_env_hvac
    { I3_ECU_IHX_TX, I3_ECU_IHX_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_IHX_KLIMA_VORN_PRG_AUC_EIN,                   {  0, 11, 11, 11 }, 0, ISOTP_EXTADR },   // 0xD930 mt_i3_v_env_autorecirc
    { I3_ECU_IHX_TX, I3_ECU_IHX_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_IHX_KLIMA_VORN_PRG_UMLUFT_EIN,                {  0, 11, 11, 11 }, 0, ISOTP_EXTADR },   // 0xD931 v_env_cabinintake

  // EPS:  Power steering (We only use this to tell if the car is ready since the EPS is only on when the car is READY)
    { I3_ECU_EPS_TX, I3_ECU_EPS_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EPS_EPS_MOMENTENSENSOR,                       {  0,  5,  5,  5 }, 0, ISOTP_EXTADR },   // 0xDB99
  
    POLL_LIST_END
  };

OvmsMetricFloat* MetricFloat(const char* name, uint16_t autostale=0, metric_unit_t units = Other) {
    OvmsMetricFloat* metric = (OvmsMetricFloat*)MyMetrics.Find(name);
    if (metric==NULL) {
        metric = new OvmsMetricFloat(name, autostale, units, 0);
    }
    return metric;
}
OvmsMetricInt* MetricInt(const char* name, uint16_t autostale=0, metric_unit_t units = Other) {
    OvmsMetricInt* metric = (OvmsMetricInt*)MyMetrics.Find(name);
    if (metric==NULL) {
        metric = new OvmsMetricInt(name, autostale, units, 0);
    }
    return metric;
}
  
OvmsMetricBool* MetricBool(const char* name, uint16_t autostale=0, metric_unit_t units = Other) {
    OvmsMetricBool* metric = (OvmsMetricBool*)MyMetrics.Find(name);
    if (metric==NULL) {
        metric = new OvmsMetricBool(name, autostale, units, 0);
    }
    return metric;
}

OvmsMetricString* MetricString(const char* name, uint16_t autostale=0, metric_unit_t units = Other) {
    OvmsMetricString* metric = (OvmsMetricString*)MyMetrics.Find(name);
    if (metric==NULL) {
        metric = new OvmsMetricString(name, autostale, units);
    }
    return metric;
}

OvmsVehicleBMWi3::OvmsVehicleBMWi3()
{
    ESP_LOGI(TAG, "BMW i3/i3s vehicle module");

    // Our metrics.
    // Charge limits
    mt_i3_charge_actual                 = MetricFloat("xi3.v.b.soc.actual",         SM_STALE_MAX,  Percentage);
    mt_i3_charge_max                    = MetricFloat("xi3.v.b.soc.actual.highlimit", SM_STALE_MAX, Percentage);
    mt_i3_charge_min                    = MetricFloat("xi3.v.b.soc.actual.lowlimit", SM_STALE_MAX, Percentage);
    // Wheel speeds
    mt_i3_wheel1_speed                  = MetricFloat("xi3.v.p.wheel1_speed",       SM_STALE_MIN,  Kph);
    mt_i3_wheel2_speed                  = MetricFloat("xi3.v.p.wheel2_speed",       SM_STALE_MIN,  Kph);
    mt_i3_wheel3_speed                  = MetricFloat("xi3.v.p.wheel3_speed",       SM_STALE_MIN,  Kph);
    mt_i3_wheel4_speed                  = MetricFloat("xi3.v.p.wheel4_speed",       SM_STALE_MIN,  Kph);
    mt_i3_wheel_speed                   = MetricFloat("xi3.v.p.wheel_speed",        SM_STALE_MIN,  Kph);
    mt_i3_batt_pack_ocv_avg             = MetricFloat("xi3.v.b.p.ocv.avg",          SM_STALE_MAX,  Volts);
    mt_i3_batt_pack_ocv_min             = MetricFloat("xi3.v.b.p.ocv.min",          SM_STALE_MAX,  Volts);
    mt_i3_batt_pack_ocv_max             = MetricFloat("xi3.v.b.p.ocv.max",          SM_STALE_MAX,  Volts);
    // Ranges in modes
    mt_i3_range_bc                      = MetricInt  ("xi3.v.b.range.bc",           SM_STALE_HIGH, Kilometers);
    mt_i3_range_comfort                 = MetricInt  ("xi3.v.b.range.comfort",      SM_STALE_HIGH, Kilometers);
    mt_i3_range_ecopro                  = MetricInt  ("xi3.v.b.range.ecopro",       SM_STALE_HIGH, Kilometers);
    mt_i3_range_ecoproplus              = MetricInt  ("xi3.v.b.range.ecoproplus",   SM_STALE_HIGH, Kilometers);
    // Charging
    mt_i3_v_charge_voltage_phase1       = MetricInt  ("xi3.v.c.voltage.phase1",     SM_STALE_MID,  Volts);
    mt_i3_v_charge_voltage_phase2       = MetricInt  ("xi3.v.c.voltage.phase2",     SM_STALE_MID,  Volts);
    mt_i3_v_charge_voltage_phase3       = MetricInt  ("xi3.v.c.voltage.phase3",     SM_STALE_MID,  Volts);
    mt_i3_v_charge_voltage_dc           = MetricFloat("xi3.v.c.voltage.dc",         SM_STALE_MID,  Volts);
    mt_i3_v_charge_voltage_dc_limit     = MetricFloat("xi3.v.c.voltage.dc.limit",   SM_STALE_MID,  Volts);
    mt_i3_v_charge_current_phase1       = MetricFloat("xi3.v.c.current.phase1",     SM_STALE_MID,  Amps);
    mt_i3_v_charge_current_phase2       = MetricFloat("xi3.v.c.current.phase2",     SM_STALE_MID,  Amps);
    mt_i3_v_charge_current_phase3       = MetricFloat("xi3.v.c.current.phase3",     SM_STALE_MID,  Amps);
    mt_i3_v_charge_current_dc           = MetricFloat("xi3.v.c.current.dc",         SM_STALE_MID,  Amps);
    mt_i3_v_charge_current_dc_limit     = MetricFloat("xi3.v.c.current.dc.limit",   SM_STALE_MID,  Amps);
    mt_i3_v_charge_current_dc_maxlimit  = MetricFloat("xi3.v.c.current.dc.maxlimit", SM_STALE_MID, Amps);
    mt_i3_v_charge_deratingreasons      = MetricInt  ("xi3.v.c.deratingreasons",    SM_STALE_HIGH, Other);
    mt_i3_v_charge_faults               = MetricInt  ("xi3.v.c.deratingreasons",    SM_STALE_HIGH, Other);
    mt_i3_v_charge_failsafetriggers     = MetricInt  ("xi3.v.c.failsafetriggers",   SM_STALE_HIGH, Other);
    mt_i3_v_charge_interruptionreasons  = MetricInt  ("xi3.v.c.interruptionreasons", SM_STALE_HIGH, Other);
    mt_i3_v_charge_errors               = MetricInt  ("xi3.v.c.error",              SM_STALE_HIGH, Other);
    mt_i3_v_charge_readytocharge        = MetricBool ("xi3.v.c.readytocharge",      SM_STALE_MID);
    mt_i3_v_charge_plugstatus           = MetricString("xi3.v.c.chargeplugstatus",  SM_STALE_MID);
    mt_i3_v_charge_pilotsignal          = MetricInt  ("xi3.v.c.pilotsignal",        SM_STALE_MID,  Amps);
    mt_i3_v_charge_cablecapacity        = MetricInt  ("xi3.v.c.chargecablecapacity", SM_STALE_MID, Amps);
    mt_i3_v_charge_dc_plugconnected     = MetricBool ("xi3.v.c.dc.plugconnected",   SM_STALE_MID);
    mt_i3_v_charge_dc_voltage           = MetricInt  ("xi3.v.c.dc.chargevoltage",   SM_STALE_MID,  Volts);
    mt_i3_v_charge_dc_controlsignals    = MetricInt  ("xi3.v.c.dc.controlsignals",  SM_STALE_MID,  Other);
    mt_i3_v_door_dc_chargeport          = MetricBool ("xi3.v.d.chargeport.dc",      SM_STALE_MID);
    mt_i3_v_charge_dc_contactorstatus   = MetricString("xi3.v.c.dc.contactorstatus", SM_STALE_MID);
    mt_i3_v_charge_dc_inprogress        = MetricBool ("xi3.v.c.dc.inprogress",      SM_STALE_MID);
    mt_i3_v_charge_chargeledstate       = MetricInt  ("xi3.v.c.chargeledstate",     SM_STALE_MID,  Other);
    mt_i3_v_charge_temp_gatedriver      = MetricInt  ("xi3.v.c.temp.gatedriver",    SM_STALE_MID,  Celcius);
    // Trip consumption
    mt_i3_v_pos_tripconsumption         = MetricInt  ("xi3.v.p.tripconsumption",    SM_STALE_MID,  WattHoursPK);

    // State
    mt_i3_obdisalive                    = MetricBool ("xi3.v.e.obdisalive",         SM_STALE_MID);
    mt_i3_pollermode                    = MetricInt  ("xi3.s.pollermode",           SM_STALE_MID);
    mt_i3_age                           = MetricInt  ("xi3.s.age",                  SM_STALE_MID,  Minutes);

    // Controls
    mt_i3_v_env_autorecirc              = MetricBool("xi3.v.e.autorecirc",          SM_STALE_MID);
 
    // Init the stuff to keep track of whether the car is talking or not
    framecount = 0;
    tickercount = 0;
    replycount = 0;
    last_obd_data_seen = 0;

    // Callbacks
    MyCan.RegisterCallback(TAG, std::bind(&OvmsVehicleBMWi3::CanResponder, this, std::placeholders::_1));
 
    // Get the Canbus setup
    RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
    PollSetPidList(m_can1, obdii_polls);
    pollerstate = POLLSTATE_SHUTDOWN;  // If the car is alive we'll get frames and switch to ALIVE
    PollSetState(pollerstate);
    mt_i3_pollermode->SetValue(pollerstate);
    mt_i3_obdisalive->SetValue(false);
    StdMetrics.ms_v_env_awake->SetValue(false);
    StdMetrics.ms_v_env_on->SetValue(false);
    PollSetThrottling(50);
    PollSetResponseSeparationTime(5);
}

OvmsVehicleBMWi3::~OvmsVehicleBMWi3()
{
    ESP_LOGI(TAG, "Shutdown BMW i3/i3s vehicle module");
}

void hexdump(string &rxbuf, uint16_t type, uint16_t pid)
{
    char *buf = NULL;
    size_t rlen = rxbuf.size(), offset = 0;
    do {
        rlen = FormatHexDump(&buf, rxbuf.data() + offset, rlen, 16);
        offset += 16;
        ESP_LOGW(TAG, "BMWI3: unhandled reply [%02x %02x]: %s", type, pid, buf ? buf : "-");
    } while (rlen);
    if (buf) {
        free(buf);
    }
}

/**
 * Synchronous CAN RX responder
 */

void OvmsVehicleBMWi3::CanResponder(const CAN_frame_t* p_frame)
{
    // We just count the arriving frames.  The ticker uses this to see if anything is arriving ie in the OBD actually alive?
    ++framecount;
}

void OvmsVehicleBMWi3::Ticker1(uint32_t ticker)
{
    ++tickercount;
    //ESP_LOGD(TAG, "Tick! tickercount=%d framecount=%d replycount=%d pollerstate=%d", tickercount, framecount, replycount, pollerstate);
    if (framecount > 0) {
        // There is life.
        if (pollerstate == POLLSTATE_SHUTDOWN) {
            // we previously shut down.  Wake up again since the car seems to be back
            pollerstate = POLLSTATE_ALIVE;
            mt_i3_pollermode->SetValue(pollerstate);
            PollSetState(pollerstate);
            ESP_LOGI(TAG, "Woke up polling since we saw CANBUS traffic from the car");
        }
        framecount = 0;
        tickercount = 0;
    }
    if (tickercount >= 3 && pollerstate != POLLSTATE_SHUTDOWN) {
        // No life for 3 seconds - stop polling
        ESP_LOGW(TAG, "No OBD traffic from car for 3 seconds, shutting down poller");
        pollerstate = POLLSTATE_SHUTDOWN;
        mt_i3_pollermode->SetValue(pollerstate);
        StdMetrics.ms_v_env_awake->SetValue(false);
        StdMetrics.ms_v_env_on->SetValue(false);
        PollSetState(pollerstate);
    }
}

void OvmsVehicleBMWi3::Ticker10(uint32_t ticker)
{
    // 1) Is the car responsive - ie replying to our polls?
    mt_i3_obdisalive->SetValue( (replycount != 0) );
    if (replycount == 0 && pollerstate != POLLSTATE_SHUTDOWN) {
        ESP_LOGI(TAG, "No replies from the car to our polls - OBD is not alive");
    }
    replycount = 0;

    // 2) Is the car 'on' (READY)?  We check if we are hearing from the EPS - which seems to only be powered when the car is READY (more or less).
    // FIXME: Would be nice to find a better way to do this
    if (eps_messages != 0) {
        StdMetrics.ms_v_env_awake->SetValue(true);
        if (pollerstate != POLLSTATE_READY) {
            ESP_LOGI(TAG, "Car is now ON");
            pollerstate = POLLSTATE_READY;
            mt_i3_pollermode->SetValue(pollerstate);
            PollSetState(pollerstate);
        }
        eps_messages = 0;
    } else {
        StdMetrics.ms_v_env_awake->SetValue(false);
        if (pollerstate == POLLSTATE_READY) {
            pollerstate = POLLSTATE_ALIVE;
            mt_i3_pollermode->SetValue(pollerstate);
            PollSetState(pollerstate);
            ESP_LOGI(TAG, "Car is now OFF");
        }
    }

    //3) Are we actually DRIVING?  - we take it that if the car is awake and in gear
    StdMetrics.ms_v_env_on->SetValue( (pollerstate == POLLSTATE_READY && StdMetrics.ms_v_env_gear->AsInt() != 0) );

    // 4) i3 always has regen braking on
    StdMetrics.ms_v_env_regenbrake->SetValue(true);

    // 5) mt_i3_age
    if (last_obd_data_seen) {
        mt_i3_age->SetValue(StdMetrics.ms_m_monotonic->AsInt() - last_obd_data_seen, Seconds);
    }
}

void OvmsVehicleBMWi3::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
    string& rxbuf = bmwi3_obd_rxbuf;
  
    // Assemble first and following frames to get complete reply
    
    // init rx buffer on first (it tells us whole length)
    if (m_poll_ml_frame == 0) {
      rxbuf.clear();
      rxbuf.reserve(length + mlremain);
    }
    // Append each piece
    rxbuf.append((char*)data, length);
    if (mlremain) {
        // we need more - return for now.
        return;
    }

  ++replycount;

  int datalen = rxbuf.size();
  
  // We now have received the whole reply - lets mine our nuggets!
  // FIXME: we need to check where we received it from in case PIDs clash?
  // FIXME: Seems OK for now since there aren't clashes - compiler complains if there are.

  last_obd_data_seen = StdMetrics.ms_m_monotonic->AsInt();

  switch (pid) {

  // --- SME --------------------------------------------------------------------------------------------------------

  case I3_PID_SME_ALTERUNG_KAPAZITAET_TS: {
    if (datalen < 4) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", length, "I3_PID_SME_ALTERUNG_KAPAZITAET_TS", 4);
        break;
    }
    unsigned long STAT_ALTERUNG_KAPAZITAET_WERT = (RXBUF_UINT32(0));
        // Remaining capacity of the memory / Restkapazitaet des Speichers
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lu%s\n", "SME", "ALTERUNG_KAPAZITAET_TS", "STAT_ALTERUNG_KAPAZITAET_WERT", STAT_ALTERUNG_KAPAZITAET_WERT, "\"%\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_bat_soh->SetValue(STAT_ALTERUNG_KAPAZITAET_WERT, Percentage);
    StdMetrics.ms_v_bat_health->SetValue(   (STAT_ALTERUNG_KAPAZITAET_WERT >= 95.0f) ?  "Excellent" :
                                            (STAT_ALTERUNG_KAPAZITAET_WERT >= 90.0f) ?  "Good" :
                                            (STAT_ALTERUNG_KAPAZITAET_WERT >= 80.0f) ?  "OK" :
                                            (STAT_ALTERUNG_KAPAZITAET_WERT >= 70.0f) ?  "Poor" :
                                                                                        "Service");

    break;
  }


  case I3_PID_SME_HV_SPANNUNG_BERECHNET: {
    if (datalen < 2) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", length, "I3_PID_SME_HV_SPANNUNG_BERECHNET", 2);
        break;
    }
    float STAT_HV_SPANNUNG_BERECHNET_WERT = (RXBUF_UINT(0)/100.0f);
        // Battery voltage behind the contactors, regardless of the contactor status / Batteriespannung hinter den
        // Schﾃｼtzen, unabhﾃ､ngig vom Schﾃｼtzzustand
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "HV_SPANNUNG_BERECHNET", "STAT_HV_SPANNUNG_BERECHNET_WERT", STAT_HV_SPANNUNG_BERECHNET_WERT, "\"V\"");

    // ==========  Add your processing here ==========
    hv_volts = STAT_HV_SPANNUNG_BERECHNET_WERT;
    StdMetrics.ms_v_bat_voltage->SetValue(hv_volts, Volts);

    break;
  }

  case I3_PID_SME_HV_STROM: {
    if (datalen < 4) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", length, "I3_PID_SME_HV_STROM", 4);
        break;
    }
    float STAT_HV_STROM_WERT = (RXBUF_SINT32(0)/100.0f);
        // HV current in A / HV-Strom in A
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "HV_STROM", "STAT_HV_STROM_WERT", STAT_HV_STROM_WERT, "\"A\"");

    // ==========  Add your processing here ==========
    STAT_HV_STROM_WERT = -STAT_HV_STROM_WERT;
    if (STAT_HV_STROM_WERT > -0.01f && STAT_HV_STROM_WERT < 0.01f) {
        STAT_HV_STROM_WERT = 0.0f;
    }
    StdMetrics.ms_v_bat_current->SetValue(STAT_HV_STROM_WERT, Amps);
    // Calculate Momentary power in kW:
    StdMetrics.ms_v_bat_power->SetValue( roundf((STAT_HV_STROM_WERT) * hv_volts) / 1000.0f, kW);

    break;
  }

  case I3_PID_SME_ANZEIGE_SOC: {
    if (datalen < 6) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", length, "I3_PID_SME_ANZEIGE_SOC", 6);
        break;
    }
    float STAT_ANZEIGE_SOC_WERT = (RXBUF_UINT(0)/10.0f);
        // current advertisement Soc / aktueller Anzeige Soc
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ANZEIGE_SOC", "STAT_ANZEIGE_SOC_WERT", STAT_ANZEIGE_SOC_WERT, "\"%\"");
    float STAT_MAXIMALE_ANZEIGE_SOC_WERT = (RXBUF_UINT(2)/10.0f);
        // upper limit of the display Soc / obere Grenze des Anzeige Soc
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ANZEIGE_SOC", "STAT_MAXIMALE_ANZEIGE_SOC_WERT", STAT_MAXIMALE_ANZEIGE_SOC_WERT, "\"%\"");
    float STAT_MINIMALE_ANZEIGE_SOC_WERT = (RXBUF_UINT(4)/10.0f);
        // lower limit of the display Soc / untere Grenze des Anzeige Soc
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ANZEIGE_SOC", "STAT_MINIMALE_ANZEIGE_SOC_WERT", STAT_MINIMALE_ANZEIGE_SOC_WERT, "\"%\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_bat_soc->SetValue(STAT_ANZEIGE_SOC_WERT, Percentage);
    soc = STAT_ANZEIGE_SOC_WERT / 100.0f;
    mt_i3_charge_max->SetValue(STAT_MAXIMALE_ANZEIGE_SOC_WERT, Percentage);
    mt_i3_charge_min->SetValue(STAT_MINIMALE_ANZEIGE_SOC_WERT, Percentage);

    break;
  }

  case I3_PID_SME_TEMPERATUREN: {
    if (datalen < 6) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_SME_TEMPERATUREN", 6);
        break;
    }
    float STAT_TCORE_MIN_WERT = (RXBUF_SINT(0)/100.0f);
        // Output of the calculated minimum cell core temperatures / Ausgabe der berechneten minimalen
        // Zellkerntemperaturen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "TEMPERATUREN", "STAT_TCORE_MIN_WERT", STAT_TCORE_MIN_WERT, "\"°C\"");
    float STAT_TCORE_MAX_WERT = (RXBUF_SINT(2)/100.0f);
        // Output of the calculated maximum cell core temperatures / Ausgabe der berechneten maximalen
        // Zellkerntemperaturen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "TEMPERATUREN", "STAT_TCORE_MAX_WERT", STAT_TCORE_MAX_WERT, "\"°C\"");
    float STAT_TCORE_MEAN_WERT = (RXBUF_SINT(4)/100.0f);
        // Output of the calculated average cell core temperatures / Ausgabe der berechneten durchschnittlichen
        // Zellkerntemperaturen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "TEMPERATUREN", "STAT_TCORE_MEAN_WERT", STAT_TCORE_MEAN_WERT, "\"°C\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_bat_pack_tmin->SetValue(STAT_TCORE_MIN_WERT, Celcius);
    StdMetrics.ms_v_bat_pack_tmax->SetValue(STAT_TCORE_MAX_WERT, Celcius);
    StdMetrics.ms_v_bat_pack_tavg->SetValue(STAT_TCORE_MEAN_WERT, Celcius);
    StdMetrics.ms_v_bat_temp->SetValue(STAT_TCORE_MEAN_WERT, Celcius);

    break;
  }


    case I3_PID_SME_ZUSTAND_SPEICHER: {                                             // 0xDFA0
    if (datalen < 38) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_SME_ZUSTAND_SPEICHER", 38);
        break;
    }

// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLKAPAZITAET_MIN_WERT=120.4100"Ah"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLKAPAZITAET_MAX_WERT=122.1400"Ah"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLKAPAZITAET_MEAN_WERT=121.2400"Ah"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLSPANNUNG_MIN_WERT=3.9219"V"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLSPANNUNG_MAX_WERT=3.9250"V"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLSPANNUNG_MEAN_WERT=3.9230"V"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLTEMPERATUR_MIN_WERT=21.0100"°C"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLTEMPERATUR_MAX_WERT=22.0100"°C"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLTEMPERATUR_MEAN_WERT=21.8100"°C"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLWIDERSTANDSFAKTOR_MIN_WERT=6.5535
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLWIDERSTANDSFAKTOR_MAX_WERT=6.5535
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLWIDERSTANDSFAKTOR_MEAN_WERT=1.0655
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLSOC_MIN_WERT=73.4700"%"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLSOC_MAX_WERT=74.6100"%"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLSOC_MEAN_WERT=74.0200"%"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLOCV_MIN_WERT=3.9220"V"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLOCV_MAX_WERT=3.9250"V"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_ZELLOCV_MEAN_WERT=3.9235"V"
// 2020-12-19 12:54:28 SAST D (686338) v-bmwi3: From ECU SME, pid ZUSTAND_SPEICHER: got STAT_IMPEDANCE_ESTIMATION_ALPHA_WERT=1.0655

    float STAT_ZELLKAPAZITAET_MIN_WERT = (RXBUF_UINT(0)/100.0f);
        // Output of the current minimum measured cell capacity of all cells in Ah / Ausgabe der aktuellen minimalen
        // gemessenen Zellkapazität aller Zellen in Ah
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLKAPAZITAET_MIN_WERT", STAT_ZELLKAPAZITAET_MIN_WERT, "\"Ah\"");

    float STAT_ZELLKAPAZITAET_MAX_WERT = (RXBUF_UINT(2)/100.0f);
        // Output of the current maximum measured cell capacity of all cells in Ah / Ausgabe der aktuellen maximalen
        // gemessenen Zellkapazität aller Zellen in Ah
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLKAPAZITAET_MAX_WERT", STAT_ZELLKAPAZITAET_MAX_WERT, "\"Ah\"");

    float STAT_ZELLKAPAZITAET_MEAN_WERT = (RXBUF_UINT(4)/100.0f);
        // Output of the current mean measured cell capacity averaged over all cells in Ah / Ausgabe der aktuellen
        // mittleren gemessenen Zellkapazität gemittelt über alle Zellen in Ah
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLKAPAZITAET_MEAN_WERT", STAT_ZELLKAPAZITAET_MEAN_WERT, "\"Ah\"");

    float STAT_ZELLSPANNUNG_MIN_WERT = (RXBUF_UINT(6)/10000.0f);
        // Output of the current minimum measured cell voltage of all cells in V. / Ausgabe der aktuellen minimalen
        // gemessenen Zellspannung aller Zellen in V
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLSPANNUNG_MIN_WERT", STAT_ZELLSPANNUNG_MIN_WERT, "\"V\"");

    float STAT_ZELLSPANNUNG_MAX_WERT = (RXBUF_UINT(8)/10000.0f);
        // Output of the current maximum measured cell voltage of all cells in V / Ausgabe der aktuellen maximalen
        // gemessenen Zellspannung aller Zellen in V
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLSPANNUNG_MAX_WERT", STAT_ZELLSPANNUNG_MAX_WERT, "\"V\"");

    float STAT_ZELLSPANNUNG_MEAN_WERT = (RXBUF_UINT(10)/10000.0f);
        // Output of the current mean measured cell voltage of all cells in V / Ausgabe der aktuellen mittleren
        // gemessenen Zellspannung aller Zellen in V
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLSPANNUNG_MEAN_WERT", STAT_ZELLSPANNUNG_MEAN_WERT, "\"V\"");

    float STAT_ZELLTEMPERATUR_MIN_WERT = (RXBUF_SINT(12)/100.0f);
        // Output of the current minimum measured cell temperature of all cells in <B0> C / Ausgabe der aktuellen minimalen
        // gemessenen Zelltemperatur aller Zellen in °C
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLTEMPERATUR_MIN_WERT", STAT_ZELLTEMPERATUR_MIN_WERT, "\"°C\"");

    float STAT_ZELLTEMPERATUR_MAX_WERT = (RXBUF_SINT(14)/100.0f);
        // Output of the current maximum measured cell temperature of all cells in <B0> C / Ausgabe der aktuellen maximalen
        // gemessenen Zelltemperatur aller Zellen in °C
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLTEMPERATUR_MAX_WERT", STAT_ZELLTEMPERATUR_MAX_WERT, "\"°C\"");

    float STAT_ZELLTEMPERATUR_MEAN_WERT = (RXBUF_SINT(16)/100.0f);
        // Output of the current mean measured cell temperature of all cells in <B0> C / Ausgabe der aktuellen mittleren
        // gemessenen Zelltemperatur aller Zellen in °C
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLTEMPERATUR_MEAN_WERT", STAT_ZELLTEMPERATUR_MEAN_WERT, "\"°C\"");

    float STAT_ZELLWIDERSTANDSFAKTOR_MIN_WERT = (RXBUF_UINT(18)/10000.0f);
        // Output of the current minimum measured resistance factor of all cells / Ausgabe des aktuellen minimalen
        // gemessenen Widerstandsfaktors aller Zellen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLWIDERSTANDSFAKTOR_MIN_WERT", STAT_ZELLWIDERSTANDSFAKTOR_MIN_WERT, "");

    float STAT_ZELLWIDERSTANDSFAKTOR_MAX_WERT = (RXBUF_UINT(20)/10000.0f);
        // Output of the current maximum measured resistance factor of all cells / Ausgabe des aktuellen maximalen
        // gemessenen Widerstandsfaktors aller Zellen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLWIDERSTANDSFAKTOR_MAX_WERT", STAT_ZELLWIDERSTANDSFAKTOR_MAX_WERT, "");

    float STAT_ZELLWIDERSTANDSFAKTOR_MEAN_WERT = (RXBUF_UINT(22)/10000.0f);
        // Output of the current mean measured resistance factor of all cells / Ausgabe des aktuellen mittleren
        // gemessenen Widerstandsfaktors aller Zellen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLWIDERSTANDSFAKTOR_MEAN_WERT", STAT_ZELLWIDERSTANDSFAKTOR_MEAN_WERT, "");

    float STAT_ZELLSOC_MIN_WERT = (RXBUF_UINT(24)/100.0f);
        // Output of the current minimum measured SoC of all cells in% / Ausgabe des aktuellen minimalen gemessenen SoC
        // aller Zellen in %
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLSOC_MIN_WERT", STAT_ZELLSOC_MIN_WERT, "\"%\"");

    float STAT_ZELLSOC_MAX_WERT = (RXBUF_UINT(26)/100.0f);
        // Output of the current maximum measured SoC of all cells in% / Ausgabe des aktuellen maximalen gemessenen SoC
        // aller Zellen in %
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLSOC_MAX_WERT", STAT_ZELLSOC_MAX_WERT, "\"%\"");

    float STAT_ZELLSOC_MEAN_WERT = (RXBUF_UINT(28)/100.0f);
        // Output of the current mean measured SoC of all cells in% / Ausgabe des aktuellen mittleren gemessenen SoC
        // aller Zellen in %
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLSOC_MEAN_WERT", STAT_ZELLSOC_MEAN_WERT, "\"%\"");

    float STAT_ZELLOCV_MIN_WERT = (RXBUF_UINT(30)/10000.0f);
        // Output of the current minimum OCV of all cells measured in the last resting state in V. / Ausgabe der
        // aktuellen minimalen im letzten Ruhezustand gemessenen OCV aller Zellen in V
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLOCV_MIN_WERT", STAT_ZELLOCV_MIN_WERT, "\"V\"");

    float STAT_ZELLOCV_MAX_WERT = (RXBUF_UINT(32)/10000.0f);
        // Output of the current maximum OCV of all cells measured in the last resting state in V. / Ausgabe der
        // aktuellen maximalen im letzten Ruhezustand gemessenen OCV aller Zellen in V
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLOCV_MAX_WERT", STAT_ZELLOCV_MAX_WERT, "\"V\"");

    float STAT_ZELLOCV_MEAN_WERT = (RXBUF_UINT(34)/10000.0f);
        // Output of the current mean OCV of all cells measured in the last resting state in V / Ausgabe der aktuellen
        // mittleren im letzten Ruhezustand gemessenen OCV aller Zellen in V
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_ZELLOCV_MEAN_WERT", STAT_ZELLOCV_MEAN_WERT, "\"V\"");

    float STAT_IMPEDANCE_ESTIMATION_ALPHA_WERT = (RXBUF_UINT(36)/10000.0f);
        // Current aging factor of the serial and parallel ohmic resistance as well as the parallel capacitance (1.5 =
        // increase of the current resistance by 50%) / Aktueller Alterungsfaktor des seriellen und parallelen ohmschen
        // Wider aktuellenstands sowie der parallelen Kapazität (1,5 = Eröhung des aktuellen Wider aktuellenstands um
        // 50%)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZUSTAND_SPEICHER", "STAT_IMPEDANCE_ESTIMATION_ALPHA_WERT", STAT_IMPEDANCE_ESTIMATION_ALPHA_WERT, "");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_bat_cac->SetValue(STAT_ZELLKAPAZITAET_MEAN_WERT, AmpHours);
    StdMetrics.ms_v_bat_pack_vmax->SetValue(STAT_ZELLSPANNUNG_MAX_WERT, Volts);
    StdMetrics.ms_v_bat_pack_vmin->SetValue(STAT_ZELLSPANNUNG_MIN_WERT, Volts);
    StdMetrics.ms_v_bat_pack_vavg->SetValue(STAT_ZELLSPANNUNG_MEAN_WERT, Volts);
    if (RXBUF_UINT(34) != 65535) {
        mt_i3_batt_pack_ocv_avg->SetValue(STAT_ZELLOCV_MEAN_WERT, Volts);
        mt_i3_batt_pack_ocv_min->SetValue(STAT_ZELLOCV_MIN_WERT, Volts);
        mt_i3_batt_pack_ocv_max->SetValue(STAT_ZELLOCV_MAX_WERT, Volts);
    }
    StdMetrics.ms_v_bat_pack_level_max->SetValue(STAT_ZELLSOC_MAX_WERT, Percentage);
    StdMetrics.ms_v_bat_pack_level_min->SetValue(STAT_ZELLSOC_MIN_WERT, Percentage);
    StdMetrics.ms_v_bat_pack_level_avg->SetValue(STAT_ZELLSOC_MEAN_WERT, Percentage);
    break;
  }

#ifdef INVESTIGATIONS
  case I3_PID_SME_PROJEKT_PARAMETER: {                                            // 0xDF71
    if (datalen < 5) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_SME_PROJEKT_PARAMETER", 5);
        break;
    }

// 2020-12-19 12:54:27 SAST D (685148) v-bmwi3: From ECU SME, pid PROJEKT_PARAMETER: got STAT_ANZAHL_ZELLEN_INSGESAMT_WERT=96
// 2020-12-19 12:54:27 SAST D (685158) v-bmwi3: From ECU SME, pid PROJEKT_PARAMETER: got STAT_ANZAHL_ZELLEN_PRO_MODUL_WERT=c
// 2020-12-19 12:54:27 SAST D (685158) v-bmwi3: From ECU SME, pid PROJEKT_PARAMETER: got STAT_ANZAHL_MODULE_WERT=8
// 2020-12-19 12:54:27 SAST D (685158) v-bmwi3: From ECU SME, pid PROJEKT_PARAMETER: got STAT_ANZAHL_TEMPERATURSENSOREN_PRO_MODUL_WERT=4
// So we have 96 cells total, split into 8 modules with 12 cells each.
// Each module contains 4 temperature probes.
// This stuff aint gonna change 

    unsigned short STAT_ANZAHL_ZELLEN_INSGESAMT_WERT = (RXBUF_UINT(0));
        // Number of cells in the HV storage system / Anzahl der Zellen des HV-Speichers
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "SME", "PROJEKT_PARAMETER", "STAT_ANZAHL_ZELLEN_INSGESAMT_WERT", STAT_ANZAHL_ZELLEN_INSGESAMT_WERT, "");

    unsigned char STAT_ANZAHL_ZELLEN_PRO_MODUL_WERT = (RXBUF_UCHAR(2));
        // Number of cells per module / Anzahl der Zellen pro Modul
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "SME", "PROJEKT_PARAMETER", "STAT_ANZAHL_ZELLEN_PRO_MODUL_WERT", STAT_ANZAHL_ZELLEN_PRO_MODUL_WERT, "");

    unsigned char STAT_ANZAHL_MODULE_WERT = (RXBUF_UCHAR(3));
        // Number of modules in the HV storage system / Anzahl der Module des HV-Speichers
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "SME", "PROJEKT_PARAMETER", "STAT_ANZAHL_MODULE_WERT", STAT_ANZAHL_MODULE_WERT, "");

    unsigned char STAT_ANZAHL_TEMPERATURSENSOREN_PRO_MODUL_WERT = (RXBUF_UCHAR(4));
        // Number of temperature sensors per module / Anzahl der Temperatursensoren pro Modul
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "SME", "PROJEKT_PARAMETER", "STAT_ANZAHL_TEMPERATURSENSOREN_PRO_MODUL_WERT", STAT_ANZAHL_TEMPERATURSENSOREN_PRO_MODUL_WERT, "");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

case I3_PID_SME_ZELLSPANNUNGEN_MIN_MAX: {                                       // 0xDDBF
    if (datalen < 4) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_SME_ZELLSPANNUNGEN_MIN_MAX", 4);
        break;
    }

    float STAT_UCELL_MIN_WERT = (RXBUF_UINT(0)/1000.0f);
        // minimum single cell voltage of all single cells / minimale Einzelzellspannung aller Einzelzellen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZELLSPANNUNGEN_MIN_MAX", "STAT_UCELL_MIN_WERT", STAT_UCELL_MIN_WERT, "\"V\"");

    float STAT_UCELL_MAX_WERT = (RXBUF_UINT(2)/1000.0f);
        // maximum single cell voltage of all single cells / maximale Einzelzellspannung aller Einzelzellen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "SME", "ZELLSPANNUNGEN_MIN_MAX", "STAT_UCELL_MAX_WERT", STAT_UCELL_MAX_WERT, "\"V\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }
#endif

// --- KOM --------------------------------------------------------------------------------------------------------

  case I3_PID_KOM_KOMBI_REICHWEITE_BEV_PHEV: {
    if (datalen < 12) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KOM_KOMBI_REICHWEITE_BEV_PHEV", 12);
        break;
    }
    float STAT_ELECTRIC_RANGE_CURRENT_WERT = (RXBUF_UINT(0)/10.0f);
        // STAT_ELECTRIC_RANGE_CURRENT [0.1 km] / STAT_ELECTRIC_RANGE_CURRENT [0,1 km]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_REICHWEITE_BEV_PHEV", "STAT_ELECTRIC_RANGE_CURRENT_WERT", STAT_ELECTRIC_RANGE_CURRENT_WERT, "\"km\"");
    float STAT_ELECTRIC_RANGE_MAXIMUM_WERT = (RXBUF_UINT(2)/10.0f);
        // STAT_ELECTRIC_RANGE_MAXIMUM (actual maximum, depends on battery deterioration etc.) [0.1 km] /
        // STAT_ELECTRIC_RANGE_MAXIMUM (actual  maximum, depends on battery deterioration  etc.)  [0,1 km]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_REICHWEITE_BEV_PHEV", "STAT_ELECTRIC_RANGE_MAXIMUM_WERT", STAT_ELECTRIC_RANGE_MAXIMUM_WERT, "\"km\"");
    float STAT_FUEL_RANGE_CURRENT_WERT = (RXBUF_UINT(4)/10.0f);
        // STAT_FUEL_RANGE_CURRENT [0.1 km] (0xFFFF if no REX is available) / STAT_FUEL_RANGE_CURRENT [0,1 km]  (0xFFFF
        // if no REX is available)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_REICHWEITE_BEV_PHEV", "STAT_FUEL_RANGE_CURRENT_WERT", STAT_FUEL_RANGE_CURRENT_WERT, "\"km\"");
    float STAT_FUEL_RANGE_MAXIMUM_WERT = (RXBUF_UINT(6)/10.0f);
        // STAT_FUEL_RANGE_MAXIMUM (actual maximum, depends on average consumption etc.) [0.1 km] (0xFFFF if no REX is
        // available) / STAT_FUEL_RANGE_MAXIMUM (actual  maximum, depends on average consumption etc.)  [0,1 km] (0xFFFF
        // if no REX is available)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_REICHWEITE_BEV_PHEV", "STAT_FUEL_RANGE_MAXIMUM_WERT", STAT_FUEL_RANGE_MAXIMUM_WERT, "\"km\"");
    float STAT_RANGE_CONSUMPTION_ELECTRIC_WERT = (RXBUF_UINT(8)/1000.0f);
        // STAT_RANGE_CONSUMPTION_ELECTRIC [kWh / 100km], (only for vehicles without STARTWETESPEICHER, 0xFFFF if not
        // used) / STAT_RANGE_CONSUMPTION_ELECTRIC [kWh/100km], (only for vehicles without STARTWETESPEICHER, 0xFFFF if
        // not used)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_REICHWEITE_BEV_PHEV", "STAT_RANGE_CONSUMPTION_ELECTRIC_WERT", STAT_RANGE_CONSUMPTION_ELECTRIC_WERT, "\"kWh/100km\"");
    float STAT_NEBENVERBRAUCHERLEISTUNG_WERT = (RXBUF_UINT(10)/100.0f);
        // STAT_NEBENVERBENVERBRAUCHERLEISTUNG_WERT [0.01 kW], (only for vehicles without STARTWERTESPEICHER, 0xFFFF if
        // not used) / STAT_NEBENVERBRAUCHERLEISTUNG_WERT  [0,01 kW], (only for vehicles without STARTWERTESPEICHER,
        // 0xFFFF if not used)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_REICHWEITE_BEV_PHEV", "STAT_NEBENVERBRAUCHERLEISTUNG_WERT", STAT_NEBENVERBRAUCHERLEISTUNG_WERT, "\"kW\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_bat_range_est->SetValue(STAT_ELECTRIC_RANGE_CURRENT_WERT, Kilometers);
    StdMetrics.ms_v_bat_range_ideal->SetValue(STAT_ELECTRIC_RANGE_MAXIMUM_WERT, Kilometers);
    if (soc > 0.0f) {
        // Take ideal range and scale up as if SOC was 100%
        StdMetrics.ms_v_bat_range_full->SetValue(floor(STAT_ELECTRIC_RANGE_CURRENT_WERT / soc), Kilometers);
    }

    break;
  }

  case I3_PID_KOM_TACHO_WERT: {
    if (datalen < 2) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KOM_TACHO_WERT", 2);
        break;
    }
    float STAT_GESCHWINDIGKEIT_WERT = (RXBUF_UINT(0)/10.0f);
        // Returns the display speed. / Rückgabe der Anzeigegeschwindigkeit.
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "TACHO_WERT", "STAT_GESCHWINDIGKEIT_WERT", STAT_GESCHWINDIGKEIT_WERT, "\"km/h\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_pos_speed->SetValue(STAT_GESCHWINDIGKEIT_WERT, Kph);

    break;
  }

  case I3_PID_KOM_GWSZ_ABSOLUT_WERT: {
    if (datalen < 8) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KOM_GWSZ_ABSOLUT_WERT", 8);
        break;
    }
    long STAT_ABSOLUT_GWSZ_RAM_WERT = (RXBUF_SINT32(0));
        // Returns the absolute total odometer from the RAM. / Liefert den absoluten Gesamtwegstreckenzähler aus dem
        // RAM.
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%ld%s\n", "KOM", "GWSZ_ABSOLUT_WERT", "STAT_ABSOLUT_GWSZ_RAM_WERT", STAT_ABSOLUT_GWSZ_RAM_WERT, "\"km\"");
    long STAT_ABSOLUT_GWSZ_EEP_WERT = (RXBUF_SINT32(4));
        // Supplies the absolute total odometer from the EEPROM. / Liefert den absoluten Gesamtwegstreckenzähler aus dem
        // EEPROM.
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%ld%s\n", "KOM", "GWSZ_ABSOLUT_WERT", "STAT_ABSOLUT_GWSZ_EEP_WERT", STAT_ABSOLUT_GWSZ_EEP_WERT, "\"km\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_pos_odometer->SetValue(STAT_ABSOLUT_GWSZ_RAM_WERT, Kilometers);

    break;
  }

  case I3_PID_KOM_A_TEMP_WERT: {
    if (datalen < 2) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KOM_A_TEMP_WERT", 2);
        break;
    }
    float STAT_A_TEMP_ANZEIGE_WERT = (RXBUF_UCHAR(0)/2.0f-40.0);
        // Display value outside temperature / Anzeigewert Außentemperatur
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "A_TEMP_WERT", "STAT_A_TEMP_ANZEIGE_WERT", STAT_A_TEMP_ANZEIGE_WERT, "\"°C\"");
    float STAT_A_TEMP_ROHWERT_WERT = (RXBUF_UCHAR(1)/2.0f-40.0);
        // Raw value outside temperature / Rohwert Außentemperatur
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "A_TEMP_WERT", "STAT_A_TEMP_ROHWERT_WERT", STAT_A_TEMP_ROHWERT_WERT, "\"°C\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_env_temp->SetValue(STAT_A_TEMP_ANZEIGE_WERT, Celcius);

    break;
  }

  case I3_PID_KOM_KOMBI_BC_BCW_KWH_KM: {
    if (datalen < 8) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KOM_KOMBI_BC_BCW_KWH_KM", 8);
        break;
    }
    float STAT_BC_DSV_KWH_KM_WERT = (RXBUF_UINT(0)/10.0f);
        // On-board computer average consumption in 0.1 [KWh / 100km] / Bordcomputer Durchschnittsverbrauch in 0,1
        // [KWh/100km]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_BC_BCW_KWH_KM", "STAT_BC_DSV_KWH_KM_WERT", STAT_BC_DSV_KWH_KM_WERT, "\"kWh/100km\"");
    float STAT_BC_DSG_KWH_KM_WERT = (RXBUF_UINT(2)/100.0f);
        // On-board computer average speed in 0.01 [km / h] / Bordcomputer Durchschnittsgeschwindigkeit in 0,01 [km/h]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_BC_BCW_KWH_KM", "STAT_BC_DSG_KWH_KM_WERT", STAT_BC_DSG_KWH_KM_WERT, "\"km/h\"");
    float STAT_BC_MVERB_KWH_KM_WERT = (RXBUF_SINT(4)/10.0f);
    //int16_t STAT_BC_MVERB_WH_KM_WERT = RXBUF_SINT(4); // Raw figure is in Wh/km which is what the metric wants (and its signed, actually)
        // On-board computer current consumption in 0.1 [KWh / h] = [KW] / Bordcomputer Momentanverbrauch in 0,1
        // [KWh/h]=[KW]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_BC_BCW_KWH_KM", "STAT_BC_MVERB_KWH_KM_WERT", STAT_BC_MVERB_KWH_KM_WERT, "\"kW\"");
    float STAT_BC_RW_KWH_KM_WERT = (RXBUF_UINT(6)/10.0f);
        // On-board computer range in 0.1 [km] / Bordcomputer Reichweite in 0,1 [km]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_BC_BCW_KWH_KM", "STAT_BC_RW_KWH_KM_WERT", STAT_BC_RW_KWH_KM_WERT, "\"km\"");

    // ==========  Add your processing here ==========
    // Leave to the calc in vehicle.cpp:  StdMetrics.ms_v_bat_consumption->SetValue(STAT_BC_MVERB_WH_KM_WERT, WattHoursPK);

    break;
  }

  case I3_PID_KOM_KOMBI_BC_RBC_KWH_KM: {
    if (datalen < 6) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KOM_KOMBI_BC_RBC_KWH_KM", 10);
        break;
    }
    float STAT_RBC_DSV_KWH_KM_WERT = (RXBUF_UINT(0)/10.0f);
    uint16_t STAT_RBC_DSV_WH_KM_WERT = RXBUF_SINT(0); // Wh/km - seems to be signed?
        // On-board computer average consumption in 0.1 [KWh / 100km] / Reisebordcomputer Durchschnittsverbrauch in 0,1
        // [KWh/100km]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_BC_RBC_KWH_KM", "STAT_RBC_DSV_KWH_KM_WERT", STAT_RBC_DSV_KWH_KM_WERT, "\"kWh/100km\"");
    float STAT_RBC_DSG_KWH_KM_WERT = (RXBUF_UINT(2)/100.0f);
        // On-board computer average speed in 0.01 [km / h] / Reisebordcomputer Durchschnittsgeschwindigkeit in 0,01
        // [km/h]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "KOMBI_BC_RBC_KWH_KM", "STAT_RBC_DSG_KWH_KM_WERT", STAT_RBC_DSG_KWH_KM_WERT, "\"km/h\"");
    // Can't process STAT_RBC_ABFAHRT_KWH_KM_WERT - don't know type string[2] (offset 4)
    // Can't process STAT_RBC_DAUER_KWH_KM_WERT - don't know type string[2] (offset 6)
    unsigned short STAT_RBC_STRECKE_KWH_KM_WERT = (RXBUF_UINT(8));
        // On-board computer route in 1 [km] / Reisebordcomputer Strecke in 1 [km]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "KOM", "KOMBI_BC_RBC_KWH_KM", "STAT_RBC_STRECKE_KWH_KM_WERT", STAT_RBC_STRECKE_KWH_KM_WERT, "\"km\"");

    // ==========  Add your processing here ==========
    ESP_LOGD(TAG, "BC departure time %d:%d, travel time %d:%d", RXBUF_UCHAR(4), RXBUF_UCHAR(5), RXBUF_UCHAR(6), RXBUF_UCHAR(6));
    if (STAT_RBC_STRECKE_KWH_KM_WERT < 65500) { // 65533 seems to mean invalid
        StdMetrics.ms_v_pos_trip->SetValue(STAT_RBC_STRECKE_KWH_KM_WERT);
    }
    if (STAT_RBC_DSV_WH_KM_WERT < 65500) { // 65533 seems to mean invalid
        mt_i3_v_pos_tripconsumption->SetValue(STAT_RBC_DSV_WH_KM_WERT, WattHoursPK);
    }

    // We can reverse-calculate these to come up with v.b.energy.used - Main battery coulomb used on trip [Ah] - ms_v_bat_energy_used
    // we want Ah.  We have consumption in Wh/km.  We have the number of kilometers.  so multiply to get Wh.  Then divide by volts to get Ah
    // FIXME: doesn't work too well since the resolution of the KM distance travelled is 1kn which is too little.

    // Might not yet have data
    if (STAT_RBC_DSV_WH_KM_WERT == 65533 || STAT_RBC_STRECKE_KWH_KM_WERT == 65533 || hv_volts < 1.0f) {
        break;
    }

    float Wh = STAT_RBC_DSV_WH_KM_WERT * STAT_RBC_STRECKE_KWH_KM_WERT;
    float Ah = Wh / hv_volts;
    ESP_LOGD(TAG, "Calculated Ah=%.3f from Wh/km=%d, km=%d, volts=%.3f", Ah, STAT_RBC_DSV_WH_KM_WERT, STAT_RBC_STRECKE_KWH_KM_WERT, hv_volts);
    StdMetrics.ms_v_bat_coulomb_used->SetValue(Ah, AmpHours);
    StdMetrics.ms_v_bat_energy_used->SetValue(Wh/1000.0f, kWh);

    break;
  }

  case I3_PID_KOM_REICHWEITE_MCV: {                                               // 0x420C
    if (datalen < 8) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KOM_REICHWEITE_MCV", 8);
        break;
    }

    unsigned short STAT_ECO_MODE_REICHWEITE_WERT = (RXBUF_UINT(0));
        // Eco-mode range / Eco-Mode-reichweite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "KOM", "REICHWEITE_MCV", "STAT_ECO_MODE_REICHWEITE_WERT", STAT_ECO_MODE_REICHWEITE_WERT, "\"km\"");

    unsigned short STAT_COMFORT_MODE_REICHWEITE_WERT = (RXBUF_UINT(2));
        // Comfort mode range (CRW) / Comfort-Mode Reichweite (CRW)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "KOM", "REICHWEITE_MCV", "STAT_COMFORT_MODE_REICHWEITE_WERT", STAT_COMFORT_MODE_REICHWEITE_WERT, "\"km\"");

    // Definition seems to be wrong: float STAT_BC_REICHWEITE_WERT = (RXBUF_UINT(4)/10.0f);
    unsigned int STAT_BC_REICHWEITE_WERT = (RXBUF_UINT(4));
        // BC range / BC-Reichweite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "KOM", "REICHWEITE_MCV", "STAT_BC_REICHWEITE_WERT", STAT_BC_REICHWEITE_WERT, "\"km\"");

    // Definition seems to be wrong: float STAT_ECO_PRO_PLUS_REICHWEITE_WERT = (RXBUF_UINT(6)/10.0f);
    unsigned int STAT_ECO_PRO_PLUS_REICHWEITE_WERT = (RXBUF_UINT(6));
        // ECO Pro + range / ECO Pro+ Reichweite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "KOM", "REICHWEITE_MCV", "STAT_ECO_PRO_PLUS_REICHWEITE_WERT", STAT_ECO_PRO_PLUS_REICHWEITE_WERT, "\"km\"");

    // ==========  Add your processing here ==========
    mt_i3_range_bc->SetValue(STAT_BC_REICHWEITE_WERT, Kilometers);
    mt_i3_range_comfort->SetValue(STAT_COMFORT_MODE_REICHWEITE_WERT, Kilometers);
    mt_i3_range_ecopro->SetValue(STAT_ECO_MODE_REICHWEITE_WERT, Kilometers);
    mt_i3_range_ecoproplus->SetValue(STAT_ECO_PRO_PLUS_REICHWEITE_WERT, Kilometers);

    break;
  }

  // Don't think this is actually what we want
  case I3_PID_KOM_SEGMENTDATEN_SPEICHER: {                                        // 0xD12F
    if (datalen < 210) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KOM_SEGMENTDATEN_SPEICHER", 210);
        break;
    }

    unsigned char STAT_BLOCK_01_SEGMENT_ID_WERT = (RXBUF_UCHAR(0));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_SEGMENT_ID_WERT", STAT_BLOCK_01_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_01_TAG_WERT = (RXBUF_UCHAR(1));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_TAG_WERT", STAT_BLOCK_01_TAG_WERT, "");

    unsigned char STAT_BLOCK_01_MONAT_WERT = (RXBUF_UCHAR(2));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_MONAT_WERT", STAT_BLOCK_01_MONAT_WERT, "");

    unsigned char STAT_BLOCK_01_JAHR_WERT = (RXBUF_UCHAR(3)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_JAHR_WERT", STAT_BLOCK_01_JAHR_WERT, "");

    unsigned char STAT_BLOCK_01_STUNDE_WERT = (RXBUF_UCHAR(4));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_STUNDE_WERT", STAT_BLOCK_01_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_01_MINUTE_WERT = (RXBUF_UCHAR(5));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_MINUTE_WERT", STAT_BLOCK_01_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_01_A_TEMP_WERT = (RXBUF_UCHAR(9)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_A_TEMP_WERT", STAT_BLOCK_01_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_01_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(10)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_AKT_DAUER_ECO_WERT", STAT_BLOCK_01_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_01_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(11)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_01_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_01_SOC_WERT = (RXBUF_UCHAR(12)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_SOC_WERT", STAT_BLOCK_01_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_01_REKUPERATION_WERT = (RXBUF_UCHAR(16));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_REKUPERATION_WERT", STAT_BLOCK_01_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_01_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(17)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_01_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_01_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_01_STERNE = (RXBUF_UCHAR(20));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_01_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_01_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_01_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_01_STERNE", (unsigned long)BF_BLOCK_01_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_02_SEGMENT_ID_WERT = (RXBUF_UCHAR(21));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_SEGMENT_ID_WERT", STAT_BLOCK_02_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_02_TAG_WERT = (RXBUF_UCHAR(22));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_TAG_WERT", STAT_BLOCK_02_TAG_WERT, "");

    unsigned char STAT_BLOCK_02_MONAT_WERT = (RXBUF_UCHAR(23));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_MONAT_WERT", STAT_BLOCK_02_MONAT_WERT, "");

    unsigned char STAT_BLOCK_02_JAHR_WERT = (RXBUF_UCHAR(24)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_JAHR_WERT", STAT_BLOCK_02_JAHR_WERT, "");

    unsigned char STAT_BLOCK_02_STUNDE_WERT = (RXBUF_UCHAR(25));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_STUNDE_WERT", STAT_BLOCK_02_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_02_MINUTE_WERT = (RXBUF_UCHAR(26));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_MINUTE_WERT", STAT_BLOCK_02_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_02_A_TEMP_WERT = (RXBUF_UCHAR(30)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_A_TEMP_WERT", STAT_BLOCK_02_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_02_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(31)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_AKT_DAUER_ECO_WERT", STAT_BLOCK_02_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_02_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(32)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_02_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_02_SOC_WERT = (RXBUF_UCHAR(33)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_SOC_WERT", STAT_BLOCK_02_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_02_REKUPERATION_WERT = (RXBUF_UCHAR(37));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_REKUPERATION_WERT", STAT_BLOCK_02_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_02_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(38)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_02_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_02_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_02_STERNE = (RXBUF_UCHAR(41));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_02_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_02_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_02_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_02_STERNE", (unsigned long)BF_BLOCK_02_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_03_SEGMENT_ID_WERT = (RXBUF_UCHAR(42));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_SEGMENT_ID_WERT", STAT_BLOCK_03_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_03_TAG_WERT = (RXBUF_UCHAR(43));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_TAG_WERT", STAT_BLOCK_03_TAG_WERT, "");

    unsigned char STAT_BLOCK_03_MONAT_WERT = (RXBUF_UCHAR(44));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_MONAT_WERT", STAT_BLOCK_03_MONAT_WERT, "");

    unsigned char STAT_BLOCK_03_JAHR_WERT = (RXBUF_UCHAR(45)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_JAHR_WERT", STAT_BLOCK_03_JAHR_WERT, "");

    unsigned char STAT_BLOCK_03_STUNDE_WERT = (RXBUF_UCHAR(46));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_STUNDE_WERT", STAT_BLOCK_03_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_03_MINUTE_WERT = (RXBUF_UCHAR(47));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_MINUTE_WERT", STAT_BLOCK_03_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_03_A_TEMP_WERT = (RXBUF_UCHAR(51)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_A_TEMP_WERT", STAT_BLOCK_03_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_03_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(52)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_AKT_DAUER_ECO_WERT", STAT_BLOCK_03_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_03_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(53)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_03_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_03_SOC_WERT = (RXBUF_UCHAR(54)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_SOC_WERT", STAT_BLOCK_03_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_03_REKUPERATION_WERT = (RXBUF_UCHAR(58));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_REKUPERATION_WERT", STAT_BLOCK_03_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_03_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(59)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_03_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_03_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_03_STERNE = (RXBUF_UCHAR(62));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_03_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_03_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_03_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_03_STERNE", (unsigned long)BF_BLOCK_03_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_04_SEGMENT_ID_WERT = (RXBUF_UCHAR(63));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_SEGMENT_ID_WERT", STAT_BLOCK_04_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_04_TAG_WERT = (RXBUF_UCHAR(64));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_TAG_WERT", STAT_BLOCK_04_TAG_WERT, "");

    unsigned char STAT_BLOCK_04_MONAT_WERT = (RXBUF_UCHAR(65));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_MONAT_WERT", STAT_BLOCK_04_MONAT_WERT, "");

    unsigned char STAT_BLOCK_04_JAHR_WERT = (RXBUF_UCHAR(66)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_JAHR_WERT", STAT_BLOCK_04_JAHR_WERT, "");

    unsigned char STAT_BLOCK_04_STUNDE_WERT = (RXBUF_UCHAR(67));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_STUNDE_WERT", STAT_BLOCK_04_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_04_MINUTE_WERT = (RXBUF_UCHAR(68));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_MINUTE_WERT", STAT_BLOCK_04_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_04_A_TEMP_WERT = (RXBUF_UCHAR(72)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_A_TEMP_WERT", STAT_BLOCK_04_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_04_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(73)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_AKT_DAUER_ECO_WERT", STAT_BLOCK_04_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_04_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(74)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_04_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_04_SOC_WERT = (RXBUF_UCHAR(75)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_SOC_WERT", STAT_BLOCK_04_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_04_REKUPERATION_WERT = (RXBUF_UCHAR(79));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_REKUPERATION_WERT", STAT_BLOCK_04_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_04_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(80)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_04_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_04_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_04_STERNE = (RXBUF_UCHAR(83));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_04_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_04_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_04_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_04_STERNE", (unsigned long)BF_BLOCK_04_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_05_SEGMENT_ID_WERT = (RXBUF_UCHAR(84));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_SEGMENT_ID_WERT", STAT_BLOCK_05_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_05_TAG_WERT = (RXBUF_UCHAR(85));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_TAG_WERT", STAT_BLOCK_05_TAG_WERT, "");

    unsigned char STAT_BLOCK_05_MONAT_WERT = (RXBUF_UCHAR(86));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_MONAT_WERT", STAT_BLOCK_05_MONAT_WERT, "");

    unsigned char STAT_BLOCK_05_JAHR_WERT = (RXBUF_UCHAR(87)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_JAHR_WERT", STAT_BLOCK_05_JAHR_WERT, "");

    unsigned char STAT_BLOCK_05_STUNDE_WERT = (RXBUF_UCHAR(88));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_STUNDE_WERT", STAT_BLOCK_05_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_05_MINUTE_WERT = (RXBUF_UCHAR(89));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_MINUTE_WERT", STAT_BLOCK_05_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_05_A_TEMP_WERT = (RXBUF_UCHAR(93)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_A_TEMP_WERT", STAT_BLOCK_05_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_05_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(94)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_AKT_DAUER_ECO_WERT", STAT_BLOCK_05_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_05_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(95)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_05_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_05_SOC_WERT = (RXBUF_UCHAR(96)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_SOC_WERT", STAT_BLOCK_05_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_05_REKUPERATION_WERT = (RXBUF_UCHAR(100));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_REKUPERATION_WERT", STAT_BLOCK_05_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_05_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(101)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_05_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_05_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_05_STERNE = (RXBUF_UCHAR(104));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_05_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_05_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_05_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_05_STERNE", (unsigned long)BF_BLOCK_05_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_06_SEGMENT_ID_WERT = (RXBUF_UCHAR(105));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_SEGMENT_ID_WERT", STAT_BLOCK_06_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_06_TAG_WERT = (RXBUF_UCHAR(106));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_TAG_WERT", STAT_BLOCK_06_TAG_WERT, "");

    unsigned char STAT_BLOCK_06_MONAT_WERT = (RXBUF_UCHAR(107));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_MONAT_WERT", STAT_BLOCK_06_MONAT_WERT, "");

    unsigned char STAT_BLOCK_06_JAHR_WERT = (RXBUF_UCHAR(108)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_JAHR_WERT", STAT_BLOCK_06_JAHR_WERT, "");

    unsigned char STAT_BLOCK_06_STUNDE_WERT = (RXBUF_UCHAR(109));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_STUNDE_WERT", STAT_BLOCK_06_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_06_MINUTE_WERT = (RXBUF_UCHAR(110));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_MINUTE_WERT", STAT_BLOCK_06_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_06_A_TEMP_WERT = (RXBUF_UCHAR(114)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_A_TEMP_WERT", STAT_BLOCK_06_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_06_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(115)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_AKT_DAUER_ECO_WERT", STAT_BLOCK_06_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_06_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(116)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_06_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_06_SOC_WERT = (RXBUF_UCHAR(117)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_SOC_WERT", STAT_BLOCK_06_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_06_REKUPERATION_WERT = (RXBUF_UCHAR(121));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_REKUPERATION_WERT", STAT_BLOCK_06_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_06_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(122)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_06_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_06_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_06_STERNE = (RXBUF_UCHAR(125));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_06_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_06_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_06_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_06_STERNE", (unsigned long)BF_BLOCK_06_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_07_SEGMENT_ID_WERT = (RXBUF_UCHAR(126));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_SEGMENT_ID_WERT", STAT_BLOCK_07_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_07_TAG_WERT = (RXBUF_UCHAR(127));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_TAG_WERT", STAT_BLOCK_07_TAG_WERT, "");

    unsigned char STAT_BLOCK_07_MONAT_WERT = (RXBUF_UCHAR(128));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_MONAT_WERT", STAT_BLOCK_07_MONAT_WERT, "");

    unsigned char STAT_BLOCK_07_JAHR_WERT = (RXBUF_UCHAR(129)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_JAHR_WERT", STAT_BLOCK_07_JAHR_WERT, "");

    unsigned char STAT_BLOCK_07_STUNDE_WERT = (RXBUF_UCHAR(130));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_STUNDE_WERT", STAT_BLOCK_07_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_07_MINUTE_WERT = (RXBUF_UCHAR(131));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_MINUTE_WERT", STAT_BLOCK_07_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_07_A_TEMP_WERT = (RXBUF_UCHAR(135)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_A_TEMP_WERT", STAT_BLOCK_07_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_07_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(136)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_AKT_DAUER_ECO_WERT", STAT_BLOCK_07_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_07_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(137)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_07_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_07_SOC_WERT = (RXBUF_UCHAR(138)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_SOC_WERT", STAT_BLOCK_07_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_07_REKUPERATION_WERT = (RXBUF_UCHAR(142));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_REKUPERATION_WERT", STAT_BLOCK_07_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_07_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(143)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_07_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_07_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_07_STERNE = (RXBUF_UCHAR(146));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_07_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_07_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_07_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_07_STERNE", (unsigned long)BF_BLOCK_07_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_08_SEGMENT_ID_WERT = (RXBUF_UCHAR(147));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_SEGMENT_ID_WERT", STAT_BLOCK_08_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_08_TAG_WERT = (RXBUF_UCHAR(148));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_TAG_WERT", STAT_BLOCK_08_TAG_WERT, "");

    unsigned char STAT_BLOCK_08_MONAT_WERT = (RXBUF_UCHAR(149));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_MONAT_WERT", STAT_BLOCK_08_MONAT_WERT, "");

    unsigned char STAT_BLOCK_08_JAHR_WERT = (RXBUF_UCHAR(150)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_JAHR_WERT", STAT_BLOCK_08_JAHR_WERT, "");

    unsigned char STAT_BLOCK_08_STUNDE_WERT = (RXBUF_UCHAR(151));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_STUNDE_WERT", STAT_BLOCK_08_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_08_MINUTE_WERT = (RXBUF_UCHAR(152));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_MINUTE_WERT", STAT_BLOCK_08_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_08_A_TEMP_WERT = (RXBUF_UCHAR(156)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_A_TEMP_WERT", STAT_BLOCK_08_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_08_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(157)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_AKT_DAUER_ECO_WERT", STAT_BLOCK_08_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_08_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(158)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_08_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_08_SOC_WERT = (RXBUF_UCHAR(159)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_SOC_WERT", STAT_BLOCK_08_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_08_REKUPERATION_WERT = (RXBUF_UCHAR(163));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_REKUPERATION_WERT", STAT_BLOCK_08_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_08_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(164)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_08_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_08_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_08_STERNE = (RXBUF_UCHAR(167));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_08_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_08_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_08_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_08_STERNE", (unsigned long)BF_BLOCK_08_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_09_SEGMENT_ID_WERT = (RXBUF_UCHAR(168));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_SEGMENT_ID_WERT", STAT_BLOCK_09_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_09_TAG_WERT = (RXBUF_UCHAR(169));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_TAG_WERT", STAT_BLOCK_09_TAG_WERT, "");

    unsigned char STAT_BLOCK_09_MONAT_WERT = (RXBUF_UCHAR(170));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_MONAT_WERT", STAT_BLOCK_09_MONAT_WERT, "");

    unsigned char STAT_BLOCK_09_JAHR_WERT = (RXBUF_UCHAR(171)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_JAHR_WERT", STAT_BLOCK_09_JAHR_WERT, "");

    unsigned char STAT_BLOCK_09_STUNDE_WERT = (RXBUF_UCHAR(172));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_STUNDE_WERT", STAT_BLOCK_09_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_09_MINUTE_WERT = (RXBUF_UCHAR(173));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_MINUTE_WERT", STAT_BLOCK_09_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_09_A_TEMP_WERT = (RXBUF_UCHAR(177)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_A_TEMP_WERT", STAT_BLOCK_09_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_09_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(178)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_AKT_DAUER_ECO_WERT", STAT_BLOCK_09_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_09_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(179)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_09_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_09_SOC_WERT = (RXBUF_UCHAR(180)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_SOC_WERT", STAT_BLOCK_09_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_09_REKUPERATION_WERT = (RXBUF_UCHAR(184));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_REKUPERATION_WERT", STAT_BLOCK_09_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_09_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(185)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_09_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_09_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_09_STERNE = (RXBUF_UCHAR(188));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_09_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_09_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_09_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_09_STERNE", (unsigned long)BF_BLOCK_09_STERNE, "\"Bit\"");

    unsigned char STAT_BLOCK_10_SEGMENT_ID_WERT = (RXBUF_UCHAR(189));
        // Segment ID / Segment ID
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_SEGMENT_ID_WERT", STAT_BLOCK_10_SEGMENT_ID_WERT, "");

    unsigned char STAT_BLOCK_10_TAG_WERT = (RXBUF_UCHAR(190));
        // Day / Tag
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_TAG_WERT", STAT_BLOCK_10_TAG_WERT, "");

    unsigned char STAT_BLOCK_10_MONAT_WERT = (RXBUF_UCHAR(191));
        // month / Monat
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_MONAT_WERT", STAT_BLOCK_10_MONAT_WERT, "");

    unsigned char STAT_BLOCK_10_JAHR_WERT = (RXBUF_UCHAR(192)+2000.0);
        // year / Jahr
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_JAHR_WERT", STAT_BLOCK_10_JAHR_WERT, "");

    unsigned char STAT_BLOCK_10_STUNDE_WERT = (RXBUF_UCHAR(193));
        // hour / Stunde
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_STUNDE_WERT", STAT_BLOCK_10_STUNDE_WERT, "\"h\"");

    unsigned char STAT_BLOCK_10_MINUTE_WERT = (RXBUF_UCHAR(194));
        // minute / Minute
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_MINUTE_WERT", STAT_BLOCK_10_MINUTE_WERT, "\"min\"");

    float STAT_BLOCK_10_A_TEMP_WERT = (RXBUF_UCHAR(198)*0.5f-40.0);
        // Outside temperature [-40 to + 50 � C] / Außentemperatur [-40 bis +50°C]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_A_TEMP_WERT", STAT_BLOCK_10_A_TEMP_WERT, "\"°C\"");

    float STAT_BLOCK_10_AKT_DAUER_ECO_WERT = (RXBUF_UCHAR(199)/2.0f);
        // Act. Duration ECO [0-100%] / Akt.-Dauer ECO [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_AKT_DAUER_ECO_WERT", STAT_BLOCK_10_AKT_DAUER_ECO_WERT, "\"%\"");

    float STAT_BLOCK_10_AKT_DAUER_ECOPLUS_SPORT_WERT = (RXBUF_UCHAR(200)/2.0f);
        // Act. Duration ECO + or sport [0-100%] / Akt.-Dauer ECO+  oder Sport [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_AKT_DAUER_ECOPLUS_SPORT_WERT", STAT_BLOCK_10_AKT_DAUER_ECOPLUS_SPORT_WERT, "\"%\"");

    float STAT_BLOCK_10_SOC_WERT = (RXBUF_UCHAR(201)/2.0f);
        // SOC [0-100%] in 0.5% steps / SOC [0-100%] in 0,5 % Schritten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_SOC_WERT", STAT_BLOCK_10_SOC_WERT, "\"%\"");

    unsigned char STAT_BLOCK_10_REKUPERATION_WERT = (RXBUF_UCHAR(205));
        // Recuperation [0-255 kWh] / Rekuperation [0-255 kWh]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_REKUPERATION_WERT", STAT_BLOCK_10_REKUPERATION_WERT, "\"kWh\"");

    float STAT_BLOCK_10_EFAHREN_VS_VERBRENNER_WERT = (RXBUF_UCHAR(206)/2.0f);
        // Share of e-driving vs. Incinerator [0-100%] / Anteil E-Fahren vs. Verbrenner [0-100%]
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "STAT_BLOCK_10_EFAHREN_VS_VERBRENNER_WERT", STAT_BLOCK_10_EFAHREN_VS_VERBRENNER_WERT, "\"%\"");

    unsigned char BF_BLOCK_10_STERNE = (RXBUF_UCHAR(209));
        // Bitfield 1: stars for acceleration, stars for braking / Bitfield 1: Sterne für Beschleunigung, Sterne für
        // Bremsen
        // BF_BLOCK_10_STERNE is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BLOCK_10_STERNE_BESCHLEUNIGUNG: Mask: 0x05 - Stars for acceleration
            // STAT_BLOCK_10_STERNE_BREMSEN: Mask: 0x38 - Stars for brakes
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KOM", "SEGMENTDATEN_SPEICHER", "BF_BLOCK_10_STERNE", (unsigned long)BF_BLOCK_10_STERNE, "\"Bit\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  // --- EME --------------------------------------------------------------------------------------------------------

  case I3_PID_EME_EME_HVPM_DCDC_ANSTEUERUNG: {                                    // 0xDE00
    if (datalen < 25) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EME_EME_HVPM_DCDC_ANSTEUERUNG", 25);
        break;
    }

    float STAT_SOC_HVB_WERT = (RXBUF_UINT(0)*0.1f);
        // HV battery charge level / Ladezustand HV Batterie
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_SOC_HVB_WERT", STAT_SOC_HVB_WERT, "\"%\"");

    float STAT_SOC_HVB_MIN_WERT = (RXBUF_UCHAR(2)*0.5f);
        // Startability limit, state of charge HV battery / Startfähigkeitsgrenze Ladezustand HV Batterie
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_SOC_HVB_MIN_WERT", STAT_SOC_HVB_MIN_WERT, "\"%\"");

    unsigned char STAT_LADEGERAET = (RXBUF_UCHAR(3));
        // Charger recognized (1 = recognized / 0 = not recognized) / Ladegerät erkannt (1 = erkannt / 0 = nicht
        // erkannt)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_LADEGERAET", STAT_LADEGERAET, "\"0/1\"");

    unsigned char STAT_FREMDLADUNG = (RXBUF_UCHAR(4));
        // External charge (1 = recognized / 0 = not recognized) / Fremdladung (1 = erkannt / 0 = nicht erkannt)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_FREMDLADUNG", STAT_FREMDLADUNG, "\"0/1\"");

    unsigned char STAT_FAHRB = (RXBUF_UCHAR(5));
        // Ready to drive (1 = active / 0 = not active) / Fahrbereitschaft (1 = aktiv / 0 = nicht aktiv)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_FAHRB", STAT_FAHRB, "\"0/1\"");

    unsigned char STAT_BA_DCDC_KOMM_NR = (RXBUF_UCHAR(6));
        // Commanded DC / DC converter operating mode / Kommandierte Betriebsart DC/DC Wandler
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_BA_DCDC_KOMM_NR", STAT_BA_DCDC_KOMM_NR, "\"0-n\"");

    float STAT_I_DCDC_HV_OUT_WERT = (RXBUF_SINT(7)*0.1f);
        // Current limit HV side / Stromgrenze HV-Seite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_I_DCDC_HV_OUT_WERT", STAT_I_DCDC_HV_OUT_WERT, "\"A\"");

    float STAT_U_DCDC_HV_OUT_WERT = (RXBUF_UINT(9)*0.1f);
        // Voltage limit HV side / Spannungsgrenze HV-Seite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_U_DCDC_HV_OUT_WERT", STAT_U_DCDC_HV_OUT_WERT, "\"V\"");

    float STAT_I_DCDC_LV_OUT_WERT = (RXBUF_SINT(11)*0.1f);
        // Current limit NV side / Stromgrenze NV-Seite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_I_DCDC_LV_OUT_WERT", STAT_I_DCDC_LV_OUT_WERT, "\"A\"");

    float STAT_U_DCDC_LV_OUT_WERT = (RXBUF_UINT(13)*0.01f);
        // Voltage limit NV side / Spannungsgrenze NV-Seite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_U_DCDC_LV_OUT_WERT", STAT_U_DCDC_LV_OUT_WERT, "\"V\"");

    unsigned char STAT_BA_DCDC_IST_NR = (RXBUF_UCHAR(15));
        // ACTUAL operating mode DCDC converter / IST-Betriebsart DCDC-Wandler
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_BA_DCDC_IST_NR", STAT_BA_DCDC_IST_NR, "\"0-n\"");

    float STAT_ALS_DCDC_WERT = (RXBUF_UCHAR(16)*0.5f);
        // DC / DC converter utilization / Auslastung DC/DC-Wandler
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_ALS_DCDC_WERT", STAT_ALS_DCDC_WERT, "\"%\"");

    float STAT_I_DCDC_HV_WERT = (RXBUF_SINT(17)*0.1f);
        // Current HV side / Strom HV-Seite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_I_DCDC_HV_WERT", STAT_I_DCDC_HV_WERT, "\"A\"");

    float STAT_U_DCDC_HV_WERT = (RXBUF_UINT(19)*0.1f);
        // Voltage HV side / Spannung HV-Seite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_U_DCDC_HV_WERT", STAT_U_DCDC_HV_WERT, "\"V\"");

    float STAT_I_DCDC_LV_WERT = (RXBUF_SINT(21)*0.1f);
        // Electricity NV side / Strom NV-Seite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_I_DCDC_LV_WERT", STAT_I_DCDC_LV_WERT, "\"A\"");

    float STAT_U_DCDC_LV_WERT = (RXBUF_UINT(23)*0.01f);
        // Voltage NV side / Spannung NV-Seite
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "EME_HVPM_DCDC_ANSTEUERUNG", "STAT_U_DCDC_LV_WERT", STAT_U_DCDC_LV_WERT, "\"V\"");

    // ==========  Add your processing here ==========
    mt_i3_charge_actual->SetValue(STAT_SOC_HVB_WERT, Percentage);

    break;
  }

  // TODO can we use this to calculate efficiency - how much battery power got to the motor?
  case I3_PID_EME_AE_STROM_EMASCHINE: {                                           // 0xDE8A
    if (datalen < 10) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EME_AE_STROM_EMASCHINE", 10);
        break;
    }

    float STAT_STROM_AC_EFF_W_WERT = (RXBUF_SINT(0)*0.0625f);
        // Effective supply current phase W / Effektiver Zuleitungsstrom Phase W
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_STROM_EMASCHINE", "STAT_STROM_AC_EFF_W_WERT", STAT_STROM_AC_EFF_W_WERT, "\"A\"");

    float STAT_STROM_AC_EFF_V_WERT = (RXBUF_SINT(2)*0.0625f);
        // Effective supply current phase V / Effektiver Zuleitungsstrom Phase V
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_STROM_EMASCHINE", "STAT_STROM_AC_EFF_V_WERT", STAT_STROM_AC_EFF_V_WERT, "\"A\"");

    float STAT_STROM_AC_EFF_U_WERT = (RXBUF_SINT(4)*0.0625f);
        // Effective supply current phase U / Effektiver Zuleitungsstrom Phase U
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_STROM_EMASCHINE", "STAT_STROM_AC_EFF_U_WERT", STAT_STROM_AC_EFF_U_WERT, "\"A\"");

    float STAT_ERREGERSTROM_WERT = (RXBUF_SINT(6)*0.0625f);
        // Current excitation current / Aktueller Erregerstrom
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_STROM_EMASCHINE", "STAT_ERREGERSTROM_WERT", STAT_ERREGERSTROM_WERT, "\"A\"");

    float STAT_STROM_DC_HV_UMRICHTER_EM_WERT = (RXBUF_SINT(8)*0.1f);
        // HV-DC current of the converter for the EM stator / HV-DC Strom des Umrichters für den EM-Stator
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_STROM_EMASCHINE", "STAT_STROM_DC_HV_UMRICHTER_EM_WERT", STAT_STROM_DC_HV_UMRICHTER_EM_WERT, "\"A\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EME_AE_TEMP_LE: {
    if (datalen < 30) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EME_AE_TEMP_LE", 30);
        break;
    }
    float STAT_TEMP_UMRICHTER_PHASE_U_WERT = (RXBUF_SINT(0)*0.0156f);
        // Temperature converter phase U / Temperatur Umrichter Phase U
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_UMRICHTER_PHASE_U_WERT", STAT_TEMP_UMRICHTER_PHASE_U_WERT, "\"°C\"");
    float STAT_TEMP_UMRICHTER_PHASE_V_WERT = (RXBUF_SINT(2)*0.0156f);
        // Temperature converter phase V / Temperatur Umrichter Phase V
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_UMRICHTER_PHASE_V_WERT", STAT_TEMP_UMRICHTER_PHASE_V_WERT, "\"°C\"");
    float STAT_TEMP_UMRICHTER_PHASE_W_WERT = (RXBUF_SINT(4)*0.0156f);
        // Temperature converter phase W / Temperatur Umrichter Phase W
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_UMRICHTER_PHASE_W_WERT", STAT_TEMP_UMRICHTER_PHASE_W_WERT, "\"°C\"");
    float STAT_TEMP_UMRICHTER_GT_WERT = (RXBUF_SINT(6)*0.0156f);
        // Temperature inverter gate driver / Temperatur Inverter Gatedriver
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_UMRICHTER_GT_WERT", STAT_TEMP_UMRICHTER_GT_WERT, "\"°C\"");
    float STAT_TEMP_DCDC_BO_WERT = (RXBUF_SINT(8)*0.0156f);
        // Temperature of the DCDC board / Temperatur des DCDC Boards
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_DCDC_BO_WERT", STAT_TEMP_DCDC_BO_WERT, "\"°C\"");
    float STAT_TEMP_DCDC_GTW_WERT = (RXBUF_SINT(10)*0.0156f);
        // Temperature DC / DC push-pull converter / Temperatur DC/DC-Gegentaktwandler
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_DCDC_GTW_WERT", STAT_TEMP_DCDC_GTW_WERT, "\"°C\"");
    float STAT_TEMP_DCDC_TS_WERT = (RXBUF_SINT(12)*0.0156f);
        // Temperature DC / DC buck converter / Temperatur DC/DC-Tiefsetzer
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_DCDC_TS_WERT", STAT_TEMP_DCDC_TS_WERT, "\"°C\"");
    float STAT_TEMP_DCDC_GR_WERT = (RXBUF_SINT(14)*0.0156f);
        // Temperature of the DCDC board / Temperatur des DCDC Boards
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_DCDC_GR_WERT", STAT_TEMP_DCDC_GR_WERT, "\"°C\"");
    float STAT_TEMP_SLE_PFC_WERT = (RXBUF_SINT(16)*0.0156f);
        // Temperature SLE-PowerFactorCorrection / Temperatur SLE-PowerFactorCorrection
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_SLE_PFC_WERT", STAT_TEMP_SLE_PFC_WERT, "\"°C\"");
    float STAT_TEMP_SLE_GR_WERT = (RXBUF_SINT(18)*0.0156f);
        // Temperature SLE rectifier / Temperatur SLE-Gleichrichter
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_SLE_GR_WERT", STAT_TEMP_SLE_GR_WERT, "\"°C\"");
    float STAT_TEMP_SLE_GTW_WERT = (RXBUF_SINT(20)*0.0156f);
        // Temperature SLE push-pull converter / Temperatur SLE-Gegentaktwandler
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_SLE_GTW_WERT", STAT_TEMP_SLE_GTW_WERT, "\"°C\"");
    float STAT_TEMP_SLE_BO_WERT = (RXBUF_SINT(22)*0.0156f);
        // Temperature of the SLE board / Temperatur des SLE Boards
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_SLE_BO_WERT", STAT_TEMP_SLE_BO_WERT, "\"°C\"");
    float STAT_TEMP_ELUP_LE_WERT = (RXBUF_SINT(24)*0.0156f);
        // Temperature on the power board - measuring point ELUP power output stage / Temperatur auf dem Powerbord -
        // Messstelle ELUP Leistungsendstufe
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_ELUP_LE_WERT", STAT_TEMP_ELUP_LE_WERT, "\"°C\"");
    float STAT_TEMP_PROZESSOR_MC0_WERT = (RXBUF_SINT(26)*0.0156f);
        // Temperature processor MC0 / Temperatur Prozessor MC0
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_PROZESSOR_MC0_WERT", STAT_TEMP_PROZESSOR_MC0_WERT, "\"°C\"");
    float STAT_TEMP_PROZESSOR_MC2_WERT = (RXBUF_SINT(28)*0.0156f);
        // Temperature processor MC2 / Temperatur Prozessor MC2
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_LE", "STAT_TEMP_PROZESSOR_MC2_WERT", STAT_TEMP_PROZESSOR_MC2_WERT, "\"°C\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_inv_temp->SetValue(
        max(STAT_TEMP_UMRICHTER_PHASE_U_WERT,
        max(STAT_TEMP_UMRICHTER_PHASE_V_WERT,
        /* max( */ STAT_TEMP_UMRICHTER_PHASE_W_WERT // ,
        /* STAT_TEMP_UMRICHTER_GT_WERT)*/ )), Celcius);  // FIXED: the GT temp is the hot one - left it out
    mt_i3_v_charge_temp_gatedriver->SetValue(STAT_TEMP_UMRICHTER_GT_WERT, Celcius);
    StdMetrics.ms_v_charge_temp->SetValue(
        max(STAT_TEMP_SLE_PFC_WERT,
        max(STAT_TEMP_SLE_GR_WERT,
        max(STAT_TEMP_SLE_GTW_WERT,
        STAT_TEMP_SLE_BO_WERT))), Celcius);

    break;
  }

  case I3_PID_EME_AE_TEMP_EMASCHINE: {
    if (datalen < 4) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EME_AE_TEMP_EMASCHINE", 4);
        break;
    }
    float STAT_TEMP1_E_MOTOR_WERT = (RXBUF_SINT(0)*0.0156f);
        // E-motor temperature 1 / E-Motor Temperatur 1
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_EMASCHINE", "STAT_TEMP1_E_MOTOR_WERT", STAT_TEMP1_E_MOTOR_WERT, "\"°C\"");
    float STAT_TEMP2_E_MOTOR_WERT = (RXBUF_SINT(2)*0.0156f);
        // E-motor temperature 2 / E-Motor Temperatur 2
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_TEMP_EMASCHINE", "STAT_TEMP2_E_MOTOR_WERT", STAT_TEMP2_E_MOTOR_WERT, "\"°C\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_mot_temp->SetValue(max(STAT_TEMP1_E_MOTOR_WERT, STAT_TEMP2_E_MOTOR_WERT), Celcius);

    break;
  }

  case I3_PID_EME_AE_ELEKTRISCHE_MASCHINE: {
    if (datalen < 7) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EME_AE_ELEKTRISCHE_MASCHINE", 7);
        break;
    }
    float STAT_ELEKTRISCHE_MASCHINE_DREHZAHL_WERT = (RXBUF_UINT(0)*0.5f-5000.0);
        // Speed of the electric machine / Drehzahl der E-Maschine
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_ELEKTRISCHE_MASCHINE", "STAT_ELEKTRISCHE_MASCHINE_DREHZAHL_WERT", STAT_ELEKTRISCHE_MASCHINE_DREHZAHL_WERT, "\"1/min\"");
    float STAT_ELEKTRISCHE_MASCHINE_IST_MOMENT_WERT = (RXBUF_SINT(2)*0.5f);
        // IS the moment of the e-machine / IST Moment der E-Maschine (moment?  torque?)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_ELEKTRISCHE_MASCHINE", "STAT_ELEKTRISCHE_MASCHINE_IST_MOMENT_WERT", STAT_ELEKTRISCHE_MASCHINE_IST_MOMENT_WERT, "\"Nm\"");
    float STAT_ELEKTRISCHE_MASCHINE_SOLL_MOMENT_WERT = (RXBUF_SINT(4)*0.5f);
        // SHOULD moment of the electric machine / SOLL Moment der E-Maschine
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EME", "AE_ELEKTRISCHE_MASCHINE", "STAT_ELEKTRISCHE_MASCHINE_SOLL_MOMENT_WERT", STAT_ELEKTRISCHE_MASCHINE_SOLL_MOMENT_WERT, "\"Nm\"");
    unsigned char STAT_ELEKTRISCHE_BETRIEBSART_NR = (RXBUF_UCHAR(6));
        // current operating mode of the e-machine / aktuelle Betriebsart der E-Maschine
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EME", "AE_ELEKTRISCHE_MASCHINE", "STAT_ELEKTRISCHE_BETRIEBSART_NR", STAT_ELEKTRISCHE_BETRIEBSART_NR, "\"0-n\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_mot_rpm->SetValue(STAT_ELEKTRISCHE_MASCHINE_DREHZAHL_WERT, Other);

    break;
  }

  // --- NBT --------------------------------------------------------------------------------------------------------

  case I3_PID_NBT_STATUS_SPEED: {                                                 // 0xD030
    if (datalen < 14) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_NBT_STATUS_SPEED", 14);
        break;
    }

    short STAT_WHEEL1_SENSOR_SPEED_WERT = (RXBUF_SINT(0));
        // Speed on bike 1 / Geschwindigkeit am Rad 1
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "NBT", "STATUS_SPEED", "STAT_WHEEL1_SENSOR_SPEED_WERT", STAT_WHEEL1_SENSOR_SPEED_WERT, "\"km/h\"");

    short STAT_WHEEL2_SENSOR_SPEED_WERT = (RXBUF_SINT(2));
        // Speed on bike 2 / Geschwindigkeit am Rad 2
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "NBT", "STATUS_SPEED", "STAT_WHEEL2_SENSOR_SPEED_WERT", STAT_WHEEL2_SENSOR_SPEED_WERT, "\"km/h\"");

    short STAT_WHEEL3_SENSOR_SPEED_WERT = (RXBUF_SINT(4));
        // Speed on the bike 3 / Geschwindigkeit am Rad 3
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "NBT", "STATUS_SPEED", "STAT_WHEEL3_SENSOR_SPEED_WERT", STAT_WHEEL3_SENSOR_SPEED_WERT, "\"km/h\"");

    short STAT_WHEEL4_SENSOR_SPEED_WERT = (RXBUF_SINT(6));
        // Speed on the bike 4 / Geschwindigkeit am Rad 4
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "NBT", "STATUS_SPEED", "STAT_WHEEL4_SENSOR_SPEED_WERT", STAT_WHEEL4_SENSOR_SPEED_WERT, "\"km/h\"");

    short STAT_COMBINED_SENSOR_SPEED_WERT = (RXBUF_SINT(8));
        // Combined speed of the wheel sensors / Kombinierte Geschwindigkeit der Radsensoren
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "NBT", "STATUS_SPEED", "STAT_COMBINED_SENSOR_SPEED_WERT", STAT_COMBINED_SENSOR_SPEED_WERT, "\"km/h\"");

    short STAT_GPS_SPEED_WERT = (RXBUF_SINT(10));
        // Speed based on the GPS signal. / Geschwindigkeit basierend auf das GPS-Signal.
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "NBT", "STATUS_SPEED", "STAT_GPS_SPEED_WERT", STAT_GPS_SPEED_WERT, "\"km/h\"");

    unsigned short STAT_SPEED_DIFFERENCE_PERCENT_WERT = (RXBUF_UINT(12));
        // Difference in percent between the speeds that were recorded with the wheel sensors or GPS / Abweichung in
        // Prozent zwischen den Geschwindigkeiten, die mit den Radsensoren bzw. mittels GPS erfasst wurden
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "NBT", "STATUS_SPEED", "STAT_SPEED_DIFFERENCE_PERCENT_WERT", STAT_SPEED_DIFFERENCE_PERCENT_WERT, "\"%\"");

    // ==========  Add your processing here ==========
    // TODO: Which wheel is which?
    mt_i3_wheel1_speed->SetValue(STAT_WHEEL1_SENSOR_SPEED_WERT, Kph);
    mt_i3_wheel2_speed->SetValue(STAT_WHEEL2_SENSOR_SPEED_WERT, Kph);
    mt_i3_wheel3_speed->SetValue(STAT_WHEEL3_SENSOR_SPEED_WERT, Kph);
    mt_i3_wheel4_speed->SetValue(STAT_WHEEL4_SENSOR_SPEED_WERT, Kph);
      // STAT_COMBINED_SENSOR_SPEED_WERT does't give a sensible anwer - so just compute a mean
    mt_i3_wheel_speed->SetValue((STAT_WHEEL1_SENSOR_SPEED_WERT+STAT_WHEEL2_SENSOR_SPEED_WERT+STAT_WHEEL3_SENSOR_SPEED_WERT+STAT_WHEEL4_SENSOR_SPEED_WERT)/4, Kph);

    break;
  }

  case I3_PID_NBT_STATUS_DIRECTION: {                                             // 0xD031
    if (datalen < 2) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_NBT_STATUS_DIRECTION", 2);
        break;
    }

    unsigned char STAT_DIRECTION = (RXBUF_SCHAR(0));
        // Current gear position see TGearType / aktuelle Gangstellung siehe TGearType
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "NBT", "STATUS_DIRECTION", "STAT_DIRECTION", STAT_DIRECTION, "\"0-n\"");

    unsigned char STAT_DIRECTION_SOURCE = (RXBUF_UCHAR(1));
        // see table TDirectionSource / siehe Tabelle TDirectionSource
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "NBT", "STATUS_DIRECTION", "STAT_DIRECTION_SOURCE", STAT_DIRECTION_SOURCE, "\"0-n\"");

    // ==========  Add your processing here ==========
    // 0: reverse, 1: drive, 2: neutral/park (find park status elsewhere)
    // OVMS wants -1 for reverse, 0 for neutral, 1 for forward
    StdMetrics.ms_v_env_gear->SetValue( STAT_DIRECTION == 0 ? -1 : STAT_DIRECTION == 2 ? 0 : 1);
    break;
  }

  // --- FZD --------------------------------------------------------------------------------------------------------

  case I3_PID_FZD_DWA_KLAPPENKONTAKTE: {                                          // 0xDCDD
    if (datalen < 12) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_FZD_DWA_KLAPPENKONTAKTE", 12);
        break;
    }

    unsigned char STAT_KONTAKT_FAHRERTUER_NR = (RXBUF_UCHAR(0));
        // Contact driver's door / Kontakt Fahrertür
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_KONTAKT_FAHRERTUER_NR", STAT_KONTAKT_FAHRERTUER_NR, "\"0-n\"");

    unsigned char STAT_KONTAKT_BEIFAHRERTUER_NR = (RXBUF_UCHAR(1));
        // Contact passenger door / Kontakt Beifahrertür
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_KONTAKT_BEIFAHRERTUER_NR", STAT_KONTAKT_BEIFAHRERTUER_NR, "\"0-n\"");

    unsigned char STAT_KONTAKT_FAHRERTUER_HINTEN_NR = (RXBUF_UCHAR(2));
        // Contact at the rear of the driver's door / Kontakt Fahrertür hinten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_KONTAKT_FAHRERTUER_HINTEN_NR", STAT_KONTAKT_FAHRERTUER_HINTEN_NR, "\"0-n\"");

    unsigned char STAT_KONTAKT_BEIFAHRERTUER_HINTEN_NR = (RXBUF_UCHAR(3));
        // Contact rear passenger door / Kontakt Beifahrertür hinten
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_KONTAKT_BEIFAHRERTUER_HINTEN_NR", STAT_KONTAKT_BEIFAHRERTUER_HINTEN_NR, "\"0-n\"");

    unsigned char STAT_KONTAKT_MOTORHAUBE_NR = (RXBUF_UCHAR(4));
        // Contact bonnet / Kontakt Motorhaube
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_KONTAKT_MOTORHAUBE_NR", STAT_KONTAKT_MOTORHAUBE_NR, "\"0-n\"");

    unsigned char STAT_KONTAKT_HECKKLAPPE_NR = (RXBUF_UCHAR(5));
        // Contact tailgate / Kontakt Heckklappe
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_KONTAKT_HECKKLAPPE_NR", STAT_KONTAKT_HECKKLAPPE_NR, "\"0-n\"");

    unsigned char STAT_KONTAKT_HECKSCHEIBE_NR = (RXBUF_UCHAR(6));
        // Contact rear window / Kontakt Heckscheibe
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_KONTAKT_HECKSCHEIBE_NR", STAT_KONTAKT_HECKSCHEIBE_NR, "\"0-n\"");

    unsigned char STAT_ZV_NR = (RXBUF_UCHAR(7));
        // Central locking status / Status Zentralverriegelung
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_ZV_NR", STAT_ZV_NR, "\"0-n\"");

    unsigned long STAT_RESERVE_WERT = (RXBUF_UINT32(8));
        // Reserve (not yet occupied) / Reserve (noch nicht belegt)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lu%s\n", "FZD", "DWA_KLAPPENKONTAKTE", "STAT_RESERVE_WERT", STAT_RESERVE_WERT, "\"HEX\"");

    // ==========  Add your processing here ==========
    //FIXME: which is the driver's side?  My car is RHD and so the front right is the drivers door
    //FIXME: how do I even know if the car is LHD or RHD?
    StdMetrics.ms_v_door_fr->SetValue(STAT_KONTAKT_FAHRERTUER_NR);
    StdMetrics.ms_v_door_fl->SetValue(STAT_KONTAKT_BEIFAHRERTUER_NR);
    StdMetrics.ms_v_door_rr->SetValue(STAT_KONTAKT_FAHRERTUER_HINTEN_NR);
    StdMetrics.ms_v_door_rl->SetValue(STAT_KONTAKT_BEIFAHRERTUER_HINTEN_NR);
    StdMetrics.ms_v_door_hood->SetValue(STAT_KONTAKT_MOTORHAUBE_NR);
    StdMetrics.ms_v_door_trunk->SetValue(STAT_KONTAKT_HECKKLAPPE_NR);
    StdMetrics.ms_v_env_locked->SetValue(!(STAT_ZV_NR&1)); // Bottom bit seems to say "at least one door unlocked"

    break;
  }

  // --- KLE --------------------------------------------------------------------------------------------------------

  case I3_PID_KLE_BETRIEBSZUSTAND_LADEGERAET: {                                   // 0xDE84
    if (datalen < 16) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KLE_BETRIEBSZUSTAND_LADEGERAET", 16);
        break;
    }

    float STAT_NETZFREQUENZ_PHASE_1_WERT = (RXBUF_UCHAR(0)/4.0f);
        // Current grid frequency phase 1 / Aktuelle Netzfrequenz Phase 1
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "STAT_NETZFREQUENZ_PHASE_1_WERT", STAT_NETZFREQUENZ_PHASE_1_WERT, "\"Hz\"");

    unsigned char BF_LADEGERAET_DERATING = (RXBUF_UCHAR(1));
        // Reason for derating / Grund für Derating
        // BF_LADEGERAET_DERATING is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_DERATING_BIT0: Mask: 0x01 - Overtemperature: 0 = not active; 1 = active
            // STAT_DERATING_BIT1: Mask: 0x02 - Mains frequency too low: 0 = not active; 1 = active
            // STAT_DERATING_BIT2: Mask: 0x04 - Failure of a charging module: 0 = not active; 1 = active
            // STAT_DERATING_BIT3: Mask: 0x08 - DC current limitation: 0 = not active; 1 = active
            // STAT_DERATING_BIT4: Mask: 0x10 - Network available power of lower nominal power: 0 = not active; 1 = active
            // STAT_DERATING_BIT5: Mask: 0x20 - Reserved
            // STAT_DERATING_BIT6: Mask: 0x40 - Reserved
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "BF_LADEGERAET_DERATING", (unsigned long)BF_LADEGERAET_DERATING, "\"Bit\"");

    float STAT_LEISTUNG_DERATING_WERT = (RXBUF_UINT(2)*10.0f);
        // Derating performance / Derating Leistung
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "STAT_LEISTUNG_DERATING_WERT", STAT_LEISTUNG_DERATING_WERT, "\"W\"");

    unsigned char STAT_DERATING_WERT = (RXBUF_UCHAR(4));
        // Derating / Derating
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "STAT_DERATING_WERT", STAT_DERATING_WERT, "\"%\"");

    unsigned char BF_LADEGERAET_FEHLERZUSTAND_UCXII = (RXBUF_UCHAR(5));
        // Charger fault conditions / Ladegerät Fehlerzustände
        // BF_LADEGERAET_FEHLERZUSTAND_UCXII is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_FEHLERZUSTAND_BIT0: Mask: 0x01 - Derating: 0 = not active; 1 = active
            // STAT_FEHLERZUSTAND_BIT1: Mask: 0x02 - Charge interruption: 0 = not active; 1 = active
            // STAT_FEHLERZUSTAND_BIT2: Mask: 0x04 - Failsafe: 0 = not active; 1 = active
            // STAT_FEHLERZUSTAND_BIT3: Mask: 0x08 - Communication failure: 0 = not active; 1 = active
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "BF_LADEGERAET_FEHLERZUSTAND_UCXII", (unsigned long)BF_LADEGERAET_FEHLERZUSTAND_UCXII, "\"Bit\"");

    unsigned short BF_AUSLOESER_FAILSAFE_UCXII = (RXBUF_UINT(6));
        // Failsafe trigger / Auslöser Failsafe
        // BF_AUSLOESER_FAILSAFE_UCXII is a BITFIELD of size unsigned int.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_AUSLOESER_FAILSAFE_BIT0: Mask: 0x001 - Hardware error: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT1: Mask: 0x002 - Undervoltage AC: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT2: Mask: 0x004 - Overvoltage AC: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT3: Mask: 0x008 - Overcurrent AC: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT4: Mask: 0x010 - Undervoltage DC: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT5: Mask: 0x020 - Overvoltage DC: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT6: Mask: 0x040 - Overcurrent DC: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT7: Mask: 0x080 - Temperature: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT8: Mask: 0x100 - Communication error: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT9: Mask: 0x200 - Load initialization timeout: 0 = not active; 1 = active
            // STAT_AUSLOESER_FAILSAFE_BIT10: Mask: 0x400 - Controller undervoltage: 0 = not active; 1 = active
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "BF_AUSLOESER_FAILSAFE_UCXII", (unsigned long)BF_AUSLOESER_FAILSAFE_UCXII, "\"Bit\"");

    unsigned char BF_LADEGERAET_URSACHE_LADEUNTERBRECHUNG = (RXBUF_UCHAR(8));
        // Cause of charging interruption / Ursache für Ladeunterbrechung
        // BF_LADEGERAET_URSACHE_LADEUNTERBRECHUNG is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_URSACHE_LADEUNTERBRECHUNG_BIT0: Mask: 0x01 - Forced separation of the plug: 0 = not active; 1 = active
            // STAT_URSACHE_LADEUNTERBRECHUNG_BIT1: Mask: 0x02 - AC voltage missing or grid connection unstable: 0 = not active; 1 = active
            // STAT_URSACHE_LADEUNTERBRECHUNG_BIT2: Mask: 0x04 - Connector not locked: 0 = not active; 1 = active
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "BF_LADEGERAET_URSACHE_LADEUNTERBRECHUNG", (unsigned long)BF_LADEGERAET_URSACHE_LADEUNTERBRECHUNG, "\"Bit\"");

    unsigned char STAT_BETRIEBSART_NR = (RXBUF_UCHAR(9));
        // Operating mode / Betriebsart
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "STAT_BETRIEBSART_NR", STAT_BETRIEBSART_NR, "\"0-n\"");

    float STAT_NETZFREQUENZ_PHASE_2_WERT = (RXBUF_UCHAR(10)/4.0f);
        // Current grid frequency phase 2 / Aktuelle Netzfrequenz Phase 2
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "STAT_NETZFREQUENZ_PHASE_2_WERT", STAT_NETZFREQUENZ_PHASE_2_WERT, "\"Hz\"");

    float STAT_NETZFREQUENZ_PHASE_3_WERT = (RXBUF_UCHAR(11)/4.0f);
        // Current grid frequency phase 3 / Aktuelle Netzfrequenz Phase 3
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "STAT_NETZFREQUENZ_PHASE_3_WERT", STAT_NETZFREQUENZ_PHASE_3_WERT, "\"Hz\"");

    unsigned long BF_MOD_ERR = (RXBUF_UINT32(12));
        // Error states load module / Fehlerzustände Lademodul
        // BF_MOD_ERR is a BITFIELD of size unsigned long.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_BF_DSP_ERR_HVDCV_OORL: Mask: 0x00000001 - PM HVDC voltage sensor outside lower threshold: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_HVDCV_OORH: Mask: 0x00000002 - PM HVDC voltage sensor outside upper threshold: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_HVDCI_OORH: Mask: 0x00000004 - PM HVDC current sensor outside upper threshold: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_IRES_OORH: Mask: 0x00000008 - PM sensor resonance current outside the upper threshold value: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_VBUS_OORL: Mask: 0x00000010 - PM sensor bus voltage outside the lower threshold value: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_VBUS_OORH: Mask: 0x00000020 - PM sensor bus voltage outside the upper threshold value: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_OVERTEMPERATURE: Mask: 0x00000040 - PM temperature sensor outside the upper threshold value: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_UNDERTEMPERATURE: Mask: 0x00000080 - PM temperature sensor outside lower threshold: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_HVDCV_SHGND: Mask: 0x00000100 - PM HVDC voltage sensor short circuit to ground: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_HVDCI_SHGND: Mask: 0x00000200 - PM HVDC current sensor short circuit to ground: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_AC_I_OORH: Mask: 0x00000400 - PM AC current sensor outside upper threshold: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_AC_I_SHGND: Mask: 0x00000800 - PM AC current sensor short circuit to ground: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_VAC_LX_SHBATT: Mask: 0x00001000 - PM AC Lx current sensor short circuit to positive: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_VAC_LX_SHGND: Mask: 0x00002000 - PM AC Lx current sensor short circuit to ground: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_IRES_SHBATT: Mask: 0x00004000 - PM sensor resonance current short circuit to plus: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_IRES_SHGND: Mask: 0x00008000 - PM sensor resonance current short circuit to ground: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_CAN_ALIVE: Mask: 0x00010000 - PM error test ALIVE: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_CAN_MSG401_TOUT: Mask: 0x00020000 - PM error timeout telegram 0x401: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_CAN_MSG300_TOUT: Mask: 0x00040000 - PM error timeout telegram 0x300: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_SPI_PROTOCOL: Mask: 0x00080000 - PM error SPI protocol: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_SPI_WIPER0_MISMATCH: Mask: 0x00100000 - PM error SPI Wiper0 discrepancy: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_SPI_WIPER1_MISMATCH: Mask: 0x00200000 - PM error SPI Wiper1 discrepancy: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_RAM: Mask: 0x00400000 - PM error RAM: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_WATCHDOG: Mask: 0x00800000 - PM watchdog error: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_STACKOVFL: Mask: 0x01000000 - PM stack overflow: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_DATA_INTEGRITY: Mask: 0x02000000 - PM data integrity: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_CLOCK: Mask: 0x04000000 - PM clock: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_CRC_CALIBRATION: Mask: 0x08000000 - PM CRC calibration: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_VAC_LY_SHBATT: Mask: 0x10000000 - PM AC Ly voltage sensor short circuit to plus: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_VAC_LY_SHGND: Mask: 0x20000000 - PM AC Ly voltage sensor short circuit to ground: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_VAC_LX_OORH: Mask: 0x40000000 - PM AC Lx voltage sensor outside upper threshold: 0 = not active; 1 = active
            // STAT_BF_DSP_ERR_VAC_LY_OORH: Mask: 0x80000000 - PM AC Ly voltage sensor outside upper threshold: 0 = not active; 1 = active
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "KLE", "BETRIEBSZUSTAND_LADEGERAET", "BF_MOD_ERR", (unsigned long)BF_MOD_ERR, "\"Bit\"");

    // ==========  Add your processing here ==========
    if (STAT_BETRIEBSART_NR == 2 || STAT_BETRIEBSART_NR == 3) {
        StdMetrics.ms_v_charge_inprogress->SetValue(true); // AC charging
    } else {
        // unless we are DC charging, we aren't charging...
        if (!mt_i3_v_charge_dc_inprogress->AsBool()) {
            StdMetrics.ms_v_charge_inprogress->SetValue(false);
        }
    }
    string state;
    // FIXME not convinced these are correctly mapped to the standard values
    switch (STAT_BETRIEBSART_NR) {
        case 1:
            state = "";  // Standby: does that always mean done?
            break;
        case 2:
            state = "charging";
            break;
        case 3:
            state = "derating"; // 
            break;
        case 4:
            state = "stopped";
            break;
        case 5:
            state = "error";
            break;
        case 6:
            state = "crash";
            break;
        case 7:
            state = "modechange";
            break;
        case 8:
            state = "prepare";
            break;
        default:
            state = "invalid";
            break;
    }
    StdMetrics.ms_v_charge_state->SetValue(state);
    mt_i3_v_charge_deratingreasons->SetValue(BF_LADEGERAET_DERATING);
    mt_i3_v_charge_faults->SetValue(BF_LADEGERAET_FEHLERZUSTAND_UCXII);
    mt_i3_v_charge_failsafetriggers->SetValue(BF_AUSLOESER_FAILSAFE_UCXII);
    mt_i3_v_charge_interruptionreasons->SetValue(BF_LADEGERAET_URSACHE_LADEUNTERBRECHUNG);
    mt_i3_v_charge_errors->SetValue(BF_MOD_ERR);

    break;
  }

  case I3_PID_KLE_LADEGERAET_LEISTUNG: {                                          // 0xDE85
    if (datalen < 4) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KLE_LADEGERAET_LEISTUNG", 4);
        break;
    }

    unsigned char STAT_WIRKUNGSGRAD_LADEZYKLUS_WERT = (RXBUF_UCHAR(0));
        // Charge cycle efficiency / Wirkungsgrad Ladezyklus
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KLE", "LADEGERAET_LEISTUNG", "STAT_WIRKUNGSGRAD_LADEZYKLUS_WERT", STAT_WIRKUNGSGRAD_LADEZYKLUS_WERT, "\"%\"");

    unsigned char STAT_WIRKUNGSGRAD_DC_WERT = (RXBUF_UCHAR(1));
        // Efficiency DC / Wirkungsgrad DC
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "KLE", "LADEGERAET_LEISTUNG", "STAT_WIRKUNGSGRAD_DC_WERT", STAT_WIRKUNGSGRAD_DC_WERT, "\"%\"");

    unsigned short STAT_LADEGERAET_DC_HV_LEISTUNG_WERT = (RXBUF_UINT(2));
        // Instantaneous power output in the intermediate circuit / Abgegebende Momentanleistung in den Zwischenkreis
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "KLE", "LADEGERAET_LEISTUNG", "STAT_LADEGERAET_DC_HV_LEISTUNG_WERT", STAT_LADEGERAET_DC_HV_LEISTUNG_WERT, "\"W\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_charge_efficiency->SetValue(STAT_WIRKUNGSGRAD_LADEZYKLUS_WERT, Percentage); // Seemed very low in my sample though

    break;
  }

  case I3_PID_KLE_LADEGERAET_SPANNUNG: {                                          // 0xDE86
    if (datalen < 12) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KLE_LADEGERAET_SPANNUNG", 12);
        break;
    }

    unsigned short STAT_SPANNUNG_RMS_AC_PHASE_1_WERT = (RXBUF_UINT(0));
        // RMS values of the AC conductor voltages (phase 1) / Effektivwerte der AC Leiterspannungen (Phase1)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "KLE", "LADEGERAET_SPANNUNG", "STAT_SPANNUNG_RMS_AC_PHASE_1_WERT", STAT_SPANNUNG_RMS_AC_PHASE_1_WERT, "\"V\"");

    float STAT_SPANNUNG_DC_HV_OBERGRENZE_WERT = (RXBUF_UINT(2)/10.0f);
        // HV DC voltage upper limit / HV DC Spannungsobergrenze
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_SPANNUNG", "STAT_SPANNUNG_DC_HV_OBERGRENZE_WERT", STAT_SPANNUNG_DC_HV_OBERGRENZE_WERT, "\"V\"");

    float STAT_SPANNUNG_DC_HV_WERT = (RXBUF_UINT(4)/10.0f);
        // HV DC voltage / HV DC Spannung
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_SPANNUNG", "STAT_SPANNUNG_DC_HV_WERT", STAT_SPANNUNG_DC_HV_WERT, "\"V\"");

    float STAT_SPANNUNG_KL30_WERT = (RXBUF_UCHAR(6)/10.0f);
        // KL30 voltage / KL30 Spannung
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_SPANNUNG", "STAT_SPANNUNG_KL30_WERT", STAT_SPANNUNG_KL30_WERT, "\"V\"");

    float STAT_SPANNUNG_KL30C_WERT = (RXBUF_UCHAR(7)/10.0f);
        // KL30C voltage / KL30C Spannung
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_SPANNUNG", "STAT_SPANNUNG_KL30C_WERT", STAT_SPANNUNG_KL30C_WERT, "\"V\"");

    unsigned short STAT_SPANNUNG_RMS_AC_PHASE_2_WERT = (RXBUF_UINT(8));
        // RMS values of the AC conductor voltages (phase 2) / Effektivwerte der AC Leiterspannungen (Phase 2)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "KLE", "LADEGERAET_SPANNUNG", "STAT_SPANNUNG_RMS_AC_PHASE_2_WERT", STAT_SPANNUNG_RMS_AC_PHASE_2_WERT, "\"V\"");

    unsigned short STAT_SPANNUNG_RMS_AC_PHASE_3_WERT = (RXBUF_UINT(10));
        // RMS values of the AC conductor voltages (phase 3) / Effektivwerte der AC Leiterspannungen (Phase 3)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "KLE", "LADEGERAET_SPANNUNG", "STAT_SPANNUNG_RMS_AC_PHASE_3_WERT", STAT_SPANNUNG_RMS_AC_PHASE_3_WERT, "\"V\"");

    // ==========  Add your processing here ==========
    if (STAT_SPANNUNG_RMS_AC_PHASE_2_WERT > STAT_SPANNUNG_RMS_AC_PHASE_1_WERT-25) {
        if (STAT_SPANNUNG_RMS_AC_PHASE_3_WERT > STAT_SPANNUNG_RMS_AC_PHASE_1_WERT-25) {
            StdMetrics.ms_v_charge_voltage->SetValue(
                (STAT_SPANNUNG_RMS_AC_PHASE_1_WERT+STAT_SPANNUNG_RMS_AC_PHASE_2_WERT+STAT_SPANNUNG_RMS_AC_PHASE_3_WERT)/3, Volts);
        } else {
            StdMetrics.ms_v_charge_voltage->SetValue(
                (STAT_SPANNUNG_RMS_AC_PHASE_1_WERT+STAT_SPANNUNG_RMS_AC_PHASE_2_WERT)/2, Volts);
        }
    } else {
        StdMetrics.ms_v_charge_voltage->SetValue(STAT_SPANNUNG_RMS_AC_PHASE_1_WERT, Volts);
    }
    mt_i3_v_charge_voltage_phase1->SetValue(STAT_SPANNUNG_RMS_AC_PHASE_1_WERT, Volts);
    mt_i3_v_charge_voltage_phase2->SetValue(STAT_SPANNUNG_RMS_AC_PHASE_2_WERT, Volts);
    mt_i3_v_charge_voltage_phase3->SetValue(STAT_SPANNUNG_RMS_AC_PHASE_3_WERT, Volts);
    mt_i3_v_charge_voltage_dc->SetValue(STAT_SPANNUNG_DC_HV_WERT, Volts);
    mt_i3_v_charge_voltage_dc_limit->SetValue(STAT_SPANNUNG_DC_HV_OBERGRENZE_WERT, Volts);


    break;
  }

  case I3_PID_KLE_LADEGERAET_STROM: {                                             // 0xDE87
    if (datalen < 14) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_KLE_LADEGERAET_STROM", 14);
        break;
    }

    float STAT_STROM_AC_PHASE_1_WERT = (RXBUF_UINT(0)/10.0f);
        // AC current phase 1 / AC Strom Phase 1
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_STROM", "STAT_STROM_AC_PHASE_1_WERT", STAT_STROM_AC_PHASE_1_WERT, "\"A\"");

    float STAT_STROM_AC_MAX_GESPEICHERT_WERT = (RXBUF_UINT(2)/10.0f);
        // Maximum stored AC current / Maximal gespeicherter AC Strom
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_STROM", "STAT_STROM_AC_MAX_GESPEICHERT_WERT", STAT_STROM_AC_MAX_GESPEICHERT_WERT, "\"A\"");

    float STAT_STROM_HV_DC_WERT = (RXBUF_UINT(4)/10.0f-204.7);
        // HV DC electricity / HV DC Strom
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_STROM", "STAT_STROM_HV_DC_WERT", STAT_STROM_HV_DC_WERT, "\"A\"");

    float STAT_STROM_HV_DC_MAX_LIMIT_WERT = (RXBUF_UINT(6)/10.0f);
        // Maximum permitted HV DC current / Maximal erlaubter HV DC Strom
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_STROM", "STAT_STROM_HV_DC_MAX_LIMIT_WERT", STAT_STROM_HV_DC_MAX_LIMIT_WERT, "\"A\"");

    float STAT_STROM_HV_DC_MAX_GESPEICHERT_WERT = (RXBUF_UINT(8)/10.0f-204.7);
        // Maximum stored DC current / Maximal gespeicherter DC Strom
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_STROM", "STAT_STROM_HV_DC_MAX_GESPEICHERT_WERT", STAT_STROM_HV_DC_MAX_GESPEICHERT_WERT, "\"A\"");

    float STAT_STROM_AC_PHASE_2_WERT = (RXBUF_UINT(10)/10.0f);
        // AC current phase 2 / AC Strom Phase 2
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_STROM", "STAT_STROM_AC_PHASE_2_WERT", STAT_STROM_AC_PHASE_2_WERT, "\"A\"");

    float STAT_STROM_AC_PHASE_3_WERT = (RXBUF_UINT(12)/10.0f);
        // AC current phase 3 / AC Strom Phase 3
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "KLE", "LADEGERAET_STROM", "STAT_STROM_AC_PHASE_3_WERT", STAT_STROM_AC_PHASE_3_WERT, "\"A\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_charge_current->SetValue(STAT_STROM_AC_PHASE_1_WERT+STAT_STROM_AC_PHASE_2_WERT+STAT_STROM_AC_PHASE_3_WERT, Amps);
    //FIXME: should the climit be multiplied by 3 for 3phase charging?
    StdMetrics.ms_v_charge_climit->SetValue(STAT_STROM_AC_MAX_GESPEICHERT_WERT, Amps);  // Think this is what the EVSE signals with the pilot signal
    mt_i3_v_charge_current_phase1->SetValue(STAT_STROM_AC_PHASE_1_WERT, Amps);
    mt_i3_v_charge_current_phase2->SetValue(STAT_STROM_AC_PHASE_2_WERT, Amps);
    mt_i3_v_charge_current_phase3->SetValue(STAT_STROM_AC_PHASE_3_WERT, Amps);
    if (STAT_STROM_HV_DC_WERT > -0.01f && STAT_STROM_HV_DC_WERT < 0.01f) {
        mt_i3_v_charge_current_dc->SetValue(0.0f, Amps);
    } else {
        mt_i3_v_charge_current_dc->SetValue(STAT_STROM_HV_DC_WERT, Amps);
    }
    if (STAT_STROM_HV_DC_MAX_GESPEICHERT_WERT > -0.01f && STAT_STROM_HV_DC_MAX_GESPEICHERT_WERT < 0.01f) {
        mt_i3_v_charge_current_dc_limit->SetValue(0.0f, Amps);
    } else {
        mt_i3_v_charge_current_dc_limit->SetValue(STAT_STROM_HV_DC_MAX_GESPEICHERT_WERT, Amps);
    }
    if (STAT_STROM_HV_DC_MAX_LIMIT_WERT > -0.01f && STAT_STROM_HV_DC_MAX_LIMIT_WERT < 0.01f) {
        mt_i3_v_charge_current_dc_maxlimit->SetValue(0.0f, Amps);
    } else {
        mt_i3_v_charge_current_dc_maxlimit->SetValue(STAT_STROM_HV_DC_MAX_LIMIT_WERT, Amps);
    }

    break;
  }

  // --- EDM --------------------------------------------------------------------------------------------------------

  case I3_PID_EDM_STATUS_MESSWERTE_IBS: {                                              // 0xDF25
    if (datalen < 6) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected at least %d", datalen, "I3_PID_EDM_STATUS_MESSWERTE_IBS", 6);
        break;
    }

    float STAT_I_BATT_WERT = I3_RES_EDM_STAT_I_BATT_WERT;
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.2f%s\n", "EDM", "STATUS_MESSWERTE_IBS", "STAT_I_BAT_WERT", STAT_I_BATT_WERT, "A");
    StdMetrics.ms_v_bat_12v_current->SetValue(STAT_I_BATT_WERT, Amps);
    StdMetrics.ms_v_env_charging12v->SetValue( (STAT_I_BATT_WERT < -0.1f) );

    break;
  }

case I3_PID_EDM_PEDALWERTGEBER: {                                               // 0xDE9C
    if (datalen < 6) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EDM_PEDALWERTGEBER", 6);
        break;
    }

    float STAT_SPANNUNG_PEDALWERT1_WERT = (RXBUF_UINT(0)*0.0049f);
        // Voltage measured at pedal encoder 1 / Spannung gemessen am Pedalwertgeber 1
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EDM", "PEDALWERTGEBER", "STAT_SPANNUNG_PEDALWERT1_WERT", STAT_SPANNUNG_PEDALWERT1_WERT, "\"V\"");

    float STAT_SPANNUNG_PEDALWERT2_WERT = (RXBUF_UINT(2)*0.0049f);
        // Voltage measured at the pedal encoder 2 / Spannung gemessen am Pedalwertgeber 2
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EDM", "PEDALWERTGEBER", "STAT_SPANNUNG_PEDALWERT2_WERT", STAT_SPANNUNG_PEDALWERT2_WERT, "\"V\"");

    float STAT_PEDALWERT_WERT = (RXBUF_UINT(4)*0.0625f);
        // Pedal value determined from pedal value sensors 1 and 2 / Aus Pedalwertgeber 1 und 2 ermittelter Pedalwert
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EDM", "PEDALWERTGEBER", "STAT_PEDALWERT_WERT", STAT_PEDALWERT_WERT, "\"%\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_env_throttle->SetValue(STAT_PEDALWERT_WERT);

    break;
  }



// --- LIM --------------------------------------------------------------------------------------------------------

  case I3_PID_LIM_STATUS_LADEKLAPPE: {
    if (datalen < 2) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected at least %d", datalen, "I3_PID_LIM_STATUS_LADEKLAPPE", 2);
        break;
    }

    unsigned short STAT_LADEKLAPPE = I3_RES_LIM_STAT_LADEKLAPPE;
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "LIM", "I3_PID_LIM_STATUS_LADEKLAPPE", "STAT_LADEKLAPPE", STAT_LADEKLAPPE, "");
    StdMetrics.ms_v_door_chargeport->SetValue( (!STAT_LADEKLAPPE) );

    break;
  }

  case I3_PID_LIM_LED_LADESTATUS: {
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected at least %d", datalen, "I3_PID_LIM_LED_LADESTATUS", 1);
        break;
    }

// From the owners manual:
// ▷ Lamp white: charging cable can be connected or removed.
// ▷ Lamp flashes yellow: charging process is being initialized
// ▷ Lamp blue: charging process is started at a set time.
// ▷ Lamp flashes blue: charging process active.  (state 3)
// ▷ Lamp flashes red: fault in the charging process.
// ▷ Lamp green: charging process completed.
    // 6 - mauve --> charger socket unlocked?  Nothing happened when i plugged in a connector with the evse "sleeping"
    // 0 - car unlocked door open but charge flap closed
    // 6 - mauve - 
    // 1 - plugged charger in, led was blinking red
    // 3 - blinking blue, charging.

    unsigned short LED_LADESTATUS = I3_RES_LIM_LED_LADESTATUS;
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "LIM", "I3_PID_LIM_LED_LADESTATUS", "LED_LADESTATUS", LED_LADESTATUS, "");
    mt_i3_v_charge_chargeledstate->SetValue(LED_LADESTATUS);

    break;
  }

   case I3_PID_LIM_LADEBEREITSCHAFT_LIM: {                                         // 0xDEF2
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_LIM_LADEBEREITSCHAFT_LIM", 1);
        break;
    }

    unsigned char STAT_LADEBEREITSCHAFT_LIM = (RXBUF_UCHAR(0));
        // Ready to charge (HW line), (1 = yes, 0 = no) sent from LIM to SLE / Ladebereitschaft (HW-Leitung), (1 = ja, 0
        // = nein) vom LIM an SLE gesendet
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "LADEBEREITSCHAFT_LIM", "STAT_LADEBEREITSCHAFT_LIM", STAT_LADEBEREITSCHAFT_LIM, "\"0/1\"");

    // ==========  Add your processing here ==========
    mt_i3_v_charge_readytocharge->SetValue(STAT_LADEBEREITSCHAFT_LIM);

    break;
  }

  case I3_PID_LIM_PILOTSIGNAL: {                                                  // 0xDEF6
    if (datalen < 7) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_LIM_PILOTSIGNAL", 7);
        break;
    }

    unsigned char STAT_PILOT_AKTIV = (RXBUF_UCHAR(0));
        // State of the pilot signal (0 = not active, 1 = active) / Zustand des Pilotsignals (0 = nicht aktiv, 1 = aktiv)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "PILOTSIGNAL", "STAT_PILOT_AKTIV", STAT_PILOT_AKTIV, "\"0/1\"");

    unsigned char STAT_PILOT_PWM_DUTYCYCLE_WERT = (RXBUF_UCHAR(1));
        // Pulse duty factor PWM pilot signal / Tastverhältnis PWM Pilotsignal
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "PILOTSIGNAL", "STAT_PILOT_PWM_DUTYCYCLE_WERT", STAT_PILOT_PWM_DUTYCYCLE_WERT, "\"%\"");

    unsigned char STAT_PILOT_CURRENT_WERT = (RXBUF_UCHAR(2));
        // Current value calculated from the pilot signal / Errechneter Stromwert aus Pilotsignal
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "PILOTSIGNAL", "STAT_PILOT_CURRENT_WERT", STAT_PILOT_CURRENT_WERT, "\"A\"");

    unsigned char STAT_PILOT_LADEBEREIT = (RXBUF_UCHAR(3));
        // Vehicle ready to charge state (0 = not ready to charge, 1 = ready to charge) / Zustand Ladebereitschaft
        // Fahrzeug (0 = nicht ladebereit, 1 = ladebereit)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "PILOTSIGNAL", "STAT_PILOT_LADEBEREIT", STAT_PILOT_LADEBEREIT, "\"0/1\"");

    unsigned short STAT_PILOT_FREQUENZ_WERT = (RXBUF_UINT(4));
        // Frequency of the pilot signal / Frequenz des Pilotsignals
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%u%s\n", "LIM", "PILOTSIGNAL", "STAT_PILOT_FREQUENZ_WERT", STAT_PILOT_FREQUENZ_WERT, "\"Hz\"");

    float STAT_PILOT_PEGEL_WERT = (RXBUF_UCHAR(6)/10.0f);
        // Pilot signal level / Pegel des Pilotsignals
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "LIM", "PILOTSIGNAL", "STAT_PILOT_PEGEL_WERT", STAT_PILOT_PEGEL_WERT, "\"V\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_charge_pilot->SetValue(STAT_PILOT_AKTIV);
    mt_i3_v_charge_pilotsignal->SetValue(STAT_PILOT_CURRENT_WERT, Amps);

    break;
  }

  case I3_PID_LIM_PROXIMITY: {                                                    // 0xDEF5
    if (datalen < 2) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_LIM_PROXIMITY", 2);
        break;
    }

    unsigned char STAT_STECKER_NR = (RXBUF_UCHAR(0));
        // Condition of the plug / Zustand des Steckers
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "PROXIMITY", "STAT_STECKER_NR", STAT_STECKER_NR, "\"0-n\"");

    unsigned char STAT_STROMTRAGFAEHIGKEIT_WERT = (RXBUF_UCHAR(1));
        // Current carrying capacity of the connected cable / Stromtragfähigkeit des angeschlossenen Kabels
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "PROXIMITY", "STAT_STROMTRAGFAEHIGKEIT_WERT", STAT_STROMTRAGFAEHIGKEIT_WERT, "\"A\"");

    // ==========  Add your processing here ==========
    mt_i3_v_charge_plugstatus->SetValue(
        (STAT_STECKER_NR == 0) ? "Not connected" :
        (STAT_STECKER_NR == 1) ? "Connected" :
        (STAT_STECKER_NR == 2) ? "Connected, unlocked" :
                                 "Invalid state"
    );
    mt_i3_v_charge_cablecapacity->SetValue(STAT_STROMTRAGFAEHIGKEIT_WERT, Amps);

    break;
  }

  case I3_PID_LIM_LADESCHNITTSTELLE_DC_TEPCO: {                                   // 0xDEF7
    if (datalen < 4) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_LIM_LADESCHNITTSTELLE_DC_TEPCO", 4);
        break;
    }

    unsigned char STAT_CHARGE_CONTROL_1 = (RXBUF_UCHAR(0));
        // Charge control status 1 line (0 = not active, 1 = active) / Zustand Charge control 1 Leitung (0 = nicht aktiv,
        // 1 = aktiv)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "LADESCHNITTSTELLE_DC_TEPCO", "STAT_CHARGE_CONTROL_1", STAT_CHARGE_CONTROL_1, "\"0/1\"");

    unsigned char STAT_CHARGE_CONTROL_2 = (RXBUF_UCHAR(1));
        // Charge control status 2 line (0 = not active, 1 = active) / Zustand Charge control 2 Leitung (0 = nicht aktiv,
        // 1 = aktiv)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "LADESCHNITTSTELLE_DC_TEPCO", "STAT_CHARGE_CONTROL_2", STAT_CHARGE_CONTROL_2, "\"0/1\"");

    unsigned char STAT_CHARGE_PERMISSION = (RXBUF_UCHAR(2));
        // Charge permission line status (0 = not active, 1 = active) / Zustand Charge Permission Leitung (0 = nicht
        // aktiv, 1 = aktiv)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "LADESCHNITTSTELLE_DC_TEPCO", "STAT_CHARGE_PERMISSION", STAT_CHARGE_PERMISSION, "\"0/1\"");

    unsigned char STAT_LADESTECKER = (RXBUF_UCHAR(3));
        // State of charging plug (0 = not plugged in, 1 = plugged in) / Zustand Ladestecker (0 = nicht gesteckt, 1 =
        // gesteckt)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "LADESCHNITTSTELLE_DC_TEPCO", "STAT_LADESTECKER", STAT_LADESTECKER, "\"0/1\"");

    // ==========  Add your processing here ==========
    // FIXME: these don't work?
    mt_i3_v_charge_dc_plugconnected->SetValue(STAT_LADESTECKER);
    mt_i3_v_charge_dc_controlsignals->SetValue( (STAT_LADESTECKER&1 << 3) | (STAT_CHARGE_PERMISSION&1 << 2) | (STAT_CHARGE_CONTROL_2&1 << 1) | (STAT_CHARGE_CONTROL_1&1) );

    break;
  }

  case I3_PID_LIM_DC_SCHUETZ_SCHALTER: {                                          // 0xDEF8
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_LIM_DC_SCHUETZ_SCHALTER", 1);
        break;
    }

    unsigned char STAT_DC_SCHUETZ_SCHALTER = (RXBUF_UCHAR(0));
        // Contactor switch status (DC charging) / Status Schützschalter (DC-Laden)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "DC_SCHUETZ_SCHALTER", "STAT_DC_SCHUETZ_SCHALTER", STAT_DC_SCHUETZ_SCHALTER, "\"0-n\"");

    // ==========  Add your processing here ==========
    mt_i3_v_charge_dc_contactorstatus->SetValue(
        (STAT_DC_SCHUETZ_SCHALTER == 0) ? "open" :
        (STAT_DC_SCHUETZ_SCHALTER == 1) ? "closed" :
        (STAT_DC_SCHUETZ_SCHALTER == 2) ? "contacts welded" :
        (STAT_DC_SCHUETZ_SCHALTER == 3) ? "diagnostics running" :
                                          "invalid"
    );
    // If the contactor is closed, led status is ?, and we have current flowing into the battery looks like we are DC charging
    if (STAT_DC_SCHUETZ_SCHALTER == 1 && StdMetrics.ms_v_bat_current->AsFloat() < -1.0f) {
        ESP_LOGI(TAG, "DC charge started");
        StdMetrics.ms_v_charge_inprogress->SetValue(true);
        mt_i3_v_charge_dc_inprogress->SetValue(true);
        if (StdMetrics.ms_v_charge_state->AsString().compare("") == 0) {
            StdMetrics.ms_v_charge_state->SetValue("charging");
        }
    }
    else if (STAT_DC_SCHUETZ_SCHALTER == 0 && mt_i3_v_charge_dc_inprogress->AsBool()) {
        ESP_LOGI(TAG, "DC charge stopped");
        mt_i3_v_charge_dc_inprogress->SetValue(false);
        StdMetrics.ms_v_charge_inprogress->SetValue(false);
        if (StdMetrics.ms_v_charge_state->AsString().compare("charging") == 0) {
            StdMetrics.ms_v_charge_state->SetValue("");
        }
    }
    
    break;
  }

  case I3_PID_LIM_DC_PINABDECKUNG_COMBO: {                                        // 0xDEFA
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_LIM_DC_PINABDECKUNG_COMBO", 1);
        break;
    }

    unsigned char STAT_DC_PINABDECKUNG = (RXBUF_UCHAR(0));
        // State of the DC pin cover for combo socket (0 = closed, 1 = open) / Zustand der DC Pinabdeckung bei
        // Combo-Steckdose (0 = geschlossen, 1 = geöffnet)
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "LIM", "DC_PINABDECKUNG_COMBO", "STAT_DC_PINABDECKUNG", STAT_DC_PINABDECKUNG, "\"0/1\"");

    // ==========  Add your processing here ==========
    // FIXME: doesn't seem to work
    mt_i3_v_door_dc_chargeport->SetValue( !STAT_DC_PINABDECKUNG );

    break;
  }

  // --- BDC --------------------------------------------------------------------------------------------------------

    // FIXME:
    // 2020-12-28 14:58:28 SAST D (468176) vehicle: PollerSend(1): send [bus=0, type=22, pid=D130], expecting 6f140/640f1-640f1
    // 2020-12-28 14:58:28 SAST D (468176) vehicle: PollerReceive[640F1]: process OBD/UDS error 22(D130) code=22

    case I3_PID_BDC_HANDBREMSE_KONTAKT: {                                           // 0xD130
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_BDC_HANDBREMSE_KONTAKT", 1);
        break;
    }

    unsigned char STAT_HANDBREMSE_KONTAKT_EIN = (RXBUF_UCHAR(0));
        // 0: handbrake released; 1: handbrake applied / 0: Handbremse gelöst; 1: Handbremse angezogen
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "BDC", "HANDBREMSE_KONTAKT", "STAT_HANDBREMSE_KONTAKT_EIN", STAT_HANDBREMSE_KONTAKT_EIN, "\"0/1\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_env_handbrake->SetValue(STAT_HANDBREMSE_KONTAKT_EIN);

    break;
  }

  case I3_PID_BDC_VIN: {
      if (datalen != 17) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_BDC_VIN", 17);
        break;
      }
      StdMetrics.ms_v_vin->SetValue(rxbuf);
  }

  // --- IHX --------------------------------------------------------------------------------------------------------

case I3_PID_IHX_TEMP_INNEN_UNBELUEFTET: {                                       // 0xD85C
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_IHX_TEMP_INNEN_UNBELUEFTET", 1);
        break;
    }

    char STAT_TEMP_INNEN_WERT = (RXBUF_SCHAR(0));
        // Calculated internal temperature / Errechnete Innentemperatur
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "IHX", "TEMP_INNEN_UNBELUEFTET", "STAT_TEMP_INNEN_WERT", STAT_TEMP_INNEN_WERT, "\"°C\"");

    // ==========  Add your processing here ==========
    if (STAT_TEMP_INNEN_WERT <= 60) {
        StdMetrics.ms_v_env_cabintemp->SetValue(STAT_TEMP_INNEN_WERT, Celcius);
    }

    break;
  }

  case I3_PID_IHX_EKMV_BETRIEBSZUSTAND_GEN20: {                                   // 0xD8C5
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_IHX_EKMV_BETRIEBSZUSTAND_GEN20", 1);
        break;
    }

    unsigned char RES_0xD8C5_D = (RXBUF_UCHAR(0));
        // Operating states of refrigerant compressor Gen. 2.0 / Betriebszustände von Kältemittelverdichter Gen. 2.0
            // RES_0xD8C5_D is a BITFIELD of unknown size.  We don't have definitions for each bit, and we GUESSED it is one byte ***
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "IHX", "EKMV_BETRIEBSZUSTAND_GEN20", "RES_0xD8C5_D", (unsigned long)RES_0xD8C5_D, "\"Bit\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_env_cooling->SetValue( (RES_0xD8C5_D == 8)); // It's bits, also see 0x28 which is suspect is that compressor is working but user has turned ac off?

    break;
  }

  case I3_PID_IHX_KLIMA_VORN_LUFTVERTEILUNG_LI_RE: {                              // 0xD91A
    if (datalen < 2) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_IHX_KLIMA_VORN_LUFTVERTEILUNG_LI_RE", 2);
        break;
    }

    unsigned char STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR = (RXBUF_UCHAR(0));
        // 1 = DOWN; 2 = CENTER; 3 = CENTER_BOTTOM; 4 = UP; 5 = TOP_UNTEN (driver only); 6 = TOP_MITTE; 7 =
        // TOP_MITTE_BOTTOM; 8 = AUTO; 32 = INDIVIDUAL; 40 = SPECIAL PROGRAM; 255 = INVALID (BASE); / 1=UNTEN; 2=MITTE;
        // 3=MITTE_UNTEN; 4=OBEN; 5=OBEN_UNTEN (Nur Fahrer); 6=OBEN_MITTE; 7=OBEN_MITTE_UNTEN; 8=AUTO; 32=INDIVIDUAL;
        // 40=SONDERPROGRAMM; 255=UNGUELTIG (BASIS);
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "IHX", "KLIMA_VORN_LUFTVERTEILUNG_LI_RE", "STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR", STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR, "\"0-n\"");

    unsigned char STAT_KLIMA_VORN_LUFTVERTEILUNG_RECHTS_NR = (RXBUF_UCHAR(1));
        // 1 = DOWN; 2 = CENTER; 3 = CENTER_BOTTOM; 4 = UP; 5 = TOP_UNTEN (driver only); 6 = TOP_MITTE; 7 =
        // TOP_MITTE_BOTTOM; 8 = AUTO; 32 = INDIVIDUAL; 40 = SPECIAL PROGRAM; 255 = INVALID (BASE); / 1=UNTEN; 2=MITTE;
        // 3=MITTE_UNTEN; 4=OBEN; 5=OBEN_UNTEN (Nur Fahrer); 6=OBEN_MITTE; 7=OBEN_MITTE_UNTEN; 8=AUTO; 32=INDIVIDUAL;
        // 40=SONDERPROGRAMM; 255=UNGUELTIG (BASIS);
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "IHX", "KLIMA_VORN_LUFTVERTEILUNG_LI_RE", "STAT_KLIMA_VORN_LUFTVERTEILUNG_RECHTS_NR", STAT_KLIMA_VORN_LUFTVERTEILUNG_RECHTS_NR, "\"0-n\"");

    // ==========  Add your processing here ==========
    // v.e.cabinvent                            feet,face                Cabin vent type (comma-separated list of feet, face, screen, etc)
    std::string status;
    if (STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR == 255 || STAT_KLIMA_VORN_LUFTVERTEILUNG_RECHTS_NR == 255) {
        status.append("invalid");
    } else if (STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR != STAT_KLIMA_VORN_LUFTVERTEILUNG_RECHTS_NR) {
        status.append("left_differs_from_right");
    } else {
        if (STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR&8)  status.append("auto,");
        if (STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR&32) status.append("individual,");
        if (STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR&1)  status.append("feet,");
        if (STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR&2)  status.append("face,");
        if (STAT_KLIMA_VORN_LUFTVERTEILUNG_LINKS_NR&4)  status.append("screen,");
        if (status.size() > 0) status.resize (status.size() - 1);
    }
    StdMetrics.ms_v_env_cabinvent->SetValue(status);

    break;
  }

  case I3_PID_IHX_KLIMA_VORN_OFF_EIN: {                                           // 0xD92C
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_IHX_KLIMA_VORN_OFF_EIN", 1);
        break;
    }

    unsigned char STAT_KLIMA_VORN_OFF_EIN = (RXBUF_UCHAR(0));
        // Function status air conditioning OFF: 0 = OFF = air conditioning is switched on, LED is off 1 = ON = air
        // conditioning is switched off, LED is on / Funktionsstatus Klima OFF: 0 = AUS = Klima ist eingeschaltet, LED
        // ist aus 1 = EIN = Klima ist ausgeschaltet, LED ist an
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "IHX", "KLIMA_VORN_OFF_EIN", "STAT_KLIMA_VORN_OFF_EIN", STAT_KLIMA_VORN_OFF_EIN, "\"0/1\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_env_hvac->SetValue( (STAT_KLIMA_VORN_OFF_EIN == 0) );

    break;
  }

  case I3_PID_IHX_KLIMA_VORN_PRG_AUC_EIN: {                                       // 0xD930
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_IHX_KLIMA_VORN_PRG_AUC_EIN", 1);
        break;
    }

    unsigned char STAT_KLIMA_VORN_PRG_AUC_EIN = (RXBUF_UCHAR(0));
        // Automatic air circulation control: 0 = OFF, 1 = ON / Automatische Umluft Control: 0 = AUS, 1 = EIN
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "IHX", "KLIMA_VORN_PRG_AUC_EIN", "STAT_KLIMA_VORN_PRG_AUC_EIN", STAT_KLIMA_VORN_PRG_AUC_EIN, "\"0/1\"");

    // ==========  Add your processing here ==========
    mt_i3_v_env_autorecirc->SetValue(STAT_KLIMA_VORN_PRG_AUC_EIN);

    break;
  }

  case I3_PID_IHX_KLIMA_VORN_PRG_UMLUFT_EIN: {                                    // 0xD931
    if (datalen < 1) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_IHX_KLIMA_VORN_PRG_UMLUFT_EIN", 1);
        break;
    }

    unsigned char STAT_KLIMA_VORN_PRG_UMLUFT_EIN = (RXBUF_UCHAR(0));
        // Recirculation program: 0 = OFF, 1 = ON / Programm Umluft: 0 = AUS, 1 = EIN
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "IHX", "KLIMA_VORN_PRG_UMLUFT_EIN", "STAT_KLIMA_VORN_PRG_UMLUFT_EIN", STAT_KLIMA_VORN_PRG_UMLUFT_EIN, "\"0/1\"");

    // ==========  Add your processing here ==========
    StdMetrics.ms_v_env_cabinintake->SetValue( STAT_KLIMA_VORN_PRG_UMLUFT_EIN ? "recirc" : "fresh" );

    break;
  }



  // --- EPS --------------------------------------------------------------------------------------------------------

  case I3_PID_EPS_EPS_MOMENTENSENSOR: {                                           // 0xDB99
    if (datalen < 3) {
        ESP_LOGV(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EPS_EPS_MOMENTENSENSOR", 3);
        break;
    }

    float STAT_MOMENT_WERT = (RXBUF_SINT(0)/128.0f);
        // Current moment / Aktuelles Moment
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EPS", "EPS_MOMENTENSENSOR", "STAT_MOMENT_WERT", STAT_MOMENT_WERT, "\"Nm\"");

    char STAT_SENSOR_ZUSTAND_NR_0XDB99 = (RXBUF_SCHAR(2));
        // State of the steering torque sensor / Zustand Sensor Lenkmoment
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "EPS_MOMENTENSENSOR", "STAT_SENSOR_ZUSTAND_NR_0XDB99", STAT_SENSOR_ZUSTAND_NR_0XDB99, "\"0-n\"");

    // ==========  Add your processing here ==========
    // So we are only polling this because the EPS (power steering) is only powered when the car is READY
    // So - we poll it and if we get a reply we know the car is "on".
    // The rest of the handling for this is found in Ticker10
    if (STAT_MOMENT_WERT > -20.0f && STAT_MOMENT_WERT < 20.0f) { // Ignore unreasonable figures we see sometimes.
        ++eps_messages;
    }

    break;
  }

  // Unknown: output if for review
  default: {
    hexdump(rxbuf, type, pid);
    break;
  }

  } /* switch */
}

class OvmsVehicleBMWi3Init
{
  public: OvmsVehicleBMWi3Init();
}
MyOvmsVehicleBMWi3Init  __attribute__ ((init_priority (9000)));

OvmsVehicleBMWi3Init::OvmsVehicleBMWi3Init()
{
  ESP_LOGI(TAG, "Registering Vehicle: BMW i3 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleBMWi3>("BMWI3", "BMW i3, i3s");
}

