/**
 * /store/scripts/lib/chgthrottle.js
 * 
 * Module plugin:
 *  Throttle charge current and/or stop charge if charger gets too hot.
 * 
 * Version 1.1   Michael Balzer <dexter@dexters-web.de>
 *  - added notifications
 * 
 * Enable:
 *  - install at above path
 *  - add to /store/scripts/ovmsmain.js:
 *        chgthrottle = require("lib/chgthrottle");
 *  - script reload
 * 
 * Config:
 *  - usr chgthrottle.enabled         …yes = enable throttling
 *  - usr chgthrottle.t1.temp         …temperature threshold 1
 *  - usr chgthrottle.t1.amps         …charge current at threshold 1
 *  - usr chgthrottle.t2.temp         …threshold 2
 *  - usr chgthrottle.t2.amps         …current 2
 *  - usr chgthrottle.t3.temp         …threshold 3
 *  - usr chgthrottle.t3.amps         …current 3
 * 
 * Set t1…3 to ascending temperature thresholds.
 * Set amps to -1 to stop charge at that level.
 * 
 * Usage:
 *  - script eval chgthrottle.info()  …show config & state (JSON)
 * 
 */

const tickerEvent = "ticker.60";

var cfg = {
  "enabled": "yes",
  "t1.temp": "50",
  "t1.amps": "25",
  "t2.temp": "55",
  "t2.amps": "15",
  "t3.temp": "60",
  "t3.amps": "-1",
};

var state = {
  enabled: false,
  ticker: false,
  temp: null,
  level: null,
  amps: null,
};

// Output info:
function printInfo() {
  JSON.print({ "cfg": cfg, "state": state });
}

// Read & process config:
function readConfig() {
  // get config update:
  var upd = Object.assign({}, cfg, OvmsConfig.GetValues("usr", "chgthrottle."));
  cfg = upd;

  // process:
  state.enabled = (cfg["enabled"] == "yes");
  if (state.enabled && !state.ticker) {
    print("throttling enabled");
    state.ticker = PubSub.subscribe(tickerEvent, ticker);
    ticker();
  } else if (!state.enabled && state.ticker) {
    print("throttling disabled");
    PubSub.unsubscribe(state.ticker);
    state.ticker = false;
    state.temp = state.level = state.amps = null;
    OvmsVehicle.SetChargeCurrent(0); // unlimited
  }
}

// Ticker:
function ticker() {
  var ctemp, level, ltemp, success, msg;

  ctemp = OvmsMetrics.AsFloat("v.c.temp");
  if (state.temp === ctemp) return;

  // determine new threshold level:
  state.temp = ctemp;
  for (level = 3; level >= 1; --level) {
    ltemp = Number(cfg["t"+level+".temp"]);
    if (ltemp && ctemp >= ltemp) break;
  }
  if (state.level === level) return;

  // set & configure new level:
  state.level = level;
  state.amps = (level > 0) ? Number(cfg["t"+level+".amps"]) : 0;
  msg = "Temp " + ctemp + "C = Level " + level + ":\n";
  if (state.amps < 0) {
    success = OvmsVehicle.StopCharge();
    if (success)
      msg += "Charge stopped";
    else
      msg += "ERROR stopping charge; take manual control!";
  } else {
    success = OvmsVehicle.SetChargeCurrent(state.amps);
    if (success)
      msg += "Charge current set to "
        + ((state.amps > 0) ? state.amps + "A" : "unlimited");
    else
      msg += "ERROR setting charge current to "
        + ((state.amps > 0) ? state.amps + "A" : "unlimited")
        + "; take manual control!";
  }

  // log & notify:
  print(msg);
  OvmsNotify.Raise("alert", "usr.chgthrottle.status", "ChgThrottle: " + msg);
}

// Init:
readConfig();
PubSub.subscribe("config.changed", readConfig);

// API exports:
exports.info = printInfo;
