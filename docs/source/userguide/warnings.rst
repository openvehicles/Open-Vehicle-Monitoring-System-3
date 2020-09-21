================
General Warnings
================

.. image:: warning.png
  :width: 100px
  :align: left

| **Warning!**
| OVMS is a hobbyist project, not a commercial product. It was designed by enthusiasts for enthusiasts. Installation and use of this module requires some technical knowledge, and if you don't have that we recommend you contact other users in your area to ask for assistance.

.. image:: warning.png
  :width: 100px
  :align: left
  
| **Warning!**
| The OVMS module is continuously powered by the car, even when the car is off.
  While the OVMS module uses extremely low power, it does continuously draw power from the
  car's battery, so it will contribute to 'vampire' power drains.

Do not allow your car battery to reach 0% SOC, and if it does, plug in and charge the car
immediately. **Failure to do this can result in unrecoverable failure of the car's battery.**

The module can monitor the main and 12V battery and send alert notifications if the SOC or
voltage drops below a healthy level.


-------------------
Average Power Usage
-------------------

The power used by the module depends on the component activation. You can save power by
disabling unused components. This can be done automatically by the Power Management module
to avoid deep discharging the 12V battery, or you can use scripts to automate switching
components off and on.

The base components need approximately these power levels continuously while powered on:

================ ========== ============
Component         Avg Power  12V Current
================ ========== ============
Base System          200 mW        17 mA
Wifi                 330 mW        28 mA
Modem                170 mW        13 mA
GPS                  230 mW        19 mA
**Total**        **930 mW**    **78 mA**
================ ========== ============

This adds up to:

  - ~  22 Wh  or   2 Ah  / day
  - ~ 156 Wh  or  13 Ah  / week
  - ~ 680 Wh  or  57 Ah  / month

Note that depending on the vehicle type, the module may also need to wake up the ECU
periodically to retrieve the vehicle status. Check the vehicle specific documentation
sections for hints on the power usage for this and options to avoid or reduce this.
