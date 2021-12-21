============================
ChgThrottle: Protect Charger
============================

**Throttle charge current / stop charge if charger gets too hot**

Version 1.2 by Michael Balzer <dexter@dexters-web.de>

The plugin monitors the charger temperature. It can reduce the charge current or
stop a running charge process if the charger temperature exceeds on of three defined
thresholds.


------------
Installation
------------

1. Save :download:`chgthrottle.js` as ``/store/scripts/lib/chgthrottle.js``
2. Add line to ``/store/scripts/ovmsmain.js``:

  - ``chgthrottle = require("lib/chgthrottle");``

3. Issue ``script reload``


-------------
Configuration
-------------

.. code-block:: none

  Param   Instance                    Description                       Default
  usr     chgthrottle.enabled         yes = enable throttling           yes
  usr     chgthrottle.t1.temp         temperature threshold 1           50
  usr     chgthrottle.t1.amps         charge current at threshold 1     25
  usr     chgthrottle.t2.temp         threshold 2                       55
  usr     chgthrottle.t2.amps         current 2                         15
  usr     chgthrottle.t3.temp         threshold 3                       60
  usr     chgthrottle.t3.amps         current 3                         -1

Set t1…3 to ascending temperature thresholds. Below t1.temp, the current is set to unlimited.

Set temp to empty/0 to disable the level.

Set amps to -1 to stop the charge at that level.


-----
Usage
-----

Simply configure as desired, the plugin will monitor the charger temperature automatically
and react as configured.

Use ``config set usr chgthrottle.enabled no`` to disable the plugin, ``… yes`` to enable again.

State changes are logged and sent as notifications. Use ``info()`` to show the current state:

.. code-block:: none

  script eval chgthrottle.info()

Notifications are sent initially, then only on level-up during charge, and only on level-down
while not charging.
