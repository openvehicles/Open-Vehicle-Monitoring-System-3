/**
 * /store/scripts/lib/WifiConsole.js
 * 
 * Module plugin: WifiConsole backend
 * 
 * Version 1.1   Michael Balzer <dexter@dexters-web.de>
 * 
 * Enable:
 *  - install at above path
 *  - add to /store/scripts/ovmsmain.js:
 *        wificon = require("lib/WifiConsole");
 *  - script reload
 * 
 * API:
 *  - wificon.info()          …output button config & current profile
 *  - wificon.load(profile)   …load profile and output result
 */

var cfg;

// Read config:
function readconfig() {
  cfg = {
    buttons: [0,1,2,3],
    label: { 0:"STD", 1:"PWR", 2:"ECO", 3:"ICE" },
  };
  var upd = OvmsConfig.GetValues("xrt", "profile");
  if (upd["_buttons"])
    cfg.buttons = eval("[" + upd["_buttons"] + "]");
  Object.keys(upd).forEach(function(key) {
    if (key.endsWith(".label"))
      cfg.label[parseInt(key)] = upd[key];
  });
}

// API method wificon.info():
exports.info = function() {
  var key;
  print("S:" + Math.floor(OvmsMetrics.AsFloat("v.b.soc")) + "\n");
  print("A:" + OvmsMetrics.AsFloat("v.b.12v.voltage").toFixed(1) + "\n");
  print("P:" + OvmsMetrics.Value("xrt.cfg.profile") + "\n");
  var ws = OvmsMetrics.Value("xrt.cfg.ws");
  for (i = 0; i < cfg.buttons.length; i++) {
    key = cfg.buttons[i];
    print((key == ws ? "B:" : "b:") + key + "," + (cfg.label[key] || "#" + key) + "\n");
  }
}

// API method wificon.load(profile):
exports.load = function(profile) {
  OvmsCommand.Exec("xrt cfg load " + profile);
  print("P:" + OvmsMetrics.Value("xrt.cfg.profile") + "\n");
}

// Init:
readconfig();
PubSub.subscribe("config.changed", readconfig);
