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
#include "ovms_log.h"
static const char *TAG = "webserver";

#include "ovms_webserver.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

#ifdef CONFIG_OVMS_COMP_CELLULAR

/**
 * HandleCfgModem: configure APN & cellular modem features & GPS (URL /cfg/modem)
 */
void OvmsWebServer::HandleCfgModem(PageEntry_t& p, PageContext_t& c)
{
  std::string apn, apn_user, apn_pass, network_dns, pincode, error, gps_parkpause, gps_parkreactivate, gps_parkreactlock, vehicle_stream, model, modem_net_type, modem_net_types_avail;
  bool enable_gps, enable_gpstime, enable_net, enable_sms, wrongpincode, gps_parkreactawake;
  float cfg_sq_good, cfg_sq_bad;

  if (c.method == "POST") {
    // process form submission:
    apn = c.getvar("apn");
    apn_user = c.getvar("apn_user");
    apn_pass = c.getvar("apn_pass");
    pincode = c.getvar("pincode");
    network_dns = c.getvar("network_dns");
    enable_net = (c.getvar("enable_net") == "yes");
    enable_sms = (c.getvar("enable_sms") == "yes");
    enable_gps = (c.getvar("enable_gps") == "yes");
    enable_gpstime = (c.getvar("enable_gpstime") == "yes");
    gps_parkpause = c.getvar("gps_parkpause");
    gps_parkreactivate = c.getvar("gps_parkreactivate");
    gps_parkreactlock = c.getvar("gps_parkreactlock");    
    gps_parkreactawake = (c.getvar("gps_awake_start") == "yes");
    vehicle_stream = c.getvar("vehicle_stream");
    cfg_sq_good = atof(c.getvar("cfg_sq_good").c_str());
    cfg_sq_bad = atof(c.getvar("cfg_sq_bad").c_str());
    modem_net_type = c.getvar("modem_net_type");

    if (cfg_sq_bad >= cfg_sq_good)
      {
      error += "<li data-input=\"cfg_sq_bad\">'Bad' signal level must be lower than 'good' level.</li>";
      }
    else 
      {
      MyConfig.SetParamValue("modem", "apn", apn);
      MyConfig.SetParamValue("modem", "apn.user", apn_user);
      MyConfig.SetParamValue("modem", "apn.password", apn_pass);
      if ( MyConfig.GetParamValueBool("modem","wrongpincode") && (MyConfig.GetParamValue("modem","pincode") != pincode) )
        {
        ESP_LOGI(TAG,"New SIM card PIN code entered. Cleared wrong_pin_code flag");
        MyConfig.SetParamValueBool("modem", "wrongpincode", false);
        }
      MyConfig.SetParamValue("modem", "pincode", pincode);
      MyConfig.SetParamValue("network", "dns", network_dns);
      if (modem_net_type != "undef") MyConfig.SetParamValue("modem", "net.type", modem_net_type);
      MyConfig.SetParamValueBool("modem", "enable.net", enable_net);
      MyConfig.SetParamValueBool("modem", "enable.sms", enable_sms);
      MyConfig.SetParamValueBool("modem", "enable.gps", enable_gps);
      MyConfig.SetParamValueBool("modem", "enable.gpstime", enable_gpstime);
      MyConfig.SetParamValue("modem", "gps.parkpause", gps_parkpause);
      MyConfig.SetParamValue("modem", "gps.parkreactivate", gps_parkreactivate);
      MyConfig.SetParamValue("modem", "gps.parkreactlock", gps_parkreactlock);
      MyConfig.SetParamValueBool("modem", "gps.parkreactawake", gps_parkreactawake);
      if (vehicle_stream == "0")
        MyConfig.DeleteInstance("vehicle", "stream");
      else
        MyConfig.SetParamValue("vehicle", "stream", vehicle_stream);
      MyConfig.SetParamValueFloat("network", "modem.sq.good", cfg_sq_good);
      MyConfig.SetParamValueFloat("network", "modem.sq.bad", cfg_sq_bad);
    }

    if (error == "")
      {
      c.head(200);
      c.alert("success", "<p class=\"lead\">Modem configured.</p>");
      OutputHome(p, c);
      c.done();
      return;
      }
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } 
  else
  {
    c.head(200);
  }

  // read configuration:
  apn = MyConfig.GetParamValue("modem", "apn");
  apn_user = MyConfig.GetParamValue("modem", "apn.user");
  apn_pass = MyConfig.GetParamValue("modem", "apn.password");
  pincode = MyConfig.GetParamValue("modem", "pincode");

  modem* m_modem=MyPeripherals->m_cellular_modem;
  model = m_modem && m_modem->m_driver ? m_modem->m_model : "auto";
  modem_net_type = MyConfig.GetParamValue("modem", "net.type","auto");
  modem_net_types_avail = m_modem && m_modem->m_driver ? m_modem->m_driver->GetNetTypes() : "auto"; 

  wrongpincode = MyConfig.GetParamValueBool("modem", "wrongpincode",false);
  network_dns = MyConfig.GetParamValue("network", "dns");
  enable_net = MyConfig.GetParamValueBool("modem", "enable.net", true);
  enable_sms = MyConfig.GetParamValueBool("modem", "enable.sms", true);
  enable_gps = MyConfig.GetParamValueBool("modem", "enable.gps", false);
  enable_gpstime = MyConfig.GetParamValueBool("modem", "enable.gpstime", false);
  gps_parkpause = MyConfig.GetParamValue("modem", "gps.parkpause","0");
  gps_parkreactivate = MyConfig.GetParamValue("modem", "gps.parkreactivate","0");
  gps_parkreactlock = MyConfig.GetParamValue("modem", "gps.parkreactlock","5");
  gps_parkreactawake = MyConfig.GetParamValueBool("modem", "gps.parkreactawake", false);
  vehicle_stream = MyConfig.GetParamValue("vehicle", "stream","0");
  cfg_sq_good = MyConfig.GetParamValueFloat("network", "modem.sq.good", -93);
  cfg_sq_bad = MyConfig.GetParamValueFloat("network", "modem.sq.bad", -95);

  // generate form:
  c.panel_start("primary", "Cellular modem configuration");
  c.form_start(p.uri);

  std::string info;
  std::string iccid = StdMetrics.ms_m_net_mdm_iccid->AsString();
  if (!iccid.empty()) {
    info = "<code class=\"autoselect\">" + iccid + "</code>";
  } else {
    info =
      "<div class=\"receiver\">"
        "<code class=\"autoselect\" data-metric=\"m.net.mdm.iccid\">(power cellular modem on to read)</code>"
        "&nbsp;"
        "<button class=\"btn btn-default\" data-cmd=\"power cellular on\" data-target=\"#pso\">Power cellular modem on</button>"
        "&nbsp;"
        "<samp id=\"pso\" class=\"samp-inline\"></samp>"
      "</div>"
      "<script>"
      "$(\".receiver\").on(\"msg:metrics\", function(e, update){"
        "$(this).find(\"[data-metric]\").each(function(){"
          "if (metrics[$(this).data(\"metric\")] != \"\" && $(this).text() != metrics[$(this).data(\"metric\")])"
            "$(this).text(metrics[$(this).data(\"metric\")]);"
        "});"
      "}).trigger(\"msg:metrics\");"
      "</script>";
  }
  c.input_info("SIM ICCID", info.c_str());
  c.input_text("SIM card PIN code", "pincode", pincode.c_str(), "",
    wrongpincode ? "<p style=\"color: red\">Wrong PIN code entered previously!</p>" : "<p>Not needed for Hologram SIM cards</p>");

  c.fieldset_start("Internet");
  c.input_checkbox("Enable IP networking", "enable_net", enable_net);
  c.input_text("APN", "apn", apn.c_str(), NULL,
    "<p>For Hologram, use APN <code>hologram</code> with empty username &amp; password</p>");
  c.input_text("…username", "apn_user", apn_user.c_str());
  c.input_text("…password", "apn_pass", apn_pass.c_str());
  c.input_text("DNS", "network_dns", network_dns.c_str(), "optional fixed DNS servers (space separated)",
    "<p>Set this to i.e. <code>8.8.8.8 8.8.4.4</code> (Google public DNS) if you encounter problems with your network provider DNS</p>");

  if ( model != "auto")
  {
    c.input_radiobtn_start("Network type preference", "modem_net_type");
    if (modem_net_types_avail.find(modem_net_type) == string::npos) modem_net_type = "auto";
    c.input_radiobtn_option("modem_net_type", "auto", "auto", modem_net_type == "auto");
    if (modem_net_types_avail.find("2G") != string::npos) c.input_radiobtn_option("modem_net_type", "2G (GSM)", "2G", modem_net_type == "2G");
    if (modem_net_types_avail.find("3G") != string::npos) c.input_radiobtn_option("modem_net_type", "3G (UMTS)", "3G", modem_net_type == "3G");
    if (modem_net_types_avail.find("4G") != string::npos) c.input_radiobtn_option("modem_net_type", "4G (LTE)", "4G", modem_net_type == "4G");
    if (modem_net_types_avail.find("5G") != string::npos) c.input_radiobtn_option("modem_net_type", "5G", "5G", modem_net_type == "5G");
    c.input_radiobtn_end(
    "<p>Automatic mode should work in most cases. In case of frequent net losses, it can help to limit the network type.</p>"
    "<p>The 3G standard has been switched off in most areas around the world. 2G/GSM is often still available.</p>");
  }
  else{
    c.input_info("Network type preference", "Modem model not identified yet");
    modem_net_type="undef";
  }

    c.fieldset_end();

  c.fieldset_start("Features");
  c.input_checkbox("Enable SMS", "enable_sms", enable_sms);
  c.input_checkbox("Enable GPS", "enable_gps", enable_gps);
  c.input_checkbox("Use GPS time", "enable_gpstime", enable_gpstime);
  c.fieldset_end();

  c.fieldset_start("Cellular client options");
  c.input_slider("Good signal level", "cfg_sq_good", 3, "dBm", -1, cfg_sq_good, -93.0, -128.0, 0.0, 0.1,
    "<p>Threshold for usable cellular signal strength</p>");
  c.input_slider("Bad signal level", "cfg_sq_bad", 3, "dBm", -1, cfg_sq_bad, -95.0, -128.0, 0.0, 0.1,
    "<p>Threshold for unusable cellular signal strength</p>");
  c.fieldset_end();

  c.fieldset_start("GPS parking and tracking");
  c.input("number", "GPS park pause", "gps_parkpause", gps_parkpause.c_str(), "Default: 0 = disabled",
    "<p>Auto pause GPS when parking for longer than this time / 0 = no auto pausing</p>"
    "<p>Pausing the GPS subsystem can help to avoid draining the 12V battery, see"
    " <a target=\"_blank\" href=\"https://docs.openvehicles.com/en/latest/userguide/warnings.html#average-power-usage\">user manual</a>"
    " for details.</p>",
    "min=\"0\" step=\"5\"", "Seconds");
  c.input("number", "GPS park re-activate", "gps_parkreactivate", gps_parkreactivate.c_str(), "Default: 0 = disabled",
    "<p>Auto re-activate GPS after parking for longer than this time / 0 = no auto re-activation</p>",
    "min=\"0\" step=\"5\"", "Minutes");
  c.input("number", "GPS lock time", "gps_parkreactlock", gps_parkreactlock.c_str(), "Default: 5",
    "<p>by default, GPS lock for 5 minutes until automatic shutdown during parking time</p>",
    "min=\"5\" step=\"1\"", "Minutes");  
  c.input_checkbox("Start GPS when Car awakes", "gps_awake_start", gps_parkreactawake,
    "<p>GPS is switched on for the GPS lock time when the GPS parking pause is active and the car wakes up.</p>"
    "<p>This reduces time to first GPS fix, but increases power consumption when Car is awake.</p>");
  c.input("number", "Location streaming", "vehicle_stream", vehicle_stream.c_str(), "Default: 0",
    "<p>While driving send location updates to server every n seconds, 0 = use default update interval</p>"
    "<p>from server configuration. Same as App feature #8.</p>",
    "min=\"0\" step=\"1\"", "Seconds");
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
#endif
