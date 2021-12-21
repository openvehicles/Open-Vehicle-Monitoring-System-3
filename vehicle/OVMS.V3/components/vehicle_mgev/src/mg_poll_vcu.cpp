/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
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

#include "vehicle_mgev.h"

namespace {

size_t min(size_t a, size_t b)
{
    return a < b ? a : b;
}

void Calibrate12v(float voltage)
{
    float reading = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
    // If the calibration is out, perform an adjustment.  The OVMS read value is smoothed
    // for noise and jitter, so we will just tweak it a little bit at a time.
    if (reading - voltage >= 0.1)
    {
        float factor = MyConfig.GetParamValueFloat("system.adc", "factor12v");
        MyConfig.SetParamValueFloat("system.adc", "factor12v", factor + 0.1);
    }
    else if (reading - voltage <= -0.1)
    {
        float factor = MyConfig.GetParamValueFloat("system.adc", "factor12v");
        MyConfig.SetParamValueFloat("system.adc", "factor12v", factor - 0.1);
    }
}

}  // anon namespace

void OvmsVehicleMgEv::IncomingVcuPoll(
        uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain)
{
    uint16_t value = (data[0] << 8 | data[1]);

    switch (pid)
    {
        case vcu12vSupplyPid:
            Calibrate12v(data[0] / 10.0);
            break;
        case vcuVehicleSpeedPid:
            // Speed in kph
            StandardMetrics.ms_v_pos_speed->SetValue(
                ((data[0] << 8 | data[1]) - 20000) / 100.0
            );
            break;
        case vcuChargerConnectedPid:
            if (StandardMetrics.ms_v_charge_type->AsString() == "ccs")
            {
                StandardMetrics.ms_v_door_chargeport->SetValue(true);
                StandardMetrics.ms_v_charge_pilot->SetValue(true);
            }
            else
            {
                StandardMetrics.ms_v_door_chargeport->SetValue(data[0] & 1);
                StandardMetrics.ms_v_charge_pilot->SetValue(data[0] & 1);
            }
            break;
        case vcuHvContactorPid:
            StandardMetrics.ms_v_env_charging12v->SetValue(data[0] & 1);
            SetEnvOn();
            break;
        case vcuIgnitionStatePid:
            // Aux only is 1, but we'll say it's on when the ignition is too
            StandardMetrics.ms_v_env_aux12v->SetValue(data[0] != 0);
            m_ignition_state->SetValue(data[0]);
            SetEnvOn();
            break;
        case vcuVinPid:
            HandleVinMessage(data, length, remain);
            break;
        case vcuCoolantTempPid:
            m_motor_coolant_temp->SetValue(data[0] - 40);
            break;
        case vcuMotorTempPid:
            StandardMetrics.ms_v_mot_temp->SetValue(data[0] - 40);
            break;
        case vcuMotorSpeedPid:
            StandardMetrics.ms_v_mot_rpm->SetValue(value - 0x7fffu);
            break;
        case vcuMotorTorquePid:
            m_motor_torque->SetValue(value - 0x7fffu);
            break;
        case vcuOdometerPid:
            // km
            StandardMetrics.ms_v_pos_odometer->SetValue(
                data[0] << 16 | data[1] << 8 | data[2]
            );
            break;
        case vcuHandbrakePid:
            StandardMetrics.ms_v_env_handbrake->SetValue(data[0]);
            break;
        case vcuGearPid:
            if (data[0] == 8)
            {
                StandardMetrics.ms_v_env_drivemode->SetValue(0);
            }
            // TODO: Get the correct value here for reverse
            else if (data[0] == 0)
            {
                StandardMetrics.ms_v_env_drivemode->SetValue(-1);
            }
            else
            {
                StandardMetrics.ms_v_env_drivemode->SetValue(1);
            }
            break;
        case vcuBrakePid:
            StandardMetrics.ms_v_env_footbrake->SetValue(value / 10.0);
            break;
        case vcuBonnetPid:
            StandardMetrics.ms_v_door_hood->SetValue(data[0] & 1);
            break;
        case chargeRatePid:
            // The available charge rate, max power BMS will accept right now
            StandardMetrics.ms_v_charge_climit->SetValue(value / 10.0);
            break;
        case vcuAcPowerPid:
            StandardMetrics.ms_v_env_cooling->SetValue(value > 0);
            break;
        case vcuBatteryVoltagePid:
            m_bat_voltage_vcu->SetValue(value / 10.0);
            break;
        case vcuRadiatorFanPid:
            m_radiator_fan->SetValue(data[0]);
            break;
        case vcuDcDcModePid:
            m_vcu_dcdc_mode->SetValue(data[0]);
            break;            
        case vcuDcDcInputCurrentPid:
            m_vcu_dcdc_input_current->SetValue((value - 0x7fffu)/10.0);
            break;
        case vcuDcDcInputVoltagePid:
            m_vcu_dcdc_input_voltage->SetValue(value / 10.0);
            break;     
        case vcuDcDcOutputCurrentPid:
            m_vcu_dcdc_output_current->SetValue(value / 10.0);
            break;
        case vcuDcDcOutputVoltagetPid:
            m_vcu_dcdc_output_voltage->SetValue(value / 10.0);
            break;     
        case vcuDcDcTempPid:
            m_vcu_dcdc_temp->SetValue(data[0] - 40);
            break;
    }
}

void OvmsVehicleMgEv::HandleVinMessage(uint8_t* data, uint8_t length, uint16_t remain)
{
    size_t len = strnlen(m_vin, sizeof(m_vin) - 1);
    // Trim the first character from the VIN as it's seemingly junk
    if (len == 0)
    {
        ++data;
        --length;
    }
    memcpy(&m_vin[len], data, min(length, sizeof(m_vin) - 1));
    if (remain == 0)
    {
        // Seems to have a bit of rubbish at the start of the VIN
        StandardMetrics.ms_v_vin->SetValue(&m_vin[1]);
        memset(m_vin, 0, sizeof(m_vin));
    }
}

void OvmsVehicleMgEv::SetEnvOn()
{
    //Vehicle is ready to drive if ignition state is 2 (start button is green) and HV contactor is engaged (ms_v_env_charging12v will be true).
    StandardMetrics.ms_v_env_on->SetValue(StandardMetrics.ms_v_env_charging12v->AsBool() && m_ignition_state->AsInt() == 2);      
}