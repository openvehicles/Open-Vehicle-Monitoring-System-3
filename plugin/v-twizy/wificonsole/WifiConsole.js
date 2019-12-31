/**
 * /store/scripts/lib/WifiConsole.js
 * 
 * Module plugin: WifiConsole backend
 * 
 * Version 1.0   Michael Balzer <dexter@dexters-web.de>
 * 
 * Hardware: https://github.com/dexterbg/WifiConsole
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
  var cmdres, lines, line, i, key;
  cfg = {
    buttons: [0,1,2,3],
    label: { 0:"STD", 1:"PWR", 2:"ECO", 3:"ICE" },
  };
  cmdres = OvmsCommand.Exec("config list xrt");
  lines = cmdres.split("\n");
  for (i = 0; i < lines.length; i++) {
    line = lines[i].match(/profile([0-9]{2})?[._]([^:]*): (.*)/);
    if (line && line.length == 4) {
      if (line[2] == "buttons")
        cfg.buttons = eval("[" + line[3] + "]");
      else if (line[2] == "label")
        cfg.label[Number(line[1])] = line[3];
    }
  }
}

// API method wificon.info():
exports.info = function() {
  var key;
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
