===========
Kia Soul EV
===========

Vehicle Type: **KS**

The Kia Soul EV vehicle support will be documented here.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module
Vehicle Cable               9658635 Kia OBD-II to DB9 Data Cable for OVMS
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes 
Range Display               Yes 
GPS Location                Yes
Speed Display               ?
Temperature Display         Yes 
BMS v+t Display             Yes
TPMS Display                Yes
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              Not currently supported
Cabin Pre-heat/cool Control Not currently supported
Lock/Unlock Vehicle         Yes 
Valet Mode Control          Not currently supported
Others
=========================== ==============

------------
OBD-II cable
------------

The Kia Soul EV have two different CAN-busses available on the OBD-II port: C-can and M-can. 

C-can is the main can-bus and M-can is the multimedia bus. The latter one is not necessary for OVMS, but some metrics are fetched from the M-bus and these metrics will be empty if you don't have the proper cable. The standard OBD-II to DB9 cable from Fasttech supports only C-can, so make sure you buy the Kia Soul specific one.

In case you want to build your own cable, here's the pinout:

======= ======= ========
J1962-M DB9-F   Signal
1       5       CAN-1H (M-can High)
4       3       Chassis / Power GND
6       7       CAN-0H (C-can High)
9       4       CAN-1L (M-can Low)
14      2       CAN-0L (C-can Low)
16      9       +12V Vehicle Power
======= ======= ========

A simple approach is to buy an OBDII (J1962-M) pigtail, and solder the DB9-F connector end appropriately.

-------------
Configuration
-------------

There are a few Kia Soul EV specific settings that you have to consider in order to get the most out of OVMS. These are battery size, real life ideal range, remote command pincode and charge port remote control.

------------
Battery size
------------

Up until the 2018 version of Kia Soul, all models had a 27kWh battery. The 2018-version have a 30kWh battery. OVMS is by default set up with 27000Wh, but you can change this configuration to fit your car using this command in the OVMS-shell:
 
``config set xks cap_act_kwh 27000``

NB! Even though it says cap_act_kwh, the number must be in Wh.

---------------------
Real life ideal range
---------------------

Even though the Kia Soul EV is equipped with a pretty good and conservative GOM, most experienced drivers know how far the car can go on a charge. This is what we call the ideal range. You can set the ideal range in kilometers, experienced at 20 degrees celsius, by using this command:

``config set xks maxrange 160``

This setting is set to 160 km by default, and matches the author driving a 2015 Kia Soul Classic in 20 degrees in southern Norway. Your mileage may vary, so please set it accordingly.

The ideal range, as shown in the OVMS APP,  are then derived from this number, multiplied by the state of charge and adjusted using a combination of the outside temperature and the battery temperature.

------------------------------
Open charge port using key fob
------------------------------

By default, OVMS listens for the the third button on the key fob and opens the charge port if Pressed. If you don’t want this behaviour, you can disable it by using this command:

``config set xks remote_charge_port 0``

-------------------------------------
Security pin code for remote commands
-------------------------------------

Remote commands like lock and unlock doors, among others, require a pin code. This pin code can be set using the web configuration or using this command:

``config set password pin 1234``

Please set this pin as soon as possible.

---------------------
Soul specific metrics
---------------------

NB! Not all metrics are correct or tested properly. This is a work in progress.

There are a lot of extra metrics from the Kia Soul. Here’s the current ones:

================================ =============
Metric                           Description
================================ =============
xks.b.cell.volt.max              The highest cell voltage
xks.b.cell.volt.max.no           The cell number with the highest voltage
xks.b.cell.volt.min              The lowest cell voltage
xks.b.cell.volt.min.no           The cell number with the lowest voltage
xks.b.cell.det.max               The highest registered cell deterioration in percent.
xks.b.cell.det.max.no            The cell with the highest registered deterioration.
xks.b.cell.det.min               The lowest registered cell deterioration in percent.
xks.b.cell.det.min.no            The cell with the lowest registered deterioration.
xks.b.min.temp                   The lowest temperature in the battery 
xks.b.max.temp                   The highest temperature in the battery 
xks.b.inlet.temp                 The air temperature at the air inlet to the battery
xks.b.heat1.temp                 The temperature of the battery heater 1
xks.b.heat2.temp                 The temperature of the battery heater 2
xks.b.bms.soc                    The internal state of charge from BMS
xks.c.power                      Charge power in kW.
xks.c.speed                      The charge speed in kilometer per hour.
xks.ldc.out.volt                 The voltage out of the low voltage DC converter.
xks.ldc.in.volt                  The voltage into the low voltage DC converter.
xks.ldc.out.amps                 The power drawn from the low voltage DC converter.
xks.ldc.temp                     The temperature of the LDC.
xks.obc.pilot.duty               The duty cycle of the pilot signal
xks.e.lowbeam                    Low beam on/off
xks.e.highbeam                   High beam on/off
xks.e.inside.temp                Actual cabin temperature
xks.e.climate.temp               Climate temperature setting
xks.e.climate.driver.only        Climate is set to driver only
xks.e.climate.resirc             Climate is set to recirculate
xks.e.climate.auto               Climate is set to auto
xks.e.climate.ac                 Air condition on/off
xks.e.climate.fan.speed          Climate fan speed
xks.e.climate.mode               Climate mode
xks.e.preheat.timer1.enabled     Preheat timer 1 enabled/disabled
xks.e.preheat.timer2.enabled     Preheat timer 2 enabled/disabled
xks.e.preheating                 Preheating on/off
xks.e.pos.dist.to.dest           Distance to destination (nav unit)
xks.e.pos.arrival.hour           Arrival time, hour part (nav unit)
xks.e.pos.arrival.minute         Arrival time, minute part(nav unit)
xks.e.pos.street                 Current street? Or Next street?
xks.v.seat.belt.driver           Seat belt sensor
xks.v.seat.belt.passenger        Seat belt sensor
xks.v.seat.belt.back.right       Seat belt sensor
xks.v.seat.belt.back.left        Seat belt sensor
xks.v.traction.control           Traction control on/off
xks.v.cruise.control.enabled     Cruise control enabled/disabled
xks.v.emergency.lights           Emergency lights enabled/disabled
xks.v.steering.mode              Steering mode: Sport, comfort, normal.
xks.v.power.usage                Power usage of the car
xks.v.trip.consumption.kWh/100km Battery consumption for current trip
xks.v.trip.consumption.km/kWh    Battery consumption for current trip
================================ =============

Note that some metrics are polled at different rates than others and some metrics are not available when car is off. This means that after a restart of the OVMS, some metrics will be missing until the car is turned on and maybe driven for few minutes.

Climate and navigation-metrics are fetched from navigation unit and needs the Kia Soul compatible OBDII-cable.

----------------------------
Soul specific shell commands
----------------------------

There are a few shell commands made for the Kia Soul. Some are read only, others can enable functions and some are used to write directly to a ECU and must therefore be used with caution.

^^^^^^^^^^^^^^^^^^
Read only commands
^^^^^^^^^^^^^^^^^^

| ``xks trip``	
| Returns info about the last trip, from you put the car in drive (D or B) and til you parked the car.

| ``xks tpms``
| Shows the tire pressures.

| ``xks aux``
| Prints out the voltage level of the auxiliary battery.

| ``xks vin``
| Prints out some more information taken from the cars VIN-number. Not complete.

^^^^^^^^^^^^^^^
Active Commands
^^^^^^^^^^^^^^^

| ``xks trunk <pin code>``
| Opens up the trunk

| ``xks chargeport <pin code>``
| Opens up the charge port

| ``xks ParkBreakService <on/off>``
| Not yet working.

| ``xks IGN1 <on/off><pin>``
| Turn on or off IGN1-relay. Can be used to wake up part of the car.

| ``xks IGN2 <on/off><pin>``
| Turn on or off IGN2-relay. Can be used to wake up part of the car.

| ``xks ACC <on/off><pin>``
| Turn on or off ACC-relay. Can be used to wake up part of the car.

| ``xks START <on/off><pin>``
| Turn on or off START-relay. Can be used to wake up part of the car.

| ``xks headlightdelay <on/off>``
| Turn on/off the “follow me home” head light delay function.

| ``xks onetouchturnsignal <0=Off, 1=3 blinks, 2=5 blinks, 3=7 blinks>``
| Configure one touch turn signal settings.

| ``xks autodoorunlock <1=Off, 2=On vehicle off, 3=On shift to P, 4=On driver door unlock>``
| Configure auto door unlock settings.

| ``xks autodoorlock <0=Off, 1=On speed, 2=On shift>``
| Configure auto door unlock settings.

^^^^^^^^^^^^^^^^^^
ECU-write commands
^^^^^^^^^^^^^^^^^^

These commands are for the extra playful people. Use with caution.

| ``xks sjb <b1><b2><b3>``
| Send command to smart junction box.

| ``xks bcm <b1><b2><b3>``
| Send command to body control module.

-----------------
12V battery drain
-----------------

OVMS will eventually drain the 12V battery, but steps have been taken to minimize the drain. However, if you are going to leave the car for a fews days, it is recommended to unplug OVMS.

