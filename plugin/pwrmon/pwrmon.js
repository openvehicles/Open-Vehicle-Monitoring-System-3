/**
 * Module plugin: PwrMon – Trip Power/Energy Chart
 *  Version 1.1 by Michael Balzer <dexter@dexters-web.de>
 * 
 * This module records trip metrics by odometer distance.
 * History data is stored in a file and automatically restored on reboot/reload.
 * 
 * Installation:
 *  - Save as /store/scripts/lib/pwrmon.js
 *  - Add to /store/scripts/ovmsmain.js: pwrmon = require("lib/pwrmon");
 *  - Issue "script reload"
 *  - Install "pwrmon.htm" web plugin
 * 
 * Configuration:
 *  No live config currently, see constants for customization.
 * 
 * Usage:
 *  - pwrmon.dump()  -- dump (print) recorded history data in JSON format
 *  - pwrmon.data()  -- get a copy of the history data object
 */

const minSampleDistance = 0.3;              // [km]
const minRecordDistance = 20;               // [km]

const storeDistance = 1;                    // [km]
const storeFile = "/store/usr/pwrmon.jx";   // empty = no storage

const maxEntries = Math.ceil(minRecordDistance / minSampleDistance);
const tickerEvent = "ticker.1";

var history = {
  "time": [],
  "v.p.odometer": [],
  "v.p.altitude": [],
  "v.b.energy.used": [],
  "v.b.energy.recd": [],
};
var lastOdo = 0;
var saveOdo = 0;

// Saving to VFS may cause short blockings, so only allow when vehicle is off:
function allowSave() {
  return !OvmsMetrics.Value("v.e.on") && !OvmsMetrics.Value("v.c.charging");
}

// Ticker:
function ticker() {
  if (OvmsMetrics.Value("v.e.on") == "no") {
    lastOdo = 0;
    return;
  }
  var m = OvmsMetrics.GetValues(history, true);
  if (m["v.p.odometer"] - lastOdo < minSampleDistance)
    return;
  lastOdo = m["v.p.odometer"];

  m.time = new Date().getTime() / 1000;
  Object.keys(m).forEach(function(key) {
    if (history[key].push(m[key]) > maxEntries)
      history[key].splice(0, 1);
  });

  if (storeFile && allowSave() && lastOdo - saveOdo >= storeDistance) {
    saveOdo = lastOdo;
    VFS.Save({ path: storeFile, data: Duktape.enc('jx', history) });
  }
}

// History dump:
function dump() {
  print(Duktape.enc('jc', history));
}

// History copy:
function data() {
  return Object.assign({}, history);
}

// Init:
if (storeFile) {
  VFS.Load({
    path: storeFile,
    done: function(data) { history = Duktape.dec('jx', data); },
    always: function() { PubSub.subscribe(tickerEvent, ticker); }
  });
} else {
  PubSub.subscribe(tickerEvent, ticker);
}

// API methods:
exports.dump = dump;
exports.data = data;
