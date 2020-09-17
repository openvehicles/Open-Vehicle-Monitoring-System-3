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

#include "ovms_log.h"
static const char *TAG = "v-mgev";

#include "vehicle_mgev.h"

namespace {

// The parameter namespace for this vehicle
const char PARAM_NAME[] = "xmg";

// Names of individual parameters
const char CAN_INTERFACE[] = "caninterface";

// The default CAN bus to use (if you're using a standard cable)
constexpr int DEFAULT_CAN = 1;

int CanInterface()
{
    return MyConfig.GetParamValueInt(PARAM_NAME, CAN_INTERFACE, DEFAULT_CAN);
}

}  // anon namespace

void OvmsVehicleMgEv::ConfigChanged(OvmsConfigParam* param)
{
    if (param && param->GetName() != PARAM_NAME)
    {
        return;
    }

    ESP_LOGI(TAG, "MG EV reload configuration");

    ConfigurePollInterface(CanInterface());
}

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
