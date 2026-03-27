/*
;    Project:       Open Vehicle Monitor System
;    Date:          December 2025
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

#ifdef CONFIG_OVMS_COMP_SERVER
#ifdef CONFIG_OVMS_COMP_SERVER_V3
/**
 * HandleCfgServerV3: configure server v3 connection (URL /cfg/server/v3)
 */
void OvmsWebServer::HandleCfgServerV3(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
  std::string error;
  std::string server, user, password, port, topic_prefix;
  std::string client_cert, client_key;
  std::string updatetime_connected, updatetime_idle, updatetime_on;
  std::string updatetime_charging, updatetime_awake, updatetime_sendall, updatetime_keepalive;
  std::string metrics_priority, metrics_include, metrics_exclude, metrics_immediately, metrics_exclude_immediately;
  std::string queue_sendall, queue_modified;
  bool tls, legacy_event_topic, updatetime_priority, updatetime_immediately, retain_depth_limit, keepalive_clamp;

  if (c.method == "POST") {
    // process form submission:
    extram::string client_cert_raw, client_key_raw;
    server = c.getvar("server");
    tls = (c.getvar("tls") == "yes");
    legacy_event_topic = (c.getvar("legacy_event_topic") == "yes");
    user = c.getvar("user");
    password = c.getvar("password");
    c.getvar("client_cert", client_cert_raw);
    c.getvar("client_key", client_key_raw);
    client_cert.assign(client_cert_raw.data(), client_cert_raw.size());
    client_key.assign(client_key_raw.data(), client_key_raw.size());
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
    retain_depth_limit = (c.getvar("retain_depth_limit") == "yes");
    keepalive_clamp = (c.getvar("keepalive_clamp") == "yes");
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
    if (!client_cert.empty() && !startsWith(client_cert, "-----BEGIN CERTIFICATE-----")) {
      error += "<li data-input=\"client_cert\">Client certificate must be in PEM CERTIFICATE format</li>";
    }
    if (!client_key.empty() &&
        !startsWith(client_key, "-----BEGIN PRIVATE KEY-----") &&
        !startsWith(client_key, "-----BEGIN RSA PRIVATE KEY-----") &&
        !startsWith(client_key, "-----BEGIN EC PRIVATE KEY-----")) {
      error += "<li data-input=\"client_key\">Client private key must be in PEM PRIVATE KEY format</li>";
    }
    if (client_cert.empty() != client_key.empty()) {
      error += "<li data-input=\"client_cert,client_key\">Both client certificate and private key must be given</li>";
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
      MyConfig.SetParamValueBool("server.v3", "retain.depth.limit", retain_depth_limit);
      MyConfig.SetParamValueBool("server.v3", "keepalive.clamp", keepalive_clamp);
      MyConfig.SetParamValue("server.v3", "metrics.priority", metrics_priority);
      MyConfig.SetParamValue("server.v3", "metrics.include", metrics_include);
      MyConfig.SetParamValue("server.v3", "metrics.exclude", metrics_exclude);
      MyConfig.SetParamValue("server.v3", "metrics.include.immediately", metrics_immediately);
      MyConfig.SetParamValue("server.v3", "metrics.exclude.immediately", metrics_exclude_immediately);
      MyConfig.SetParamValue("server.v3", "queue.sendall", queue_sendall);
      MyConfig.SetParamValue("server.v3", "queue.modified", queue_modified);
      if (client_cert.empty()) {
        MyConfig.DeleteInstance("server.v3", "client.cert");
        MyConfig.DeleteInstance("server.v3", "client.key");
      }
      else {
        MyConfig.SetParamValue("server.v3", "client.cert", client_cert);
        MyConfig.SetParamValue("server.v3", "client.key", client_key);
      }

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
    client_cert = MyConfig.GetParamValue("server.v3", "client.cert");
    client_key = MyConfig.GetParamValue("server.v3", "client.key");
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
    retain_depth_limit = MyConfig.GetParamValueBool("server.v3", "retain.depth.limit", true);
    keepalive_clamp = MyConfig.GetParamValueBool("server.v3", "keepalive.clamp", true);
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
  c.input_checkbox("Limit retain to 8-segment topics", "retain_depth_limit", retain_depth_limit,
    "<p>When enabled, metrics on topics deeper than 8 path segments are published without the MQTT RETAIN flag."
    " <strong>Required for AWS IoT Core</strong>, which hard-limits retained messages to topics with at most"
    " 8 segments (e.g. <code>ovms-user-VIN/metric/v/p/latitude</code>: 5 segments &mdash; fine;"
    " <code>ovms/user/VIN/metric/v/p/latitude</code>: 7 segments &mdash; fine;"
    " topics with 9+ segments would be silently dropped as retained on AWS)."
    " Disable this only if your MQTT broker supports retained publishes on arbitrarily deep topics."
    " Default: enabled.</p>");
  c.input_checkbox("Clamp keepalive to 1200s", "keepalive_clamp", keepalive_clamp,
    "<p>When enabled, the MQTT keepalive is clamped to a maximum of 1200 seconds at connect time."
    " <strong>Required for AWS IoT Core</strong>, which rejects keepalive values above 1200 s with a disconnect."
    " Disable this only if your MQTT broker accepts keepalive values above 1200 s"
    " (e.g. a self-hosted broker with the default 1740 s setting)."
    " Default: enabled.</p>");
  c.input_text("Port", "port", port.c_str(), "optional, default: 1883 (no TLS) / 8883 (TLS)");
  c.input_text("Username", "user", user.c_str(), "Enter user login name",
    NULL, "autocomplete=\"section-serverv3 username\"");
  c.input_password("Password", "password", "", "Enter user password, empty = no change",
    NULL, "autocomplete=\"section-serverv3 current-password\"");

  c.fieldset_start("TLS client authentication (optional)");
  c.printf(
    "<div class=\"form-group\">\n"
      "<label class=\"control-label col-sm-3\" for=\"input-content\">Client certificate:</label>\n"
      "<div class=\"col-sm-9\">\n"
        "<textarea class=\"form-control font-monospace\" style=\"font-size:80%%;white-space:pre;\"\n"
          "autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"off\" spellcheck=\"false\"\n"
          "placeholder=\"-----BEGIN CERTIFICATE-----&#13;&#10;...&#13;&#10;-----END CERTIFICATE-----\"\n"
          "rows=\"5\" id=\"input-client_cert\" name=\"client_cert\">%s</textarea>\n"
        "<p class=\"help-block\">Leave both fields empty to use username/password only.</p>\n"
        "<p class=\"help-block\">Current length: <span id=\"client-cert-len\">0</span> bytes"
        " <span id=\"client-cert-short\" class=\"text-warning\" style=\"display:none;\">(looks short)</span></p>\n"
      "</div>\n"
    "</div>\n"
    , c.encode_html(client_cert).c_str());
  c.printf(
    "<div class=\"form-group\">\n"
      "<label class=\"control-label col-sm-3\" for=\"input-content\">Client private key:</label>\n"
      "<div class=\"col-sm-9\">\n"
        "<textarea class=\"form-control font-monospace\" style=\"font-size:80%%;white-space:pre;\"\n"
          "autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"off\" spellcheck=\"false\"\n"
          "placeholder=\"-----BEGIN PRIVATE KEY-----&#13;&#10;...&#13;&#10;-----END PRIVATE KEY-----\"\n"
          "rows=\"5\" id=\"input-client_key\" name=\"client_key\">%s</textarea>\n"
        "<p class=\"help-block\">Supports PKCS#8, RSA and EC PEM private keys.</p>\n"
        "<p class=\"help-block\">Current length: <span id=\"client-key-len\">0</span> bytes"
        " <span id=\"client-key-short\" class=\"text-warning\" style=\"display:none;\">(looks short)</span></p>\n"
      "</div>\n"
    "</div>\n"
    , c.encode_html(client_key).c_str());
  c.fieldset_end();

  c.printf(
    "<script>\n"
    "(function() {\n"
    "  function byId(id) { return document.getElementById(id); }\n"
    "  function byteLen(s) {\n"
    "    if (window.TextEncoder) return (new TextEncoder().encode(s)).length;\n"
    "    return unescape(encodeURIComponent(s)).length;\n"
    "  }\n"
    "  function showLen(inputId, lenId, warnId, minWarn) {\n"
    "    var input = byId(inputId), lenEl = byId(lenId), warnEl = byId(warnId);\n"
    "    if (!input || !lenEl || !warnEl) return;\n"
    "    var n = byteLen(input.value || '');\n"
    "    lenEl.textContent = String(n);\n"
    "    warnEl.style.display = (n > 0 && n < minWarn) ? 'inline' : 'none';\n"
    "  }\n"
    "  function update() {\n"
    "    showLen('input-client_cert', 'client-cert-len', 'client-cert-short', 256);\n"
    "    showLen('input-client_key', 'client-key-len', 'client-key-short', 128);\n"
    "  }\n"
    "  var cert = byId('input-client_cert');\n"
    "  var key = byId('input-client_key');\n"
    "  if (cert) cert.addEventListener('input', update);\n"
    "  if (key) key.addEventListener('input', update);\n"
    "  update();\n"
    "})();\n"
    "</script>\n");

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
