==============================
ChgInd: Charge State Indicator
==============================

**Control three LEDs to indicate charging & SOC level**

Version 1.0 by Michael Balzer <dexter@dexters-web.de>

Following an idea by "green_fox" on the german Twizy forum (not limited to the Twizy) to
implement an externally visible charge indicator.

Use a red, a green and a blue LED to signal charge state:

- OFF = not charging
- RED = charging, SOC below 20%
- YELLOW (RED+GREEN) = charging, SOC 20% … 80%
- GREEN = charging, SOC above 80%
- BLUE = fully charged (held for 15 minutes)

This plugin uses three EGPIO lines to control the three LEDs (KISS). By using a shift register
this could be reduced to two lines, by using a serial protocol to one line. The latter would
need protocol support in the EGPIO (todo).


------------
Installation
------------

1. Save :download:`chgind.js` as ``/store/scripts/lib/chgind.js``
2. Add line to ``/store/scripts/ovmsmain.js``:

    - ``chgind = require("lib/chgind");``

3. Issue ``script reload`` or evaluate the ``require`` line


-------------
Configuration
-------------

.. code-block:: none

  // EGPIO ports to use:
  const LED_red   = 4;  // EIO_3
  const LED_green = 5;  // EIO_4
  const LED_blue  = 6;  // EIO_5

  // Time to hold BLUE state after charge stop:
  const holdTimeMinutes = 15;


----------
Plugin API
----------

.. code-block:: none

  chgind.set(state)   -- set LED state ('off'/'red'/'yellow'/'green'/'blue')

