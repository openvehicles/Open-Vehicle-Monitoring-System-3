/**
 * Module plugin:
 *  Auxiliary (12V) Battery History Chart
 *  Version 2.0 by Michael Balzer <dexter@dexters-web.de>
 * 
 * This module records a set of metrics with a fixed time interval.
 * History data is stored in a file and automatically restored on reboot/reload.
 * 
 * Installation:
 *  - Save as /store/scripts/lib/auxbatmon.js
 *  - Add to /store/scripts/ovmsmain.js: auxbatmon = require("lib/auxbatmon");
 *  - Issue "script reload"
 *  - Install "auxbatmon.htm" web plugin
 * 
 * Configuration:
 *  No live config currently. You can customize the sample interval (default: 60 seconds)
 *  and the history size (default: 24 hours). Take care to match this in the web plugin.
 * 
 * Usage:
 *  - [script eval] auxbatmon.dump()  -- dump recorded history data in JSON format
 */

// Customization (needs to match web plugin):
const sampleInterval = 60;  // Seconds, needs to match a ticker event
const historyHours = 24;    // RAM ~ historyHours / sampleInterval

const tickerEvent = 'ticker.' + sampleInterval;
const maxEntries = historyHours * 3600 / sampleInterval;
const storeFile = "/store/usr/auxbatmon.jx";

var history = {
  "time": null,
  "v.b.12v.voltage": [],
  "v.b.12v.voltage.ref": [],
  "v.b.12v.current": [],
  "v.c.temp": [],
  "v.e.temp": [],
};

// Ticker:
function ticker() {
  history["time"] = new Date().getTime();
  Object.keys(history).forEach(function(key) {
    if (key != "time") {
      if (history[key].push(OvmsMetrics.AsFloat(key)) > maxEntries)
        history[key].splice(0, 1);
    }
  });
  if (globalThis.VFS) VFS.Save({
    path: storeFile,
    data: Duktape.enc('jx', history)
  });
}

// History dump:
function dump() {
  print(Duktape.enc('jc', history));
}

// Init:
if (globalThis.VFS) {
  VFS.Load({
    path: storeFile,
    done: function(data) {
      history = Duktape.dec('jx', data);
    },
    always: function() {
      PubSub.subscribe(tickerEvent, ticker);
    }
  });
} else {
  PubSub.subscribe(tickerEvent, ticker);
}

// API methods:
exports.dump = dump;
