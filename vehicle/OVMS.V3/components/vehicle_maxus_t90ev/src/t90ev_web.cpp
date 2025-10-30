/*
;    Project:       Open Vehicle Monitor System
;    Date:          2024
;
;    (C) 2024       [Your Name or Handle Here]
;    (C) 2024       OVMS Community
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

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER

#include <stdio.h>
#include <string>
#include <sstream>
#include "ovms_metrics.h"
#include "ovms_config.h"
#include "ovms_webserver.h"
#include "vehicle_maxus_t90ev.h"

using namespace std;

//
// WebInit: register pages
//
void OvmsVehicleMaxusT90EV::WebInit()
{
    // vehicle menu:
    MyWebServer.RegisterPage("/t90ev/features", "T90EV Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
}

//
// WebDeInit: deregister pages
//
void OvmsVehicleMaxusT90EV::WebDeInit()
{
    MyWebServer.DeregisterPage("/t90ev/features");
}

//
// WebCfgFeatures: handle vehicle specific features configuration
//
void OvmsVehicleMaxusT90EV::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
    // Placeholder for configuration settings unique to the T90EV
    std::string error;

    if (c.method == "POST") {
        // process form submission... (Add logic here later)

        if (error == "") {
            // store configuration... (Add logic here later)

            c.head(200);
            c.alert("success", "<p class=\"lead\">Maxus T90EV setup saved.</p>");
            MyWebServer.OutputHome(p, c);
            c.done();
            return;
        }

        // output error, return to form:
        error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
        c.head(400);
        c.alert("danger", error.c_str());
    }
    else {
        // read configuration: (Add logic here later)
        c.head(200);
    }

    // generate form:
    c.panel_start("primary", "Maxus T90EV Features");
    c.form_start(p.uri);

    c.fieldset_start("Vehicle Configuration");

    // Placeholder for configuration inputs:
    c.print(
        "<p>Configuration options for the Maxus T90EV will be added here once specific features are defined.</p>"
    );

    c.fieldset_end();

    c.print("<hr>");
    c.input_button("default", "Save");
    c.form_end();
    c.panel_end();
    c.done();
}

//
// GetDashboardConfig: Maxus T90EV specific dashboard setup
//
void OvmsVehicleMaxusT90EV::GetDashboardConfig(DashboardConfig& cfg)
{
    // These gauges are modeled after the eDeliver 3 and assume similar EV performance characteristics
    // Adjust MinMax and Bands based on actual T90EV specs (Voltage, Power, etc.)

    // Speed:
    dash_gauge_t speed_dash(NULL, Kph);
    speed_dash.SetMinMax(0, 140, 5);
    speed_dash.AddBand("green", 0, 70);
    speed_dash.AddBand("yellow", 70, 110);
    speed_dash.AddBand("red", 110, 140);

    // Voltage (Assuming 400V nominal system, similar to eDeliver 3):
    dash_gauge_t voltage_dash(NULL, Volts);
    voltage_dash.SetMinMax(300, 420);
    voltage_dash.AddBand("red", 300, 340);
    voltage_dash.AddBand("yellow", 340, 380);
    voltage_dash.AddBand("green", 380, 420);

    // SOC:
    dash_gauge_t soc_dash("SOC ", Percentage);
    soc_dash.SetMinMax(10, 100);
    soc_dash.AddBand("red", 10, 15.5);
    soc_dash.AddBand("yellow", 15.5, 25);
    soc_dash.AddBand("green", 25, 100);

    // Efficiency:
    dash_gauge_t eff_dash(NULL, WattHoursPK);
    eff_dash.SetMinMax(0, 600);
    eff_dash.AddBand("green", 0, 250);
    eff_dash.AddBand("yellow", 250, 450);
    eff_dash.AddBand("red", 450, 600);

    // Power:
    // T90EV power is higher than eDeliver 3 (130kW vs 90kW), so increase max power here.
    dash_gauge_t power_dash(NULL, kW);
    power_dash.SetMinMax(-30, 150); // Regen up to -30kW, Motor up to 150kW
    power_dash.AddBand("violet", -30, 0); // Regeneration
    power_dash.AddBand("green", 0, 50);
    power_dash.AddBand("yellow", 50, 100);
    power_dash.AddBand("red", 100, 150);

    // Battery temperature:
    dash_gauge_t batteryt_dash("BAT ", Celcius);
    batteryt_dash.SetMinMax(-15, 65);
    batteryt_dash.SetTick(25);
    batteryt_dash.AddBand("red", -15, 0);
    batteryt_dash.AddBand("normal", 0, 40);
    batteryt_dash.AddBand("red", 40, 65);

    // Combine into JSON string
    std::ostringstream str;
    str << "yAxis: ["
        << speed_dash << "," // Speed
        << voltage_dash << "," // Voltage
        << soc_dash << "," // SOC
        << eff_dash << "," // Efficiency
        << power_dash << "," // Power
        << batteryt_dash // Battery temperature
        << "]";
    cfg.gaugeset1 = str.str();
}

#endif //CONFIG_OVMS_COMP_WEBSERVER
