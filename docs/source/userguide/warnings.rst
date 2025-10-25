====================
Warnings and Notices
====================

---------------------------
OVMS-33-7600G FCC Statement
---------------------------

This equipment has been tested and found to comply with the limits for a Class B digital device,
pursuant to part 15 of the FCC Rules. These limits are designed to provide reasonable protection
against harmful interference in a residential installation. This equipment generates, uses and
can radiate radio frequency energy and, if not installed and used in accordance with the
instructions, may cause harmful interference to radio communications. However, there is no
guarantee that interference will not occur in a particular installation. If this equipment
does cause harmful interference to radio or television reception, which can be determined by
turning the equipment off and on, the user is encouraged to try to correct the interference
by one or more of the following measures:

  - Reorient or relocate the receiving antenna.
  - Increase the separation between the equipment and receiver.
  - Connect the equipment into an outlet on a circuit different from that to which the receiver is connected.
  - Consult the dealer or an experienced radio/TV technician for help.

Caution: Any changes or modiﬁcations to this device not explicitly approved by manufacturer
could void your authority to operate this equipment.

This device complies with part 15 of the FCC Rules. Operation is subject to the following two conditions:
(1) This device may not cause harmful interference, and (2) this device must accept any interference
received, including interference that may cause undesired operation.

--------------------------------------------------------
OVMS-33-7600G Specific Absorption Rate (SAR) information
--------------------------------------------------------

This OVMS-33-7600G meets the government's requirements for exposure to radio
waves. The guidelines are based on standards that were developed by independent
scientific organizations through periodic and thorough evaluation of scientific studies.
The standards include a substantial safety margin designed to assure the safety of all
persons regardless of age or health. FCC RF Exposure Information and Statement
the SAR limit of USA (FCC) is 1.6 W/kg averaged over one gram of tissue. Device
types: OVMS-33-7600G has also been tested against this SAR limit. This device
was tested for typical body-worn operations with the back of the phone kept 0mm
from the body. To maintain compliance with FCC RF exposure requirements, use
accessories that maintain an 0mm separation distance between the user's body and
the back of the phone. The use of belt clips, holsters and similar accessories should
not contain metallic components in its assembly. The use of accessories that do not
satisfy these requirements may not comply with FCC RF exposure requirements, and
should be avoided.

--------------------------
OVMS-33-7600G CE Statement
--------------------------

EU importer information：

|   OpenEnergyMonitor
|   InTec F9
|   Ffordd y Parc, Parc Menai
|   Bangor
|   LL57 4FG
|   United Kingdom

This product can be used across EU member states.

Specification:

+--------------------+----------------------------------------------------------------+
| Manufacturer       | Open Vehicles Ltd                                              |
+--------------------+----------------------------------------------------------------+
| Hardware Version   | /                                                              |
+--------------------+----------------------------------------------------------------+
| Software Version   | /                                                              |
+--------------------+----------------------------------------------------------------+
| Frequency Range    | - 2.4G WIFI: 2412MHz ~ 2472MHz(Max power: Rated 15.69dBm)      |
|                    | - GSM 900: 880MHz ~ 915MHz(Max power: Rated 32.66dBm)          |
|                    | - DCS 1800: 1710MHz ~ 1785MHz(Max power: Rated 29.75dBm)       |
|                    | - WCDMA Band I: 1920MHz ~ 1980MHz(Max power: Rated 23.40dBm)   |
|                    | - WCDMA Band VIII: 880MHz~915MHz(Max power: Rated 23.10dBm)    |
|                    | - E-UTRA Band 1: 1920MHz ~ 1980MHz(Max power: Rated 22.80dBm)  |
|                    | - E-UTRA Band 3: 1710MHz~1785MHz(Max power: Rated 22.40dBm)    |
|                    | - E-UTRA Band 7: 2500MHz ~ 2570MHz(Max power: Rated 22.97dBm)  |
|                    | - E-UTRA Band 8: 880MHz ~ 915MHz(Max power: Rated 22.40dBm)    |
|                    | - E-UTRA Band 20: 832MHz ~ 862MHz(Max power: Rated 22.95dBm)   |
|                    | - E-UTRA Band 28: 703MHz ~ 748MHz(Max power: Rated 22.37dBm)   |
|                    | - E-UTRA Band 34: 2010MHz ~ 2025MHz(Max power: Rated 22.79dBm) |
|                    | - E-UTRA Band 38: 2570MHz ~ 2620MHz(Max power: Rated 22.96dBm) |
|                    | - E-UTRA Band 40: 2300MHz ~ 2400MHz(Max power: Rated 23.48dBm) |
+--------------------+----------------------------------------------------------------+
| Receiver category  | 3                                                              |
+--------------------+----------------------------------------------------------------+

Members applicability:

1. Extreme temperature: -20-45 Celcius

2. The device complies with RF specifications when the device used at 5mm form your body.  

3. RED 2014/53/EU

Declaration of Conformity 

Hereby, Open Vehicles Ltd declares that this OVMS-33-7600G product is in compliance with essential
requirements and other relevant provisions of Directive 2014/53/EU. A copy of the Declaration
of conformity can be found at www.openvehicles.com.

----------------
General Warnings
----------------

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

The power used by the module depends on the component activation and hardware configuration.
You can save power by disabling unused components. This can be done automatically by the
Power Management module to avoid deep discharging the 12V battery, or you can use scripts 
to automate switching components off and on.

The power manager can trigger deep sleep mode to save the 12V battery from deep discharging.

Most vehicles keep the 12V battery charged automatically using the main battery, so the
power usage isn't a real issue unless parking the vehicle for a long period.

If you want or need to have minimum power consumption, configure the module for minimum
power usage (see below) and consider using an OBD cable with an integrated power switch,
so you can easily turn off the module when not needed.

If you have fixed vehicle usage/parking times, you can use the scripting system to schedule
regular sleep periods (see command ``module sleep``).

Beginning with firmware release 3.3.004, the 12V monitoring can be configured to enable
deep sleep based on a low 12V voltage level. On most vehicles, this can be used to
automatically shut down the module when parking and reboot when driving or charging.

Beginning with firmware release 3.3.005, you can couple the GPS operation mode
to the parking/driving state, and you can set the Wifi AP+client mode to switch
back to client only mode if no stations connected to the AP for a given timeout
(see Cellular & Wifi configurations).


~~~~~~~~~~~~~~~~~~
Module Version 3.3
~~~~~~~~~~~~~~~~~~

For module hardware version 3.3, the base components need approximately these power
levels continuously while powered on:

================ =========== ============
Component          Avg Power  12V Current
================ =========== ============
Base System           340 mW        28 mA
Wifi Client            20 mW         2 mA
Wifi AP Mode          300 mW        25 mA
Modem                 100 mW         8 mA
GPS                   270 mW        23 mA
**Total**        **1030 mW**    **86 mA**
================ =========== ============

With all features enabled, this adds up to:

  - ~  25 Wh  or   2 Ah  / day
  - ~ 173 Wh  or  14 Ah  / week
  - ~ 754 Wh  or  63 Ah  / month

Note that depending on the vehicle type, the module may also need to wake up the ECU
periodically to retrieve the vehicle status. Check the vehicle specific documentation
sections for hints on the power usage for this and options to avoid or reduce this.

**GPS** consumption includes the active GPS antenna, which accounts for ~ 5 mA / 55 mW.
Antenna power cannot be switched by the module.

ESP32 **deep sleep mode** (e.g. triggered by the power management module) reduces the
consumption to ~ 15 mA / 165 mW with the active GPS antenna attached, or ~ 10 mA / 110 mW
with the GPS antenna detached. The hardware isn't capable to go any lower than this.


~~~~~~~~~~~~~~~~~~
Module Version 3.2
~~~~~~~~~~~~~~~~~~

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

