===============
Mitsubishi Trio
===============

Vehicle Type: **MI**

This should be used to support the Mitsubishi i-Miev (Citroen C-Zero, Peugeot iOn) vehicles.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module
Vehicle Cable               1139300 OBD-II to DB9 Data Cable for OVMS (left)
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes
GPS Location                Yes
Speed Display               Yes
Temperature Display         No
BMS v+t Display             Yes
TPMS Display                Not currently supported
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              No
Cabin Pre-heat/cool Control Not currently supported
Lock/Unlock Vehicle         Not currently supported
Valet Mode Control          Not currently supported
Others
=========================== ==============

---------------------
Trio specific metrics
---------------------

NB! Not all metrics are correct or tested properly. This is a work in progress.

==================================== ==================
Metric                               Description
==================================== ==================
xmi.b.power.min                      The battery minimum power usage 
xmi.b.power.max                      The battery maximum power usage
xmi.e.lowbeam                        Low beam light status
xmi.e.highbeam                       High beam light status
xmi.e.frontfog                       Front fog light status
xmi.e.rearfog                        Rear fog light status
xmi.e.rightblinker                   Right blinker status
xmi.e.leftblinker                    Left blinker status
xmi.e.warninglight                   Warning light status
xmi.c.kwh.dc                         Charge energy on DC side in kWh
xmi.c.kwh.ac                         Charge energy on AC side in kWh
xmi.c.efficiency                     Charge efficiency (DC/AC)
xmi.c.power.ac                       Charge power on AC side in kW
xmi.c.power.dc                       Charge power on DC side in kW
xmi.c.time                           Charge time (h:mm.ss)
xmi.c.soc.start                      Charge start soc in %
xmi.c.soc.stop                       Charge stop soc in %
xmi.e.heating.amp                    Heating energy usage in A
xmi.e.heating.watt                   Heating energy usage in W
xmi.e.heating.temp.return            Heater return water temperature 
xmi.e.heating.temp.flow              Heater flow water temperature
xmi.e.ac.amp                         AC energy usage in A
xmi.e.ac.watt                        AC energy usage in W
xmi.e.trip.park                      Trip start odometer in km
xmi.e.trip.park.energy.used          Energy used in kWh
xmi.e.trip.park.energy.recuperated   Recuperated energy in kWh
xmi.e.trip.park.heating.kwh          Heater energy usage on trip in kWh
xmi.e.trip.park.ac.kwh               AC energy usage on trip in kWh
xmi.e.trip.park.soc.start            Trip start soc
xmi.e.trip.park.soc.stop             Trip stop soc
xmi.e.trip.charge                    Trip start odometer in km since charge
xmi.e.trip.charge.energy.used        Energy used in kWh since charge
xmi.e.trip.charge.energy.recuperated Recupe.. energy in kWh since charge
xmi.e.trip.charge.heating.kwh        Heating usage in kWh since charge
xmi.e.trip.charge.ac.kwh             AC energy usage in kWh since charge
xmi.e.trip.charge.soc.start          Trip start soc since charge
xmi.e.trip.charge.soc.stop           Trip stop soc since charge
==================================== ==================

Note that some metrics are polled at different rates than others and some metrics are not available when car is off. This means that after a restart of the OVMS, some metrics will be missing until the car is turned on and maybe driven for few minutes.

---------------
Custom Commands
---------------

^^^^^^^^^^^^^^^^^^
Read only commands
^^^^^^^^^^^^^^^^^^

| ``xmi trip``
| Returns info about the last trip, from you start the car with key.

| ``xmi tripc``
| Returns info about the trip since last charge.

| ``xmi aux``
| Prints out the voltage level of the auxiliary battery.

| ``xmi vin``
| Prints out some more information taken from the cars VIN-number.

---------------------------
Trio Regen Brake Light Hack
---------------------------

^^^^^^^^^^^^^^
Parts required
^^^^^^^^^^^^^^

* 1x DA26 / Sub-D 26HD Plug & housing
  * Note: housings for Sub-D 15 fit for 26HD
  * e.g. Assmann-WSW-A-HDS-26-LL-Z
  * Encitech-DPPK15-BK-K-D-SUB-Gehaeuse
* 1x 12V Universal Car Relay + Socket
  * e.g. Song-Chuan-896H-1CH-C1-12V-DC-Kfz-Relais
  * GoodSky-Relaissockel-1-St.-GRL-CS3770
* 1x 1 Channel 12V Relay Module With Optocoupler Isolation
  * 12V 1 channel relay module with Optocoupler Isolation  

Car wire-tap connectors, car crimp connectors, 0.5 mm² wires, zipties, shrink-on tube, tools

Note: if you already use the switched 12V output of the OVMS for something different, you can use one of the free EGPIO outputs. That requires additionally routing an EGPIO line to the DA26 connector at the expansion slot (e.g. using a jumper) and using a relay module (2/b: relay shield) with separate power input instead of the standard car relay.

I use 2/b (relay shield) variant: Be aware the MAX71317 outputs are open drain, so you need a pull up resistor to e.g. +3.3. According to the data sheet, the current should stay below 6 mA.

Inside OVMS Box: Connect JP1 Pin10 (GEP7) to Pin12 (EGPIO_8) with jumper

In DA26 connector::

  pin 24(+3.3) ----- [ 680 Ohms ] ---+--- [ Relay board IN ]                                   		
                                     |
                            pin 21 (EGPIO_8)
  pin 9 ----- [Relay board DC+]
  pin 8 ----- [Relay board DC-]
  [Relay board COM] ----- Brake pedal switch one side
  [Relay board NO] ----- Brake pedal switch other side

^^^^^^^^^^^^^
Configuration
^^^^^^^^^^^^^

See OVMS web user interface, menu Trio → Brake Light:

.. image:: trio1.png

Set the port as necessary and the checkbox to enable the brakelight.

For monitoring and fine tuning, use the „regenmon“ web plugin:
https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/blob/master/vehicle/OVMS.V3/components/ovms_webserver/dev/regenmon.htm

.. image:: trio2.png

