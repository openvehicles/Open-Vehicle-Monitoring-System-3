=========
Chevrolet Bolt EV/Ampera-e
=========

Vehicle Type: **CBOLT**

This vehicle type supports the 2017-2018 Chevrolet Bolt EV/Ampera-e. Other versions of the vehicle are currently untested.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    OVMS v3
Vehicle Cable               Works with an OBD-II to DB9 cable (currently confirming whether this has a unique or standard pinout)
GSM Antenna                 Works with any OVMS v3 supported antenna
GPS Antenna                 Works with any OVMS v3 supported antenna
SOC Display                 Yes
Range Display               Yes
GPS Location                Yes (from GPS module/antenna)
Speed Display               Yes
Temperature Display         Yes
BMS v+t Display             Yes
TPMS Display                Yes
Charge Status Display       Yes
Charge Interruption Alerts  tba
Charge Control              Yes (requires SWCAN module)
Cabin Pre-heat/cool Control Yes (requires SWCAN module)
Lock/Unlock Vehicle         Yes (requires SWCAN module)
Valet Mode Control          No
Others                      12V battery
=========================== ==============

SWCAN module can be most easily acquired from eBay: https://www.ebay.com/itm/365182274464 (seemingly sold by @WaferThinLogic) (installation details to be added later)

Can also be sourced from PCBWay here: https://www.pcbway.com/project/shareproject/SWCAN_adapter_for_OVMS_b002c070.html

However, PCBWay has its own challenges because the files provided do not include a Centroid/Pick-and-Place file. This has led to confusion with the manufacturer and risks components being soldered in incorrectly. It is probably better for most users to procure a prebuilt version online.

SWCAN PCB design appears to have been authored by @kssmll here: https://github.com/kssmll/ovms-swcan-board (Notice the high resolution photos matching the ones on PCBWay)

Please note that OVMS v3 on the Bolt is generally compatible with OBD splitters, allowing other components to also plug into the CAN bus at the same time, such as Bluetooth OBD dongles for use with A Better Route Planner (ABRP).

For more detailed information, see this ongoing thread (still active as of 2026): https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/704
