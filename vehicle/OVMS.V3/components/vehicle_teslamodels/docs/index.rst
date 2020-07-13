=============
Tesla Model S
=============

Vehicle Type: **TS**

At present, support for the Tesla Model S in OVMS is experimental and under development. This vehicle type should not be used by anyone other than those actively involved in development of support for this vehicle.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module. Vehicle support: Not widely tested, but should be all.
Vehicle Cable               9665972 OVMS Data Cable for Early Teslas
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 None, not required
SOC Display                 Yes
Range Display               Yes
GPS Location                Yes (from car's built in GPS)
Speed Display               Yes
Temperature Display         Yes
BMS v+t Display             Yes
TPMS Display                Not currently supported
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              Not currently supported
Cabin Pre-heat/cool Control Not currently supported
Lock/Unlock Vehicle         Not currently supported
Valet Mode Control          Not currently supported
Others                      Adhesive velcro strips useful for vehicle attachment
=========================== ==============

-----------
TPMS Option
-----------

Reading and writing TPMS wheel sensor IDs from/to the Baolong TPMS ECU is supported by OVMS in Tesla Model S cars using the Baolong system
(vehicles produced up to around August 2014). You can easily tell if you have this Tesla Model S Baolong TPMS system as Tesla doesn't
support displaying tyre pressures in the instrument cluster. By contrast, the later Continental TPMS system does show the pressures in
the instrument cluster.

OVMS directly supports the Baolong TPMS in Tesla Model S cars, without any extra hardware required. You simply need the usual OVT1 cable.

To read the current wheel sensor IDs from the Baolong TPMS ECU, ensure that the car is ON (simplest is to wake the car up and put in drive), and issue the 'tpms read' command in OVMS.

Similarly, to write wheel sensor IDs to the Baolong TPMS ECU, ensure that the car is ON, and issue the 'tpms write' command in OVMS.
