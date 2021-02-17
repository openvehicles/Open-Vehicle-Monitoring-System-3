/**
 * Project:      Open Vehicle Monitor System
 * Module:       Mitsubishi iMiEV, Citroen C-Zero, Peugeot iOn Webserver
 *
 * (C) 2018	    Nikolay Shishkov <nshishkov@yahoo.com>
 * (C) 2018	    Geir Øyvind Væidalo <geir@validalo.net>
 * (C) 2017     Michael Balzer <dexter@dexters-web.de>
 * (C) 2018-2020 Tamás Kovács (KommyKT)
 *
 *Changes:
 ;    1.0.0  Initial release:
 ;       - Dashboard modifications
 ;    1.0.1
 ;       - Dashboard modification from 80 cell charge_state
 ;       - Add Ideal range to settings
 ;       - Add 80 cell support for settings
 ;    1.0.4
 ;       - Commands fix
 ;    1.0.6
 ;       - Remove SOH settings
 ;       - Remove ideal range settings
 ;
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_mgev.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void OvmsVehicleMgEv::WebInit()
{
    // vehicle menu:
    
    MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * GetDashboardConfig: Mitsubishi i-MiEV Citroen C-Zero, Peugeot iOn specific dashboard setup
 */
void OvmsVehicleMgEv::GetDashboardConfig(DashboardConfig& cfg)
{
    cfg.gaugeset1 =
    "yAxis: [{"
    // Speed:
    "min: 0, max: 135,"
    "plotBands: ["
    "{ from: 0, to: 60, className: 'green-band' },"
    "{ from: 60, to: 100, className: 'yellow-band' },"
    "{ from: 100, to: 135, className: 'red-band' }]"
    "},{"
    // Voltage:
    "min: 310, max: 460,"
    "plotBands: ["
    "{ from: 310, to: 360, className: 'red-band' },"
    "{ from: 360, to: 410, className: 'yellow-band' },"
    "{ from: 410, to: 460, className: 'green-band' }]"
    "},{"
    // SOC:
    "min: 10, max: 100,"
    "plotBands: ["
    "{ from: 10, to: 15.5, className: 'red-band' },"
    "{ from: 15.5, to: 25, className: 'yellow-band' },"
    "{ from: 25, to: 100, className: 'green-band' }]"
    "},{"
    // Efficiency:
    "min: 0, max: 300,"
    "plotBands: ["
    "{ from: 0, to: 120, className: 'green-band' },"
    "{ from: 120, to: 250, className: 'yellow-band' },"
    "{ from: 250, to: 300, className: 'red-band' }]"
    "},{"
    // Power:
    "min: -30, max: 85,"
    "plotBands: ["
    "{ from: -30, to: 0, className: 'violet-band' },"
    "{ from: 0, to: 20, className: 'green-band' },"
    "{ from: 20, to: 50, className: 'yellow-band' },"
    "{ from: 50, to: 85, className: 'red-band' }]"
    "},{"
    // Charger temperature:
    "min: -10, max: 55, tickInterval: 20,"
    "plotBands: ["
    "{ from: -10, to: 40, className: 'normal-band border' },"
    "{ from: 40, to: 55, className: 'red-band border' }]"
    "},{"
    // Battery temperature:
    "min: -15, max: 65, tickInterval: 25,"
    "plotBands: ["
    "{ from: -15, to: 0, className: 'red-band border' },"
    "{ from: 0, to: 40, className: 'normal-band border' },"
    "{ from: 40, to: 65, className: 'red-band border' }]"
    "},{"
    // Inverter temperature:
    "min: -10, max: 55, tickInterval: 20,"
    "plotBands: ["
    "{ from: -10, to: 40, className: 'normal-band border' },"
    "{ from: 40, to: 55, className: 'red-band border' }]"
    "},{"
    // Motor temperature:
    "min: 20, max: 100, tickInterval: 25,"
    "plotBands: ["
    "{ from: 20, to: 75, className: 'normal-band border' },"
    "{ from: 75, to: 100, className: 'red-band border' }]"
    "}]";
}

#endif //CONFIG_OVMS_COMP_WEBSERVER
