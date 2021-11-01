/**
 * /store/scripts/lib/xvu-ecu.js
 * 
 * Module plugin: VW e-Up / Seat Mii / Skoda Citigo ECU communication
 * 
 * Version 0.5.1
 * Author: Michael Balzer <dexter@dexters-web.de>
 * 
 * Enable:
 *  - install at above path
 *  - add to /store/scripts/ovmsmain.js:
 *        xvu = {
 *          ecu: require("lib/xvu-ecu"),
 *        };
 *  - script reload
 * 
 * Script usage example:
 *  var ecuinfo = xvu.ecu.scan("01");
 * 
 * Shell usage example:
 *  script eval xvu.ecu.$scan("01")
 * 
 * A full scan via the web UI needs continuous output to avoid a timeout, do this:
 *  xvu.ecu.getInstalledDevices().list.forEach(function (ecuid) {
 *    xvu.ecu.$scan(ecuid);
 *  });
 * 
 */

const ISOTP_STD = 0, VWTP_20 = 20;

var config =
{
  version: "0.5",
  debug: false,
  readonly: false,
};

var defaults =
{
  timeout: 5000, login: "1003", logout: "1001", protocol: ISOTP_STD
};

var devices =
{
  "01": { name: "Engine               ", txid: 0x7e0, rxid: 0x7e8 },
  "03": { name: "ABS Brakes           ", txid: 0x713, rxid: 0x77d },
  //"04": { name: "Steering Angle       ", txid: 0x, rxid: 0x },
  "08": { name: "Auto HVAC            ", txid: 0x200, rxid: 0x2c, protocol: VWTP_20, login: "1089", logout: "",
                                                                  subsystems: { baselist: [ 0x002c, 0x3033 ] } },
  "09": { name: "Central Electronics  ", txid: 0x200, rxid: 0x20, protocol: VWTP_20, login: "1089", logout: "",
                                                                  subsystems: { baselist: [ 0x0011, 0x002e ] } },
  "10": { name: "Park/Steer Assist    ", txid: 0x70a, rxid: 0x774 },
  "15": { name: "Airbags              ", txid: 0x715, rxid: 0x77f },
  "17": { name: "Instruments          ", txid: 0x714, rxid: 0x77e },
  "19": { name: "CAN Gateway          ", txid: 0x200, rxid: 0x1f, protocol: VWTP_20, login: "1089", logout: "" },
  "23": { name: "Brake Booster        ", txid: 0x73b, rxid: 0x7a5 },
  "25": { name: "Immobilizer          ", txid: 0x711, rxid: 0x77b },
  "44": { name: "Steering Assist      ", txid: 0x712, rxid: 0x77c },
  "51": { name: "Electric Drive       ", txid: 0x7e6, rxid: 0x7ee },
  "5F": { name: "Information Electr.  ", txid: 0x773, rxid: 0x7dd },
  "61": { name: "Battery Regulation   ", txid: 0x200, rxid: 0x33, protocol: VWTP_20, login: "1089", logout: "",
                                                                  subsystems: { baselist: [ 0x0034 ] } },
  "75": { name: "Telematics           ", txid: 0x767, rxid: 0x7d1 },
  "8C": { name: "Hybrid Battery Mgmt. ", txid: 0x7e5, rxid: 0x7ed },
  "A5": { name: "Frt Sens. Drv. Assist", txid: 0x74f, rxid: 0x7b9 },
  "AD": { name: "Brake Sensors        ", txid: 0x762, rxid: 0x7cc },
  "BD": { name: "Hi Volt. Batt. Charg.", txid: 0x765, rxid: 0x7cf },
  "C6": { name: "Batt. Chrg.          ", txid: 0x744, rxid: 0x7ae },
};


/*************************************************
 * Utilities
 */

function setDebug(onoff)
{
  if (onoff !== undefined)
    config.debug = onoff;
  return Number(config.debug);
}

function setReadOnly(onoff)
{
  if (onoff !== undefined)
    config.readonly = onoff;
  return Number(config.readonly);
}

function configure(options)
{
  if (typeof options == "object")
    Object.assign(config, options);
  return config;
}

function logDebug(text)
{
  if (config.debug) print("# " + text + "\n");
  return false;
}

function println(text)
{
  print(text + "\n");
}

function requestToHex(request)
{
  if (typeof request == "string")
    return request;
  else
    return Duktape.enc('hex', request);
}

function padNum(num, size)
{
  var t = "" + num;
  if (t.length < size) t = "0".repeat(size - t.length) + t;
  return t;
}

var textdecoder = new TextDecoder(); // unknown encoding, probably just ASCII

function responseToText(response)
{
  var t = textdecoder.decode(response);
  // remove trailing NUL char & spaces:
  var i = t.indexOf("\0");
  if (i >= 0) t = t.substring(0, i);
  return t.trim();
}


/*************************************************
 * ECU configuration & OBD access
 * 
 * Note: as TP20 only supports one active channel, there is no point
 *  in supporting multiple ECU connections. So we simply use single
 *  objects for the current ECU configuration & OBD result.
 */

var ecu = {}, obd = {};

function select(ecuid)
{
  obd = {};
  if (ecu.id == ecuid && !ecu.error) return true;
  if (ecu.loggedIn) logout();
  ecu = { id: ecuid };
  var cfg = devices[ecuid];
  if (!cfg) {
    ecu.error = "Unknown ECU ID / protocol";
    return false;
  }
  Object.assign(ecu, defaults, cfg);
  logDebug("xvu.ecu.select " + ecuid + ": OK");
  return true;
}

function get(request)
{
  if (!ecu.id) {
    obd = { error: true, errordesc: "No ECU selected" };
    return false;
  }
  if (config.readonly && requestToHex(request).match(/^(14|2704|2e)/i)) {
    obd = { error: false, response_hex: "", response: new Uint8Array() };
    logDebug("xvu.ecu.get " + requestToHex(request) + ": " + "SKIPPED (READONLY)");
    return true;
  }
  ecu.request = request;
  obd = OvmsVehicle.ObdRequest(ecu);
  if (obd.error) {
    logDebug("xvu.ecu.get " + requestToHex(request) + ": " + obd.errordesc);
    return false;
  } else {
    logDebug("xvu.ecu.get " + requestToHex(request) + ": " + (obd.response_hex ? obd.response_hex : "OK"));
    return true;
  }
}


/*************************************************
 * ECU session management
 */

function login(ecuid)
{
  if (ecuid) select(ecuid);
  if (!ecu.id) return false;
  if (ecu.loggedIn) return true;
  if (ecu.login) {
    ecu.loggedIn = get(ecu.login);
    return ecu.loggedIn;
  } else {
    return true;
  }
}

function logout()
{
  if (ecu.loggedIn) {
    if (ecu.logout) {
      ecu.loggedIn = !get(ecu.logout);
    } else {
      obd = OvmsVehicle.ObdRequest({ txid: ecu.txid, rxid: 0, request: "000000" });
      ecu.loggedIn = (obd.error != 0);
    }
  }
  return !ecu.loggedIn;
}


/*************************************************
 * WorkShopCode management
 */

function decodeWorkShopCode(code)
{
  if (!code) return "";
  if (typeof code == "string")
    code = Duktape.dec('hex', code);

  var wsc = (code[3] & 0x01) << 16 | code[4] << 8 | code[5];
  var imp = (code[2] & 0x07) << 7 | (code[3] & 0xfe) >> 1;
  var equ = code[0] << 13 | code[1] << 5 | (code[2] & 0xf8) >> 3;

  return padNum(wsc,5) + "-" + padNum(imp,3) + "-" + padNum(equ,5);
}

var workshopcode_override = null;

function getWorkShopCodeOverride()
{
  return workshopcode_override;
}

function setWorkShopCodeOverride(userwsc)
{
  if (!userwsc) {
    workshopcode_override = null;
    logDebug("xvu.ecu.setWorkShopCodeOverride: WSC cleared");
    return true;
  }

  if (userwsc.match(/^[0-9a-f]{12}$/i)) {
    // hex:
    workshopcode_override = userwsc;
    logDebug("xvu.ecu.setWorkShopCodeOverride: WSC = " + workshopcode_override);
    return true;
  }

  var parts = userwsc.split("-");
  if (parts.length == 3) {
    // <wsc>-<imp>-<equ>:
    var wsc = parseInt(parts[0], 10),
        imp = parseInt(parts[1], 10),
        equ = parseInt(parts[2], 10);
    var asm = new Uint8Array(6);
    asm[5] = (wsc & 0xff);
    asm[4] = (wsc >> 8) & 0xff;
    asm[3] = ((wsc >> 16) & 0x01) | ((imp << 1) & 0xfe);
    asm[2] = ((imp >> 7) & 0x07) | ((equ << 3) & 0xf8);
    asm[1] = ((equ >> 5) & 0xff);
    asm[0] = ((equ >> 13) & 0xff);
    workshopcode_override = Duktape.enc('hex', asm);
    logDebug("xvu.ecu.setWorkShopCodeOverride: WSC = " + workshopcode_override);
    return true;
  }

  return { error: "Invalid WSC: " + userwsc };
}

function getWorkShopCode(ecuid, reload)
{
  if (ecuid) select(ecuid);
  if (!ecu.id) return false;

  // (re)load WSC cache?
  if (devices[ecu.id].wsc == undefined || reload) {
    if (get("22f1a5"))
      devices[ecu.id].wsc = ecu.wsc = obd.response_hex;
    else if (obd.error > 0)
      devices[ecu.id].wsc = ecu.wsc = ""; // ECU has no WSC
    else
      return false; // request failed
  }

  return devices[ecu.id].wsc;
}

function putWorkShopCode(ecuid)
{
  var wsc_hex = getWorkShopCode(ecuid);
  if (wsc_hex === false) return false;
  if (wsc_hex === "") return true; // ECU has no WSC

  if (workshopcode_override)
    wsc_hex = workshopcode_override;

  if (!login()) return false;

  // set date: 2ef199<YYMMDD>
  var d = new Date();
  get("2ef199" + d.toISOString().substr(2,8).replace(/-/g,''));
  // set WSC: 2ef198<wsc>
  if (get("2ef198" + wsc_hex)) {
    // update WSC cache on success:
    devices[ecu.id].wsc = ecu.wsc = wsc_hex;
  }

  // note: we ignore all errors as either or both writes are optional (fail) on some ECUs
  return wsc_hex;
}


/*************************************************
 * Security Access
 */

function putSecurityCode(code)
{
  if (code == undefined || code === "") return true;
  if (!ecu.id || !login()) return false;

  // request seed:
  if (!get("2703")) return false;
  // calculate key:
  var key = parseInt(obd.response_hex, 16) + parseInt(code, 10);
  // send key:
  return get("2704" + padNum(key.toString(16), 8).substr(-8));
}


/*************************************************
 * ECU status & device info scanner
 */

function getInstalledDevices(options)
{
  options = Object.assign({}, options);
  var res = { list: [] };

  // login to CAN Gateway:
  if (!login("19")) {
    res.error = "Login to CAN Gateway (SG19) failed";
    return res;
  }
  
  // get ECU inventory:
  if (!get("2204a1")) {
    res.error = "Cannot read PID 04a1 of CAN Gateway (SG19)";
    return res;
  }

  // extract ECUs installed on this car:
  var eculist = [], i, ecuid;
  for (i = 0; i < obd.response.length; i += 4) {
    if (obd.response[i+2] & 0x01) {
      ecuid = obd.response_hex.substr(i*2,2).toUpperCase();
      if (devices[ecuid] || options.include_unknown) {
        eculist.push(ecuid);
        res[ecuid] = Object.assign({}, devices[ecuid], {
          gwinfo: obd.response_hex.substr(i*2+2,6)
        });
      }
    }
  }
  res.list = eculist.sort();

  logout();
  return res;
}

function getDevices(options)
{
  options = Object.assign({}, options);
  if (options.query_installed) {
    return getInstalledDevices(options);
  } else {
    var res = Object.assign({}, devices);
    res.list = Object.keys(devices).sort();
    return res;
  }
}

// Subsystem PIDs follow different addressing schemes:
var subsystem_scheme =
{
  "0608": {
    "ComponentName": 0x6c00,
    "HWPartNo": 0x6600,
    "HWVersion": 0x6800,
    "SWPartNo": 0x6200,
    "SWVersion": 0x6400,
    "SerialNo": 0x6a00,
    //"Coding": 0x6000?,
  },
  "default": {
    "ComponentName": 0x720,
    "HWPartNo": 0x690,
    "HWVersion": 0x6c0,
    "SWPartNo": 0x630,
    "SWVersion": 0x660,
    "SerialNo": 0x6f0,
    "Coding": 0x600,
  },
};

function scanSubsystem(idx, base, scheme)
{
  var res = {};

  // check for subsystem role in 6ca0+idx:
  var adr = 0x6ca0 + idx;
  var req = "22" + padNum(adr.toString(16), 4);
  if (get(req))
    res["Role"] = responseToText(obd.response);

  // fetch subsystem info:
  for (info in scheme) {
    adr = base + scheme[info];
    req = "22" + padNum(adr.toString(16), 4);
    if (get(req)) {
      if (info == "Coding") {
        res[info] = obd.response_hex;
      } else {
        res[info] = responseToText(obd.response);
      }
    }
  }

  if (Object.keys(res).length == 0)
    return null;
  else
    return res;
}

function scan(ecuid)
{
  var wasLoggedIn = (ecu.id == ecuid && ecu.loggedIn);
  var res = { "ECUID": ecuid };

  // select ECU & login:
  if (!select(ecuid)) {
    res.error = ecu.error;
    return res;
  }
  res["ECUName"] = ecu.name.trim();
  if (!login(ecuid)) {
    res.error = "Login failed: " + obd.errordesc;
    return res;
  }

  res["Date"] = new Date().toLocaleString();

  // fetch text info:
  var fetch = {
    "VIN": "22f190",
    "HWPartNo": "22f191",
    "HWVersion": "22f1a3",
    "SWPartNo": "22f187",
    "SWVersion": "22f189",
    "ComponentName": "22f197",
    "ComponentType": "22f1aa",
    "ComponentRevision": "22f17e",
    "SerialNo": "22f18c",
    "BuildStamp": "22f17c",
    "ASAMDatasetName": "22f19e",
    "ASAMDatasetVersion": "22f1a2",
  };
  for (info in fetch) {
    if (get(fetch[info])) {
      res[info] = responseToText(obd.response);
    }
  }

  // fetch WSC:
  if (get("22f1a5")) {
    devices[ecu.id].wsc = ecu.wsc = obd.response_hex;
    res["WorkShopCode"] = decodeWorkShopCode(obd.response);
  }

  // fetch coding:
  if (get("220600"))
    res["Coding"] = obd.response_hex;

  // get subsystem info:
  var sublist, baselist = [], scheme = "default";
  if (get("220608")) {
    // this seems to be a list of 16 bit base addresses:
    sublist = obd.response_hex.match(/.{4}/g);
    baselist = sublist.map(function (hex) { return parseInt(hex, 16) });
    scheme = "0608";
  }
  else if (get("220607")) {
    // this seems to be a list of 16 bit presence flags:
    sublist = obd.response_hex.match(/.{4}/g);
    // baselist needs to be set per ECU, no known derivation
  }
  else if (get("220606")) {
    // this can be a list of 16 bit or 8 bit offsets, or of 16 bit presence flags;
    // trying to distinguish here by the first byte:
    if (obd.response[0] == 0 || obd.response[0] > 0x10) {
      sublist = obd.response_hex.match(/.{4}/g);
    } else {
      sublist = obd.response_hex.match(/.{2}/g);
    }
    // note: this base derivation currently only applies to ECU 15:
    baselist = sublist.map(function (hex) { return 0x000f + parseInt(hex, 16) });
  }

  // overlay ECU specific subsystem configuration:
  if (ecu.subsystems) {
    if (ecu.subsystems.baselist)
      baselist = ecu.subsystems.baselist;
    if (ecu.subsystems.scheme)
      scheme = ecu.subsystems.scheme;
  }

  // scan subsystems:
  var subinfo = [];
  for (i = 0; i < baselist.length; i++) {
    var subscan = scanSubsystem(i, baselist[i], subsystem_scheme[scheme]);
    if (subscan) subinfo.push(subscan);
  }
  if (sublist)
    res["Subsystems"] = sublist;
  if (subinfo.length > 0)
    res["SubsystemInfo"] = subinfo;

  // get assumed routines info:
  if (get("31b80000"))
    res["Routines"] = obd.response_hex.match(/.{4}/g);
  
  // fetch DTC list:
  if (get("1802ff00"))
    res["DTC"] = obd.response_hex.substr(2).match(/.{6}/g);
  else if (get("1902ae"))
    res["DTC"] = obd.response_hex.substr(2).match(/.{8}/g);
  
  if (!wasLoggedIn) logout();
  return res;
}

function scanAll(idlist)
{
  var res = {};
  if (!idlist) idlist = Object.keys(devices);
  idlist.forEach(function (ecuid) {
    res[ecuid] = scan(ecuid);
  });
  return res;
}


/*************************************************
 * Coding
 */

function getCodingParts()
{
  var res = {
    adrs: [],
    parts: []
  };
  
  // get main coding:
  if (!get("220600"))
    throw "Cannot read coding: " + obd.errordesc;
  res.adrs.push("0600");
  res.parts.push(obd.response_hex);

  // get subsystem codings & addresses:
  if (ecu.subsystems && ecu.subsystems.baselist) {
    var scheme = subsystem_scheme[ecu.subsystems.scheme || "default"];
    ecu.subsystems.baselist.forEach(function (base) {
      var adr = padNum((base + scheme["Coding"]).toString(16), 4);
      if (get("22"+adr)) {
        res.adrs.push(adr);
        res.parts.push(obd.response_hex);
      }
    });
  }

  res.hex = res.parts.join("");
  res.bin = Duktape.dec('hex', res.hex);

  return res;
}

function putCodingParts(ecu_coding, new_coding, security_code)
{
  var i, offset, nc_part, nc_adr;

  if (!ecu_coding || typeof ecu_coding != "object")
    throw "Invalid argument (ecu_coding)";
  if (typeof new_coding != "string" || !new_coding.match(/^[0-9a-f]+$/i))
    throw "New coding needs to be a hex encoded string";
  if (new_coding.length > ecu_coding.hex.length)
    throw "New coding too long";
  if (new_coding.length < ecu_coding.parts[0].length)
    throw "New coding too short";

  if (!putWorkShopCode())
    throw "Cannot put workshop code: " + obd.errordesc;
  if (!putSecurityCode(security_code))
    throw "Security access failed: " + obd.errordesc;

  // write parts (main & subsystems) as far as given:
  for (i = 0, offset = 0; i < ecu_coding.parts.length; i++) {
    nc_part = new_coding.substr(offset, ecu_coding.parts[i].length);
    if (nc_part.length == 0)
      break;
    else if (nc_part.length < ecu_coding.parts[i].length)
      throw "New subsystem "+i+" coding too short";
    offset += ecu_coding.parts[i].length;
    nc_adr = ecu_coding.adrs[i];
    if (!get("2e" + nc_adr + nc_part))
      throw "Coding update failed on " + nc_adr + ": " + obd.errordesc;
  }
}

function coding(ecuid, new_coding, security_code)
{
  var wasLoggedIn = (ecu.id == ecuid && ecu.loggedIn);
  var ecu_coding, error;
  
  try {
    // login:
    if (!login(ecuid))
      throw "Login failed: " + obd.errordesc;
    
    // get coding:
    ecu_coding = getCodingParts();
    
    // change coding?
    if (new_coding != undefined) {
      logDebug("xvu.ecu.coding: old=" + ecu_coding.hex + " new=" + new_coding);
      putCodingParts(ecu_coding, new_coding, security_code);
      ecu_coding.hex = new_coding;
    }
  }
  catch (e) {
    logDebug("xvu.ecu.coding: ERROR: " + e);
    error = e;
  }

  if (!wasLoggedIn) logout();
  return error ? { "error": error } : ecu_coding.hex;
}

function codingBit(ecuid, bytenr, bitnr, new_state, security_code)
{
  var wasLoggedIn = (ecu.id == ecuid && ecu.loggedIn);
  var ecu_coding, bit_state, bit_mask, error;
  
  try {
    // login:
    if (!login(ecuid))
      throw "Login failed: " + obd.errordesc;

    // get coding:
    ecu_coding = getCodingParts();
    
    // extract bit:
    if (bytenr < 0 || bytenr >= ecu_coding.bin.length || bitnr < 0 || bitnr > 7)
      throw "Invalid bit address";
    bit_mask = 1 << bitnr;
    bit_state = (ecu_coding.bin[bytenr] & bit_mask) >> bitnr;
    
    // change coding?
    if (new_state != undefined) {
      if (new_state)
        ecu_coding.bin[bytenr] |= bit_mask;
      else
        ecu_coding.bin[bytenr] &= ~bit_mask;
      var new_coding = Duktape.enc('hex', ecu_coding.bin);

      logDebug("xvu.ecu.codingBit: old=" + ecu_coding.hex + " new=" + new_coding);
      putCodingParts(ecu_coding, new_coding, security_code);
      bit_state = new_state ? 1 : 0;
    }
  }
  catch (e) {
    logDebug("xvu.ecu.codingBit: ERROR: " + e);
    error = e;
  }

  if (!wasLoggedIn) logout();
  return error ? { "error": error } : bit_state;
}


/*************************************************
 * Exports
 */

// Script API:

exports.setDebug = setDebug;
exports.setReadOnly = setReadOnly;

exports.select = select;
exports.get = get;
exports.res = function () { return obd };
exports.getRes = function (a1, a2) {
  // called with (ecuid, request)?
  if (a2 != undefined) {
    if (!select(a1)) return { error: ecu.error };
    a1 = a2;
  }
  get(a1); return obd;
};

exports.login = login;
exports.logout = logout;

exports.getWorkShopCodeOverride = getWorkShopCodeOverride;
exports.setWorkShopCodeOverride = setWorkShopCodeOverride;
exports.getWorkShopCode = getWorkShopCode;
exports.putWorkShopCode = putWorkShopCode;

exports.putSecurityCode = putSecurityCode;

exports.getDevices = getDevices;
exports.getInstalledDevices = getInstalledDevices;
exports.getSelectedDevice = function () { return ecu };

exports.scan = scan;
exports.scanAll = scanAll;

exports.coding = coding;
exports.codingBit = codingBit;


// Shell/Web User Interface Wrappers:

function printResult(res)
{
  if (res == undefined)
    print("undefined\n");
  else if (typeof res == "object" && res.errordesc)
    print("ERROR: " + res.errordesc + "\n");
  else if (typeof res == "object" && res.error)
    print("ERROR: " + res.error + "\n");
  else if (typeof res == "object")
    print(JSON.stringify(res, null, 2) + "\n");
  else if (typeof res == "string" || typeof res == "number")
    print(res + "\n");
  else if (res)
    print("OK\n");
  else if (!ecu.id)
    print("ERROR: No ECU selected\n");
  else if (ecu.error)
    print("ERROR: " + ecu.error + "\n");
  else if (obd.error)
    print("ERROR: " + obd.errordesc + "\n");
  else
    print("ERROR\n");
}

function printWSCResult(wsc_hex)
{
  if (wsc_hex === false)
    printResult(false);
  else
    printResult({
      "WorkShopCode": decodeWorkShopCode(wsc_hex),
      "WorkShopCodeHex": wsc_hex
    });
}

function printOBDResult(res)
{
  // add text decoding if applicable:
  if (!res.error && res.response.length > 0 && res.response[0] >= 0x20 && res.response[0] <= 0x7f) {
    var txt = responseToText(res.response);
    if (txt.match(/^[a-z0-9!"#$%&'()*+,.\/:;<=>?@\[\] ^_`{|}~-]*$/i))
      res.response_text = txt;
  }
  printResult(res);
}

exports.$debug = function () { println(setDebug.apply(null, arguments)) };
exports.$readonly = function () { println(setReadOnly.apply(null, arguments)) };
exports.$config = function () { println(JSON.stringify(configure.apply(null, arguments), null, 2)) };

exports.$select = function () { printResult(select.apply(null, arguments)) };
exports.$get = function () { printOBDResult(exports.getRes.apply(null, arguments)) };
exports.$login = function () { printResult(login.apply(null, arguments)) };
exports.$logout = function () { printResult(logout.apply(null, arguments)) };

exports.$getwscor = function () { printWSCResult(getWorkShopCodeOverride.apply(null, arguments)) };
exports.$setwscor = function () { printResult(setWorkShopCodeOverride.apply(null, arguments)) };

exports.$getwsc = function () { printWSCResult(getWorkShopCode.apply(null, arguments)) };
exports.$putwsc = function () { printResult(putWorkShopCode.apply(null, arguments)) };

exports.$putsec = function () { printResult(putSecurityCode.apply(null, arguments)) };

exports.$devices = function () { printResult(getDevices.apply(null, arguments)) };
exports.$device = function () { printResult(exports.getSelectedDevice.apply(null, arguments)) };

exports.$scan = function () { printResult(scan.apply(null, arguments)) };

exports.$scanall = function () {
  // stop VWUP polling:
  OvmsCommand.Exec("vehicle module NONE");
  OvmsCommand.Exec("can can1 start active 500000");
  // suppress warnings from long script execution:
  OvmsCommand.Exec("log level error script");

  print("[");
  getInstalledDevices().list.forEach(function (ecuid) {
    print(JSON.stringify(scan(ecuid), null, 2)); print(",");
  });
  print("]\n");

  // restore defaults & VWUP module:
  OvmsCommand.Exec("log level info script");
  OvmsCommand.Exec("vehicle module VWUP");
};

exports.$coding = function () { printResult(coding.apply(null, arguments)) };
exports.$codingbit = function () { printResult(codingBit.apply(null, arguments)) };

print("Plugin lib/xvu-ecu.js initialized\n");
