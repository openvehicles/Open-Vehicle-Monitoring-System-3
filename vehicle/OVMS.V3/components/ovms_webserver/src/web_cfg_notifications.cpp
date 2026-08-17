/*
;    Project:       Open Vehicle Monitor System
;    Date:          November 2025
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

#include "ovms_webserver.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * HandleCfgNotifications: configure notifications & data logging (URL /cfg/notifications)
 */
void OvmsWebServer::HandleCfgNotifications(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
  std::string error;
  std::string vehicle_minsoc, vehicle_stream;
  std::string log_trip_storetime, log_trip_minlength, log_grid_storetime;
  std::string rl_default_info_rate, rl_default_info_burst;
  std::string rl_default_alert_rate, rl_default_alert_burst;
  std::string rl_default_error_rate, rl_default_error_burst;
  std::string rl_queue_info_max_entries, rl_queue_alert_max_entries, rl_queue_error_max_entries;
  bool report_trip_enable;
  bool debug_tasks, debug_heap_alert;
  std::string report_trip_minlength;

  if (c.method == "POST") {
    // process form submission:
    vehicle_minsoc = c.getvar("vehicle_minsoc");
    vehicle_stream = c.getvar("vehicle_stream");
    log_trip_storetime = c.getvar("log_trip_storetime");
    log_trip_minlength = c.getvar("log_trip_minlength");
    log_grid_storetime = c.getvar("log_grid_storetime");
    rl_default_info_rate = c.getvar("rl_default_info_rate");
    rl_default_info_burst = c.getvar("rl_default_info_burst");
    rl_default_alert_rate = c.getvar("rl_default_alert_rate");
    rl_default_alert_burst = c.getvar("rl_default_alert_burst");
    rl_default_error_rate = c.getvar("rl_default_error_rate");
    rl_default_error_burst = c.getvar("rl_default_error_burst");
    rl_queue_info_max_entries = c.getvar("rl_queue_info_max_entries");
    rl_queue_alert_max_entries = c.getvar("rl_queue_alert_max_entries");
    rl_queue_error_max_entries = c.getvar("rl_queue_error_max_entries");
    report_trip_enable = (c.getvar("report_trip_enable") == "yes");
    report_trip_minlength = c.getvar("report_trip_minlength");
    debug_tasks = (c.getvar("debug_tasks") == "yes");
    debug_heap_alert = (c.getvar("debug_heap_alert") == "yes");

    if (vehicle_minsoc != "") {
      if (atoi(vehicle_minsoc.c_str()) < 0 || atoi(vehicle_minsoc.c_str()) > 100) {
        error += "<li data-input=\"vehicle_minsoc\">Min SOC must be in the range 0…100 %</li>";
      }
    }
    if (vehicle_stream != "") {
      if (atoi(vehicle_stream.c_str()) < 0 || atoi(vehicle_stream.c_str()) > 60) {
        error += "<li data-input=\"vehicle_stream\">GPS log interval must be in the range 0…60 seconds</li>";
      }
    }

    if (log_trip_storetime != "") {
      if (atoi(log_trip_storetime.c_str()) < 0 || atoi(log_trip_storetime.c_str()) > 365) {
        error += "<li data-input=\"log_trip_storetime\">Trip history log storage time must be in the range 0…365 days</li>";
      }
    }
    if (log_trip_minlength != "") {
      if (atoi(log_trip_minlength.c_str()) < 0) {
        error += "<li data-input=\"log_trip_minlength\">Trip min length must not be negative</li>";
      }
    }
    if (log_grid_storetime != "") {
      if (atoi(log_grid_storetime.c_str()) < 0 || atoi(log_grid_storetime.c_str()) > 365) {
        error += "<li data-input=\"log_grid_storetime\">Grid history log storage time must be in the range 0…365 days</li>";
      }
    }

    if (report_trip_minlength != "") {
      if (atoi(report_trip_minlength.c_str()) < 0) {
        error += "<li data-input=\"report_trip_minlength\">Trip report min length must not be negative</li>";
      }
    }

    if (rl_default_info_rate != "") {
      float v = atof(rl_default_info_rate.c_str());
      if (v <= 0 || v > 100) {
        error += "<li data-input=\"rl_default_info_rate\">Info rate must be in range 0.001…100.0 msg/s</li>";
      }
    }
    if (rl_default_info_burst != "") {
      float v = atof(rl_default_info_burst.c_str());
      if (v < 1 || v > 1000) {
        error += "<li data-input=\"rl_default_info_burst\">Info burst must be in range 1…1000</li>";
      }
    }
    if (rl_default_alert_rate != "") {
      float v = atof(rl_default_alert_rate.c_str());
      if (v <= 0 || v > 100) {
        error += "<li data-input=\"rl_default_alert_rate\">Alert rate must be in range 0.001…100.0 msg/s</li>";
      }
    }
    if (rl_default_alert_burst != "") {
      float v = atof(rl_default_alert_burst.c_str());
      if (v < 1 || v > 1000) {
        error += "<li data-input=\"rl_default_alert_burst\">Alert burst must be in range 1…1000</li>";
      }
    }
    if (rl_default_error_rate != "") {
      float v = atof(rl_default_error_rate.c_str());
      if (v <= 0 || v > 100) {
        error += "<li data-input=\"rl_default_error_rate\">Error rate must be in range 0.001…100.0 msg/s</li>";
      }
    }
    if (rl_default_error_burst != "") {
      float v = atof(rl_default_error_burst.c_str());
      if (v < 1 || v > 1000) {
        error += "<li data-input=\"rl_default_error_burst\">Error burst must be in range 1…1000</li>";
      }
    }

    if (rl_queue_info_max_entries != "") {
      int v = atoi(rl_queue_info_max_entries.c_str());
      if (v < 1 || v > 50000) {
        error += "<li data-input=\"rl_queue_info_max_entries\">Info queue limit must be in range 1…50000</li>";
      }
    }
    if (rl_queue_alert_max_entries != "") {
      int v = atoi(rl_queue_alert_max_entries.c_str());
      if (v < 1 || v > 50000) {
        error += "<li data-input=\"rl_queue_alert_max_entries\">Alert queue limit must be in range 1…50000</li>";
      }
    }
    if (rl_queue_error_max_entries != "") {
      int v = atoi(rl_queue_error_max_entries.c_str());
      if (v < 1 || v > 50000) {
        error += "<li data-input=\"rl_queue_error_max_entries\">Error queue limit must be in range 1…50000</li>";
      }
    }

    if (error == "") {
      // success:
      if (vehicle_minsoc == "0")
        MyConfig.DeleteInstance("vehicle", "minsoc");
      else
        MyConfig.SetParamValue("vehicle", "minsoc", vehicle_minsoc);
      if (vehicle_stream == "0")
        MyConfig.DeleteInstance("vehicle", "stream");
      else
        MyConfig.SetParamValue("vehicle", "stream", vehicle_stream);

      if (log_trip_storetime == "")
        MyConfig.DeleteInstance("notify", "log.trip.storetime");
      else
        MyConfig.SetParamValue("notify", "log.trip.storetime", log_trip_storetime);
      if (log_trip_minlength == "")
        MyConfig.DeleteInstance("notify", "log.trip.minlength");
      else
        MyConfig.SetParamValue("notify", "log.trip.minlength", log_trip_minlength);
      if (log_grid_storetime == "")
        MyConfig.DeleteInstance("notify", "log.grid.storetime");
      else
        MyConfig.SetParamValue("notify", "log.grid.storetime", log_grid_storetime);

      MyConfig.SetParamValueBool("notify", "report.trip.enable", report_trip_enable);
      if (report_trip_minlength == "")
        MyConfig.DeleteInstance("notify", "report.trip.minlength");
      else
        MyConfig.SetParamValue("notify", "report.trip.minlength", report_trip_minlength);

      if (rl_default_info_rate == "")
        MyConfig.DeleteInstance("notify", "rl.default.info.rate");
      else
        MyConfig.SetParamValue("notify", "rl.default.info.rate", rl_default_info_rate);
      if (rl_default_info_burst == "")
        MyConfig.DeleteInstance("notify", "rl.default.info.burst");
      else
        MyConfig.SetParamValue("notify", "rl.default.info.burst", rl_default_info_burst);

      if (rl_default_alert_rate == "")
        MyConfig.DeleteInstance("notify", "rl.default.alert.rate");
      else
        MyConfig.SetParamValue("notify", "rl.default.alert.rate", rl_default_alert_rate);
      if (rl_default_alert_burst == "")
        MyConfig.DeleteInstance("notify", "rl.default.alert.burst");
      else
        MyConfig.SetParamValue("notify", "rl.default.alert.burst", rl_default_alert_burst);

      if (rl_default_error_rate == "")
        MyConfig.DeleteInstance("notify", "rl.default.error.rate");
      else
        MyConfig.SetParamValue("notify", "rl.default.error.rate", rl_default_error_rate);
      if (rl_default_error_burst == "")
        MyConfig.DeleteInstance("notify", "rl.default.error.burst");
      else
        MyConfig.SetParamValue("notify", "rl.default.error.burst", rl_default_error_burst);

      if (rl_queue_info_max_entries == "")
        MyConfig.DeleteInstance("notify", "rl.queue.info.max_entries");
      else
        MyConfig.SetParamValue("notify", "rl.queue.info.max_entries", rl_queue_info_max_entries);
      if (rl_queue_alert_max_entries == "")
        MyConfig.DeleteInstance("notify", "rl.queue.alert.max_entries");
      else
        MyConfig.SetParamValue("notify", "rl.queue.alert.max_entries", rl_queue_alert_max_entries);
      if (rl_queue_error_max_entries == "")
        MyConfig.DeleteInstance("notify", "rl.queue.error.max_entries");
      else
        MyConfig.SetParamValue("notify", "rl.queue.error.max_entries", rl_queue_error_max_entries);

      MyConfig.SetParamValueBool("module", "debug.tasks", debug_tasks);
      MyConfig.SetParamValueBool("module", "debug.heap.alert", debug_heap_alert);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Notifications configured.</p>");
      OutputHome(p, c);
      c.done();
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
    vehicle_minsoc = MyConfig.GetParamValue("vehicle", "minsoc");
    vehicle_stream = MyConfig.GetParamValue("vehicle", "stream");
    log_trip_storetime = MyConfig.GetParamValue("notify", "log.trip.storetime");
    log_trip_storetime = MyConfig.GetParamValue("notify", "log.trip.storetime");
    log_trip_minlength = MyConfig.GetParamValue("notify", "log.trip.minlength");
    log_grid_storetime = MyConfig.GetParamValue("notify", "log.grid.storetime");
    rl_default_info_rate = MyConfig.GetParamValue("notify", "rl.default.info.rate");
    rl_default_info_burst = MyConfig.GetParamValue("notify", "rl.default.info.burst");
    rl_default_alert_rate = MyConfig.GetParamValue("notify", "rl.default.alert.rate");
    rl_default_alert_burst = MyConfig.GetParamValue("notify", "rl.default.alert.burst");
    rl_default_error_rate = MyConfig.GetParamValue("notify", "rl.default.error.rate");
    rl_default_error_burst = MyConfig.GetParamValue("notify", "rl.default.error.burst");
    rl_queue_info_max_entries = MyConfig.GetParamValue("notify", "rl.queue.info.max_entries");
    rl_queue_alert_max_entries = MyConfig.GetParamValue("notify", "rl.queue.alert.max_entries");
    rl_queue_error_max_entries = MyConfig.GetParamValue("notify", "rl.queue.error.max_entries");
    report_trip_enable = MyConfig.GetParamValueBool("notify", "report.trip.enable");
    report_trip_minlength = MyConfig.GetParamValue("notify", "report.trip.minlength");
    debug_tasks = MyConfig.GetParamValueBool("module", "debug.tasks");
    debug_heap_alert = MyConfig.GetParamValueBool("module", "debug.heap.alert");

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Notifications &amp; Data Logging");
  c.form_start(p.uri);

  c.fieldset_start("Vehicle Monitoring");

  c.input_slider("Location streaming", "vehicle_stream", 3, "sec",
    -1, atoi(vehicle_stream.c_str()), 0, 0, 60, 1,
    "<p>While driving send location updates to server every n seconds, 0 = use default update interval "
    "from server configuration. Same as App feature #8.</p>");

  c.input_slider("Minimum SOC", "vehicle_minsoc", 3, "%",
    -1, atoi(vehicle_minsoc.c_str()), 0, 0, 100, 1,
    "<p>Send an alert when SOC drops below this level, 0 = off. Same as App feature #9.</p>");

  c.fieldset_end();

  c.fieldset_start("Vehicle Reports");

  c.input_checkbox("Enable trip report", "report_trip_enable", report_trip_enable,
    "<p>This will send a textual report on driving statistics after each trip.</p>");
  c.input("number", "Report min trip length", "report_trip_minlength", report_trip_minlength.c_str(), "Default: 0.2 km",
    "<p>Only trips over at least this distance will produce a report. If your vehicle does not support the "
    "<code>v.p.trip</code> metric, set this to 0.</p>",
    "min=\"0\" step=\"0.1\"", "km");

  c.fieldset_end();

  c.fieldset_start("Data Log Storage Times");

  c.input("number", "Trip history log", "log_trip_storetime", log_trip_storetime.c_str(), "Default: empty/0 = disabled",
    "<p>Empty/0 = disabled. If enabled, the trip log receives one entry per trip, "
    "see <a target=\"_blank\" href=\""
    "https://docs.openvehicles.com/en/latest/userguide/notifications.html#trip-history-log"
    "\">user manual</a> for details.</p>",
    "min=\"0\" max=\"365\" step=\"1\"", "days");
  c.input("number", "Trip min length", "log_trip_minlength", log_trip_minlength.c_str(), "Default: 0.2 km",
    "<p>Only trips over at least this distance will be logged. If your vehicle does not support the "
    "<code>v.p.trip</code> metric, set this to 0.</p>",
    "min=\"0\" step=\"0.1\"", "km");

  c.input("number", "Grid history log", "log_grid_storetime", log_grid_storetime.c_str(), "Default: empty/0 = disabled",
    "<p>Empty/0 = disabled. If enabled, the grid log receives one entry per charge/generator state change, "
    "see <a target=\"_blank\" href=\""
    "https://docs.openvehicles.com/en/latest/userguide/notifications.html#grid-history-log"
    "\">user manual</a> for details.</p>",
    "min=\"0\" max=\"365\" step=\"1\"", "days");

  c.fieldset_end();

  c.fieldset_start("Notification Flood Protection");

  c.input("number", "Default info rate", "rl_default_info_rate", rl_default_info_rate.c_str(), "Default: 0.1",
    "<p>Allowed message rate for <code>info</code> notifications (messages per second).</p>"
    "<p>e.g. 0.1 = 1 message every 10 seconds, 1 = 1 message per second, 10 = 10 messages per second.</p>",
    "min=\"0.001\" max=\"100\" step=\"0.001\"", "msg/s");
  c.input("number", "Default info burst", "rl_default_info_burst", rl_default_info_burst.c_str(), "Default: 3",
    "<p>Token bucket burst size for <code>info</code>. This is the maximum number of messages that can pass immediately "
    "during a short spike before throttling starts.</p>"
    "<p>Functional example: with rate = 0.1 msg/s and burst = 3, up to 3 info messages can be sent right away. "
    "After that, the bucket refills by 1 token every 10 seconds, so the next message is allowed after about 10 seconds.</p>",
    "min=\"1\" max=\"1000\" step=\"1\"");

  c.input("number", "Default alert rate", "rl_default_alert_rate", rl_default_alert_rate.c_str(), "Default: 0.2",
    "<p>Allowed message rate for <code>alert</code> notifications.</p>"
    "<p>e.g. 0.1 = 1 message every 10 seconds, 1 = 1 message per second, 10 = 10 messages per second.</p>",
    "min=\"0.001\" max=\"100\" step=\"0.001\"", "msg/s");
  c.input("number", "Default alert burst", "rl_default_alert_burst", rl_default_alert_burst.c_str(), "Default: 5",
    "<p>Token bucket burst size for <code>alert</code>. This is the maximum number of messages that can pass immediately "
    "during a short spike before throttling starts.</p>"
    "<p>Functional example: with rate = 0.2 msg/s and burst = 5, up to 5 alert messages can be sent right away. "
    "After that, the bucket refills by 1 token every 5 seconds.</p>",
    "min=\"1\" max=\"1000\" step=\"1\"");

  c.input("number", "Default error rate", "rl_default_error_rate", rl_default_error_rate.c_str(), "Default: 0.5",
    "<p>Allowed message rate for <code>error</code> notifications.</p>"
    "<p>e.g. 0.1 = 1 message every 10 seconds, 1 = 1 message per second, 10 = 10 messages per second.</p>",
    "min=\"0.001\" max=\"100\" step=\"0.001\"", "msg/s");
  c.input("number", "Default error burst", "rl_default_error_burst", rl_default_error_burst.c_str(), "Default: 10",
    "<p>Token bucket burst size for <code>error</code>. This is the maximum number of messages that can pass immediately "
    "during a short spike before throttling starts.</p>"
    "<p>Functional example: with rate = 0.5 msg/s and burst = 10, up to 10 error messages can be sent right away. "
    "After that, the bucket refills by 1 token every 2 seconds.</p>",
    "min=\"1\" max=\"1000\" step=\"1\"");

  c.input("number", "Info queue limit", "rl_queue_info_max_entries", rl_queue_info_max_entries.c_str(), "Default: 200",
    "<p>Maximum queued <code>info</code> notifications before new messages are dropped.</p>",
    "min=\"1\" max=\"50000\" step=\"1\"", "entries");
  c.input("number", "Alert queue limit", "rl_queue_alert_max_entries", rl_queue_alert_max_entries.c_str(), "Default: 200",
    "<p>Maximum queued <code>alert</code> notifications before new messages are dropped.</p>",
    "min=\"1\" max=\"50000\" step=\"1\"", "entries");
  c.input("number", "Error queue limit", "rl_queue_error_max_entries", rl_queue_error_max_entries.c_str(), "Default: 300",
    "<p>Maximum queued <code>error</code> notifications before new messages are dropped.</p>",
    "min=\"1\" max=\"50000\" step=\"1\"", "entries");

  c.input_info("Advanced overrides",
    "Use notify keys like <code>rl.channel.&lt;caller&gt;.&lt;type&gt;.rate</code> and "
    "<code>rl.channel.&lt;caller&gt;.&lt;type&gt;.subtype.&lt;subtype&gt;.rate</code> for channel/subtype specific limits. "
    "Example: set <code>config set notify rl.channel.ovmsv3.alert.rate=0.5</code> to allow up to 1 alert every 2 seconds on ovmsv3, "
    "and set <code>config set notify rl.channel.ovmsv3.alert.subtype.vehicle.idle.rate=0.1</code> to further limit that subtype to 1 every 10 seconds.");

  c.fieldset_end();

  c.fieldset_start("OVMS Debugging");

  c.input_checkbox("Enable task logging", "debug_tasks", debug_tasks,
    "<p>Record running task statistics every 5 minutes in server table <code>*-OVM-DebugTasks</code>.</p>"
    "<p>The data recorded is the same as shown in the <kbd class=\"autoselect\">module tasks</kbd> command output.</p>"
    "<p>Enable this if your module spuriously becomes slow or unresponsive, or if you encounter"
    " random crashes, so a developer can check the log for unusual task behaviour.</p>");

  c.input_checkbox("Enable heap integrity alert", "debug_heap_alert", debug_heap_alert,
    "<p>Check heap memory integrity every 5 minutes and send an alert notification when a corruption is detected.</p>"
    "<p>Enable this if you encounter random crashes, reboot ASAP on receiving the alert.</p>"
    "<p>To be able to provide additional info to a developer, also enable task logging above and"
    " <a href=\"/cfg/logging\" target=\"#main\">file logging</a> at debug level.</p>");

  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end(
    "<p>Data logs are sent to the server (V2/V3). A V2 server may store the logs as requested by you "
    "for up to 365 days, depending on the server configuration. You can download the log tables "
    "in CSV format from the server using the standard server APIs or the browser UI provided "
    "by the server.</p>"
    "<p>An MQTT (V3) server normally won't store data records for longer than a few days, possibly "
    "hours. You need to fetch them as soon as possible. There are automated MQTT tools available "
    "for this purpose.</p>"
    "<p>See <a target=\"_blank\" href=\""
    "https://docs.openvehicles.com/en/latest/userguide/notifications.html"
    "\">user manual</a> for details on notifications.</p>"
    );
  c.done();
}
