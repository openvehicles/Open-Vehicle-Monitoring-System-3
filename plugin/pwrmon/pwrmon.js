/**
 * Module plugin: PwrMon – Trip Power/Energy Chart
 *  Version 1.3 by Michael Balzer <dexter@dexters-web.de>
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
 *  - pwrmon.dump('CBOR')  -- dump (print) recorded history data in CBOR format (binary)
 *  - pwrmon.dump()  -- dump (print) recorded history data in JSON format
 *  - pwrmon.data()  -- get a copy of the history data object
 */

const minSampleDistance = 0.3;              // [km]
const minRecordDistance = 50;               // [km]

const storeDistance = 1;                    // [km]
const storeFile = "/store/usr/pwrmon.cbor"; // empty = no storage

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

var listen_sdmount = null;

// Saving to VFS may cause short blockings, so only allow when vehicle is off:
function allowSave() {
  return !OvmsMetrics.Value("v.e.on") && !OvmsMetrics.Value("v.c.charging");
}

// Ticker:
function ticker() {
  if (!OvmsMetrics.Value("v.e.on")) {
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
    VFS.Save({ path: storeFile, data: CBOR.encode(history) });
  }
}

// History dump:
function dump(fmt) {
  fmt = String(fmt).toUpperCase();
  if (fmt == "CBOR")
    write(CBOR.encode(history));
  else if (fmt == "HEX")
    print(Duktape.enc('hex', CBOR.encode(history)));
  else
    print(Duktape.enc('jc', history));
}

// History copy:
function data() {
  return Object.assign({}, history);
}

// Init:

function loadStoreFile() {
  VFS.Load({
    path: storeFile,
    binary: true,
    done: function(data) {
      print(storeFile + " loaded\n");
      history = CBOR.decode(data);
      startRecording();
    },
    fail: function(error) {
      print(storeFile + ": " + this.error + "\n");
      if (!listen_sdmount && storeFile.startsWith("/sd/") && this.error == "volume not mounted") {
        // retry once after SD mount:
        listen_sdmount = PubSub.subscribe("sd.mounted", loadStoreFile);
      } else {
        startRecording();
      }
    }
  });
}

function startRecording() {
  if (listen_sdmount) {
    PubSub.unsubscribe(listen_sdmount);
    listen_sdmount = null;
  }
  PubSub.subscribe(tickerEvent, ticker);
}

if (storeFile) {
  loadStoreFile();
} else {
  startRecording();
}

// API methods:
exports.dump = dump;
exports.data = data;
