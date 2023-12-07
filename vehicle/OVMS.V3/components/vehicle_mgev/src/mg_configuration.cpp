/*
;    Project:       Open Vehicle Monitor System
;    Date:          20th August 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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

static const char *TAG = "v-mgev";

#include "vehicle_mgev.h"

namespace {

// The parameter namespace for this vehicle
const char PARAM_NAME[] = "xmg";

// Names of individual parameters
const char CAN_INTERFACE[] = "caninterface";

// The default CAN bus to use (if you're using a standard cable)
constexpr int DEFAULT_CAN = 1;

}  // anon namespace

int OvmsVehicleMgEv::CanInterface()
{
    return MyConfig.GetParamValueInt(PARAM_NAME, CAN_INTERFACE, DEFAULT_CAN);
}

//Called by OVMS when a config param is changed
void OvmsVehicleMgEv::ConfigChanged(OvmsConfigParam* param)
{
    if (param && param->GetName() != PARAM_NAME)
    {
        return;
    }

    ESP_LOGI(TAG, "%s config changed", PARAM_NAME);

    // Instances:
    // xmg
    //  suffsoc              Sufficient SOC [%] (Default: 0=disabled)
    //  suffrange            Sufficient range [km] (Default: 0=disabled)
    StandardMetrics.ms_v_charge_limit_soc->SetValue(
            (float) MyConfig.GetParamValueInt("xmg", "suffsoc"),   Percentage );
    
    if (MyConfig.GetParamValue("vehicle", "units.distance") == "K")
    {
        StandardMetrics.ms_v_charge_limit_range->SetValue(
                (float) MyConfig.GetParamValueInt("xmg", "suffrange"), Kilometers );
    }
    else
    {
        StandardMetrics.ms_v_charge_limit_range->SetValue(
            (float) MyConfig.GetParamValueInt("xmg", "suffrange"), Miles );
    }
    
    if (StandardMetrics.ms_v_type->AsString() == "MGA") {
        int BMSVersion = MyConfig.GetParamValueInt("xmg", "bmsval", DEFAULT_BMS_VERSION);
        m_dod_lower->SetValue(BMSDoDLimits[BMSVersion].Lower);
        m_dod_upper->SetValue(BMSDoDLimits[BMSVersion].Upper);
        ESP_LOGD(TAG, "BMS DoD lower = %f upper = %f", MyConfig.GetParamValueFloat("xmg","bms.dod.lower"), MyConfig.GetParamValueFloat("xmg","bms.dod.upper"));
    }
    
    if(StandardMetrics.ms_v_type->AsString() == "MG5") {
        int VehicleVersion = MyConfig.GetParamValueInt("xmg", "vehval", 0);
        //ESP_LOGI(TAG, "Vehicle Version %c", MyConfig.GetParamValue("xmg", "vehtype","unknown"));
        if(VehicleVersion == 0) {
            ESP_LOGV(TAG,"MG5 Version - SR");
            StandardMetrics.ms_v_bat_range_full->SetValue(290.0);
            m_batt_capacity->SetValue(48.8);
            m_max_dc_charge_rate->SetValue(80);
            m_dod_lower->SetValue(950.0);
            m_dod_upper->SetValue(36.0);
        } else {
            ESP_LOGV(TAG,"MG5 Version - LR");
            StandardMetrics.ms_v_bat_range_full->SetValue(320.0);
            m_batt_capacity->SetValue(57.4);
            m_max_dc_charge_rate->SetValue(87);
            m_dod_lower->SetValue(950.0);
            m_dod_upper->SetValue(36.0);
        }
        ESP_LOGD(TAG, "MG5 Values - Range: %0.1f Battery kWh: %0.1f Charge Max: %0.1f", StandardMetrics.ms_v_bat_range_full->AsFloat(), m_batt_capacity->AsFloat(), m_max_dc_charge_rate->AsFloat());
    }
}


// Called by OVMS when server requests to set feature
bool OvmsVehicleMgEv::SetFeature(int key, const char *value) {
    switch (key) {
        case 10:
        {
            int bus = atoi(value);
            if (bus < 1 || bus > 4)
            {
                return false;
            }
            MyConfig.SetParamValueInt(PARAM_NAME, CAN_INTERFACE, bus);
            return true;
        }
        default:
            return OvmsVehicle::SetFeature(key, value);
    }
}

// Called by OVMS when server requests to get feature
const std::string OvmsVehicleMgEv::GetFeature(int key) {
    switch (key) {
        case 10:
        {
            int bus = MyConfig.GetParamValueInt(PARAM_NAME, CAN_INTERFACE, DEFAULT_CAN);
            char buf[2];
            snprintf(buf, sizeof(buf), "%d", bus);
            return std::string(buf);
        }
        default:
            return OvmsVehicle::GetFeature(key);
    }
}
