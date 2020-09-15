/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX

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

/*
;    Subproject:    Integration of support for the VW e-UP
;    Date:          14st September 2020
;
;    Changes:
;    0.1.0  Initial code
:           Code frame with correct TAG and vehicle/can registration
;
;    0.1.1  Started with some SOC capture demo code
;
;    0.1.2  Added VIN, speed, 12 volt battery detection
;
;    0.1.3  Added ODO (Dimitrie78), fixed speed
;
;    0.1.4  Added WLTP based ideal range, uncertain outdoor temperature
;
;    0.1.5  Finalized SOC calculation (sharkcow), added estimated range
;
;    0.1.6  Created a climate control first try, removed 12 volt battery status
;
;    0.1.7  Added status of doors
;
;    0.1.8  Added config page for the webfrontend. Features canwrite and modelyear.
;           Differentiation between model years is now possible.
;
;    0.1.9  "Fixed" crash on climate control. Added A/C indicator.
;           First shot on battery temperatur.
;
;    0.2.0  Added key detection and car_on / pollingstate routine
;
;    0.2.1  Removed battery temperature, corrected outdoor temperature
;
;    0.2.2  Collect VIN only once
;
;    0.2.3  Redesign climate control, removed bluetooth template
;
;    0.2.4  First implementation of the ringbus and ocu heartbeat
;
;    0.2.5  Fixed heartbeat
;
;    0.2.6  Refined climate control template
;
;    0.2.7  Implemented dev_mode, 5A7, 69E climate control messages

;    0.2.8  Refactoring for T26A <-> OBD source separation (by SokoFromNZ)
;
;    0.2.9  Fixed climate control
;
;    0.3.0  Added upgrade policy for pre vehicle id splitting versions
;
;    0.3.1  Removed alpha OBD source, corrected class name
;
;    0.3.2  First implementation of charging detection
;
;    0.3.3  Buffer 0x61C ghost messages
;
;    0.3.4  Climate Control is now beginning to work
:
;    0.3.5  Stabilized Climate Control
;
;    0.3.6  Corrected log tag and namespaces
;
;    0.3.7  Add locked detection, add climate control via Homelink for iOS
;
;    0.3.8  Add lights, rear doors and trunk detection
;
;    (C) 2020       Chris van der Meijden
;
;    Big thanx to sharkcow, Dimitrie78 and E-Imo.
*/

#include "ovms_log.h"
static const char *TAG = "v-vweup-t26";

#define VERSION "0.3.8"

#include <stdio.h>
#include "pcp.h"
#include "t26_eup.h"
#include "metrics_standard.h"
#include "ovms_webserver.h"
#include "ovms_events.h"
#include "ovms_metrics.h"

void sendOcuHeartbeat(TimerHandle_t timer)
{
    OvmsVehicleVWeUpT26 *vwup = (OvmsVehicleVWeUpT26 *)pvTimerGetTimerID(timer);
    vwup->SendOcuHeartbeat();
}

void ccCountdown(TimerHandle_t timer)
{
    OvmsVehicleVWeUpT26 *vwup = (OvmsVehicleVWeUpT26 *)pvTimerGetTimerID(timer);
    vwup->CCCountdown();
}

OvmsVehicleVWeUpT26::OvmsVehicleVWeUpT26()
{
    ESP_LOGI(TAG, "Start VW e-Up T26A vehicle module");
    memset(m_vin, 0, sizeof(m_vin));

    RegisterCanBus(3, CAN_MODE_ACTIVE, CAN_SPEED_100KBPS);

    MyConfig.RegisterParam("xut", "VW e-Up", true, true);
    ConfigChanged(NULL);
    vin_part1 = false;
    vin_part2 = false;
    vin_part3 = false;
    vwup_remote_climate_ticker = 0;
    ocu_awake = false;
    ocu_working = false;
    ocu_what = false;
    ocu_wait = false;
    vweup_cc_on = false;
    vweup_cc_turning_on = false;
    vwup_cc_temp_int = 21;
    fas_counter_on = 0;
    fas_counter_off = 0;
    signal_ok = false;
    cc_count = 0;
    cd_count = 0;

    dev_mode = false; // true disables writing on the comfort CAN. For code debugging only.

    StandardMetrics.ms_v_env_locked->SetValue(true);
    StandardMetrics.ms_v_env_headlights->SetValue(false);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
#endif
}

OvmsVehicleVWeUpT26::~OvmsVehicleVWeUpT26()
{
    ESP_LOGI(TAG, "Stop VW e-Up T26A vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebDeInit();
#endif
}

bool OvmsVehicleVWeUpT26::SetFeature(int key, const char *value)
{
    switch (key)
    {
    case 15:
    {
        int bits = atoi(value);
        MyConfig.SetParamValueBool("xut", "canwrite", (bits & 1) != 0);
        return true;
    }
    default:
        return OvmsVehicle::SetFeature(key, value);
    }
}

const std::string OvmsVehicleVWeUpT26::GetFeature(int key)
{
    switch (key)
    {
    case 15:
    {
        int bits =
            (MyConfig.GetParamValueBool("xut", "canwrite", false) ? 1 : 0);
        char buf[4];
        sprintf(buf, "%d", bits);
        return std::string(buf);
    }
    default:
        return OvmsVehicle::GetFeature(key);
    }
}

void OvmsVehicleVWeUpT26::ConfigChanged(OvmsConfigParam *param)
{
    ESP_LOGD(TAG, "VW e-Up reload configuration");

    vwup_enable_write = MyConfig.GetParamValueBool("xut", "canwrite", false);
    vwup_modelyear = MyConfig.GetParamValueInt("xut", "modelyear", DEFAULT_MODEL_YEAR);
    vwup_cc_temp_int = MyConfig.GetParamValueInt("xut", "cc_temp", 21);
}

// Takes care of setting all the state appropriate when the car is on
// or off.
//
void OvmsVehicleVWeUpT26::vehicle_vweup_car_on(bool isOn)
{
    if (isOn && !StandardMetrics.ms_v_env_on->AsBool())
    {
        // Log once that car is being turned on
        ESP_LOGI(TAG, "CAR IS ON");
        StandardMetrics.ms_v_env_on->SetValue(true);
        // Turn off eventually running climate control timer
        if (ocu_awake)
        {
            xTimerStop(m_sendOcuHeartbeat, 0);
            xTimerDelete(m_sendOcuHeartbeat, 0);
            m_sendOcuHeartbeat = NULL;
        }
        if (cc_count != 0)
        {
            xTimerStop(m_ccCountdown, 0);
            xTimerDelete(m_ccCountdown, 0);
            m_ccCountdown = NULL;
        }
        ocu_awake = false;
        ocu_working = false;
        vwup_remote_climate_ticker = 0;
        fas_counter_on = 0;
        fas_counter_off = 0;
    }
    else if (!isOn && StandardMetrics.ms_v_env_on->AsBool())
    {
        // Log once that car is being turned off
        ESP_LOGI(TAG, "CAR IS OFF");
        StandardMetrics.ms_v_env_on->SetValue(false);
    }
}

void OvmsVehicleVWeUpT26::IncomingFrameCan3(CAN_frame_t *p_frame)
{
    uint8_t *d = p_frame->data.u8;

    static bool isCharging = false;
    static bool lastCharging = false;

    // This will log all incoming frames
    // ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

    switch (p_frame->MsgID)
    {

    case 0x61A: // SOC.
        StandardMetrics.ms_v_bat_soc->SetValue(d[7] / 2.0);
        if (vwup_modelyear >= 2020)
        {
            StandardMetrics.ms_v_bat_range_ideal->SetValue((260 * (d[7] / 2.0)) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
        }
        else
        {
            StandardMetrics.ms_v_bat_range_ideal->SetValue((160 * (d[7] / 2.0)) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
        }
        break;

    case 0x52D: // KM range left (estimated).
        if (d[0] != 0xFE)
            StandardMetrics.ms_v_bat_range_est->SetValue(d[0]);
        break;

    case 0x65F: // VIN
        switch (d[0])
        {
        case 0x00:
            // Part 1
            m_vin[0] = d[5];
            m_vin[1] = d[6];
            m_vin[2] = d[7];
            vin_part1 = true;
            break;
        case 0x01:
            // Part 2
            if (vin_part1)
            {
                m_vin[3] = d[1];
                m_vin[4] = d[2];
                m_vin[5] = d[3];
                m_vin[6] = d[4];
                m_vin[7] = d[5];
                m_vin[8] = d[6];
                m_vin[9] = d[7];
                vin_part2 = true;
            }
            break;
        case 0x02:
            // Part 3 - VIN complete
            if (vin_part2 && !vin_part3)
            {
                m_vin[10] = d[1];
                m_vin[11] = d[2];
                m_vin[12] = d[3];
                m_vin[13] = d[4];
                m_vin[14] = d[5];
                m_vin[15] = d[6];
                m_vin[16] = d[7];
                m_vin[17] = 0;
                vin_part3 = true;
                StandardMetrics.ms_v_vin->SetValue((string)m_vin);
            }
            break;
        }
        break;

    case 0x65D: // ODO
        StandardMetrics.ms_v_pos_odometer->SetValue(((uint32_t)(d[3] & 0xf) << 12) | ((UINT)d[2] << 8) | d[1]);
        break;

    case 0x320: // Speed
        // We need some awake message.
        if (StandardMetrics.ms_v_env_awake->IsStale())
        {
            StandardMetrics.ms_v_env_awake->SetValue(false);
        }
        StandardMetrics.ms_v_pos_speed->SetValue(((d[4] << 8) + d[3] - 1) / 190);
        break;

    case 0x527: // Outdoor temperature
        StandardMetrics.ms_v_env_temp->SetValue((d[5] / 2) - 50);
        break;

    case 0x381: // Vehicle locked
        if (d[0] == 0x00)
        {
             StandardMetrics.ms_v_env_locked->SetValue(false);
        }
        else
        {
             StandardMetrics.ms_v_env_locked->SetValue(true);
        }
        break;

    case 0x3E3: // Cabin temperature
        StandardMetrics.ms_v_env_cabintemp->SetValue((d[2]-100)/2);
        // Set PEM inv temp to support older app version with cabin temp workaround display
        StandardMetrics.ms_v_inv_temp->SetValue((d[2] - 100) / 2);
        break;

    case 0x470: // Doors
        StandardMetrics.ms_v_door_fl->SetValue((d[1] & 0x01) > 0);
        StandardMetrics.ms_v_door_fr->SetValue((d[1] & 0x02) > 0);
        StandardMetrics.ms_v_door_rl->SetValue((d[1] & 0x04) > 0);
        StandardMetrics.ms_v_door_rr->SetValue((d[1] & 0x08) > 0);
        StandardMetrics.ms_v_door_trunk->SetValue((d[1] & 0x20) > 0);
        StandardMetrics.ms_v_door_hood->SetValue((d[1] & 0x10) > 0);
        break;

    case 0x531: // Head lights
        if (d[0] == 0x00)
        {
             StandardMetrics.ms_v_env_headlights->SetValue(false);
        }
        else
        {
             StandardMetrics.ms_v_env_headlights->SetValue(true);
        }
        break;

    // Check for running hvac.
    case 0x3E1:
        if (d[4] > 0)
        {
            StandardMetrics.ms_v_env_hvac->SetValue(true);
        }
        else
        {
            StandardMetrics.ms_v_env_hvac->SetValue(false);
        }
        break;

    case 0x61C: // Charge detection
      cd_count++;
      if ((d[2] == 0x00) || (d[2] == 0x01)) {
         isCharging = true;
      } else {
         isCharging = false;
      }
      if (isCharging != lastCharging) { 
        // count till 3 messages in a row to stop ghost triggering
        if (isCharging && cd_count == 3) {
          cd_count = 0;
          StandardMetrics.ms_v_charge_pilot->SetValue(true);
          StandardMetrics.ms_v_charge_inprogress->SetValue(true);
          StandardMetrics.ms_v_door_chargeport->SetValue(true);
          StandardMetrics.ms_v_charge_mode->SetValue("standard");
          StandardMetrics.ms_v_charge_state->SetValue("charging");
          StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
          ESP_LOGI(TAG,"Car charge session started");
        } 
        if (!isCharging && cd_count == 3) {
          cd_count = 0;
          StandardMetrics.ms_v_charge_pilot->SetValue(false);
          StandardMetrics.ms_v_charge_inprogress->SetValue(false);
          StandardMetrics.ms_v_door_chargeport->SetValue(false);
          StandardMetrics.ms_v_charge_mode->SetValue("standard");
          StandardMetrics.ms_v_charge_state->SetValue("done");
          StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
          ESP_LOGI(TAG,"Car charge session ended");
        } 
      } else {
          cd_count = 0;
      }
      if (cd_count == 0) {
         lastCharging = isCharging;
      }
      break;

    case 0x575: // Key position
        switch (d[0])
        {
        case 0x00: // No key
            vehicle_vweup_car_on(false);
            break;
        case 0x01: // Key in position 1, no ignition
            vehicle_vweup_car_on(false);
            break;
        case 0x03: // Ignition is turned off
            break;
        case 0x05: // Ignition is turned on
            break;
        case 0x07: // Key in position 2, ignition on
            vehicle_vweup_car_on(true);
            break;
        case 0x0F: // Key in position 3, start the engine
            break;
        }
        break;

    case 0x400: // Welcome to the ring from ILM
    case 0x40C: // We know this one too. Climatronic.
    case 0x436: // Working in the ring.
    case 0x439: // Who are 436 and 439 and why do they differ on some cars?
        if (d[1] == 0x31 && ocu_awake)
        { // We should go to sleep, no matter what
            ESP_LOGI(TAG, "Comfort CAN calls for sleep");
            xTimerStop(m_sendOcuHeartbeat, 0);
            xTimerDelete(m_sendOcuHeartbeat, 0);
            m_sendOcuHeartbeat = NULL;
            if (cc_count != 0)
            {
                xTimerStop(m_ccCountdown, 0);
                xTimerDelete(m_ccCountdown, 0);
                m_ccCountdown = NULL;
            }
            ocu_awake = false;
            ocu_working = false;
            vwup_remote_climate_ticker = 0;
            fas_counter_on = 0;
            fas_counter_off = 0;
            break;
        }
        if (d[0] == 0x1D)
        { // We are called in the ring

            unsigned char data[8];
            uint8_t length;
            length = 8;

            canbus *comfBus;
            comfBus = m_can3;

            data[0] = 0x00; // As it seems that we are always the highest in the ring we always send to 0x400
            if (ocu_working)
            {
                data[1] = 0x01;
                if (dev_mode)
                    ESP_LOGI(TAG, "OCU working");
            }
            else
            {
                data[1] = 0x11; // Ready to sleep. This is very important!
                if (dev_mode)
                    ESP_LOGI(TAG, "OCU ready to sleep");
            }
            data[2] = 0x02;
            data[3] = d[3]; // Not understood
            data[4] = 0x00;
            if (ocu_what)
            { // This is not understood and not implemented yet.
                data[5] = 0x14;
            }
            else
            {
                data[5] = 0x10;
            }
            data[6] = 0x00;
            data[7] = 0x00;
            vTaskDelay(50 / portTICK_PERIOD_MS);
            if (vwup_enable_write && !dev_mode)
                comfBus->WriteStandard(0x43D, length, data); // We answer

            ocu_awake = true;
            break;
        }
        break;

    default:
        // This will log all unknown incoming frames
        // ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
        break;
    }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUpT26::RemoteCommandHandler(RemoteCommand command)
{
    ESP_LOGI(TAG, "RemoteCommandHandler");

    vwup_remote_command = command;
    SendCommand(vwup_remote_command);
    if (signal_ok)
    {
        signal_ok = false;
        return Success;
    }
    return NotImplemented;
}

////////////////////////////////////////////////////////////////////////
// Send a RemoteCommand on the CAN bus.
//
// Does nothing if @command is out of range
//
void OvmsVehicleVWeUpT26::SendCommand(RemoteCommand command)
{

    switch (command)
    {
    case ENABLE_CLIMATE_CONTROL:
        if (StandardMetrics.ms_v_env_on->AsBool())
        {
            ESP_LOGI(TAG, "Climate Control can't be enabled - car is on");
            break;
        }

        if (fas_counter_off > 0 && fas_counter_off < 10)
        {
            ESP_LOGI(TAG, "Climate Control can't be enabled - CC off is still running");
            break;
        }

        if (vwup_remote_climate_ticker == 0)
        {
            vwup_remote_climate_ticker = 1800;
        }
        else
        {
            ESP_LOGI(TAG, "Enable Climate Control - already enabled");
            break;
        }

        vweup_cc_turning_on = true;

        CommandWakeup();

        ESP_LOGI(TAG, "Enable Climate Control");

        m_ccCountdown = xTimerCreate("VW e-Up CC Countdown", 1000 / portTICK_PERIOD_MS, pdTRUE, this, ccCountdown);
        xTimerStart(m_ccCountdown, 0);

        signal_ok = true;
        break;
    case DISABLE_CLIMATE_CONTROL:
        if (!vweup_cc_on)
        {
            ESP_LOGI(TAG, "Climate Control can't be disabled - CC is not enabled.");
            break;
        }
        if (StandardMetrics.ms_v_env_on->AsBool())
        {
            ESP_LOGI(TAG, "Climate Control is already disabled - car is on");
            break;
        }

        if ((fas_counter_on > 0 && fas_counter_on < 10) || ocu_wait)
        {
            // Should we implement a "wait for turn off" timer here?
            ESP_LOGI(TAG, "Climate Control can't be disabled - CC on is still running");
            break;
        }

        if (vwup_remote_climate_ticker == 0)
        {
            ESP_LOGI(TAG, "Disable Climate Control - already disabled");
            break;
        }

        ESP_LOGI(TAG, "Disable Climate Control");

        vwup_remote_climate_ticker = 0;

        CCOff();

        ocu_awake = true;
        signal_ok = true;

        break;
    case AUTO_DISABLE_CLIMATE_CONTROL:
        if (StandardMetrics.ms_v_env_on->AsBool())
        {
            // Should not be possible
            ESP_LOGI(TAG, "Error: CC AUTO_DISABLE when car is on");
            break;
        }

        vwup_remote_climate_ticker = 0;

        ESP_LOGI(TAG, "Auto Disable Climate Control");

        CCOff();

        ocu_awake = true;
        signal_ok = true;

        break;
    default:
        return;
    }
}

// Wakeup implentation over the VW ring commands
// We need to register in the ring with a call to our self from 43D with 0x1D in the first byte
//
OvmsVehicle::vehicle_command_t OvmsVehicleVWeUpT26::CommandWakeup()
{

    if (!vwup_enable_write)
        return Fail;

    if (!ocu_awake)
    {

        unsigned char data[8];
        uint8_t length;
        length = 8;

        unsigned char data2[2];
        uint8_t length2;
        length2 = 2;

        canbus *comfBus;
        comfBus = m_can3;

        // This is a very dirty workaround to get the wakup of the ring working.
        // We send 0x69E some values without knowing what they do.
        // This throws an AsynchronousInterruptHandler error, but wakes 0x400
        // So we wait two seconds and then call us in the ring 

        data[0] = 0x14;
        data[1] = 0x42;
        if (vwup_enable_write && !dev_mode)
            comfBus->WriteStandard(0x69E, length2, data2);

        vTaskDelay(2000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Sent Wakeup Command - stage 1");

        data[0] = 0x1D; // We call ourself to wake up the ring
        data[1] = 0x02;
        data[2] = 0x02;
        data[3] = 0x00;
        data[4] = 0x00;
        data[5] = 0x14; // What does this do?
        data[6] = 0x00;
        data[7] = 0x00;
        if (vwup_enable_write && !dev_mode)
            comfBus->WriteStandard(0x43D, length, data);

        vTaskDelay(50 / portTICK_PERIOD_MS);

        data[0] = 0x00; // We need to talk to 0x400 first to get accepted in the ring
        data[1] = 0x01;
        data[2] = 0x02;
        data[3] = 0x04; // This could be a problem
        data[4] = 0x00;
        data[5] = 0x14; // What does this do?
        data[6] = 0x00;
        data[7] = 0x00;
        if (vwup_enable_write && !dev_mode)
            comfBus->WriteStandard(0x43D, length, data);

        ocu_working = true;
        ocu_awake = true;

        vTaskDelay(50 / portTICK_PERIOD_MS);

        m_sendOcuHeartbeat = xTimerCreate("VW e-Up OCU heartbeat", 1000 / portTICK_PERIOD_MS, pdTRUE, this, sendOcuHeartbeat);
        xTimerStart(m_sendOcuHeartbeat, 0);

        ESP_LOGI(TAG, "Sent Wakeup Command - stage 2");
    }
    // This can be done better. Gives always success, even when already awake.
    return Success;
}

void OvmsVehicleVWeUpT26::SendOcuHeartbeat()
{

    if (!vweup_cc_on)
    {
        fas_counter_on = 0;
        if (fas_counter_off < 10 && !vweup_cc_turning_on)
        {
            fas_counter_off++;
            ocu_working = true;
            if (dev_mode)
                ESP_LOGI(TAG, "OCU working");
        }
        else
        {
            if (vweup_cc_turning_on)
            {
                ocu_working = true;
                if (dev_mode)
                    ESP_LOGI(TAG, "OCU working");
            }
            else
            {
                ocu_working = false;
                if (dev_mode)
                    ESP_LOGI(TAG, "OCU ready to sleep");
            }
        }
    }
    else
    {
        fas_counter_off = 0;
        ocu_working = true;
        if (dev_mode)
            ESP_LOGI(TAG, "OCU working");
        if (fas_counter_on < 10)
            fas_counter_on++;
    }

    unsigned char data[8];
    uint8_t length;
    length = 8;

    canbus *comfBus;
    comfBus = m_can3;

    data[0] = 0x00;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x5A9, length, data);

    if ((fas_counter_on > 0 && fas_counter_on < 10) || (fas_counter_off > 0 && fas_counter_off < 10))
    {
        data[0] = 0x60;
        if (dev_mode)
            ESP_LOGI(TAG, "OCU Heartbeat - 5A7 60");
    }
    else
    {
        data[0] = 0x00;
        if (dev_mode)
            ESP_LOGI(TAG, "OCU Heartbeat - 5A7 00");
    }
    data[1] = 0x16;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x5A7, length, data);
}

void OvmsVehicleVWeUpT26::CCCountdown()
{
    cc_count++;
    ocu_wait = true;
    if (cc_count == 5)
    {
        CCOnP(); // This is weird. We first need to send a Climate Contol Off before it will turn on ???
    }
    if (cc_count == 10)
    {
        CCOn();
        xTimerStop(m_ccCountdown, 0);
        xTimerDelete(m_ccCountdown, 0);
        m_ccCountdown = NULL;
        ocu_wait = false;
        ocu_awake = true;
        cc_count = 0;
    }
}

void OvmsVehicleVWeUpT26::CCOn()
{
    unsigned char data[8];
    uint8_t length;
    length = 8;

    unsigned char data_s[4];
    uint8_t length_s;
    length_s = 4;

    canbus *comfBus;
    comfBus = m_can3;

    data[0] = 0x60;
    data[1] = 0x16;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x5A7, length, data);

    // d5 could be a counter?
    data[0] = 0x80;
    data[1] = 0x20;
    data[2] = 0x29;
    data[3] = 0x59;
    data[4] = 0x21;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x01;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC0;
    data[1] = 0x06;
    data[2] = 0x00;
    data[3] = 0x10;
    data[4] = 0x1E;
    data[5] = 0xFF;
    data[6] = 0xFF;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC1;
    data[1] = 0xFF;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = 0xFF;
    data[5] = 0x01;
    data[6] = 0x6E; // This is the target temperature. T = 10 + d6/10

    if (vwup_cc_temp_int == 19)
    {
        data[6] = 0x5A;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 19");
    }
    if (vwup_cc_temp_int == 20)
    {
        data[6] = 0x64;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 20");
    }
    if (vwup_cc_temp_int == 21)
    {
        data[6] = 0x6E;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 21");
    }
    if (vwup_cc_temp_int == 22)
    {
        data[6] = 0x78;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 22");
    }
    if (vwup_cc_temp_int == 23)
    {
        data[6] = 0x82;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 23");
    }

    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC2;
    data[1] = 0x1E;
    data[2] = 0x1E;
    data[3] = 0x0A;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x08;
    data[7] = 0x4F;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC3;
    data[1] = 0x70;
    data[2] = 0x74;
    data[3] = 0x69;
    data[4] = 0x6F;
    data[5] = 0x6E;
    data[6] = 0x65;
    data[7] = 0x6E;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0x60;
    data[1] = 0x16;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x5A7, length, data);

    data_s[0] = 0x29;
    data_s[1] = 0x58;
    data_s[2] = 0x00;
    data_s[3] = 0x01;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length_s, data_s);

    ESP_LOGI(TAG, "Wrote second stage Climate Control On Messages to Comfort CAN.");
    vweup_cc_on = true;
    vweup_cc_turning_on = false;
}

void OvmsVehicleVWeUpT26::CCOnP()
{
    unsigned char data[8];
    uint8_t length;
    length = 8;

    unsigned char data_s[4];
    uint8_t length_s;
    length_s = 4;

    canbus *comfBus;
    comfBus = m_can3;

    data[0] = 0x60;
    data[1] = 0x16;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x5A7, length, data);

    // d5 could be a counter
    data[0] = 0x80;
    data[1] = 0x20;
    data[2] = 0x29;
    data[3] = 0x59;
    data[4] = 0x22;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x01;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0x60;
    data[1] = 0x16;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x5A7, length, data);

    data[0] = 0xC0;
    data[1] = 0x06;
    data[2] = 0x00;
    data[3] = 0x10;
    data[4] = 0x1E;
    data[5] = 0xFF;
    data[6] = 0xFF;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC1;
    data[1] = 0xFF;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = 0xFF;
    data[5] = 0x01;
    data[6] = 0x6E; // This is the target temperature. T = 10 + d6/10

    if (vwup_cc_temp_int == 19)
    {
        data[6] = 0x5A;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 19");
    }
    if (vwup_cc_temp_int == 20)
    {
        data[6] = 0x64;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 20");
    }
    if (vwup_cc_temp_int == 21)
    {
        data[6] = 0x6E;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 21");
    }
    if (vwup_cc_temp_int == 22)
    {
        data[6] = 0x78;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 22");
    }
    if (vwup_cc_temp_int == 23)
    {
        data[6] = 0x82;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 23");
    }

    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC2;
    data[1] = 0x1E;
    data[2] = 0x1E;
    data[3] = 0x0A;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x08;
    data[7] = 0x4F;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC3;
    data[1] = 0x70;
    data[2] = 0x74;
    data[3] = 0x69;
    data[4] = 0x6F;
    data[5] = 0x6E;
    data[6] = 0x65;
    data[7] = 0x6E;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data_s[0] = 0x29;
    data_s[1] = 0x58;
    data_s[2] = 0x00;
    data_s[3] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length_s, data_s);

    ESP_LOGI(TAG, "Wrote first stage Climate Control On Messages to Comfort CAN.");
}

void OvmsVehicleVWeUpT26::CCOff()
{
    unsigned char data[8];
    uint8_t length;
    length = 8;

    unsigned char data_s[4];
    uint8_t length_s;
    length_s = 4;

    canbus *comfBus;
    comfBus = m_can3;

    data[0] = 0x60;
    data[1] = 0x16;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x5A7, length, data);

    // d5 could be a counter
    data[0] = 0x80;
    data[1] = 0x20;
    data[2] = 0x29;
    data[3] = 0x59;
    data[4] = 0x22;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x01;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0x60;
    data[1] = 0x16;
    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x5A7, length, data);

    data[0] = 0xC0;
    data[1] = 0x06;
    data[2] = 0x00;
    data[3] = 0x10;
    data[4] = 0x1E;
    data[5] = 0xFF;
    data[6] = 0xFF;
    data[7] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC1;
    data[1] = 0xFF;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = 0xFF;
    data[5] = 0x01;
    data[6] = 0x6E; // This is the target temperature. T = 10 + d6/10

    if (vwup_cc_temp_int == 19)
    {
        data[6] = 0x5A;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 19");
    }
    if (vwup_cc_temp_int == 20)
    {
        data[6] = 0x64;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 20");
    }
    if (vwup_cc_temp_int == 21)
    {
        data[6] = 0x6E;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 21");
    }
    if (vwup_cc_temp_int == 22)
    {
        data[6] = 0x78;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 22");
    }
    if (vwup_cc_temp_int == 23)
    {
        data[6] = 0x82;
        if (dev_mode)
            ESP_LOGI(TAG, "Cabin temperature set: 23");
    }

    data[0] = 0xC2;
    data[1] = 0x1E;
    data[2] = 0x1E;
    data[3] = 0x0A;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x08;
    data[7] = 0x4F;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xC3;
    data[1] = 0x70;
    data[2] = 0x74;
    data[3] = 0x69;
    data[4] = 0x6F;
    data[5] = 0x6E;
    data[6] = 0x65;
    data[7] = 0x6E;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length, data);

    data_s[0] = 0x29;
    data_s[1] = 0x58;
    data_s[2] = 0x00;
    data_s[3] = 0x00;
    if (vwup_enable_write && !dev_mode)
        comfBus->WriteStandard(0x69E, length_s, data_s);

    ESP_LOGI(TAG, "Wrote Climate Control Off Message to Comfort CAN.");
    vweup_cc_on = false;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUpT26::CommandHomelink(int button, int durationms)
  {
  // This is needed to enable climate control via Homelink for the iOS app
  ESP_LOGI(TAG, "CommandHomelink");
  if (button == 0 && vwup_enable_write)
    {
    return RemoteCommandHandler(ENABLE_CLIMATE_CONTROL);
    }
  if (button == 1 && vwup_enable_write)
    {
    return RemoteCommandHandler(DISABLE_CLIMATE_CONTROL);
    }
  return NotImplemented;
  }


OvmsVehicle::vehicle_command_t OvmsVehicleVWeUpT26::CommandClimateControl(bool climatecontrolon)
{
    ESP_LOGI(TAG, "CommandClimateControl");
    if (vwup_enable_write)
        return RemoteCommandHandler(climatecontrolon ? ENABLE_CLIMATE_CONTROL : DISABLE_CLIMATE_CONTROL);
    else
        return NotImplemented;
}

void OvmsVehicleVWeUpT26::Ticker1(uint32_t ticker)
{
    // This is just to be sure that we really have an asleep message. It has delay of 120 sec.
    // Do we still need this?
    if (StandardMetrics.ms_v_env_awake->IsStale())
    {
        StandardMetrics.ms_v_env_awake->SetValue(false);
    }

    // Autodisable climate control ticker (30 min.)
    if (vwup_remote_climate_ticker != 0)
    {
        vwup_remote_climate_ticker--;
        if (vwup_remote_climate_ticker == 1)
        {
            SendCommand(AUTO_DISABLE_CLIMATE_CONTROL);
        }
    }
}
