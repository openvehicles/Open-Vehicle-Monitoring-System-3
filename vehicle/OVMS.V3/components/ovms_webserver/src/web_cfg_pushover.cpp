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

#ifdef CONFIG_OVMS_COMP_PUSHOVER
#include "pushover.h"

/**
 * HandleCfgPushover: Configure pushover notifications (URL /cfg/pushover)
 */
void OvmsWebServer::HandleCfgPushover(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
  std::string error;
  OvmsConfigParam* param = MyConfig.CachedParam("pushover");
  ConfigParamMap pmap;
  int i, max;
  char buf[100];
  std::string name, msg, pri;

  if (c.method == "POST") {
    // process form submission:
    pmap["enable"] = (c.getvar("enable") == "yes") ? "yes" : "no";
    pmap["user_key"] = c.getvar("user_key");
    pmap["token"] = c.getvar("token");

    // validate:
    //if (server.length() == 0)
    //  error += "<li data-input=\"server\">Server must not be empty</li>";
    if (pmap["enable"]=="yes")
      {
      if (pmap["user_key"].length() == 0)
        error += "<li data-input=\"user_key\">User key must not be empty</li>";
      if (pmap["user_key"].find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">User key may only contain ASCII letters and digits</li>";
      if (pmap["token"].length() == 0)
        error += "<li data-input=\"token\">Token must not be empty</li>";
      if (pmap["token"].find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Token may only contain ASCII letters and digits</li>";
      }

    pmap["sound.normal"] = c.getvar("sound.normal");
    pmap["sound.high"] = c.getvar("sound.high");
    pmap["sound.emergency"] = c.getvar("sound.emergency");
    pmap["expire"] = c.getvar("expire");
    pmap["retry"] = c.getvar("retry");

    // read notification type/subtypes and their priorities
    max = atoi(c.getvar("npmax").c_str());
    for (i = 1; i <= max; i++) {
      sprintf(buf, "nfy_%d", i);
      name = c.getvar(buf);
      if (name == "") continue;
      sprintf(buf, "np_%d", i);
      pri = c.getvar(buf);
      if (pri == "") continue;
      snprintf(buf, sizeof(buf), "np.%s", name.c_str());
      pmap[buf] = pri;
    }

    // read events, their messages and priorities
    max = atoi(c.getvar("epmax").c_str());
    for (i = 1; i <= max; i++) {
      sprintf(buf, "en_%d", i);
      name = c.getvar(buf);
      if (name == "") continue;
      sprintf(buf, "em_%d", i);
      msg = c.getvar(buf);
      sprintf(buf, "ep_%d", i);
      pri = c.getvar(buf);
      if (pri == "") continue;
      snprintf(buf, sizeof(buf), "ep.%s", name.c_str());
      pri.append("/");
      pri.append(msg);
      pmap[buf] = pri;
    }

    if (error == "") {
      if (c.getvar("action") == "save") {
        // save:
        param->SetMap(pmap);

        c.head(200);
        c.alert("success", "<p class=\"lead\">Pushover connection configured.</p>");
        OutputHome(p, c);
        c.done();
        return;
      } else if (c.getvar("action") == "test")
      {
        std::string reply;
        std::string popup;
        c.head(200);
        c.alert("success", "<p class=\"lead\">Sending message</p>");
        if (!MyPushoverClient.SendMessageOpt(
            c.getvar("user_key"),
            c.getvar("token"),
            c.getvar("test_message"),
            atoi(c.getvar("test_priority").c_str()),
            c.getvar("test_sound"),
            atoi(c.getvar("retry").c_str()),
            atoi(c.getvar("expire").c_str()),
            true /* receive server reply as reply/pushover-type notification */ ))
        {
          c.alert("danger", "<p class=\"lead\">Could not send test message!</p>");
        }
      }
    }
    else {
      // output error, return to form:
      error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
      c.head(400);
      c.alert("danger", error.c_str());
    }

  }
  else {
    // read configuration:
    pmap = param->GetMap();

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Pushover server configuration");
  c.form_start(p.uri);

  c.printf("<div><p>Please visit <a href=\"https://pushover.net\">Pushover web site</a> to create an account (identified by a <b>user key</b>) "
    " and then register OVMS as an application in order to receive an application <b>token</b>.<br>"
    "Install Pushover iOS/Android application and specify your user key. <br>Finally enter both the user key and the application token here and test connectivity.<br>"
    "To receive specific notifications and events, configure them below.</p></div>" );

  c.input_checkbox("Enable Pushover connectivity", "enable", pmap["enable"] == "yes");
  c.input_text("User key", "user_key", pmap["user_key"].c_str(), "Enter user key (alphanumerical key consisting of around 30 characters");
  c.input_text("Token", "token", pmap["token"].c_str(), "Enter token (alphanumerical key consisting of around 30 characters");

  auto gen_options_priority = [&c](std::string priority) {
    c.printf(
        "<option value=\"-2\" %s>Lowest</option>"
        "<option value=\"-1\" %s>Low</option>"
        "<option value=\"0\" %s>Normal</option>"
        "<option value=\"1\" %s>High</option>"
        "<option value=\"2\" %s>Emergency</option>"
      , (priority=="-2") ? "selected" : ""
      , (priority=="-1") ? "selected" : ""
      , (priority=="0"||priority=="") ? "selected" : ""
      , (priority=="1") ? "selected" : ""
      , (priority=="2") ? "selected" : "");
  };

  auto gen_options_sound = [&c](std::string sound) {
    c.printf(
        "<option value=\"pushover\" %s>Pushover (default)</option>"
        "<option value=\"bike\" %s>Bike</option>"
        "<option value=\"bugle\" %s>Bugle</option>"
        "<option value=\"cashregister\" %s>Cashregister</option>"
        "<option value=\"classical\" %s>Classical</option>"
        "<option value=\"cosmic\" %s>Cosmic</option>"
        "<option value=\"falling\" %s>Falling</option>"
        "<option value=\"gamelan\" %s>Gamelan</option>"
        "<option value=\"incoming\" %s>Incoming</option>"
        "<option value=\"intermission\" %s>Intermission</option>"
        "<option value=\"magic\" %s>Magic</option>"
        "<option value=\"mechanical\" %s>Mechanical</option>"
        "<option value=\"pianobar\" %s>Piano bar</option>"
        "<option value=\"siren\" %s>Siren</option>"
        "<option value=\"spacealarm\" %s>Space alarm</option>"
        "<option value=\"tugboat\" %s>Tug boat</option>"
        "<option value=\"alien\" %s>Alien alarm (long)</option>"
        "<option value=\"climb\" %s>Climb (long)</option>"
        "<option value=\"persistent\" %s>Persistent (long)</option>"
        "<option value=\"echo\" %s>Pushover Echo (long)</option>"
        "<option value=\"updown\" %s>Up Down (long)</option>"
        "<option value=\"none\" %s>None (silent)</option>"
      , (sound=="pushover") || (sound=="") ? "selected" : ""
      , (sound=="bike") ? "selected" : ""
      , (sound=="bugle") ? "selected" : ""
      , (sound=="cashregister") ? "selected" : ""
      , (sound=="classical") ? "selected" : ""
      , (sound=="cosmic") ? "selected" : ""
      , (sound=="falling") ? "selected" : ""
      , (sound=="gamelan") ? "selected" : ""
      , (sound=="incoming") ? "selected" : ""
      , (sound=="intermission") ? "selected" : ""
      , (sound=="magic") ? "selected" : ""
      , (sound=="mechanical") ? "selected" : ""
      , (sound=="pianobar") ? "selected" : ""
      , (sound=="siren") ? "selected" : ""
      , (sound=="spacealarm") ? "selected" : ""
      , (sound=="tugboat") ? "selected" : ""
      , (sound=="alien") ? "selected" : ""
      , (sound=="climb") ? "selected" : ""
      , (sound=="persistent") ? "selected" : ""
      , (sound=="echo") ? "selected" : ""
      , (sound=="updown") ? "selected" : ""
      , (sound=="none") ? "selected" : "");
  };

  c.input_select_start("Normal priority sound", "sound.normal");
  gen_options_sound(pmap["sound.normal"]);
  c.input_select_end();
  c.input_select_start("High priority sound", "sound.high");
  gen_options_sound(pmap["sound.high"]);
  c.input_select_end();
  c.input_select_start("Emergency priority sound", "sound.emergency");
  gen_options_sound(pmap["sound.emergency"]);
  c.input_select_end();

  c.input("number", "Retry", "retry", pmap["retry"].c_str(), "Default: 30",
    "<p>Time period after which new notification is sent if emergency priority message is not acknowledged.</p>",
    "min=\"30\" step=\"1\"", "secs");
  c.input("number", "Expiration", "expire", pmap["expire"].c_str(), "Default: 1800",
    "<p>Time period after an emergency priority message will expire (and will not cause a new notification) if the message is not acknowledged.</p>",
    "min=\"0\" step=\"1\"", "secs");

  // Test message area
  c.print(
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Test connection</label>"
    "<div class=\"col-sm-9\">"
      "<div class=\"table-responsive\">"
        "<table class=\"table\">"
          "<tbody>"
            "<tr>"
              "<td width=\"40%\">");

  c.input_text("Message", "test_message", c.getvar("test_message").c_str(), "Enter test message");
  c.print("</td><td width=\"25%\">");
  c.input_select_start("Priority", "test_priority");
  gen_options_priority(c.getvar("test_priority") != "" ? c.getvar("test_priority") : "0");
  c.input_select_end();
  c.print("</td><td width=\"25%\">");

  c.input_select_start("Sound", "test_sound");
  gen_options_sound( c.getvar("test_sound") != "" ? c.getvar("test_sound").c_str() : pmap["sound.normal"]);
  c.input_select_end();
  c.print("</td><td width=\"10%\">");
  c.input_button("default", "Send", "action", "test");
  c.printf(
          "</td></tr>"
        "</tbody>"
      "</table>"
    "</div>"
    "</div>"
    "</div>");


  // Input area for Notifications
  c.print(
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Notification filtering</label>"
    "<div class=\"col-sm-9\">"
      "<div class=\"table-responsive\">"
        "<table class=\"table\">"
          "<thead>"
            "<tr>"
              "<th width=\"10%\"></th>"
              "<th width=\"45%\">Type/Subtype</th>"
              "<th width=\"45%\">Priority</th>"
            "</tr>"
          "</thead>"
          "<tbody>");


  max = 0;
  for (auto &kv: pmap) {
    if (!startsWith(kv.first, "np."))
      continue;
    max++;
    name = kv.first.substr(3);
    c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"nfy_%d\" value=\"%s\" placeholder=\"Enter notification type/subtype\""
              " autocomplete=\"section-notification-type\"></td>"
            "<td width=\"20%%\"><select class=\"form-control\" name=\"np_%d\" size=\"1\">"
      , max, _attr(name)
      , max);
    gen_options_priority(kv.second);
    c.print(
            "</select></td>"
          "</tr>");
  }

  c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-success\" onclick=\"addRow_nfy(this)\"><strong>✚</strong></button></td>"
            "<td></td>"
            "<td></td>"
          "</tr>"
        "</tbody>"
      "</table>"
      "<input type=\"hidden\" name=\"npmax\" value=\"%d\">"
    "</div>"
    "<p>Enter the type of notification (for example <i>\"alert\"</i> or <i>\"info\"</i>) or more specifically the type/subtype tuple (for example <i>\"alert/alarm.sounding\"</i>). "
    " If a notification matches multiple filters, only the more specific will be used. "
    "For more complete listing, see <a href=\"https://docs.openvehicles.com/en/latest/userguide/notifications.html\">OVMS User Guide</a></p>"
    "</div>"
    "</div>"
    , max);


  // Input area for Events
  c.print(
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Event filtering</label>"
    "<div class=\"col-sm-9\">"
      "<div class=\"table-responsive\">"
        "<table class=\"table\">"
          "<thead>"
            "<tr>"
              "<th width=\"10%\"></th>"
              "<th width=\"20%\">Event</th>"
              "<th width=\"55%\">Message</th>"
              "<th width=\"15%\">Priority</th>"
            "</tr>"
          "</thead>"
          "<tbody>");


  max = 0;
  for (auto &kv: pmap) {
    if (!startsWith(kv.first, "ep."))
      continue;
    max++;
    // Priority and message is saved as "priority/message" tuple (eg. "-1/this is a message")
    name = kv.first.substr(3);
    if (kv.second[1]=='/') {
      pri = kv.second.substr(0,1);
      msg = kv.second.substr(2);
    } else
    if (kv.second[2]=='/') {
      pri = kv.second.substr(0,2);
      msg = kv.second.substr(3);
    } else continue;
    c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"en_%d\" value=\"%s\" placeholder=\"Enter event name\""
              " autocomplete=\"section-event-name\"></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"em_%d\" value=\"%s\" placeholder=\"Enter message\""
              " autocomplete=\"section-event-message\"></td>"
            "<td><select class=\"form-control\" name=\"ep_%d\" size=\"1\">"
      , max, _attr(name)
      , max, _attr(msg)
      , max);
    gen_options_priority(pri);
    c.print(
            "</select></td>"
          "</tr>");
  }

  c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-success\" onclick=\"addRow_ev(this)\"><strong>✚</strong></button></td>"
            "<td></td>"
            "<td></td>"
          "</tr>"
        "</tbody>"
      "</table>"
      "<input type=\"hidden\" name=\"epmax\" value=\"%d\">"
    "</div>"
    "<p>Enter the event name (for example <i>\"vehicle.locked\"</i> or <i>\"vehicle.alert.12v.on\"</i>). "
    "For more complete listing, see <a href=\"https://docs.openvehicles.com/en/latest/userguide/events.html\">OVMS User Guide</a></p>"
    "</div>"
    "</div>"
    , max);


  c.input_button("default", "Save","action","save");
  c.form_end();

  c.print(
    "<script>"
    "function delRow(el){"
      "$(el).parent().parent().remove();"
    "}"
    "function addRow_nfy(el){"
      "var counter = $('input[name=npmax]');"
      "var nr = Number(counter.val()) + 1;"
      "var row = $('"
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"nfy_' + nr + '\" placeholder=\"Enter type/subtype\""
              " autocomplete=\"section-notification-type\"></td>"
            "<td><select class=\"form-control\" name=\"np_' + nr + '\" size=\"1\">"
              "<option value=\"-2\">Lowest</option>"
              "<option value=\"-1\">Low</option>"
              "<option value=\"0\" selected>Normal</option>"
              "<option value=\"1\">High</option>"
              "<option value=\"2\">Emergency</option>"
            "</select></td>"
          "</tr>"
        "');"
      "$(el).parent().parent().before(row).prev().find(\"input\").first().focus();"
      "counter.val(nr);"
    "}"
    "function addRow_ev(el){"
      "var counter = $('input[name=epmax]');"
      "var nr = Number(counter.val()) + 1;"
      "var row = $('"
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"en_' + nr + '\" placeholder=\"Enter event name\""
              " autocomplete=\"section-event-name\"></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"em_' + nr + '\" placeholder=\"Enter message\""
              " autocomplete=\"section-event-message\"></td>"
            "<td><select class=\"form-control\" name=\"ep_' + nr + '\" size=\"1\">"
              "<option value=\"-2\">Lowest</option>"
              "<option value=\"-1\">Low</option>"
              "<option value=\"0\" selected>Normal</option>"
              "<option value=\"1\">High</option>"
              "<option value=\"2\">Emergency</option>"
            "</select></td>"
          "</tr>"
        "');"
      "$(el).parent().parent().before(row).prev().find(\"input\").first().focus();"
      "counter.val(nr);"
    "}"
    "</script>");


  c.panel_end();
  c.done();
}
#endif
