============================
AuxBatMon: 12V History Chart
============================

**Web chart showing last 24 hours of 12V battery metrics (similar to Android App)**

Version 2.3 by Michael Balzer <dexter@dexters-web.de>

.. image:: auxbatmon-screenshot.png

The module plugin records the relevant 12V battery metrics (voltage, reference voltage,
current, charger and environment temperatures) once per minute for up to 24 hours by
default.

The web plugin renders these recordings into a time series chart and continues feeding
live updates into the chart. The chart is zoomable and can be panned along the X axis.

Since version 2.0, metrics history data is stored in a file and restored automatically
on reboot/reload. This needs OVMS firmware >= 3.2.008-235 to work (will fallback to
no saving on earlier versions).

**Version 2.3** changes the file and transfer encoding from JSON to CBOR. Upgrading is
strongly recommended, as this has a substantial performance gain for both the module
(scripting engine) and the web user interface (loading the chart).


------------
Installation
------------

1. Save :download:`auxbatmon.js` as ``/store/scripts/lib/auxbatmon.js``
2. Add line to ``/store/scripts/ovmsmain.js``:

  - ``auxbatmon = require("lib/auxbatmon");``

3. Issue ``script reload`` or evaluate the ``require`` line
4. Install :download:`auxbatmon.htm` web plugin, recommended setup:

  - Type:    Page
  - Page:    ``/usr/auxbatmon``
  - Label:   12V History
  - Menu:    Tools
  - Auth:    Cookie


-------------
Configuration
-------------

No live config currently. You can customize the sample interval (default: 60 seconds)
and the history size (default: 24 hours) by changing the constants in the code.
Take care to match a customization in both the module and the web plugin.

The persistent history storage file is ``/store/usr/auxbatmon.cbor`` by default.
It needs ~30 kB with the default configuration. If space is tight on your ``/store``
partition you can change the file location to ``/sd/…`` in the source code.


----------
Plugin API
----------

.. code-block:: none

  auxbatmon.dump('CBOR')    -- dump recorded history data in CBOR format (binary)
  auxbatmon.dump()          -- dump recorded history data in JSON format
  auxbatmon.dump()          -- get a copy of the history data object

