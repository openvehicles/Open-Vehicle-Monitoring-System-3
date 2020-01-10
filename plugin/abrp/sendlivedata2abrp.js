/**
 *       /store/scripts/sendlivedata2abrp.js
 * 
 * Module plugin:
 *  Send live data to a better route planner
 *  This version uses the embedded GSM of OVMS, so there's an impact on data consumption
 *  /!\ requires OVMS firmware version 3.2.008-147 minimum (for HTTP call)
 * 
 * Version 1.0   2019   dar63 (forum https://www.openvehicles.com)
 * 
 * Enable:
 *  - install at above path
 *  - add to /store/scripts/ovmsmain.js:
 *                 abrp = require("sendlivedata2abrp");
 *  - script reload
 * 
 * Usage:
 *  - script eval abrp.info()         => to display vehicle data to be sent to abrp
 *  - script eval abrp.onetime()      => to launch one time the request to abrp server
 *  - script eval abrp.send(1)        => toggle send data to abrp
 *  -                      (0)        => stop sending data
 *
 **/



/*
 * Declarations:
 *   CAR_MODEL: find your car model here: https://api.iternio.com/1/tlm/get_carmodels_list?api_key=32b2162f-9599-4647-8139-66e9f9528370
 *   OVMS_API_KEY : API_KEY to access to ABRP API, given by the developer
 *   MY_TOKEN : Your token (corresponding to your abrp profile)
 *   URL : url to send telemetry to abrp following: https://iternio.com/index.php/iternio-telemetry-api/
 */
 
  const CAR_MODEL = "kia:niro:19:64:other";  
  const OVMS_API_KEY = "32b2162f-9599-4647-8139-66e9f9528370";
  const MY_TOKEN = "@@@@@@@@-@@@@-@@@@-@@@@-@@@@@@@@@@@@";
  const URL = "http://api.iternio.com/1/tlm/send";
  const CR = '\n';
  var objTLM;
  var objTimer;


  // Make json telemetry object
  function GetTelemetryObj() {
    var myJSON = { 
      "utc": 0,
      "soc": 0,
      "soh": 0,
      "speed": 0,
      "car_model": CAR_MODEL,
      "lat": 0,
      "lon": 0,
      "elevation": 0,
      "ext_temp": 0,
      "is_charging": 0,
      "batt_temp": 0,
      "voltage": 0,
      "current": 0,
      "power": 0
    };
    return myJSON;
  }

  // Fill json telemetry object
  function UpdateTelemetryObj(myJSON) {
    var read_bool = false;
   
    myJSON.lat = OvmsMetrics.AsFloat(["v.p.latitude"]).toFixed(3);
    myJSON.lon = OvmsMetrics.AsFloat(["v.p.longitude"]).toFixed(3);
    myJSON.elevation = OvmsMetrics.AsFloat(["v.p.altitude"]).toFixed();
    myJSON.power = OvmsMetrics.AsFloat(["v.b.power"]).toFixed(1);

    myJSON.soc = OvmsMetrics.AsFloat("v.b.soc");
    myJSON.soh = OvmsMetrics.AsFloat("v.b.soh");
    myJSON.speed = OvmsMetrics.AsFloat("v.p.speed");
    myJSON.batt_temp = OvmsMetrics.AsFloat("v.b.temp");
    myJSON.ext_temp = OvmsMetrics.AsFloat("v.e.temp");
    myJSON.voltage = OvmsMetrics.AsFloat("v.b.voltage");
    myJSON.current = OvmsMetrics.AsFloat("v.b.current");

    var d = new Date();
    myJSON.utc = Math.trunc(d.getTime()/1000);
    //myJSON.utc = OvmsMetrics.Value("m.time.utc");

    read_bool = Boolean(OvmsMetrics.Value("v.c.charging"));
    if (read_bool == true) {
      myJSON.is_charging = 1;
    } else {
      myJSON.is_charging = 0;
    }
    return true;
  }

  // Show available vehicle data
  function DisplayLiveData(myJSON) {
    var newcontent = "";
    newcontent += "altitude = " + myJSON.elevation + "m"  + CR;    //GPS altitude
    newcontent += "latitude = " + myJSON.lat       + "°"  + CR;    //GPS latitude
    newcontent += "longitude= " + myJSON.lon       + "°"  + CR;    //GPS longitude
    newcontent += "ext temp = " + myJSON.ext_temp  + "°C" + CR;    //Ambient temperature
    newcontent += "charge   = " + myJSON.soc       + "%"  + CR;    //State of charge
    newcontent += "health   = " + myJSON.soh       + "%"  + CR;    //State of health
    newcontent += "bat temp = " + myJSON.batt_temp + "°C" + CR;    //Main battery momentary temperature
    newcontent += "voltage  = " + myJSON.voltage   + "V"  + CR;    //Main battery momentary voltage
    newcontent += "current  = " + myJSON.current   + "A"  + CR;    //Main battery momentary current
    newcontent += "power    = " + myJSON.power     + "kW" + CR;    //Main battery momentary power
    newcontent += "charging = " + myJSON.is_charging + CR;         //yes = currently charging
    print(newcontent);
  }

  function InitObjTelemetry() {
    objTLM = GetTelemetryObj();
  }
  
  function UpdateObjTelemetry() {
    UpdateTelemetryObj(objTLM);
    DisplayLiveData(objTLM);
  }

  // http request callback if successful
  function OnRequestDone(resp) {
    print("response="+JSON.stringify(resp)+'\n');
  }

  // http request callback if failed
  function OnRequestFail(error) {
    print("error="+JSON.stringify(error)+'\n');
  }

  // Return full url with JSON telemetry object
  function GetUrlABRP() {
    var urljson = URL;
    urljson += "?";
    urljson += "api_key=" + OVMS_API_KEY;
    urljson += "&";
    urljson += "token=" + MY_TOKEN;
    urljson += "&";
    urljson += "tlm=" + encodeURIComponent(JSON.stringify(objTLM));
    print(urljson + CR);
    return urljson;
  }    

  // Return config object for HTTP request
  function GetURLcfg() {
    var cfg = { 
      url: GetUrlABRP(),
      done: function(resp) {OnRequestDone(resp)},
      fail: function(err)  {OnRequestFail(err)}
    };
    return cfg;
  }

  function SendLiveData() {
    UpdateObjTelemetry();
    HTTP.Request( GetURLcfg() );
  }

  //test purpose : one time execution
  function onetime() {
    InitObjTelemetry();
    SendLiveData();
  }

  // API method abrp.onetime():
  exports.onetime = function() {
    onetime();
  }

  // API method abrp.info():
  exports.info = function() {
    InitObjTelemetry();
    UpdateObjTelemetry();
  }

  // API method abrp.send():
  exports.send = function(onoff) {
    if (onoff) {
      onetime();
      objTimer = PubSub.subscribe("ticker.60", SendLiveData); // update each 60s
    } else {
      PubSub.unsubscribe(objTimer);
    }
  }
