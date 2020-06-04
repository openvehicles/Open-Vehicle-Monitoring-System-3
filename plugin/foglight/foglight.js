/**
 * /store/scripts/lib/foglight.js
 * 
 * Module plugin:
 *  Foglight control with speed adaption and auto off on vehicle off.
 * 
 * Version 1.0   Michael Balzer <dexter@dexters-web.de>
 * 
 * Enable:
 *  - install at above path
 *  - add to /store/scripts/ovmsmain.js:
 *        foglight = require("lib/foglight");
 *  - script reload
 * 
 * Config:
 *  - vehicle foglight.port           …EGPIO output port number
 *  - vehicle foglight.auto           …yes = speed automation
 *  - vehicle foglight.speed.on       …auto turn on below this speed
 *  - vehicle foglight.speed.off      …auto turn off above this speed
 * 
 * Usage:
 *  - script eval foglight.set(1)     …toggle foglight on
 *  - script eval foglight.set(0)     …toggle foglight off
 *  - script eval foglight.info()     …show config & state (JSON)
 * 
 */

var cfg = {
  "foglight.port":      "1",
  "foglight.auto":      "no",
  "foglight.speed.on":  "45",
  "foglight.speed.off": "55",
};

var state = {
  on: 0,            // foglight on/off
  port: 0,          // current port output state
  ticker: false,    // ticker subscription
};

// Read config:
function readconfig() {
  var cmdres, lines, cols, i;
  cmdres = OvmsCommand.Exec("config list vehicle");
  lines = cmdres.split("\n");
  for (i=0; i<lines.length; i++) {
    if (lines[i].indexOf("foglight") >= 0) {
      cols = lines[i].substr(2).split(": ");
      cfg[cols[0]] = cols[1];
    }
  }
  // update ticker subscription:
  if (cfg["foglight.auto"] == "yes" && !state.ticker) {
    state.ticker = PubSub.subscribe("ticker.1", checkspeed);
  } else if (cfg["foglight.auto"] != "yes" && state.ticker) {
    PubSub.unsubscribe(state.ticker);
    state.ticker = false;
  }
}

// EGPIO port control:
function toggle(onoff) {
  if (state.port != onoff) {
    OvmsCommand.Exec("egpio output " + cfg["foglight.port"] + " " + onoff);
    state.port = onoff;
    OvmsCommand.Exec("event raise usr.foglight." + (onoff ? "on" : "off"));
  }
}

// Check speed:
function checkspeed() {
  if (!state.on)
    return;
  var speed = OvmsMetrics.AsFloat("v.p.speed");
  if (speed <= cfg["foglight.speed.on"])
    toggle(1);
  else if (speed >= cfg["foglight.speed.off"])
    toggle(0);
}

// API method foglight.set(onoff):
exports.set = function(onoff) {
  if (onoff) {
    state.on = 1;
    if (cfg["foglight.auto"] == "yes") {
      checkspeed();
      print("Foglight AUTO mode\n");
    } else {
      toggle(1);
      print("Foglight ON\n");
    }
  } else {
    state.on = 0;
    toggle(0);
    print("Foglight OFF\n");
  }
}

// API method foglight.info():
exports.info = function() {
  JSON.print({ "cfg": cfg, "state": state });
}

// Init:
readconfig();
PubSub.subscribe("config.changed", readconfig);
PubSub.subscribe("vehicle.off", function(){ exports.set(0); });
