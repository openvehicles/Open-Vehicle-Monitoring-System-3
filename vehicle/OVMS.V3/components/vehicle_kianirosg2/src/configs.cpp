/*
;    Project:       Open Vehicle Monitor System
;    Date:          21th January 2019
;
;    (C) 2019       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_kia_niroevsg2.h"
#include "common.h"
#include "ovms_boot.h"

static const char *TAG = "v-kianiroevsg2";

bool OvmsVehicleKiaNiroEvSg2::ConfigChanged()
{
    reset_by_config = true;
    int task_limit = 20;
    if (uxQueueSpacesAvailable(MyEvents.m_taskqueue) < task_limit)
    {
        return false;
    }
    configured = false;
    return true;
}

void OvmsVehicleKiaNiroEvSg2::VerifySingleConfig(std::string param, std::string instance, std::string defValue, std::string value)
{
    if (MyConfig.GetParamValue(param, instance, defValue) != value)
    {
        if (ConfigChanged())
        {
            MyConfig.SetParamValue(param, instance, value);
            ESP_LOGE(TAG, "Set new configuration %s %s", param.c_str(), instance.c_str());
        }
    }
}

void OvmsVehicleKiaNiroEvSg2::VerifySingleConfigInt(std::string param, std::string instance, int defValue, int value)
{
    if (MyConfig.GetParamValueInt(param, instance, defValue) != value)
    {
        if (ConfigChanged())
        {
            MyConfig.SetParamValueInt(param, instance, value);
            ESP_LOGE(TAG, "Set new configuration %s %s", param.c_str(), instance.c_str());
        }
    }
}

void OvmsVehicleKiaNiroEvSg2::VerifySingleConfigBool(std::string param, std::string instance, bool defValue, bool value)
{
    if (MyConfig.GetParamValueBool(param, instance, defValue) != value)
    {
        if (ConfigChanged())
        {
            MyConfig.SetParamValueBool(param, instance, value);
            ESP_LOGE(TAG, "Set new configuration %s %s", param.c_str(), instance.c_str());
        }
    }
}
void OvmsVehicleKiaNiroEvSg2::VerifyConfigs(bool verify)
{
    if (verify && fully_configured)
    {
        return;
    }

    std::string id = "000000000000000";
    std::string prefix = "ovms/car/";
    std::string iccid = StdMetrics.ms_m_net_mdm_iccid->AsString();

    configured = true;

    prefix.append(id);
    prefix.append("/");

    if (MyConfig.GetParamValue("vehicle", "id").empty())
    {
        if (ConfigChanged())
        {
            MyConfig.SetParamValue("vehicle", "id", id);
            ESP_LOGE(TAG, "Set new configuration %s %s", "vehicle", "id");
        }
    }
    if (MyConfig.GetParamValue("wifi.ap", id).empty())
    {
        if (ConfigChanged())
        {
            MyConfig.SetParamValue("wifi.ap", id, "Yx1EDhoawUW4");
            ESP_LOGE(TAG, "Set new configuration %s %s", "wifi.ap", id.c_str());
        }
    }
    if (MyConfig.GetParamValue("server.v3", "topic.prefix").empty())
    {
        if (ConfigChanged())
        {
            MyConfig.SetParamValue("server.v3", "topic.prefix", prefix);
            ESP_LOGE(TAG, "Set new configuration %s %s", "server.v3", "topic.prefix");
        }
    }
    if (MyConfig.GetParamValue("auto", "wifi.ssid.ap").empty())
    {
        if (ConfigChanged())
        {
            MyConfig.SetParamValue("auto", "wifi.ssid.ap", id);
            ESP_LOGE(TAG, "Set new configuration %s %s", "auto", "wifi.ssid.ap");
        }
    }
    VerifySingleConfig("module", "init", "", "done");
    VerifySingleConfig("wifi.ssid", "remoteWifiTucar", "", "&P9j1890Kz$8MzX");
    VerifySingleConfig("wifi.ssid", "DriverTucarApp", "", "driverpasstucarapp");
    VerifySingleConfig("server.v3", "server", "", "30f6ea97fbdf42edaee1065da27105c5.s1.eu.hivemq.cloud");
    VerifySingleConfig("auto", "wifi.ssid.client", "yes", "");
    VerifySingleConfig("password", "module", "", "Yx1EDhoawUW4");
    VerifySingleConfig("password", "server.v3", "", "nGX0PCfHxVRJ");
    VerifySingleConfigBool("auto", "dbc", true, false);
    VerifySingleConfigBool("auto", "egpio", false, true);
    VerifySingleConfigBool("auto", "ext12v", true, false);
    VerifySingleConfigBool("auto", "init", false, true);
    VerifySingleConfigBool("auto", "modem", false, true);
    VerifySingleConfigBool("auto", "ota", true, false);
    VerifySingleConfigBool("auto", "scripting", false, true);
    VerifySingleConfigBool("auto", "server.v2", true, false);
    VerifySingleConfigBool("auto", "server.v3", false, true);
    VerifySingleConfig("auto", "vehicle.type", "", "KN2");
    VerifySingleConfig("auto", "wifi.mode", "", "client");
    VerifySingleConfigBool("log", "file.enable", true, false);
    VerifySingleConfig("log", "level", "", "error");
    VerifySingleConfig("modem", "apn", "", "m2m.entel.cl");
    VerifySingleConfigBool("modem", "enable.gps", false, true);
    VerifySingleConfigBool("modem", "enable.gpstime", false, true);
    VerifySingleConfigBool("modem", "enable.net", false, true);
    VerifySingleConfigBool("modem", "enable.sms", false, true);
    VerifySingleConfig("server.v3", "port", "", "8883");
    VerifySingleConfigBool("server.v3", "tls", false, true);
    VerifySingleConfigInt("server.v3", "updatetime.awake", 0, 1);
    VerifySingleConfigInt("server.v3", "updatetime.charging", 0, 5);
    VerifySingleConfigInt("server.v3", "updatetime.connected", 0, 1);
    VerifySingleConfigInt("server.v3", "updatetime.idle", 0, 60);
    VerifySingleConfigInt("server.v3", "updatetime.on", 0, 1);
    VerifySingleConfigInt("server.v3", "updatetime.sendall", 0, 300);
    VerifySingleConfig("server.v3", "user", "", "ovmsTucar");
    VerifySingleConfigBool("vehicle", "bms.alerts.enabled", true, false);
    VerifySingleConfig("vehicle", "units.accel", "", "kmphps");
    VerifySingleConfig("vehicle", "units.accelshort", "", "mpss");
    VerifySingleConfig("vehicle", "units.charge", "", "amphours");
    VerifySingleConfig("vehicle", "units.consumption", "", "kmpkwh");
    VerifySingleConfig("vehicle", "units.distance", "", "K");
    VerifySingleConfig("vehicle", "units.distanceshort", "", "meters");
    VerifySingleConfig("vehicle", "units.energy", "", "kwh");
    VerifySingleConfig("vehicle", "units.power", "", "kw");
    VerifySingleConfig("vehicle", "units.pressure", "", "kpa");
    VerifySingleConfig("vehicle", "units.ratio", "", "percent");
    VerifySingleConfig("vehicle", "units.signal", "", "dbm");
    VerifySingleConfig("vehicle", "units.speed", "", "kmph");
    VerifySingleConfig("vehicle", "units.temp", "", "celcius");

    if (!iccid.empty() && MyConfig.GetParamValue("vehicle", "id").compare(id) == 0)
    {
        fully_configured = false;
        std::string alt_id = "000000000001";
        alt_id.append(iccid.substr(17, 3).c_str());
        prefix = "ovms/car/";
        prefix.append(alt_id);
        prefix.append("/");
        VerifySingleConfig("wifi.ap", alt_id, "", "Yx1EDhoawUW4");
        VerifySingleConfig("server.v3", "topic.prefix", "", prefix);
        VerifySingleConfig("auto", "wifi.ssid.ap", "", alt_id);

        if (configured)
        {
            if (ConfigChanged())
            {
                MyConfig.SetParamValue("vehicle", "id", alt_id);
                ESP_LOGE(TAG, "Set new configuration %s %s", "vehicle", "id");
            }
        }
    }

    if (configured && reset_by_config)
    {
        ESP_LOGE(TAG, "Configurations were not set");
        MyBoot.Restart();
    }
    if (configured)
    {
        fully_configured = true;
    }
}