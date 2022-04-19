=============
Toyota RAV4 EV
=============

Vehicle Type: **TYR4**

At present, support for the 2012-2014 Toyota RAV4 EV in OVMS is experimental and under development. This vehicle type can be used by early adopters, as long as you understand the limitations. The current release only shows information from the Tesla CAN bus, not the Toyota CAN bus and is read-only.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module. Vehicle support: All 2012-2014 RAV4 EV are identical and no other years are supported.
Vehicle Cable               A custom cable is required to connect at the left rear of the vehicle. Tesla and Toyota CAN buses are available at the Gateway ECU.
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA)
SOC Display                 Yes (Tesla BMS value)
Range Display               Not currently supported
GPS Location                Yes
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
Others                      
=========================== ==============

The Tesla CAN bus already had decodes available, so those were implemented already. The primary focus going forward will be decoding the Toyota CAN bus to replicate SafetyConnect connected features like Remote Climate before the 3G shutdown ends that manufacturer provided functionality.

The file xr4mon.htm should be installed as a Page type Web Plugin to display detailed information about the car's status.
