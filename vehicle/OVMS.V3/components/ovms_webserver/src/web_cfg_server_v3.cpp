/*
;    Project:       Open Vehicle Monitor System
;    Date:          November 2025
;
;    Changes:
;    1.0  Initial release - Climate schedule configuration
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
#include <string.h>
#include <stdio.h>
#include <sstream> 
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


#ifdef CONFIG_OVMS_COMP_SERVER
#ifdef CONFIG_OVMS_COMP_SERVER_V3
/**
 * HandleCfgServerV3: configure server v3 connection (URL /cfg/server/v3)
 */
void OvmsWebServer::HandleCfgServerV3(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string server, user, password, port, topic_prefix;
  std::string updatetime_connected, updatetime_idle, updatetime_on;
  std::string updatetime_charging, updatetime_awake, updatetime_sendall, updatetime_keepalive;
  std::string metrics_priority, metrics_include, metrics_exclude, metrics_immediately, metrics_exclude_immediately;
  std::string queue_sendall, queue_modified;
  bool tls, legacy_event_topic, updatetime_priority, updatetime_immediately;

  if (c.method == "POST") {
    // process form submission:
    server = c.getvar("server");
    tls = (c.getvar("tls") == "yes");
    legacy_event_topic = (c.getvar("legacy_event_topic") == "yes");
    user = c.getvar("user");
    password = c.getvar("password");
    port = c.getvar("port");
    topic_prefix = c.getvar("topic_prefix");
    updatetime_connected = c.getvar("updatetime_connected");
    updatetime_idle = c.getvar("updatetime_idle");
    updatetime_on = c.getvar("updatetime_on");
    updatetime_charging = c.getvar("updatetime_charging");
    updatetime_awake = c.getvar("updatetime_awake");
    updatetime_sendall = c.getvar("updatetime_sendall");
    updatetime_keepalive = c.getvar("updatetime_keepalive");
    updatetime_priority = (c.getvar("updatetime_priority") == "yes");
    updatetime_immediately = (c.getvar("updatetime_immediately") == "yes");
    metrics_priority = c.getvar("metrics_priority");
    metrics_include = c.getvar("metrics_include");
    metrics_exclude = c.getvar("metrics_exclude");
    metrics_immediately = c.getvar("metrics_immediately");
    metrics_exclude_immediately = c.getvar("metrics_exclude_immediately");
    queue_sendall = c.getvar("queue_sendall");
    queue_modified = c.getvar("queue_modified");

    // validate:
    if (port != "") {
      if (port.find_first_not_of("0123456789") != std::string::npos
          || atoi(port.c_str()) < 0 || atoi(port.c_str()) > 65535) {
        error += "<li data-input=\"port\">Port must be an integer value in the range 0…65535</li>";
      }
    }
    if (updatetime_connected != "") {
      if (atoi(updatetime_connected.c_str()) < 1) {
        error += "<li data-input=\"updatetime_connected\">Update interval (connected) must be at least 1 second</li>";
      }
    }
    if (updatetime_idle != "") {
      if (atoi(updatetime_idle.c_str()) < 1) {
        error += "<li data-input=\"updatetime_idle\">Update interval (idle) must be at least 1 second</li>";
      }
    }
    if (updatetime_on != "") {
      if (atoi(updatetime_on.c_str()) < 1) {
        error += "<li data-input=\"updatetime_on\">Update interval (on) must be at least 1 second</li>";
      }
    }
    if (updatetime_charging != "") {
      if (atoi(updatetime_charging.c_str()) < 1) {
        error += "<li data-input=\"updatetime_charging\">Update interval (charging) must be at least 1 second</li>";
      }
    }
    if (updatetime_awake != "") {
      if (atoi(updatetime_awake.c_str()) < 1) {
        error += "<li data-input=\"updatetime_awake\">Update interval (awake) must be at least 1 second</li>";
      }
    }
    if (updatetime_sendall != "") {
      if (atoi(updatetime_sendall.c_str()) < 60) {
        error += "<li data-input=\"updatetime_sendall\">Update interval (sendall) must be at least 60 seconds</li>";
      }
    }
    if (updatetime_keepalive != "") {
      if (atoi(updatetime_keepalive.c_str()) < 60) {
        error += "<li data-input=\"updatetime_keepalive\">Keepalive interval must be at least 60 seconds</li>";
      }
    }

    if (error == "") {
      // success:
      MyConfig.SetParamValue("server.v3", "server", server);
      MyConfig.SetParamValueBool("server.v3", "tls", tls);
      MyConfig.SetParamValueBool("server.v3", "events.legacy_topic", legacy_event_topic);
      MyConfig.SetParamValue("server.v3", "user", user);
      if (password != "")
        MyConfig.SetParamValue("password", "server.v3", password);
      MyConfig.SetParamValue("server.v3", "port", port);
      MyConfig.SetParamValue("server.v3", "topic.prefix", topic_prefix);
      MyConfig.SetParamValue("server.v3", "updatetime.connected", updatetime_connected);
      MyConfig.SetParamValue("server.v3", "updatetime.idle", updatetime_idle);
      MyConfig.SetParamValue("server.v3", "updatetime.on", updatetime_on);
      MyConfig.SetParamValue("server.v3", "updatetime.charging", updatetime_charging);
      MyConfig.SetParamValue("server.v3", "updatetime.awake", updatetime_awake);
      MyConfig.SetParamValue("server.v3", "updatetime.sendall", updatetime_sendall);
      MyConfig.SetParamValue("server.v3", "updatetime.keepalive", updatetime_keepalive);
      MyConfig.SetParamValueBool("server.v3", "updatetime.priority", updatetime_priority);
      MyConfig.SetParamValueBool("server.v3", "updatetime.immediately", updatetime_immediately);
      MyConfig.SetParamValue("server.v3", "metrics.priority", metrics_priority);
      MyConfig.SetParamValue("server.v3", "metrics.include", metrics_include);
      MyConfig.SetParamValue("server.v3", "metrics.exclude", metrics_exclude);
      MyConfig.SetParamValue("server.v3", "metrics.include.immediately", metrics_immediately);
      MyConfig.SetParamValue("server.v3", "metrics.exclude.immediately", metrics_exclude_immediately);
      MyConfig.SetParamValue("server.v3", "queue.sendall", queue_sendall);
      MyConfig.SetParamValue("server.v3", "queue.modified", queue_modified);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Server V3 (MQTT) connection configured.</p>");
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
    server = MyConfig.GetParamValue("server.v3", "server");
    tls = MyConfig.GetParamValueBool("server.v3", "tls", false);
    legacy_event_topic = MyConfig.GetParamValueBool("server.v3", "events.legacy_topic", true);
    user = MyConfig.GetParamValue("server.v3", "user");
    password = MyConfig.GetParamValue("password", "server.v3");
    port = MyConfig.GetParamValue("server.v3", "port");
    topic_prefix = MyConfig.GetParamValue("server.v3", "topic.prefix");
    updatetime_connected = MyConfig.GetParamValue("server.v3", "updatetime.connected");
    updatetime_idle = MyConfig.GetParamValue("server.v3", "updatetime.idle");
    updatetime_on = MyConfig.GetParamValue("server.v3", "updatetime.on");
    updatetime_charging = MyConfig.GetParamValue("server.v3", "updatetime.charging");
    updatetime_awake = MyConfig.GetParamValue("server.v3", "updatetime.awake");
    updatetime_sendall = MyConfig.GetParamValue("server.v3", "updatetime.sendall");
    updatetime_keepalive = MyConfig.GetParamValue("server.v3", "updatetime.keepalive", "1740");
    updatetime_priority = MyConfig.GetParamValueBool("server.v3", "updatetime.priority", false);
    updatetime_immediately = MyConfig.GetParamValueBool("server.v3", "updatetime.immediately", false);
    metrics_priority = MyConfig.GetParamValue("server.v3", "metrics.priority");
    metrics_include = MyConfig.GetParamValue("server.v3", "metrics.include");
    metrics_exclude = MyConfig.GetParamValue("server.v3", "metrics.exclude");
    metrics_immediately = MyConfig.GetParamValue("server.v3", "metrics.include.immediately");
    metrics_exclude_immediately = MyConfig.GetParamValue("server.v3", "metrics.exclude.immediately");
    queue_sendall = MyConfig.GetParamValue("server.v3", "queue.sendall");
    queue_modified = MyConfig.GetParamValue("server.v3", "queue.modified");

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Server V3 (MQTT) configuration");
  c.form_start(p.uri);

  c.input_text("Server", "server", server.c_str(), "Enter host name or IP address",
    "<p>Public OVMS V3 servers (MQTT brokers):</p>"
    "<ul>"
      "<li><code>api.openvehicles.com</code> <a href=\"https://www.openvehicles.com/user/register\" target=\"_blank\">Registration</a></li>"
      "<li><code>ovms.dexters-web.de</code> <a href=\"https://dexters-web.de/?action=NewAccount\" target=\"_blank\">Registration</a></li>"
      "<li><a href=\"https://github.com/mqtt/mqtt.github.io/wiki/public_brokers\" target=\"_blank\">More public MQTT brokers</a></li>"
    "</ul>");
  c.input_checkbox("Enable TLS", "tls", tls,
    "<p>Note: enable transport layer security (encryption) if your server supports it.</p>");
  c.input_checkbox("Enable legacy event topic", "legacy_event_topic", legacy_event_topic,
    "In addition to MQTT-style topics, also publish on <i>&lt;prefix&gt;</i>/event.");
  c.input_text("Port", "port", port.c_str(), "optional, default: 1883 (no TLS) / 8883 (TLS)");
  c.input_text("Username", "user", user.c_str(), "Enter user login name",
    NULL, "autocomplete=\"section-serverv3 username\"");
  c.input_password("Password", "password", "", "Enter user password, empty = no change",
    NULL, "autocomplete=\"section-serverv3 current-password\"");
  c.input_text("Topic Prefix", "topic_prefix", topic_prefix.c_str(),
    "optional, default: ovms/<username>/<vehicle id>/");

  c.fieldset_start("Update intervals");
  c.input("number", "…idle", "updatetime_idle", updatetime_idle.c_str(), "default: 600", "default: 600, update interval when client not connected", "min=\"1\" max=\"1200\" step=\"1\"", "seconds");
  c.input("number", "…on", "updatetime_on", updatetime_on.c_str(), "default: 5", "default: 5, update interval when Car is on", "min=\"1\" max=\"600\" step=\"1\"", "seconds");
  c.input("number", "…charging", "updatetime_charging", updatetime_charging.c_str(), "default: 10", "default: 10, update interval when Car is charging", "min=\"1\" max=\"600\" step=\"1\"", "seconds");
  c.input("number", "…awake", "updatetime_awake", updatetime_awake.c_str(), "default: 60", "default: 60, update interval when Car is awake", "min=\"1\" max=\"600\" step=\"1\"", "seconds");
  c.input("number", "…connected", "updatetime_connected", updatetime_connected.c_str(), "default: 10", "default: 10, update interval when App/Client is connected", "min=\"1\" max=\"600\" step=\"1\"", "seconds");
  c.input("number", "…sendall", "updatetime_sendall", updatetime_sendall.c_str(), "default: 1200", "default: 1200", "min=\"60\" max=\"3600\" step=\"1\"", "seconds");
  c.input("number", "…keepalive", "updatetime_keepalive", updatetime_keepalive.c_str(), "default: 1740",
    "<p>default: 1740. Keepalive defines how often PINGREQs should be sent if there's inactivity. "
    "It should be set slightly shorter than the network's NAT timeout "
    "and the timeout of your MQTT server. If these are unknown you can use trial "
    "and error. Symptoms of keepalive being too high are a lack of metric updates "
    "after a certain point, or &quot;Disconnected from OVMS Server V3&quot; "
    "appearing in the log.</p>",
    "min=\"60\" max=\"3600\" step=\"1\"", "seconds");  
  c.fieldset_end();
  
  c.fieldset_start("Update Configuration");
  c.input_checkbox("prioritize metrics", "updatetime_priority", updatetime_priority,
    "<p>Metrics should be updated before other MQTT traffic when Car is awake. This can contribute to smoother tracking on V3 servers.</p>"
    "<p><strong>Note:</strong> The update interval corresponds to the <strong>...on</strong> setting!</p>");
  c.input_checkbox("Update metrics immediately", "updatetime_immediately", updatetime_immediately,
    "<p>Metrics should be sent immediately when they change.</p>"
    "<p><strong>Note:</strong> This setting significantly increases data transfer!</p>");
  c.input("number", "queue size sendall", "queue_sendall", queue_sendall.c_str(), "default: 100", "default: 100", "min=\"1\" max=\"500\" step=\"1\"", "size");
  c.input("number", "queue size modified", "queue_modified", queue_modified.c_str(), "default: 150", "default: 150", "min=\"1\" max=\"500\" step=\"1\"", "size");
  c.input_text("priority metrics", "metrics_priority", metrics_priority.c_str(), NULL,
    "<p>default priority: v.p.latitude, v.p.longitude, v.p.altitude, v.p.speed, v.p.gpsspeed, m.time.utc</br>"
    "additional comma-separated list of metrics to prioritize when Car is awake, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.input_text("metrics include", "metrics_include", metrics_include.c_str(), NULL,
    "<p>comma-separated list of metrics to include update, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.input_text("metrics exclude", "metrics_exclude", metrics_exclude.c_str(), NULL,
    "<p>comma-separated list of metrics to exclude update, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.input_text("metrics include immediate", "metrics_immediately", metrics_immediately.c_str(), "set which ones update immediately",
    "<p>comma-separated list of metrics to send immediately when they change, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.input_text("metrics exclude immediate", "metrics_exclude_immediately", metrics_exclude_immediately.c_str(), "set which ones not update immediately",
    "<p>comma-separated list of metrics to <strong>not</strong> send immediately when they change, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();

  c.panel_end();
  c.done();
}
#endif // CONFIG_OVMS_COMP_SERVER_V3
#endif // CONFIG_OVMS_COMP_SERVER
