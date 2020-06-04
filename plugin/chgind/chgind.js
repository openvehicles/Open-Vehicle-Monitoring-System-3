/**
 * Module plugin: ChgInd – Charge State Indicator
 *  Version 1.0 by Michael Balzer <dexter@dexters-web.de>
 * 
 * Control LEDs to signal charge state:
 *  - OFF = not charging
 *  - RED = charging, SOC below 20%
 *  - YELLOW (RED+GREEN) = charging, SOC 20% … 80%
 *  - GREEN = charging, SOC above 80%
 *  - BLUE = fully charged (held for 15 minutes)
 * 
 * Dependencies:
 *  - OVMS firmware >= 3.2.008-266
 * 
 * Installation:
 *  - Save as /store/scripts/lib/chgind.js
 *  - Add to /store/scripts/ovmsmain.js: chgind = require("lib/chgind");
 *  - Issue "script reload"
 * 
 * API:
 *  - chgind.set(state)   -- set LED state ('off'/'red'/'yellow'/'green'/'blue')
 */

// ================== CONFIGURATION ====================

// EGPIO ports to use:
const LED_red   = 4;  // EIO_3
const LED_green = 5;  // EIO_4
const LED_blue  = 6;  // EIO_5

// Time to hold BLUE state after charge stop:
const holdTimeMinutes = 15;

// ================== END OF CONFIG ====================

var ledState, ledTimeout, ticker;

// Hardware control:
function setLEDs(state) {
  state = (""+state).toUpperCase();
  if (ledState == state) return;
  switch (state) {
    case 'RED':
      OvmsCommand.Exec("egpio output "+LED_red+" 1 "+LED_green+" 0 "+LED_blue+" 0");
      break;
    case 'YELLOW':
      OvmsCommand.Exec("egpio output "+LED_red+" 1 "+LED_green+" 1 "+LED_blue+" 0");
      break;
    case 'GREEN':
      OvmsCommand.Exec("egpio output "+LED_red+" 0 "+LED_green+" 1 "+LED_blue+" 0");
      break;
    case 'BLUE':
      OvmsCommand.Exec("egpio output "+LED_red+" 0 "+LED_green+" 0 "+LED_blue+" 1");
      break;
    case 'OFF':
      OvmsCommand.Exec("egpio output "+LED_red+" 0 "+LED_green+" 0 "+LED_blue+" 0");
      break;
    default:
      print("ERROR: invalid/unknown state: " + state);
      return;
  }
  print("new LED state: " + state);
  ledState = state;
}

// Check charge state:
function checkState(event) {
  var metrics = OvmsMetrics.GetValues(["v.c.charging", "v.b.soc", "v.c.state"]);

  if (metrics["v.c.charging"]) {
    // charging:
    if (metrics["v.b.soc"] < 20)
      setLEDs('RED');
    else if (metrics["v.b.soc"] < 80)
      setLEDs('YELLOW');
    else
      setLEDs('GREEN');
  }
  else if (event == "vehicle.charge.stop") {
    // charge done/abort:
    if (metrics["v.c.state"] == "done") {
      setLEDs('BLUE');
      ledTimeout = holdTimeMinutes * 60 / 10;
    } else {
      setLEDs('OFF');
    }
  }
  else if (event == "vehicle.charge.finish") {
    // charge port closed:
    setLEDs('OFF');
  }
  else if (ledTimeout > 0) {
    // LED hold timeout:
    if (--ledTimeout <= 0)
      setLEDs('OFF');
  }
  else {
    // not charging:
    setLEDs('OFF');
  }

  // start/stop ticker:
  if (ledState == 'OFF') {
    if (ticker) PubSub.unsubscribe(ticker);
    ticker = null;
    ledTimeout = 0;
  } else if (!ticker) {
    ticker = PubSub.subscribe("ticker.10", checkState);
  }
}

// Init:
PubSub.subscribe("vehicle.charge.start", checkState);
PubSub.subscribe("vehicle.charge.stop", checkState);
PubSub.subscribe("vehicle.charge.finish", checkState);
checkState();

// API exports:
exports.set = setLEDs;
