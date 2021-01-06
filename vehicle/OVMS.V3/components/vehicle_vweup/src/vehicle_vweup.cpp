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
;    Date:          30 December 2020
;
;    Changes:
;    0.1.0  Initial code
:           Crude merge of code from Chris van der Meijden (KCAN) and SokoFromNZ (OBD2)
;
;    0.1.1  make OBD code depend on car status from KCAN, no more OBD polling in off state
;
;    0.1.2  common namespace, metrics webinterface
;
;    0.1.3  bugfixes gen1/gen2, new metrics: temperatures & maintenance
;
;    0.1.4  bugfixes gen1/gen2, OBD refactoring
;
;    0.1.5  refactoring
;
;    0.1.6  Standard SoC now from OBD (more resolution), switch between car on and charging
;
;    0.2.0  refactoring: this is now plain VWEUP-module, KCAN-version moved to VWEUP_T26
;
;    0.2.1  SoC from OBD/KCAN depending on config & state
;
;    0.3.0  car state determined depending on connection
;
;    0.3.1  added charger standard metrics
;
;    0.3.2  refactoring; init CAN buses depending on connection; transmit charge current as float (server V2)
;
;    0.3.3  new standard metrics for maintenance range & days; added trip distance & energies as deltas
;
;    0.4.0  update changes in t26 code by devmarxx, update docs
;
;    (C) 2020 sharkcow <sharkcow@gmx.de> / Chris van der Meijden / SokoFromNZ
;
;    Biggest thanks to Dexter, Dimitrie78, E-Imo and 'der kleine Nik'.
*/

#include "ovms_log.h"
#include <string>
static const char *TAG = "v-vweup";

#define VERSION "0.4.0"

#include <stdio.h>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"

#include "vehicle_vweup.h"

using namespace std;

/**
 * Framework registration
 */

class OvmsVehicleVWeUpInit
{
public: OvmsVehicleVWeUpInit();
} OvmsVehicleVWeUpInit __attribute__((init_priority(9000)));

OvmsVehicleVWeUpInit::OvmsVehicleVWeUpInit()
{
    ESP_LOGI(TAG, "Registering Vehicle: VW e-Up (9000)");
    MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeUp>("VWUP", "VW e-Up");
}


OvmsVehicleVWeUp* OvmsVehicleVWeUp::GetInstance(OvmsWriter* writer)
{
  OvmsVehicleVWeUp* eup = (OvmsVehicleVWeUp*) MyVehicleFactory.ActiveVehicle();
  
  string type = StdMetrics.ms_v_type->AsString();
  if (!eup || type != "VWUP") {
    if (writer)
      writer->puts("Error: VW e-Up vehicle module not selected");
    return NULL;
  }
  return eup;
}


/**
 * Constructor & destructor
 */

//size_t OvmsVehicleVWeUp::m_modifier = 0;

OvmsVehicleVWeUp::OvmsVehicleVWeUp()
{
    ESP_LOGI(TAG, "Start VW e-Up vehicle module");

// init configs:
    MyConfig.RegisterParam("xvu", "VW e-Up", true, true);
    
    ConfigChanged(NULL);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
#endif
}

OvmsVehicleVWeUp::~OvmsVehicleVWeUp()
{
    ESP_LOGI(TAG, "Stop VW e-Up vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebDeInit();
#endif
}

/*
const char* OvmsVehicleVWeUp::VehicleShortName()
{
  return "e-Up";
}
*/

bool OvmsVehicleVWeUp::SetFeature(int key, const char *value)
{
    int i;
    int n;
    switch (key)
    {
    case 15:
    {
        int bits = atoi(value);
        MyConfig.SetParamValueBool("xvu", "canwrite", (bits & 1) != 0);
        return true;
    }
    case 20:
        // check:
        if (strlen(value) == 0) value = "2020";
        for (i = 0; i < strlen(value); i++) {
           if (isdigit(value[i]) == false) {
             value = "2020";
             break;
           }
        }
        n = atoi(value);
        if (n < 2013) value = "2013";
        MyConfig.SetParamValue("xvu", "modelyear", value);
        return true;
    case 21:
        // check:
        if (strlen(value) == 0) value = "22";
        for (i = 0; i < strlen(value); i++) {
           if (isdigit(value[i]) == false) {
             value = "22";
             break;
           }
        }
        n = atoi(value);
        if (n < 15) value = "15";
        if (n > 30) value = "30";
        MyConfig.SetParamValue("xvu", "cc_temp", value);
        return true;
    default:
        return OvmsVehicle::SetFeature(key, value);
    }
}

const std::string OvmsVehicleVWeUp::GetFeature(int key)
{
    switch (key)
    {
    case 15:
    {
        int bits = (MyConfig.GetParamValueBool("xvu", "canwrite", false) ? 1 : 0);
        char buf[4];
        sprintf(buf, "%d", bits);
        return std::string(buf);
    }
    case 20:
      return MyConfig.GetParamValue("xvu", "modelyear", STR(DEFAULT_MODEL_YEAR));
    case 21:
      return MyConfig.GetParamValue("xvu", "cc_temp", STR(21));
    default:
        return OvmsVehicle::GetFeature(key);
    }
}

void OvmsVehicleVWeUp::ConfigChanged(OvmsConfigParam *param)
{
    if (param && param->GetName() != "xvu")
       return;

    ESP_LOGD(TAG, "VW e-Up reload configuration");
    vweup_modelyear_new = MyConfig.GetParamValueInt("xvu", "modelyear", DEFAULT_MODEL_YEAR);
    vweup_enable_obd = MyConfig.GetParamValueBool("xvu", "con_obd", true);
    vweup_enable_t26 = MyConfig.GetParamValueBool("xvu", "con_t26", true);
    vweup_enable_write = MyConfig.GetParamValueBool("xvu", "canwrite", false);
    vweup_cc_temp_int = MyConfig.GetParamValueInt("xvu", "cc_temp", 22);
    if (vweup_enable_t26)
        T26Init();    
    vweup_con = vweup_enable_obd * 2 + vweup_enable_t26;
    if (vweup_enable_obd || (vweup_modelyear<2020 && vweup_modelyear_new>2019) || (vweup_modelyear_new<2020 && vweup_modelyear>2019)) // switch between generations: reload OBD poll list
    {
        if (vweup_modelyear_new>2019) // set battery capacity & init calculated range
        {
            StandardMetrics.ms_v_bat_range_ideal->SetValue((260 * StandardMetrics.ms_v_bat_soc->AsFloat()) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
            StandardMetrics.ms_v_charge_climit->SetValue(32); // set max charge current to max possible for now
        }
        else
        {
            StandardMetrics.ms_v_bat_range_ideal->SetValue((160 * StandardMetrics.ms_v_bat_soc->AsFloat()) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
            StandardMetrics.ms_v_charge_climit->SetValue(16); // set max charge current to max possible for now
        }
        OBDInit();
        #ifdef CONFIG_OVMS_COMP_WEBSERVER
            WebDeInit();    // this can probably be done more elegantly... :-/
            WebInit();
        #endif
    }
    if (!vweup_enable_obd)
        OBDDeInit();
        #ifdef CONFIG_OVMS_COMP_WEBSERVER
            WebDeInit();    // this can probably be done more elegantly... :-/
            WebInit();
        #endif
    vweup_modelyear = vweup_modelyear_new;
    if (!vweup_con)
        ESP_LOGW(TAG,"Module will not work without any connection!");
}

void OvmsVehicleVWeUp::Ticker1(uint32_t ticker)
{
    if (vweup_con == 2) // only OBD connected -> get car state by polling OBD
    {
        OBDCheckCarState();
    }
    else // T26 connected
    {
        // This is just to be sure that we really have an asleep message. It has delay of 120 sec.
        // Do we still need this?
        if (StandardMetrics.ms_v_env_awake->IsStale())
        {
            StandardMetrics.ms_v_env_awake->SetValue(false);
        }

        // Autodisable climate control ticker (30 min.)
        if (vweup_remote_climate_ticker != 0)
        {
            vweup_remote_climate_ticker--;
            if (vweup_remote_climate_ticker == 1)
            {
                SendCommand(AUTO_DISABLE_CLIMATE_CONTROL);
            }
        }
        // Car disabled climate control
        if (!StandardMetrics.ms_v_env_on->AsBool() && vweup_remote_climate_ticker < 1770 && vweup_remote_climate_ticker != 0 && !StandardMetrics.ms_v_env_hvac->AsBool())
        {
            vweup_remote_climate_ticker = 0;

            ESP_LOGI(TAG, "Car disabled Climate Control or cc did not turn on");

            vweup_cc_on = false;
            ocu_awake = true;
        }
        
    }
    
}


/**
 * GetNotifyChargeStateDelay: framework hook
 */
int OvmsVehicleVWeUp::GetNotifyChargeStateDelay(const char* state)
{
  // With OBD data, wait for first voltage & current when starting the charge:
  if (vweup_con == 2 && strcmp(state, "charging") == 0)
    return 5;
  else
    return 3;
}
