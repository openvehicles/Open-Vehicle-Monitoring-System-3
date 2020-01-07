============================
AuxBatMon: 12V History Chart
============================

**Web chart showing last 24 hours of 12V battery metrics (similar to Android App)**

Version 1.0 by Michael Balzer <dexter@dexters-web.de>

The module plugin records the relevant 12V battery metrics (voltage, reference voltage,
current, charger and environment temperatures) once per minute for up to 24 hours by
default.

The web plugin renders these recordings into a time series chart and continues feeding
live updates into the chart. The chart is zoomable and can be panned along the X axis.

Note: metrics history data is currently recorded in RAM, i.e. you won't see any history
immediately after installation, and you will lose the history over reboot or ``script reload``.


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


-----
Usage
-----

.. code-block:: none

  script eval auxbatmon.dump()        -- dump recorded history data in JSON format

