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
;    Date:          28 November 2020
;
;    Changes:
;    0.1.0  Initial code
:           Crude merge of code from Chris van der Meijden (KCAN) and SokofromNZ (OBD2)
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
;    (C) 2020       sharkcow <sharkcow@gmx.de>
;
;    Biggest thanks to Chris van der Meijden, Dexter, SokofromNZ, Dimitrie78, E-Imo and 'der kleine Nik'.
*/

#include "ovms_log.h"
#include <string>
static const char *TAG = "v-vweup";

#define VERSION "0.3.0"


#include "vehicle_vweup.h"


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
    // Preparing for different VWEUP classes.
    // Example:
    //
    // MyVehicleFactory.RegisterVehicle<VWeUpObd>("VWUP.OBD", "VW e-Up (OBD2)");
    MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeUp>("VWUP", "VW e-Up");
}

/*
OvmsVehicleVWeUp* OvmsVehicleVWeUp::GetInstance(OvmsWriter* writer)
{
  OvmsVehicleVWeUp* vweup = (OvmsVehicleVWeUp*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();
  if (!vweup || type != "VWEUP") {
    if (writer)
      writer->puts("Error: VW e-Up vehicle module not selected");
    return NULL;
  }
  return vweup;
}
*/

/**
 * Constructor & destructor
 */

//size_t OvmsVehicleVWeUp::m_modifier = 0;

OvmsVehicleVWeUp::OvmsVehicleVWeUp()
{
    ESP_LOGI(TAG, "Start VW e-Up vehicle module");
    memset(m_vin, 0, sizeof(m_vin));

    RegisterCanBus(3, CAN_MODE_ACTIVE, CAN_SPEED_100KBPS);

    MyConfig.RegisterParam("xvu", "VW e-Up", true, true);
    ConfigChanged(NULL);
    vin_part1 = false;
    vin_part2 = false;
    vin_part3 = false;
    vweup_remote_climate_ticker = 0;
    ocu_awake = false;
    ocu_working = false;
    ocu_what = false;
    ocu_wait = false;
    vweup_cc_on = false;
    vweup_cc_turning_on = false;
    vweup_cc_temp_int = 21;
    fas_counter_on = 0;
    fas_counter_off = 0;
    signal_ok = false;
    cc_count = 0;
    cd_count = 0;

    dev_mode = false; // true disables writing on the comfort CAN. For code debugging only.

    //OBD2
//now done in OBdInit()    RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
    // init OBD2 poller:
    ObdInit();

    StandardMetrics.ms_v_env_locked->SetValue(true);
    StandardMetrics.ms_v_env_headlights->SetValue(false);

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
    switch (key)
    {
    case 15:
    {
        int bits = atoi(value);
        MyConfig.SetParamValueBool("xvu", "canwrite", (bits & 1) != 0);
        return true;
    }
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
        int bits =
            (MyConfig.GetParamValueBool("xvu", "canwrite", false) ? 1 : 0);
        char buf[4];
        sprintf(buf, "%d", bits);
        return std::string(buf);
    }
    default:
        return OvmsVehicle::GetFeature(key);
    }
}

void OvmsVehicleVWeUp::ConfigChanged(OvmsConfigParam *param)
{
    ESP_LOGD(TAG, "VW e-Up reload configuration");
    vweup_modelyear_new = MyConfig.GetParamValueInt("xvu", "modelyear", DEFAULT_MODEL_YEAR);
    vweup_enable_obd = MyConfig.GetParamValueBool("xvu", "con_obd", true);
    vweup_enable_t26 = MyConfig.GetParamValueBool("xvu", "con_t26", true);
    vweup_enable_write = MyConfig.GetParamValueBool("xvu", "canwrite", false);
    vweup_cc_temp_int = MyConfig.GetParamValueInt("xvu", "cc_temp", 21);
    vweup_con = vweup_enable_obd * 2 + vweup_enable_t26;
    ESP_LOGD(TAG,"vweup_con: %i",vweup_con);
    if (vweup_enable_obd || (vweup_modelyear<2020 && vweup_modelyear_new>2019) || (vweup_modelyear_new<2020 && vweup_modelyear>2019)) // switch between generations: reload OBD poll list
        ObdInit();
    if (!vweup_enable_obd)
        ObdDeInit();
    vweup_modelyear = vweup_modelyear_new;
    if (!vweup_enable_obd && !vweup_enable_t26)
        ESP_LOGW(TAG,"Module will not work without any connection!");
}

void OvmsVehicleVWeUp::Ticker1(uint32_t ticker)
{
    if (vweup_con == 2) // only OBD connected -> get car state by polling OBD
    {
//        ESP_LOGI(TAG,"only OBD");
        CheckCarStateOBD();
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
    }
    
}
