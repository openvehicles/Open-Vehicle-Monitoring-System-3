/**
 * Module plugin:
 *  Tasmota Smart Plug control -- see https://tasmota.github.io/
 *  Version 2.1 by Michael Balzer <dexter@dexters-web.de>
 * 
 * History:
 *  - v2.1: energy data dialog in status plugin, suppress repeated on/off events, add getdata command
 *  - v2.0: power/energy monitoring
 *  - v1.0: initial release
 * 
 * Installation:
 *  - Save as /store/scripts/lib/tasmotasp.js
 *  - Add to /store/scripts/ovmsmain.js: tasmotasp = require("lib/tasmotasp");
 *  - Issue "script reload"
 *  - Install "tasmotasp.htm" web plugin
 * 
 * Config: usr/tasmotasp.*, fields see below
 * 
 * Usage:
 *  - script eval tasmotasp.get([true]) -- read power state, true = also read sensor data
 *  - script eval tasmotasp.set("on" | "off" | 1 | 0 | true | false)
 *  - script eval tasmotasp.getdata([true]) -- read sensor data, true = force
 *  - script eval tasmotasp.info() -- output config, state & data
 *  - script eval tasmotasp.status() -- only output state & data
 *  - script eval tasmotasp.sendcmd(command, [true]) -- send any Tasmota command, true = followed by sensor read
 * 
 * Notes: get(), set() & sendcmd() are asynchronous operations, the results are logged.
 *  Use info() or status() to show/fetch the current state.
 *  Sensor monitoring (if enabled) is done on request and while the power is switched on,
 *  with a single final update after switching the power off.
 *  See Tasmota documentation on command syntax.
 * 
 * Hint: to use a Tasmota plug for charging on the road, bind it to the module's Wifi access point.
 */

const tickerEvent = "ticker.60";

var cfg = {
  ip: "",                 // TasmotaSP HTTP API IP address
  user: "",               // … username for HTTP API
  pass: "",               // … password for HTTP API
  socket: "",             // … optional output id (1-8, 0=all, empty=default)
  location: "",           // optional: restrict auto switch to this location
  soc_on: "",             // optional: switch on if SOC at/below
  soc_off: "",            // optional: switch off if SOC at/above
  chg_stop_off: "",       // optional: yes = switch off on vehicle.charge.stop
  aux_volt_on: "",        // optional: switch on if 12V level at/below
  aux_volt_off: "",       // optional: switch off if 12V level at/above
  aux_stop_off: "",       // optional: yes = switch off on vehicle.charge.12v.stop
  power_mon: "",          // optional: yes = monitor plug power & energy sensors
};

var state = {
  power: "",
  auto: false,
  ticker: false,
  error: "",
  monitoring: false,
};

var oldstate = {
  power: "",
};

var data = {};

// Process call result:
function processResult(tag) {
  if (state.error) {
    print("TasmotaSP " + tag + " ERROR: " + state.error);
    OvmsEvents.Raise("usr.tasmotasp.error");
  } else {
    print("TasmotaSP " + tag + ": power=" + state.power);
    if (state.power != oldstate.power) {
      oldstate.power = state.power;
      OvmsEvents.Raise("usr.tasmotasp." + state.power);
    }
  }
}

// Create HTTP API URL:
function makeCommandURL(command) {
  var url = "http://" + cfg.ip
    + "/cm?username=" + encodeURIComponent(cfg.user)
    + "&password=" + encodeURIComponent(cfg.pass)
    + "&cmnd=" + encodeURIComponent(command);
  return url;
}

// Send Tasmota command:
function sendCommand(command, check_sensors) {
  if (cfg.location && !OvmsLocation.Status(cfg.location)) {
    print("TasmotaSP: vehicle not at plug location (" + cfg.location + ")");
    return;
  }
  print("TasmotaSP: executing command: " + command);
  HTTP.Request({
    url: makeCommandURL(command),
    done: function(resp) {
      if (resp.statusCode == 200) {
        print("TasmotaSP: command response: " + resp.body);
        // also read sensor state?
        if (check_sensors && cfg.power_mon == "yes") {
          getSensorData(true);
        }
      } else {
        state.error = "Access error: " + resp.statusCode + " " + resp.statusText;
      }
    },
  });
}

// Get power state:
function getPowerState(check_sensors) {
  if (cfg.location && !OvmsLocation.Status(cfg.location)) {
    print("TasmotaSP: vehicle not at plug location (" + cfg.location + ")");
    return;
  }
  HTTP.Request({
    url: makeCommandURL("Power" + cfg.socket),
    done: function(resp) {
      if (resp.statusCode == 200) {
        var ts = null;
        try { ts = JSON.parse(resp.body); } catch (e) {}
        if (ts && ts["POWER"] != undefined) {
          // success:
          state.error = "";
          state.power = ts["POWER"].toLowerCase();
          // also read sensor state?
          if (check_sensors && cfg.power_mon == "yes") {
            getSensorData(true);
          }
        } else {
          state.error = "Unable to parse TasmotaSP response";
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
  if (cfg.location && !OvmsLocation.Status(cfg.location)) {
    print("TasmotaSP: vehicle not at plug location (" + cfg.location + ")");
    return;
  }
  if (onoff != "on" && onoff != "off")
    onoff = onoff ? "on" : "off";
  HTTP.Request({
    url: makeCommandURL("Power" + cfg.socket + " " + onoff),
    done: function(resp) {
      if (resp.statusCode == 200) {
        var ts = null;
        try { ts = JSON.parse(resp.body); } catch (e) {}
        if (ts && ts["POWER"] != undefined) {
          // success:
          state.error = "";
          state.power = ts["POWER"].toLowerCase();
          // schedule sensor update:
          if (cfg.power_mon == "yes") {
            OvmsEvents.Raise("usr.tasmotasp.getdata", (state.power == "on") ? 15000 : 2000);
          }
        } else {
          state.error = "Unable to parse TasmotaSP response";
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

// Get sensor data:
function getSensorData(force) {
  if (cfg.location && !OvmsLocation.Status(cfg.location)) {
    return;
  }
  if (force || state.power == "on" || state.monitoring || data["StatusSNS"] == undefined) {
    HTTP.Request({
      url: makeCommandURL("Status 8"),
      done: function(resp) {
        if (resp.statusCode == 200) {
          try {
            var ts = JSON.parse(resp.body);
            data = Object.assign(data, ts);
            // send update to web UI:
            OvmsNotify.Raise("stream", "usr.tasmotasp.data", Duktape.enc('jc', data));
          } catch (e) {
            print("TasmotaSP getSensorData ERROR: " + e);
          }
        }
        // follow power state changes:
        state.monitoring = (state.power == "on");
      },
    });
  }
}

// Output info:
function printInfo() {
  JSON.print({ "cfg": cfg, "state": state, "data": data });
}

// Output status:
function printStatus() {
  JSON.print({ "state": state, "data": data });
}

// Read & process config:
function readConfig() {
  // check for config update:
  var upd = Object.assign({}, cfg, OvmsConfig.GetValues("usr", "tasmotasp."));
  if (Duktape.enc('jx', upd) == Duktape.enc('jx', cfg))
    return;
  cfg = upd;
  // process:
  state.error = "";
  state.auto = (cfg.ip && (cfg.soc_on || cfg.soc_off || cfg.aux_volt_on || cfg.aux_volt_off || cfg.power_mon == "yes")) != "";
  if (state.auto && !state.ticker) {
    state.ticker = PubSub.subscribe(tickerEvent, ticker);
  } else if (!state.auto && state.ticker) {
    PubSub.unsubscribe(state.ticker);
    state.ticker = false;
  }
  if (cfg.ip && (!cfg.location || OvmsLocation.Status(cfg.location))) {
    getPowerState(true);
  }
}

// Ticker:
function ticker() {
  if (cfg.location && !OvmsLocation.Status(cfg.location))
    return;
  var soc = OvmsMetrics.AsFloat("v.b.soc");
  var aux_volt = OvmsMetrics.AsFloat("v.b.12v.voltage");
  // check SOC:
  if (cfg.soc_on && soc <= Number(cfg.soc_on) && state.power != "on") {
    print("TasmotaSP ticker: low SOC => switching on");
    setPowerState("on");
  }
  else if (cfg.soc_off && soc >= Number(cfg.soc_off) && state.power != "off") {
    print("TasmotaSP ticker: sufficient SOC => switching off");
    setPowerState("off");
  }
  // check 12V level:
  else if (cfg.aux_volt_on && aux_volt <= Number(cfg.aux_volt_on) && state.power != "on") {
    print("TasmotaSP ticker: low 12V level => switching on");
    setPowerState("on");
  }
  else if (cfg.aux_volt_off && aux_volt >= Number(cfg.aux_volt_off) && state.power != "off") {
    print("TasmotaSP ticker: sufficient 12V level => switching off");
    setPowerState("off");
  }
  // get power state if unknown:
  else if (state.power == "") {
    getPowerState();
  }
  // get sensor data:
  else if (cfg.power_mon == "yes") {
    getSensorData();
  }
}

// Event handler:
function handleEvent(event) {
  if (cfg.ip == "" || (cfg.location && !OvmsLocation.Status(cfg.location)))
    return;
  if (event == "network.wifi.up") {
    getPowerState(true);
  }
  else if (event == "usr.tasmotasp.getdata") {
    getSensorData(true);
  }
  else if (event == "vehicle.charge.stop") {
    if (cfg.chg_stop_off == "yes" && state.power == "on") {
      print("TasmotaSP event: main charge stop => switching off");
      setPowerState("off");
    }
  }
  else if (event == "vehicle.charge.12v.stop") {
    if (cfg.aux_stop_off == "yes" && state.power == "on") {
      print("TasmotaSP event: 12V charge stop => switching off");
      setPowerState("off");
    }
  }
}

// Init:
readConfig();
PubSub.subscribe("config.changed", readConfig);
PubSub.subscribe("network.wifi.up", handleEvent);
PubSub.subscribe("vehicle.charge.stop", handleEvent);
PubSub.subscribe("vehicle.charge.12v.stop", handleEvent);
PubSub.subscribe("usr.tasmotasp.getdata", handleEvent);

// API exports:
exports.get = getPowerState;
exports.set = setPowerState;
exports.getdata = getSensorData;
exports.info = printInfo;
exports.status = printStatus;
exports.sendcmd = sendCommand;
