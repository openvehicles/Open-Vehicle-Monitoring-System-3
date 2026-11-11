=======================
Maxus eDeliver3
=======================

Vehicle Type: **MED3**

The Maxus eDeliver3 will be documented here.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    OVMS v3 (or later)
Vehicle Cable               OBD-II to DB9 Data Cable for OVMS (1441200 right, or 1139300 left)
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes
GPS Location                Yes (from modem module GPS)
Speed Display               No
Temperature Display         Yes (External Temp and Battery)
BMS v+t Display             Yes
TPMS Display                No
Charge Status Display       No
Charge Interruption Alerts  No
Charge Control              No
Cabin Pre-heat/cool Control No
Lock/Unlock Vehicle         No
Valet Mode Control          No
Others                      Distance to next service (MJ2022+)
=========================== ==============

--------------
Model versions
--------------

The battery and BMS changed with model year 2022:

* MJ2020/21: UABC NCM battery, 35 kWh or 52.5 kWh (96 cells)
* MJ2022+: CATL LFP battery, 50.2 kWh (104 cells)

The 10th character of the VIN is the model year (L = 2020, M = 2021, N = 2022, P = 2023, ...).
The module starts with the MJ2020/21 polls. When frame 0x5B0 (last 8 VIN characters) shows
model year 2022 or later, it switches to the MJ2022+ polls once. The metric ``xmg.v.bat.lfp``
shows whether the MJ2022+ handling is active.

----------------------------
Data sources MJ2022+ (LFP)
----------------------------

Broadcast frames on the OBD port:

======= =================================================== ================================
CAN ID  Decoding                                            Use
======= =================================================== ================================
0x540   bytes 0-2, big endian, km                           ``v.p.odometer`` (all model years)
0x540   bytes 6-7, big endian, km                           ``v.e.service.range``
0x6F0   byte 0 + high nibble of byte 1 (12 bit), km         ``v.b.range.est`` (ignition on only)
0x5B0   ASCII, last 8 VIN characters                        model year detection
0x510   only sent with the ignition on                      poll state RUNNING
0x604   only sent while AC charging                         poll state CHARGING
0x606   only sent while DC charging                         poll state CHARGING
======= =================================================== ================================

BMS DIDs (UDS ReadDataByIdentifier, request ID 0x748, response ID 0x7C8), only polled with
the ignition on or while charging:

====== ================================================== ================================
DID    Decoding                                           Metric
====== ================================================== ================================
B105   (raw - 0x8000) * 0.1 A, negative = charging        ``v.b.current``, ``v.b.power``
B143   UINT16, 0.1 V                                      ``v.b.voltage``
D117   UINT16, 0.1 %                                      ``v.b.soc``
E101   UINT24, 0.1 km                                     ``v.p.odometer``
B142   110 x UINT16 mV, 104 slots in use                  cell voltages
====== ================================================== ================================

With the ignition off, ``v.b.range.est`` is calculated from SOC, SOH, battery temperature
and consumption instead.
