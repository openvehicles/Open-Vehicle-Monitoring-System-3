========================
PwrMon: Trip Power Chart
========================

**Web chart showing average speed, power & energy use on the road**

Version 1.0 by Michael Balzer <dexter@dexters-web.de>

.. image:: pwrmon-screenshot.png

The plugin calculates average speed (kph), power (kW) & energy use (Wpk = Wh/km) from the
odometer, altitude and energy counters (metrics ``v.b.energy.used`` & ``v.b.energy.recd``),
so the vehicle needs to provide these with reasonable precision & resolution.

The optional module plugin continuously records and stores the metrics history, so the
chart can load the last 20 km (by default) when opened. The chart will then continue to
add live metrics data. Without the module plugin, the chart will only display live data.

The chart is zoomable and can be panned along the X axis. The four data series can be
selected and shown/hidden as usual by clicking on the series names in the legend, click
into the chart to highlight a specific data point.

The live data may be more accurate than the data stored by the module plugin, as it
will react directly to the odometer change, while the module plugin can only check
the odometer once per second.


------------
Installation
------------

1. Install :download:`pwrmon.htm` web plugin, recommended setup:

  - Type:    Page
  - Page:    ``/usr/pwrmon``
  - Label:   Trip Power Chart
  - Menu:    Tools
  - Auth:    Cookie

2. Optionally:

  - Save :download:`pwrmon.js` as ``/store/scripts/lib/pwrmon.js``
  - Add line to ``/store/scripts/ovmsmain.js``:

    - ``pwrmon = require("lib/pwrmon");``

  - Issue ``script reload`` or evaluate the ``require`` line


-------------
Configuration
-------------

No live config currently. You can customize the sample interval (default: 0.3 km)
and the history size (default: 20 km) by changing the constants in the code. The
storage file and interval (1 km) can also be changed.

On the web plugin, you can customize the live sample interval (default 0.3 km, doesn't
need to match the module plugin) and the initial zoom (default: 8 km).

The persistent history storage file is ``/store/usr/pwrmon.jx``. It needs ~9 kB with
the default sample configuration. If space is tight on your ``/store`` partition you can
change the file location to ``/sd/…`` in the source code or disable the file by setting
an empty string as the filename.


----------
Plugin API
----------

.. code-block:: none

  pwrmon.dump()        -- dump (print) recorded history data in JSON format
  pwrmon.data()        -- get a copy of the history data object

