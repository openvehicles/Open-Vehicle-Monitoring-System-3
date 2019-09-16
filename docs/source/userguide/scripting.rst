=========
Scripting
=========

---------------
Command Scripts
---------------

Lists of commands can be entered into a script file and stored in the VFS for execution (in the /store/scripts directory). These are called ‘command scripts’ and are simple sequential lists of OVMS commands. A command script can be executed with::

  OVMS# . <script>
  OVMS# script run <script>

Command scripts can also be stored in the /store/events/<eventname> directory structure. Whenever events are triggered, all the scripts in the corresponding /store/events/<eventname> directory are executed.

Note that the developer building firmware can optionally set the OVMS_DEV_SDCARDSCRIPTS build flag. If that is set, then the system will also check /sd/scripts and /sd/events for scripts.

In addition to command scripts, more sophisticated scripting capabilities may be enabled if the JavaScript environment is enabled in the build. This is discussed in the next section of this guide.

-------------
JavaScripting
-------------

OVMS v3 includes a powerful JavaScript engine. In addition to the standard, relatively fixed, firmware flashed to the module, JavaScripting can be used to dynamically load script code to run alongside the standard firmware. This javascript code can respond to system events, and perform background monitoring and other such tasks.

The simplest way of running javascript is to place a piece of javascript code in the /store/scripts directory, with the file extension ‘.js’. Then, the standard mechanism of running scripts can be employed::

  OVMS# . <script.js>
  OVMS# script run <script.js>

Short javascript snippets can also be directly evaluated with::

  OVMS# script eval <code>

Such javascript code can also be placed in the /store/events/<eventname> directories, to be automatically executed when the specified event is triggered.

---------------------
Persistent JavaScript
---------------------

When a javascript script is executed, it is evaluated in the global javascript context. Care should be taken that local variables may pollute that context, so it is in general recommended that all JavaScript scripts are wrapped::

  (function(){
    … user code …
  })();

It is also possible to deliberately load functions, and other code, into the global context persistently, and have that code permanently available and running. When the JavaScript engine initialises, it automatically runs a special startup script::

  /store/script/ovmsmain.js

That script can in turn include other code. If you make a change to such persistent code, and want to reload it, you can with::

  OVMS# script reload

------------------
JavaScript Modules
------------------

The OVMS JavaScript engine supports the concept of modules (using the node.js style of exports). Such modules can be written like this::

  exports.print = function(obj, ind) {
    var type = typeof obj;
    if (type == "object" && Array.isArray(obj)) type = "array";
    if (!ind) ind = '';

    switch (type) {
      case "string":
        print('"' + obj.replace(/\"/g, '\\\"') + '"');
        break;
      case "array":
        print('[\n');
        for (var i = 0; i < obj.length; i++) {
          print(ind + '  ');
          exports.print(obj[i], ind + '  ');
          if (i != obj.length-1) print(',');
          print('\n');
        }
        print(ind + ']');
        break;
      case "object":
        print('{\n');
        var keys = Object.keys(obj);
        for (var i = 0; i < keys.length; i++) {
          print(ind + '  "' + keys[i] + '": ');
          exports.print(obj[keys[i]], ind + '  ');
          if (i != keys.length-1) print(',');
          print('\n');
        }
        print(ind + '}');
        break;
      default:
        print(obj);
    }

    if (ind == '') print('\n');
  }

By convention, modules such as this are placed in the /store/scripts/lib directory as <modulename>.js. These modules can be loaded with::

  JSON = require("lib/JSON");

And used as::

  JSON.print(this);

There are a number of internal modules already provided with the firmware, and by convention these are provided under the int/<modulename> namespace. The above JSON module is, for example, provided as int/JSON and automatically loaded into the global context. These internal modules can be directly used (so JSON.print(this) work directly).
Internal Modules

The JSON module is provided with a single ‘print’ method, to print out a given javascript object in JSON format.

The PubSub module is provided to provide access to a Publish-Subscribe framework. In particular, this framework is used to deliver events to the persistent JavaScript framework in a high performance flexible manner. An example script to print out the ticker.10 event is::

  var myTicker=function(msg,data){ print("Event: "+msg+"\n"); };

  PubSub.subscribe("ticker.10",myTicker);

The above example created a function MyTicker in global context, to print out the provided event name. Then, the PubSub.subscribe module method is used to subscribe to the ticker.10 event and have it call myTicker every ten seconds. The result is ‘Event: ticker.10’ printed once every ten seconds.

--------------------------------------
Internal Objects and Functions/Methods
--------------------------------------

A number of OVMS objects have been exposed to the JavaScript engine, and are available for use by custom scripts. These include:

``assert(condition,message)``

Assert that the given condition is true. If not, raise a JavaScript exception error with the given message.

``print(string)``

 Print the given string on the current terminal. If no terminal (for example a background script) then print to the system console as an informational message.

``OvmsCommand``

The OvmsCommand object exposes one method “Exec”. This method is passed a single parameter as the command to be executed, runs that command, and then returns the textual output of the command as a string. For example::

  print(OvmsCommand.Exec(“boot status”));
  Last boot was 14 second(s) ago
    This is reset #0 since last power cycle
    Detected boot reason: PowerOn (1/14)
    Crash counters: 0 total, 0 early

``OvmsLocation``

The OvmsLocation object exposes one method “Status”. This method is passed a single parameter as the location name. It returns true if the vehicle is currently in that location’s geofence, false if not, or undefined if the location name passed is not valid.

``OvmsMetrics``

The OvmsMetrics object exposes the Value() and AsFloat() methods. Both are passed a single string as the metric name to lookup, and return the metric value as strings or floats appropriately.

``OvmsVehicle``

The OvmsVehicle object is the most comprehensive, and exposes several methods to access the current vehicle. These include:

* Type() to return the type of the currently loaded vehicle module
* Wakeup() to wakeup the vehicle (return TRUE if successful)
* Homelink(button,durationms) to fire the given homelink button
* ClimateControl(onoff) to turn on/off climate control
* Lock(pin) to lock the vehicle
* Unlock(pin) to unlock the vehicle
* Valet(pin) to activate valet mode
* Unvalet(pin) to deactivate valet mode
* SetChargeMode(mode) to set the charge mode
* SetChargeCurrent(limit) to set the charge current limit
* SetChargeTimer(onoff, start) to set the charge timer
* StartCharge() to start the charge
* StopCharge() to stop the charge
* StartCooldown() to start a cooldown charge
* StopCooldown() to stop the cooldown charge

You can see the global context objects, methods, functions, and modules with the JSON.print(this) method::

  OVMS# script eval 'JSON.print(this)'
  {
    "assert": function () { [native code] },
    "print": function () { [native code] },
    "OvmsCommand": {
      "Exec": function Exec() { [native code] }
    },
    "OvmsLocation": {
      "Status": function Status() { [native code] }
    },
    "OvmsMetrics": {
      "AsFloat": function AsFloat() { [native code] },
      "Value": function Value() { [native code] }
    },
    "OvmsVehicle": {
      "ClimateControl": function ClimateControl() { [native code] },
      "Homelink": function Homelink() { [native code] },
      "Lock": function Lock() { [native code] },
      "SetChargeCurrent": function SetChargeCurrent() { [native code] },
      "SetChargeMode": function SetChargeMode() { [native code] },
      "SetChargeTimer": function SetChargeTimer() { [native code] },
      "StartCharge": function StartCharge() { [native code] },
      "StartCooldown": function StartCooldown() { [native code] },
      "StopCharge": function StopCharge() { [native code] },
      "StopCooldown": function StopCooldown() { [native code] },
      "Type": function Type() { [native code] },
      "Unlock": function Unlock() { [native code] },
      "Unvalet": function Unvalet() { [native code] },
      "Valet": function Valet() { [native code] },
      "Wakeup": function Wakeup() { [native code] }
    },
    "JSON": {
      "print": function () { [ecmascript code] }
    },
    "PubSub": {
      "publish": function () { [ecmascript code] },
      "subscribe": function () { [ecmascript code] },
      "clearAllSubscriptions": function () { [ecmascript code] },
      "clearSubscriptions": function () { [ecmascript code] },
      "unsubscribe": function () { [ecmascript code] }
    }
  }
