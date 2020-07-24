===================
eDriver BMS Monitor
===================

**Status monitor for the Twizy custom eDriver BMS by Pascal Ripp**

Version 1.0 by Michael Balzer <dexter@dexters-web.de>

.. image:: edrvmon-screenshot.png

This is a simple web page plugin to display the standard battery cell info along
with the additional custom data provided by the eDriver BMS on one page.

Additional custom data:

  - Main state
  - Error state
  - BMS internal temperature
  - AUX relay state
  - Cell balancing states

Note: these are read from their respective custom metrics ``xrt.bms.…``,
see :doc:`/components/vehicle_renaulttwizy/docs/metrics`.

Overall SOC, voltage, current and coulomb/energy counters are also shown to simplify
battery capacity and current sensor calibration.

Check out the eDriver BMS Manual for details or contact Pascal in case of questions.


------------
Installation
------------

1. Install :download:`edrvmon.htm` web plugin, recommended setup:

  - Type:    Page
  - Page:    ``/usr/edrvmon``
  - Label:   eDriver BMS Monitor
  - Menu:    Vehicle
  - Auth:    Cookie

