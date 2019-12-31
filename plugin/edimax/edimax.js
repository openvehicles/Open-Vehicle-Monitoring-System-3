/**
 * Module plugin:
 *  Smart Plug control for Edimax models SP-1101W, SP-2101W et al
 *  Version 1.0 by Michael Balzer <dexter@dexters-web.de>
 *  Note: may need digest auth support to work with newer Edimax firmware (untested)
 * 
 * Installation:
 *  - Save as /store/scripts/lib/edimax.js
 *  - Add to /store/scripts/ovmsmain.js: edimax = require("lib/edimax");
 *  - Issue "script reload"
 *  - Install "edimax.htm" web plugin
 * 
 * Config: usr/edimax.*, fields see below
 * 
 * Usage:
 *  - script eval edimax.get()
 *  - script eval edimax.set("on" | "off")
 *  - script eval edimax.info()
 * 
 * Note: get() & set() do an async update, the result is logged.
 *  Use info() to show the current state.
 */

const tickerEvent = "ticker.60";

var cfg = {
  ip: "",                 // Edimax IP address
  user: "admin",          // … username, "admin" by default
  pass: "1234",           // … password, "1234" by default
  location: "",           // optional: restrict auto switch to this location
  soc_on: "",             // optional: switch on if SOC at/below
  soc_off: "",            // optional: switch off if SOC at/above
};

var state = {
  power: "",
  auto: false,
  ticker: false,
  error: "",
};

// Process call result:
function processResult(tag) {
  if (state.error) {
    print("Edimax " + tag + " ERROR: " + state.error);
    OvmsEvents.Raise("usr.edimax.error");
  } else {
    print("Edimax " + tag + ": power=" + state.power);
    OvmsEvents.Raise("usr.edimax." + state.power);
  }
}

// Get power state:
function getPowerState() {
  if (cfg.location != "" && !OvmsLocation.Status(cfg.location)) {
    print("Edimax: vehicle not at plug location (" + cfg.location + ")");
    return;
  }
  HTTP.request({
    url: "http://" + cfg.user + ":" + cfg.pass + "@" + cfg.ip + ":10000/smartplug.cgi",
    headers: [{ "Content-Type": "text/xml" }],
    post: '<?xml version="1.0" encoding="utf-8"?>'
      + '<SMARTPLUG id="edimax"><CMD id="get"><Device.System.Power.State>'
      + '</Device.System.Power.State></CMD></SMARTPLUG>',
    done: function(resp) {
      if (resp.statusCode == 200) {
        var m = resp.body.match('<Device.System.Power.State>([^<]*)');
        if (m && m.length == 2) {
          state.power = m[1];
        } else {
          state.error = "Unable to parse Edimax response";
        }
      } else {
        state.error = "Access error: " + resp.statusCode + " " + resp.statusText;
      }
      processResult("getPowerState");
    },
    fail: function(error) {
      state.error = "Network error: " + error;
      processResult("getPowerState");
    },
  });
}

// Set power state:
function setPowerState(onoff) {
  if (cfg.location != "" && !OvmsLocation.Status(cfg.location)) {
    print("Edimax: vehicle not at plug location (" + cfg.location + ")");
    return;
  }
  if (onoff != "on" && onoff != "off")
    onoff = onoff ? "on" : "off";
  HTTP.request({
    url: "http://" + cfg.user + ":" + cfg.pass + "@" + cfg.ip + ":10000/smartplug.cgi",
    headers: [{ "Content-Type": "text/xml" }],
    post: '<?xml version="1.0" encoding="utf-8"?>'
      + '<SMARTPLUG id="edimax"><CMD id="setup"><Device.System.Power.State>' + onoff
      + '</Device.System.Power.State></CMD></SMARTPLUG>',
    done: function(resp) {
      if (resp.statusCode == 200) {
        var m = resp.body.match('<CMD id="setup">([^<]*)');
        if (m && m.length == 2) {
          if (m[1] == "OK")
            state.power = onoff;
          else
            state.error = "Edimax command failed";
        } else {
          state.error = "Unable to parse Edimax response";
        }
      } else {
        state.error = "Access error: " + resp.statusCode + " " + resp.statusText;
      }
      processResult("setPowerState");
    },
    fail: function(error) {
      state.error = "Network error: " + error;
      processResult("setPowerState");
    },
  });
}

// Output info:
function printInfo() {
  JSON.print({ "cfg": cfg, "state": state });
}

// Read & process config:
function readConfig() {
  Object.assign(cfg, OvmsConfig.GetValues("usr", "edimax."));
  state.error = "";
  state.auto = (cfg.ip != "" && (cfg.soc_on != "" || cfg.soc_off != ""));
  if (state.auto && !state.ticker) {
    state.ticker = PubSub.subscribe(tickerEvent, ticker);
  } else if (!state.auto && state.ticker) {
    PubSub.unsubscribe(state.ticker);
    state.ticker = false;
  }
  if (cfg.ip && (cfg.location == "" || OvmsLocation.Status(cfg.location))) {
    getPowerState();
  }
}

// Ticker:
function ticker() {
  if (cfg.location != "" && !OvmsLocation.Status(cfg.location))
    return;
  var soc = OvmsMetrics.AsFloat("v.b.soc");
  if (cfg.soc_on != "" && soc <= Number(cfg.soc_on) && state.power != "on") {
    print("Edimax ticker: low SOC => switching on");
    setPowerState("on");
  }
  else if (cfg.soc_off != "" && soc >= Number(cfg.soc_off) && state.power != "off") {
    print("Edimax ticker: sufficient SOC => switching off");
    setPowerState("off");
  }
  else if (state.power == "") {
    getPowerState();
  }
}

// Init:
readConfig();
PubSub.subscribe("config.changed", readConfig);

// API exports:
exports.get = getPowerState;
exports.set = setPowerState;
exports.info = printInfo;
