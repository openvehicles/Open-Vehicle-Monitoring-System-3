/*
;    Project:       Open Vehicle Monitor System
;    Subproject:    VW e-Golf web plugin
;
;    (C) 2026  Andreas Jansson
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

#include "metrics_standard.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_webserver.h"
#include "vehicle_vwegolf.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())

void OvmsVehicleVWeGolf::WebInit() {
    MyWebServer.RegisterPage("/xvg/metrics", "Battery & Charge", WebDispMetrics, PageMenu_Vehicle,
                             PageAuth_Cookie);
}

void OvmsVehicleVWeGolf::WebDeInit() {
    MyWebServer.DeregisterPage("/xvg/metrics");
}

void OvmsVehicleVWeGolf::WebDispMetrics(PageEntry_t& p, PageContext_t& c) {
    c.head(200);
    PAGE_HOOK("body.pre");

    c.print(
        "<div class=\"panel panel-primary\">"
        "<div class=\"panel-heading\">VW e-Golf Battery &amp; Charge</div>"
        "<div class=\"panel-body\">"
        "<div class=\"receiver\">"

        "<div class=\"clearfix\">"
        "<div class=\"metric progress\" data-metric=\"v.b.soc\" data-prec=\"1\">"
        "<div class=\"progress-bar value-low text-left\" role=\"progressbar\""
        " aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
        "<div>"
        "<span class=\"label\">SoC</span>"
        "<span class=\"value\">?</span>"
        "<span class=\"unit\">%</span>"
        "</div>"
        "</div>"
        "</div>"
        "</div>"

        "<div class=\"clearfix\">"
        "<h6 style=\"margin-bottom:0;color:#676767;font-size:15px\">Pack:</h6>"
        "<div class=\"metric progress\" data-metric=\"v.b.voltage\" data-prec=\"1\">"
        "<div class=\"progress-bar progress-bar-info value-low text-left\" role=\"progressbar\""
        " aria-valuenow=\"0\" aria-valuemin=\"280\" aria-valuemax=\"400\" style=\"width:0%\">"
        "<div>"
        "<span class=\"label\">Voltage</span>"
        "<span class=\"value\">?</span>"
        "<span class=\"unit\">V</span>"
        "</div>"
        "</div>"
        "</div>"
        "<div class=\"metric number\" data-metric=\"v.b.current\" data-prec=\"1\">"
        "<span class=\"label\">Current</span>"
        "<span class=\"value\">?</span>"
        "<span class=\"unit\">A</span>"
        "</div>"
        "<div class=\"metric number\" data-metric=\"v.b.power\" data-prec=\"2\">"
        "<span class=\"label\">Power</span>"
        "<span class=\"value\">?</span>"
        "<span class=\"unit\">kW</span>"
        "</div>"
        "</div>"

        "<div class=\"clearfix\">"
        "<h6 style=\"margin-bottom:0;color:#676767;font-size:15px\">Trip energy:</h6>"
        "<div class=\"metric number\" data-metric=\"v.b.energy.used\" data-prec=\"3\">"
        "<span class=\"label\">Used</span>"
        "<span class=\"value\">?</span>"
        "<span class=\"unit\">kWh</span>"
        "</div>"
        "<div class=\"metric number\" data-metric=\"v.b.energy.recd\" data-prec=\"3\">"
        "<span class=\"label\">Recovered</span>"
        "<span class=\"value\">?</span>"
        "<span class=\"unit\">kWh</span>"
        "</div>"
        "</div>"

        "</div>"
        "</div>"
        "</div>"
        "<script>"
        "$(\"#livestatus\").on(\"msg:metrics\", function(e, update) {"
        "$(\"[data-metric]\", $(this)).each(function() {"
        "$(this).trigger(\"update\");"
        "});"
        "});"
        "</script>");

    PAGE_HOOK("body.post");
    c.done();
}

// Settings page removed with the cc-temp parameter: remote climate uses the setpoint
// stored in the car's global Battery Control Profile (set via infotainment) — an OVMS-side
// temperature setting had no effect on the wire (see clima-control-bap.md, PR #1430 review).

#endif  // CONFIG_OVMS_COMP_WEBSERVER
