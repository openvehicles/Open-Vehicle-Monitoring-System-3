=========
Scripting
=========

---------------
Command Scripts
---------------

Lists of commands can be entered into a script file and stored in the VFS for execution (in the 
``/store/scripts`` directory). These are called ‘command scripts’ and are simple sequential lists of 
OVMS commands. A command script can be executed with::

  OVMS# . <script>
  OVMS# script run <script>

Command scripts can also be stored in the ``/store/events/<eventname>`` directory structure. 
Whenever events are triggered, all the scripts in the corresponding ``/store/events/<eventname>`` 
directory are executed. Event scripts are executed in alphanumerical order of their names. Good 
practice is to prefix script names with 2-3 digit numbers in steps of 10 or 100 (i.e. first script 
named ``50-…``), so new scripts can easily be integrated at a specific place.

Output of background scripts without console association (e.g. event scripts) will be sent to the 
log with tag ``script`` at "info" level.

Note that the developer building firmware can optionally set the ``OVMS_DEV_SDCARDSCRIPTS`` build 
flag. If that is set, then the system will also check ``/sd/scripts`` and ``/sd/events`` for 
scripts. This should not be used for production builds, as you could hack the system just by 
plugging in an SD card.

In addition to command scripts, more sophisticated scripting capabilities may be enabled if the 
JavaScript environment is enabled in the build. This is discussed in the next section of this guide.

-------------
JavaScripting
-------------

OVMS v3 includes a powerful JavaScript engine. In addition to the standard, relatively fixed, 
firmware flashed to the module, JavaScripting can be used to dynamically load script code to run 
alongside the standard firmware. This javascript code can respond to system events, and perform 
background monitoring and other such tasks.

The simplest way of running javascript is to place a piece of javascript code in the ``/store/scripts``
directory, with the file extension ``.js``. Then, the standard mechanism of running scripts can be 
employed::

  OVMS# . <script.js>
  OVMS# script run <script.js>

Short javascript snippets can also be directly evaluated with::

  OVMS# script eval <code>

Such javascript code can also be placed in the ``/store/events/<eventname>`` directories, to be 
automatically executed when the specified event is triggered. The script file name suffix must be 
``.js`` to run the Javascript interpreter.

.. note:: The scripting engine used is `Duktape <https://duktape.org/>`_. Duktape supports 
  `ECMAScript E5/E5.1 <http://www.ecma-international.org/ecma-262/5.1/>`_ with some additions from 
  later ECMAScript standards. Duktape does not emulate a browser environment, so you don't have window 
  or document objects etc., just core Javascript plus the OVMS API and plugins.
  
  Duktape builtin objects and functions: https://duktape.org/guide.html#duktapebuiltins

---------------------
Persistent JavaScript
---------------------

When a javascript script is executed, it is evaluated in the global javascript context. Care should 
be taken that local variables may pollute that context, so it is in general recommended that all 
JavaScript scripts are wrapped::

  (function(){
    … user code …
  })();

It is also possible to deliberately load functions, and other code, into the global context 
persistently, and have that code permanently available and running. When the JavaScript engine 
initialises, it automatically runs a special startup script::

  /store/scripts/ovmsmain.js

That script can in turn include other code. If you make a change to such persistent code, and want 
to reload it, you can with::

  OVMS# script reload

------------------
JavaScript Modules
------------------

The OVMS JavaScript engine supports the concept of modules (using the node.js style of exports). 
Such modules can be written like this:

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


----------------------------
Testing JavaScript / Modules
----------------------------

Use the **editor** (see Tools menu) to test or evaluate arbitrary Javascript code. This can be done
on the fly, i.e. without saving the code to a file first. Think of it as a server side Javascript
shell.

**Testing modules** normally involves reloading the engine, as the ``require()`` call caches all loaded 
modules until restart. To avoid this during module development, use the following template code.
This mimics the ``require()`` call without caching and allows to do tests within the same evaluation
run:

.. code-block:: javascript

  // Load module:
  mymodule = (function(){
    exports = {};
    
    // … insert module code here …
    
    return exports;
  })();
  
  // Module API tests:
  mymodule.myfunction1();
  JSON.print(mymodule.myfunction2());

As the module is actually loaded into the global context this way just like using ``require()``,
anything else using the module API (e.g. a web plugin) will also work after evaluation.


--------------------------------------
Internal Objects and Functions/Methods
--------------------------------------

A number of OVMS objects have been exposed to the JavaScript engine, and are available for use by custom
scripts via the global context.

The global context is the analog to the ``window`` object in a browser context, it can be referenced
explicitly as ``this`` on the JavaScript toplevel or as ``globalThis`` from any context.

You can see the global context objects, methods, functions and modules with the ``JSON.print(this)``
method::

  OVMS# script eval 'JSON.print(this)'
  {
    "performance": {
      "now": function now() { [native code] }
    },
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
      "AsJSON": function AsJSON() { [native code] },
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

- ``performance.now()``
    Returns monotonic time since boot in milliseconds, with microsecond resolution.


JSON
^^^^

The JSON module extends the native builtin ``JSON.stringify`` and ``JSON.parse`` methods by a 
``format`` and a ``print`` method, to format and/or print out a given javascript object in JSON 
format. Both by default insert spacing and indentation for readability and accept an optional 
``false`` as a second parameter to produce a compact version for transmission.

- ``JSON.print(data)``
    Output data (any Javascript data) as JSON, readable
- ``JSON.print(data, false)``
    …compact (without spacing/indentation)
- ``str = JSON.format(data)``
    Format data as JSON string, readable
- ``str = JSON.format(data, false)``
    …compact (without spacing/indentation)
- ``JSON.stringify(value[, replacer[, space]])``
    see `MDN JSON/stringify <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify>`_
- ``JSON.parse(text[, reviver])``
    see `MDN JSON/parse <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/parse>`_

.. note:: The ``JSON`` module is provided for compatibility with standard Javascript object dumps
  and for readability. If performance is an issue, consider using the Duktape native builtins
  ``JSON.stringify()`` / ``Duktape.enc()`` and ``JSON.parse()`` / ``Duktape.dec()`` (see Duktape 
  builtins and `Duktape JSON <https://github.com/svaarala/duktape/blob/master/doc/json.rst>`_
  for explanations of these).
  
  For example, ``Duktape.enc('JC', data)`` is equivalent to ``JSON.format(data, false)`` except for
  the representation of functions. Using the ``JX`` encoding will omit unnecessary quotings.


HTTP
^^^^

The HTTP API provides asynchronous GET & POST requests for HTTP and HTTPS. Requests can return 
text and binary data and follow 301/302 redirects automatically. Basic authentication is supported 
(add username & password to the URL), digest authentication is not yet implemented.

The handler automatically excludes the request objects from garbage collection until finished 
(success/failure), so you don't need to store a global reference to the request.

- ``req = HTTP.Request(cfg)``
    Perform asynchronous HTTP/HTTPS GET or POST request.

    Pass the request parameters using the ``cfg`` object:

    - ``url``: standard URL/URI syntax, optionally including user auth and query string
    - ``post``: optional POST data, set to an empty string to force a POST request. Note: you
      need to provide this in encoded form. If no ``Content-Type`` header is given, it will 
      default to ``x-www-form-urlencoded``.
    - ``headers``: optional array of objects containing key-value pairs of request headers.
      Note: ``User-Agent`` will be set to the standard OVMS user agent if not present here.
    - ``timeout``: optional timeout in milliseconds, default: 120 seconds.
    - ``binary``: optional flag: ``true`` = perform a binary request (see ``response`` object).
    - ``done``: optional success callback function, called with the ``response`` object as argument,
      with ``this`` pointing to the request object.
    - ``fail``: optional error callback function, called with the ``error`` string as argument,
      with ``this`` pointing to the request object.
    - ``always``: optional final callback function, no arguments, ``this`` = request object.

    The ``cfg`` object is extended and returned by the API (``req``). It will remain stable at 
    least until the request has finished and callbacks have been executed. On completion, the 
    ``req`` object may contain an updated ``url`` and a ``redirectCount`` if redirects have been 
    followed. Member ``error`` (also passed to the ``fail`` callback) will be set to the error 
    description if an error occurred. The ``always`` callback if present is called in any case,
    after a ``done`` or ``fail`` callback has been executed. Check ``this.error`` in the
    ``always`` callback to know if an error occurred.

    On success, member object ``response`` will be present and contain:

    - ``statusCode``: the numerical HTTP Status response code
    - ``statusText``: the HTTP Status response text
    - ``headers``: array of response headers, each represented by an object ``{ <name>: <value> }``
    - ``body``: only for text requests: response body as a standard string
    - ``data``: only for binary requests: response body as a Uint8Array

    Notes: any HTTP response from the server is considered success, check ``response.statusCode`` 
    for server specific errors. Callbacks are executed without an output channel, so all ``print`` 
    outputs will be written to the system log. Hint: use ``JSON.print(this, false)`` in the callback 
    to get a debug log dump of the request.

    **Examples**:

    .. code-block:: javascript
      
      // simple POST, ignore all results:
      HTTP.Request({ url: "http://smartplug.local/switch", post: "state=on&when=now" });
      
      // fetch and inspect a JSON object:
      HTTP.Request({
        url: "http://solarcontroller.local/status?fmt=json",
        done: function(resp) {
          if (resp.statusCode == 200) {
            var status = JSON.parse(resp.body);
            if (status["power"] > 5000)
              OvmsVehicle.StartCharge();
            else if (status["power"] < 3000)
              OvmsVehicle.StopCharge();
          }
        }
      });
      
      // override user agent, log completed request object:
      HTTP.Request({
        url: "https://dexters-web.de/f/test.json",
        headers: [{ "User-Agent": "Mr. What Zit Tooya" }],
        always: function() { JSON.print(this, false); }
      });

- ``HTTP.request()``
    Legacy alias for ``HTTP.Request()``, please do not use.


.. note::
  **SSL requests (https)** can take up to 12 seconds on an idle module.
  SSL errors also may not reflect the actual error, for example an empty server response
  with code 400 may be reported as a general "SSL error".
  If you get "SSL error" on a valid request, you may need to install a custom root CA
  certificate; see :doc:`ssltls`.


VFS
^^^

The VFS API provides asynchronous loading and saving of files on ``/store`` and ``/sd``.
Text and binary data is supported. Currently only complete files can be loaded, the saver
supports an append mode. In any case, the data to save/load needs to fit into RAM twice,
as the buffer needs to be converted to/from Javascript.

The handler automatically excludes the request objects from garbage collection until finished 
(success/failure), so you don't need to store a global reference to the request.

Loading or saving protected paths (``/store/ovms_config/…``) is not allowed. Saving to
a path automatically creates missing directories.

See :doc:`/plugin/auxbatmon/README` for a complete application usage example.

- ``req = VFS.Load(cfg)``
    Perform asynchronous file load.

    Pass the request parameters using the ``cfg`` object:

    - ``path``: full file path, e.g. ``/sd/mydata/telemetry.json``
    - ``binary``: optional flag: ``true`` = perform a binary request, returned ``data`` will
      be an Uint8Array)
    - ``done``: optional success callback function, called with the ``data`` content read as
      the single argument, ``this`` pointing to the request object
    - ``fail``: optional error callback function, called with the ``error`` string as argument,
      with ``this`` pointing to the request object
    - ``always``: optional final callback function, no arguments, ``this`` = request object

    The ``cfg`` object is extended and returned by the API (``req``). It will remain stable at 
    least until the request has finished and callbacks have been executed. On success, the 
    ``req`` object contains a ``data`` property (also passed to the ``done`` callback), which
    is either a string (text mode) or a Uint8Array (binary mode).
    
    Member ``error`` (also passed to the ``fail`` callback) will be set to the error 
    description if an error occurred. The ``always`` callback if present is called in any case,
    after a ``done`` or ``fail`` callback has been executed. Check ``this.error`` in the
    ``always`` callback to know if an error occurred.

    **Example**:

    .. code-block:: javascript
      
      // Load a custom telemetry object from a JSON file on SD card:
      var telemetry;
      VFS.Load({
        path: "/sd/mydata/telemetry.json",
        done: function(data) {
          telemetry = Duktape.dec('jx', data);
          // …process telemetry…
        },
        fail: function(error) {
          print("Error loading telemetry: " + error);
        }
      });

- ``req = VFS.Save(cfg)``
    Perform asynchronous file save.

    Pass the request parameters using the ``cfg`` object:

    - ``data``: the string or Uint8Array to save
    - ``path``: full file path (missing directories will automatically be created)
    - ``append``: optional flag: ``true`` = append to the end of the file (also creating the
      file as necessary)
    - ``done``: optional success callback function, called with no arguments, ``this`` pointing
      to the request object
    - ``fail``: optional error callback function, called with the ``error`` string as argument,
      with ``this`` pointing to the request object
    - ``always``: optional final callback function, no arguments, ``this`` = request object

    The ``cfg`` object is extended and returned by the API (``req``). It will remain stable at 
    least until the request has finished and callbacks have been executed.
    
    Member ``error`` (also passed to the ``fail`` callback) will be set to the error 
    description if an error occurred. The ``always`` callback if present is called in any case,
    after a ``done`` or ``fail`` callback has been executed. Check ``this.error`` in the
    ``always`` callback to know if an error occurred.

    **Example**:

    .. code-block:: javascript
      
      // Save the above telemetry object in JSON format on SD card:
      VFS.Save({
        path: "/sd/mydata/telemetry.json",
        data: Duktape.enc('jx', telemetry),
        fail: function(error) {
          print("Error saving telemetry: " + error);
        }
      });


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
- ``array = OvmsConfig.Instances(param)``
    Returns the list of instances for a specific parameter.
- ``string = OvmsConfig.Get(param,instance,default)``
    Returns the specified parameter/instance value.
- ``object = OvmsConfig.GetValues(param, [prefix])``
    Gets all param instances matching the optional prefix with their associated values.
    If a prefix is given, the returned property names will have the prefix removed.
    Note: all values are returned as strings, you need to convert them as needed.
- ``OvmsConfig.Set(param,instance,value)``
    Sets the specified parameter/instance value.
- ``OvmsConfig.SetValues(param, prefix, object)``
    Sets all properties of the given object as param instances after adding the prefix.
    Note: non-string property values will be converted to their string representation.
- ``OvmsConfig.Delete(param,instance)``
    Deletes the specified parameter instance.

Beginning with firmware release 3.2.009, a dedicated configuration parameter ``usr`` is provided
for plugins. You can add new config instances simply by setting them, for example by
``OvmsConfig.Set("usr", "myplugin.level", 123)`` or by the ``config set`` command.

Read plugin configuration example:

.. code-block:: javascript
  
  // Set default configuration:
  var cfg = { level: 100, enabled: "no" };
  
  // Read user configuration:
  Object.assign(cfg, OvmsConfig.GetValues("usr", "myplugin."));
  
  if (cfg["enabled"] == "yes") {
    print("I'm enabled at level " + Number(cfg["level"]));
  }

Keep in mind to prefix all newly introduced instances by a unique plugin name, so your plugin
can nicely coexist with others.


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

- ``str = OvmsMetrics.Value(metricname [,decode])``
    Returns the typed value (default) or string representation (with ``decode`` = false)
    of the metric value.
- ``num = OvmsMetrics.AsFloat(metricname)``
    Returns the float representation of the metric value.
- ``str = OvmsMetrics.AsJSON(metricname)``
    Returns the JSON representation of the metric value.
- ``obj = OvmsMetrics.GetValues([filter] [,decode])``
    Returns an object of all metrics matching the optional name filter/template (see below),
    by default decoded into Javascript types (i.e. numerical values will be JS numbers, arrays
    will be JS arrays etc.). The object returned is a snapshot, the values won't be updated.
    
    The ``filter`` argument may be a string (for substring matching as with ``metrics list``),
    an array of full metric names, or an object of which the property names are used as
    the metric names to get. The object won't be changed by the call, see ``Object.assign()``
    for a simple way to merge objects. Passing an object is especially convenient if you
    already have an object to collect metrics data.
    
    The ``decode`` argument defaults to ``true``, pass ``false`` to retrieve the metrics
    string representations instead of typed values.

With the introduction of the ``OvmsMetrics.GetValues()`` call, you can get multiple metrics
at once and let the system decode them for you. Using this you can for example do:

.. code-block:: javascript

  // Get all metrics matching substring "v.b.c." (vehicle battery cell):
  var metrics = OvmsMetrics.GetValues("v.b.c.");
  print("Temperature of cell 3: " + metrics["v.b.c.temp"][2] + " °C\n");
  print("Voltage of cell 7: " + metrics["v.b.c.voltage"][6] + " V\n");
  
  // Get some specific metrics:
  var ovmsinfo = OvmsMetrics.GetValues(["m.version", "m.hardware"]);
  JSON.print(ovmsinfo);

This obsoletes the old pattern of parsing a metric's JSON representation using ``eval()``, 
``JSON.parse()`` or ``Duktape.dec()`` you may still find in some plugins. Example:

.. code-block:: javascript

  var celltemps = eval(OvmsMetrics.AsJSON("v.b.c.temp"));
  print("Temperature of cell 3: " + celltemps[2] + " °C\n");

.. warning::
  **Never use** ``eval()`` **on unsafe data, e.g. user input!**
  ``eval()`` executes arbitrary Javascript, so can be exploited for code injection attacks.


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


--------------
Test Utilities
--------------

You can use the web UI editor and shell to edit, upload and test script files. If you need many
test cycles, a convenient alternative is to use shell scripts to automate the process.

If you've configured ssh public key authentication, you can simply use ``scp`` to upload scripts
and ``ssh`` to execute commands:

.. code-block:: bash

  #!/bin/bash
  # Upload & execute a script file:

  FILE="test.js"
  PATH="/store/scripts/"

  OVMS_HOST="yourovms.local"

  SCP="/usr/bin/scp -q"
  SSH="/usr/bin/ssh"

  # Upload:
  $SCP "${FILE}" "${OVMS_HOST}:${PATH}${FILE}"

  # Execute:
  $SSH "${OVMS_HOST}" "script run ${FILE}"

Customize to your needs. If you want to test a plugin, simply replace the ``script run``
command by ``script reload`` followed by some ``script eval`` calls to your plugin API.

Note: this may be slow, as the ``ssh`` session needs to be negotiated for every command.

A faster option is using the OVMS HTTP REST API. The following script uses ``curl`` to upload
and execute a script:

.. code-block:: bash

  #!/bin/bash
  # Upload & execute a script file:

  FILE="test.js"
  PATH="/store/scripts/"

  OVMS_HOST="http://yourovms.local"
  OVMS_USER="admin"
  OVMS_PASS="yourpassword"

  CURL="/usr/bin/curl -c .auth -b .auth"
  SED="/usr/bin/sed"
  DATE="/usr/bin/date"

  # Login?
  if [[ -e ".auth" ]] ; then
    AUTHAGE=$(($($DATE +%s) - $($DATE +%s -r ".auth")))
  else
    AUTHAGE=3600
  fi
  if [[ "$AUTHAGE" -ge 3600 ]] ; then
    RES=$($CURL "${OVMS_HOST}/login" --data-urlencode "username=${OVMS_USER}" --data-urlencode "password=${OVMS_PASS}" 2>/dev/null)
    if [[ "$RES" =~ "Error" ]] ; then
      echo -n "LOGIN ERROR: "
      echo $RES | $SED -e 's:.*<li>\([^<]*\).*:\1:g'
      rm .auth
      exit 1
    fi
  fi

  # Upload:
  RES=$($CURL "${OVMS_HOST}/edit" --data-urlencode "path=${PATH}${FILE}" --data-urlencode "content@${FILE}" 2>/dev/null)
  if [[ "$RES" =~ "Error" ]] ; then
    echo -n "UPLOAD ERROR: "
    echo $RES | $SED -e 's:.*<li>\([^<]*\).*:\1:g'
    rm .auth
    exit 1
  fi

  # Execute:
  $CURL "${OVMS_HOST}/api/execute" --data-urlencode "command=script run ${FILE}"

