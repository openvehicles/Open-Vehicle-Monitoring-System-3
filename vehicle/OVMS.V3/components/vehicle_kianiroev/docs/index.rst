==========
Kia e-Niro
==========

Vehicle Type: **KN**

The Kia e-Niro vehicle support will be documented here.

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
Range Display               Yes (based on consumption of past 20 trips longer than 1km)
GPS Location                Yes
Speed Display               Not currently supported
Temperature Display         Yes 
BMS v+t Display             Yes
TPMS Display                Yes
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              Not currently supported
Cabin Pre-heat/cool Control Not currently supported
Lock/Unlock Vehicle         Yes
Valet Mode Control          Not currently supported
Others                      ODO Not currently supported 
=========================== ==============

------------
OBD-II cable
------------

The Kia e-Niro have one CAN-bus available on the OBD-II port: D-can. You can use the standard OBD-II to DB9 cable from Fasttech.

In case you want to build your own cable, here’s the pinout:

======= ======= ========
J1962-M DB9-F   Signal
4       3       Chassis / Power GND
6       7       CAN-0H (C-can High)
14      2       CAN-0L (C-can Low)
16      9       +12V Vehicle Power
======= ======= ========

A simple approach is to buy an OBDII (J1962-M) pigtail, and solder the DB9-F connector end appropriately.

-------------
Configuration
-------------

TODO. Configuration is quite similar to the Kia Soul, so please check that out. Please use the Web based configuration!

---------------
Estimated Range
---------------

Currently, there is no known way to get the estimated range directly from the car, so the estimated range is calculated by looking at the consumption from the last 20 trips that are longer than 1 km. 

-----------------
12V battery drain
-----------------

OVMS will eventually drain the 12V battery, but steps have been taken to minimize the drain. However, if you are going to leave the car for a fews days, it is recommended to unplug OVMS. OVMS will send an alert if 12V drops under 12V alert threshold. See 12V Calibration section. 

