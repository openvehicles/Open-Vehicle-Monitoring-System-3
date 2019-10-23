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

The OVMS JavaScript engine supports the concept of modules (using the node.js style of exports). Such modules can be written like this:

.. code-block:: javascript

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

By convention, modules such as this are placed in the ``/store/scripts/lib`` directory as ``<modulename>.js``.
These modules can be loaded with:

.. code-block:: javascript

  JSON = require("lib/JSON");

And used as:

.. code-block:: javascript

  JSON.print(this);

To automatically load a custom module on startup, add the ``MyPlugin = require("lib/MyPlugin");`` line to ``ovmsmain.js``.

There are a number of **internal modules** already provided with the firmware, and by convention these are
provided under the ``int/<modulename>`` namespace. The above JSON module is, for example, provided as
``int/JSON`` and automatically loaded into the global context. These internal modules can be directly used (so
``JSON.print(this)`` works directly).


--------------------------------------
Internal Objects and Functions/Methods
--------------------------------------

A number of OVMS objects have been exposed to the JavaScript engine, and are available for use by custom
scripts via the global context.

The global context is the analog to the ``window`` object in a browser context, it can be referenced
explicitly as ``this`` on the JavaScript toplevel.

You can see the global context objects, methods, functions and modules with the ``JSON.print(this)``
method::

  OVMS# script eval 'JSON.print(this)'
    {
    "assert": function () { [native code] },
    "print": function () { [native code] },
    "OvmsCommand": {
      "Exec": function Exec() { [native code] }
    },
    "OvmsConfig": {
      "Delete": function Delete() { [native code] },
      "Get": function Get() { [native code] },
      "Instances": function Instances() { [native code] },
      "Params": function Params() { [native code] },
      "Set": function Set() { [native code] }
    },
    "OvmsEvents": {
      "Raise": function Raise() { [native code] }
    },
    "OvmsLocation": {
      "Status": function Status() { [native code] }
    },
    "OvmsMetrics": {
      "AsFloat": function AsFloat() { [native code] },
      "Value": function Value() { [native code] }
    },
    "OvmsNotify": {
      "Raise": function Raise() { [native code] }
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
      "format": function () { [ecmascript code] },
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


Global Context
^^^^^^^^^^^^^^

- ``assert(condition,message)``
    Assert that the given condition is true. If not, raise a JavaScript exception error with the given message.

- ``print(string)``
    Print the given string on the current terminal. If no terminal (for example a background script) then
    print to the system console as an informational message.


JSON
^^^^

The JSON module is provided with a ``format`` and a ``print`` method, to format and/or print out a given
javascript object in JSON format. Both by default insert spacing and indentation for readability and accept an
optional ``false`` as a second parameter to produce a compact version for transmission.

- ``JSON.print(data)``
    Output data (any Javascript data) as JSON, readable
- ``JSON.print(data, false)``
    …compact (without spacing/indentation)
- ``str = JSON.format(data)``
    Format data as JSON string, readable
- ``str = JSON.format(data, false)``
    …compact (without spacing/indentation)


PubSub
^^^^^^

The PubSub module provides access to a Publish-Subscribe framework. In particular, this framework is used to
deliver events to the persistent JavaScript framework in a high performance flexible manner. An example script
to print out the ticker.10 event is:

.. code-block:: javascript

  var myTicker=function(msg,data){ print("Event: "+msg+"\n"); };

  PubSub.subscribe("ticker.10",myTicker);

The above example created a function ``myTicker`` in global context, to print out the provided event name.
Then, the ``PubSub.subscribe`` module method is used to subscribe to the ``ticker.10`` event and have it call
``myTicker`` every ten seconds. The result is "Event: ticker.10" printed once every ten seconds.

- ``id = PubSub.subscribe(topic, handler)``
    Subscribe the function ``handler`` to messages of the given topic. Note that types are not limited to
    OVMS events. The method returns an ``id`` to be used to unsubscribe the handler.
- ``PubSub.publish(topic, [data])``
    Publish a message of the given topic. All subscribed handlers will be called with the topic and data as
    arguments. ``data`` can be any Javascript data.
- ``PubSub.unsubscribe(id | handler | topic)``
    Cancel a specific subscription, all subscriptions of a specific handler or all subscriptions
    to a topic.


OvmsCommand
^^^^^^^^^^^

- ``str = OvmsCommand.Exec(command)``
    The OvmsCommand object exposes one method “Exec”. This method is passed a single parameter as the command
    to be executed, runs that command, and then returns the textual output of the command as a string. For
    example::

      print(OvmsCommand.Exec("boot status"));
      Last boot was 14 second(s) ago
        This is reset #0 since last power cycle
        Detected boot reason: PowerOn (1/14)
        Crash counters: 0 total, 0 early

OvmsConfig
^^^^^^^^^^

- ``array = OvmsConfig.Params()``
    Returns the list of available configuration parameters.
- ``array = OvmsMetrics.Instances(param)``
    Returns the list of instances for a specific parameter.
- ``string = OvmsMetrics.Get(param,instance,default)``
    Returns the specified parameter/instance value.
- ``OvmsMetrics.Set(param,instance,value)``
    Sets the specified parameter/instance value.
- ``OvmsMetrics.Delete(param,instance)``
    Deletes the specified parameter instance.

OvmsEvents
^^^^^^^^^^

This provides access to the OVMS event system. While you may raise system events, the primary use is to raise
custom events. Sending custom events is a lightweight method to inform the web UI (or other plugins) about
simple state changes. Use the prefix ``usr.`` on custom event names to prevent conflicts with later framework
additions.

Another use is the emulation of the ``setTimeout()`` and ``setInterval()`` browser methods by subscribing to a
delayed event. Pattern:

.. code-block:: javascript

  function myTimeoutHandler() {
    // raise the timeout event again here to emulate setInterval()
  }
  PubSub.subscribe('usr.myplugin.timeout', myTimeoutHandler);

  // start timeout:
  OvmsEvents.Raise('usr.myplugin.timeout', 1500);

- ``OvmsEvents.Raise(event, [delay_ms])``
    Signal the event, optionally with a delay (milliseconds, must be given as a number).
    Delays are handled by the event system, the method call returns immediately.


OvmsLocation
^^^^^^^^^^^^

- ``isatlocation = OvmsLocation.Status(location)``
    Check if the vehicle is currently in a location's geofence (pass the location name as defined).
    Returns ``true`` or ``false``, or ``undefined`` if the location name passed is not valid.

Note: to get the actual GPS coordinates, simply read metrics ``v.p.latitude``, ``v.p.longitude`` and
``v.p.altitude``.


OvmsMetrics
^^^^^^^^^^^

- ``str = OvmsMetrics.Value(metricname)``
    Returns the string representation of the metric value.
- ``num = OvmsMetrics.AsFloat(metricname)``
    Returns the float representation of the metric value.


OvmsNotify
^^^^^^^^^^

- ``id = OvmsNotify.Raise(type, subtype, message)``
    Send a notification of the given type and subtype with message as contents.
    Returns the message id allocated or 0 in case of failure.
    Examples:

    .. code-block:: javascript

      // send an info notification to the user:
      OvmsNotify.Raise("info", "usr.myplugin.status", "Alive and kicking!");

      // send a JSON stream to a web plugin:
      OvmsNotify.Raise("stream", "usr.myplugin.update", JSON.format(streamdata, false));

      // send a CSV data record to a server:
      OvmsNotify.Raise("data", "usr.myplugin.record", "*-MyStatus,0,86400,Alive");


OvmsVehicle
^^^^^^^^^^^

The OvmsVehicle object is the most comprehensive, and exposes several methods to access the current vehicle. These include:

- ``str = OvmsVehicle.Type()``
    Return the type of the currently loaded vehicle module
- ``success = OvmsVehicle.Wakeup()``
    Wakeup the vehicle (return TRUE if successful)
- ``success = OvmsVehicle.Homelink(button,durationms)``
    Fire the given homelink button
- ``success = OvmsVehicle.ClimateControl(onoff)``
    Turn on/off climate control
- ``success = OvmsVehicle.Lock(pin)``
    Lock the vehicle
- ``success = OvmsVehicle.Unlock(pin)``
    Unlock the vehicle
- ``success = OvmsVehicle.Valet(pin)``
    Activate valet mode
- ``success = OvmsVehicle.Unvalet(pin)``
    Deactivate valet mode
- ``success = OvmsVehicle.SetChargeMode(mode)``
    Set the charge mode ("standard" / "storage" / "range" / "performance")
- ``success = OvmsVehicle.SetChargeCurrent(limit)``
    Set the charge current limit (in amps)
- ``success = OvmsVehicle.SetChargeTimer(onoff, start)``
    Set the charge timer
- ``success = OvmsVehicle.StartCharge()``
    Start the charge
- ``success = OvmsVehicle.StopCharge()``
    Stop the charge
- ``success = OvmsVehicle.StartCooldown()``
    Start a cooldown charge
- ``success = OvmsVehicle.StopCooldown()``
    Stop the cooldown charge
