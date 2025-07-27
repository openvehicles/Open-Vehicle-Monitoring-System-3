
==================
NIU MQi GT EVO/100
==================

Vehicle Type: **NEVO**

This vehicle type supports the MQi GT EVO and GT100 electric scooters from NIU. It can not only read data, but it is also possible to use a third-party (fast) charger with it.

----------------
Support Overview
----------------

.. list-table::
   :header-rows: 1

   * - Function
     - Support Status
   * - **Hardware**
     - OVMS v3.3
   * - **Vehicle Cable**
     - Honda 6-pin Diag to OBD adapter or custom cable
   * - **GSM Antenna**
     - 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
   * - **GPS Antenna**
     - 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
   * - **SOC Display**
     - Yes
   * - **Range Display**
     - Yes
   * - **GPS Location**
     - Yes (via modem module GPS)
   * - **Speed Display**
     - Yes
   * - **Temperature Display**
     - Yes
   * - **BMS Voltage/Temperature Display**
     - Yes
   * - **TPMS Display**
     - No
   * - **Charge Status Display**
     - Yes
   * - **Charge Interruption Alerts**
     - Yes
   * - **Charge Control**
     - Yes (if 3rd party charger used)
   * - **Cabin Pre-heat/Cool Control**
     - No
   * - **Lock/Unlock Vehicle**
     - No
   * - **Valet Mode Control**
     - No

-------------------
Custom Battery Pack
-------------------

If you build a custom battery pack and use the original or a battery simulator, you can set the Ah size of your battery in the web settings to ensure correct range calculation. If you use multiple batteries, set the Ah value for only one battery; it will be multiplied if two batteries are used.

-----------------------------------
Range Calculation and Battery Usage
-----------------------------------

Since our scooters have no reliable built-in range calculation, we estimate the range and energy usage using the average consumption from the last 25 km within the OVMS module. This provides a more reliable range estimate than the original system.
The trip distance can be reset manually, ideal if you want to keep track of longer trips, although our scooter resets it every time you power it on.

-------------------------------------
Additional NIU MQi GT EVO/100 Metrics
-------------------------------------

.. list-table::
   :header-rows: 1

   * - Name
     - Function
   * - **xnevo.b.A.current***
     - Battery A current
   * - **xnevo.b.A.cycles***
     - Battery A SOC cycles
   * - **xnevo.b.A.detected***
     - Battery A is detected by OVMS
   * - **xnevo.b.A.firmware***
     - Battery A firmware version
   * - **xnevo.b.A.hardware***
     - Battery A hardware version
   * - **xnevo.b.A.soc***
     - Battery A SOC
   * - **xnevo.b.A.voltage***
     - Battery A voltage
   * - **xnevo.v.bus.awake**
     - CAN bus is awake and running
   * - **xnevo.v.bus.charger.emulation**
     - Charger emulation status
   * - **xnevo.v.pos.odometer.start**
     - Odometer temporary variable for trip distance calculation

* Metrics are also available for battery B.

------------------
Available Commands
------------------

.. list-table::
   :header-rows: 1

   * - Name
     - Function
   * - **wakeup***
     - Wakes up and retrieves all battery-related data.
   * - **xnevo charger enable**
     - Emulates original charger CAN messages for charging with a third-party (fast) charger.
   * - **xnevo charger disable**
     - Stops emulating original charger CAN messages.
   * - **xnevo trip**
     - Resets trip distance and used/recuperated energy values.
   * - **xnevo debug**
     - Debug output of custom functions like rolling average consumption calculation.

   * - **homelink 1**
     - Similar to 'xnevo charger enable', this command is designed for convenient use within the smartphone app.
   * - **homelink 2**
     - Similar to 'xnevo charger disable'.

------------------
Community Channels
------------------

https://www.elektroroller-forum.de/viewtopic.php?f=63&t=41644
