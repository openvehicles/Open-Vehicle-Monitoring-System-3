==========================
Tasmota Smart Plug Control
==========================

**Smart Plug control for hardware running the Tasmota open source firmware**

Version 1.0 by Michael Balzer <dexter@dexters-web.de>

- `Tasmota open source firmware <https://tasmota.github.io/>`_
- `List of Tasmota based smart plugs <https://templates.blakadder.com/plug.html>`_

The plugin primarily aims at using the smart plug to control & automate starting
and/or stopping of the charge process, but of course can also be used to implement
remote power control for other devices/processes.

The smart plug can be bound to a defined location. Automatic periodic recharging or charge stop can
be configured via main battery SOC level and/or 12V battery voltage level. The plugin can also
switch off power at the charge stop event of the main and/or 12V battery.

------------
Installation
------------

1. Save :download:`tasmotasp.js` as ``/store/scripts/lib/tasmotasp.js``
2. Add line to ``/store/scripts/ovmsmain.js``:

  - ``tasmotasp = require("lib/tasmotasp");``

3. Issue ``script reload``
4. Install :download:`tasmotasp.htm` web plugin, recommended setup:

  - Type:    Page
  - Page:    ``/usr/tasmotasp``
  - Label:   Tasmota Smart Plug
  - Menu:    Config
  - Auth:    Cookie

5. Install :download:`tasmotasp-status.htm` web plugin, recommended setup:

  - Type:    Hook
  - Page:    ``/status``
  - Hook:    ``body.post``

The ``tasmotasp-status.htm`` plugin adds power control to the Status page's vehicle panel.

-------------
Configuration
-------------

Use the web frontend for simple configuration.

.. code-block:: none

  Param   Instance                Description
  usr     tasmotasp.ip               Tasmota IP address
  usr     tasmotasp.user             optional: username
  usr     tasmotasp.pass             optional: password
  usr     tasmotasp.socket           optional: output id (1-8, 0=all, empty=default)
  usr     tasmotasp.location         optional: restrict auto switch to this location
  usr     tasmotasp.soc_on           optional: switch on if SOC at/below
  usr     tasmotasp.soc_off          optional: switch off if SOC at/above
  usr     tasmotasp.chg_stop_off     optional: yes = switch off on vehicle.charge.stop
  usr     tasmotasp.aux_volt_on      optional: switch on if 12V level at/below
  usr     tasmotasp.aux_volt_off     optional: switch off if 12V level at/above
  usr     tasmotasp.aux_stop_off     optional: yes = switch off on vehicle.charge.12v.stop

-----
Usage
-----

.. code-block:: none

  script eval tasmotasp.get()
  script eval tasmotasp.set("on" | "off" | 0 | 1 | true | false)
  script eval tasmotasp.info()

Note: ``get()`` & ``set()`` do an async update (if the location matches), the result is logged.
Use ``info()`` to show the current state.

------
Events
------

.. code-block:: none

  usr.tasmotasp.on
  usr.tasmotasp.off
  usr.tasmotasp.error

