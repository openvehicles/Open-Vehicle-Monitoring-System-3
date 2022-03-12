==========================
Edimax: Smart Plug Control
==========================

**Smart Plug control for Edimax models SP-1101W, SP-2101W et al**

Version 2.0 by Michael Balzer <dexter@dexters-web.de>

Note: may need ``HTTP.request()`` digest auth support to work with newer Edimax firmware (untested)

The smart plug can be bound to a defined location. Automatic periodic recharging or charge stop can
be configured via main battery SOC level and/or 12V battery voltage level. The plugin can also
switch off power at the charge stop event of the main and/or 12V battery.

------------
Installation
------------

1. Save :download:`edimax.js` as ``/store/scripts/lib/edimax.js``
2. Add line to ``/store/scripts/ovmsmain.js``:

  - ``edimax = require("lib/edimax");``

3. Issue ``script reload``
4. Install :download:`edimax.htm` web plugin, recommended setup:

  - Type:    Page
  - Page:    ``/usr/edimax``
  - Label:   Edimax Smart Plug
  - Menu:    Config
  - Auth:    Cookie

5. Install :download:`edimax-status.htm` web plugin, recommended setup:

  - Type:    Hook
  - Page:    ``/status``
  - Hook:    ``body.post``

The ``edimax-status.htm`` plugin adds power control to the Status page's vehicle panel.

-------------
Configuration
-------------

Use the web frontend for simple configuration.

.. code-block:: none

  Param   Instance                Description
  usr     edimax.ip               Edimax IP address
  usr     edimax.user             … username, "admin" by default
  usr     edimax.pass             … password, "1234" by default
  usr     edimax.location         optional: restrict auto switch to this location
  usr     edimax.soc_on           optional: switch on if SOC at/below
  usr     edimax.soc_off          optional: switch off if SOC at/above
  usr     edimax.chg_stop_off     optional: yes = switch off on vehicle.charge.stop
  usr     edimax.aux_volt_on      optional: switch on if 12V level at/below
  usr     edimax.aux_volt_off     optional: switch off if 12V level at/above
  usr     edimax.aux_stop_off     optional: yes = switch off on vehicle.charge.12v.stop

-----
Usage
-----

.. code-block:: none

  script eval edimax.get()
  script eval edimax.set("on" | "off")
  script eval edimax.info()

Note: ``get()`` & ``set()`` do an async update (if the location matches), the result is logged.
Use ``info()`` to show the current state.

------
Events
------

.. code-block:: none

  usr.edimax.on
  usr.edimax.off
  usr.edimax.error

