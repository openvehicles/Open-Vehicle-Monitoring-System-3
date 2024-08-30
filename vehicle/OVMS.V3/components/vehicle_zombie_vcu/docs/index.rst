=====
ZomberVerter VCU
=====

ZomberVerter is an Open Source VCU by Damien Maguire https://openinverter.org/wiki/ZombieVerter_VCU used increasingly
in electric vehicle conversion projects.

The OVMS integration makes use of the ODB2 module in the ZomberVerter VCU, so all you need to do is set the ODB2Can parameter
in the Zombie Web UI to match the can interface the OVMS module is on and it'll be able to make the requests for the spot params.

This early version only supports 1 way communication, but I plan to add preheating and cooling as well as charge control as I get to 
it.


- Vehicle Type: **Zombie**
- Log tag: ``v-zombie-vcu``
- Namespace: ``xzom``
- Maintainers: `Jamie Jones`


----------------
Support Overview
----------------

=================================== ==============
Function                            Support Status
=================================== ==============
Hardware                            Any OVMS v3 (or later) module.
Vehicle Cable                       Wire to one of the 2 main ZomberVerter can buses
GSM Antenna                         1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                         1020200 Universal GPS Antenna (or any compatible antenna)
SOC Display                         Yes
Range Display                       No
Cabin Pre-heat/cool Control         No
GPS Location                        Yes (from modem module GPS)
Speed Display                       No
Temperature Display                 Yes
BMS v+t Display                     No
TPMS Display                        No
Charge Status Display               Yes
Charge Interruption Alerts          Yes
Charge Control                      No
Lock/Unlock Vehicle                 No
Valet Mode Control                  No
Others                              
=================================== ==============

**Poll states:**

=  ==========  =
0  Sleep  the OVMS module is only checking the opmode
1  Charging    the OVMS module sends charging specific queries.
2  Driving     the OVMS module sends driving specific queries.
=  ==========  =

-----------------
Development notes
-----------------

Developers welcome! Follow the developer's guide on https://www.openvehicles.com/developers to get started! Join our slack group (see below) to discuss the nerdy details!

------------------
Community channels
------------------

| Forum: https://openinverter.org/wiki/Main_Page
