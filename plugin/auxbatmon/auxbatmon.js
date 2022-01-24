/**
 * Module plugin:
 *  Auxiliary (12V) Battery History Chart
 *  Version 2.3.2 by Michael Balzer <dexter@dexters-web.de>
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
 *  - auxbatmon.dump("CBOR")  -- dump recorded history data in CBOR format (binary)
 *  - auxbatmon.dump()  -- dump recorded history data in JSON format
 *  - auxbatmon.data()  -- get a copy of the history data object
 */

// Customization (needs to match web plugin):
const sampleInterval = 60;  // Seconds, needs to match a ticker event
const historyHours = 24;    // RAM ~ historyHours / sampleInterval

const tickerEvent = 'ticker.' + sampleInterval;
const maxEntries = historyHours * 3600 / sampleInterval;
const storeFile = "/store/usr/auxbatmon.cbor";  // empty string = no storage

var history = {
  "time": null,
  "v.b.12v.voltage": [],
  "v.b.12v.voltage.ref": [],
  "v.b.12v.current": [],
  "v.c.temp": [],
  "v.e.temp": [],
};

var listen_sdmount = null;

// Saving to VFS may cause short blockings, so only allow when vehicle is off:
function allowSave() {
  return !OvmsMetrics.Value("v.e.on") && !OvmsMetrics.Value("v.c.charging");
}

// Ticker:
function ticker() {
  history["time"] = new Date().getTime();
  Object.keys(history).forEach(function(key) {
    if (key != "time") {
      if (history[key].push(OvmsMetrics.AsFloat(key)) > maxEntries)
        history[key].splice(0, 1);
    }
  });
  if (storeFile && allowSave()) {
    VFS.Save({
      path: storeFile,
      data: CBOR.encode(history)
    });
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
      try {
        history = CBOR.decode(data);
      } catch (ex) {
        print("ERROR: CBOR.decode failed: " + ex);
      }
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
