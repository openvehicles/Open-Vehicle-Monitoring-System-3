=========
OBDII ECU
=========

-------
Purpose
-------

The OBDII interface is a connector, electrical specification, and protocol used for viewing the operation and status of a car.  It is mandated in all light duty vehicles sold in North America beginning in 1996, and while the focus of the information it provides is for monitoring the emissions of Internal Combustion Engines, it has proven to be a handy port for connecting a variety of aftermarket displays and monitors to a car.  Electric Vehicles have no need for emissions monitoring, so often omit the port from the car, thus making the these aftermarket devices incompatible.  The OBDII ECU capability of the OVMS v3 is used to create a simulated OBDII port, which can be used to attach many of these aftermarket devices to the car.

-------
Cabling
-------

The cable and OBDII connector are not provided with the OVMSv3 module, and can be built using the diagram below. Alternatively, you can purchase the `OVMS V3 HUD / OBD II-F Cable from FastTech <https://www.fasttech.com/product/9652027-ovms-v3-hud-obd-ii-f-cable>`_.

==== ============ ===========
DA26 OBDII Female Signal name
==== ============ ===========
6    14           CAN3-L
16   6            CAN3-H
8    4 & 5        Chassis & Signal Ground
18   16           +12v switched output to HUD/dongle
==== ============ ===========

N.B. Also place a 120 ohm resistor between OBDII pins 14 & 6 for bus termination

-----
Setup
-----
From the Web Interface, check the "Start OBD2ECU" box, and select CAN3 from the dropdown menu (if using the wiring diagram above).  This will enable the OBDII ECU Task to run the next time the OVMSv3 module is powered on or reset.  Also check the "Power on external 12V" box in order to feed 12v power through to the device.  Click on Save at the bottom of the page.

From the command line, the following commands are available::

  obdii ecu start can3  Starts the OBDII ECU task.
  obdii ecu stop        Stops the OBDII ECU task
  obdii ecu list        Displays the parameters being served, and their current value
  obdii ecu reload      Reloads the map of parameters, after a config change

  power ext12v on	Turns on power feed to the device
  power ext12v off	Turns off power feed to the device

---------
Operation
---------

During operation, an OBDII device, for example, a Head-Up Display (HUD) or OBDII Diagnostic module, will make periodic requests, usually a few times per second, for a set of parameters.  The OVMSv3 module will reply to those parameters with the metric if configured to do so, on an individual basis.  These parameters can be common items such as vehicle speed, engine RPM, and engine coolant temperature, but because of the differences between ICE and EV vehicles, many of the parameters do not have equivalent values in an EV.  Speed and engine (motor) RPM can be directly mapped, but there is typically no "engine coolant".  That parameter (in fact, most parameters) can be mapped to some other value of interest.  For example, the Engine Coolant display on the HUD can be configured to display motor or battery temperature instead. Engine Load (PID 4, a percentage value), is mapped by default to battery State of Charge (also a percentage). However, note that not all vehicle metrics may be supported by all vehicles.

Parameters requested by the OBDII device are referred to by "PID value". Note that each PID has a specific range of allowed values, and that it is not possible to directly represent values outside that range.  For example, PID 5 (Engine coolant temperature) has a range of -40 to +215; it would not be possible to map the full range of motor RPMs to this parameter.  Values outside the allowed range are limited to the range boundary value.  The complete table of possible parameters are described here:  https://en.wikipedia.org/wiki/OBD-II_PIDs#Mode_01

Vehicle metrics are referred to by name.  See the table in Appendix 1 for a list of available metrics, which vehicles report them, and which of those are of a format that can be mapped by the OBDII ECU task.

--------------------------
Defaults and customization
--------------------------

By default, the following mapping of PID value to OVMSv3 metric is used:

========= ========= =================== =================== ===========
PID (Dec) PID (Hex) Requested Parameter Mapped Metric Value Metric Name
========= ========= =================== =================== ===========
4         0x04      Engine Load         Battery SOC         v.b.soc
5         0x05      Coolant Temp        Motor Temp          v.m.temp
12        0x0c      Motor RPM           Motor RPM           v.m.rpm
13        0x0d      Vehicle Speed       Vehicle Speed       v.p.speed
16        0x10      MAF Air Flow        12v Battery         v.b.12v.voltage
========= ========= =================== =================== ===========

The OBDII ECU task supports the remapping and reporting of PIDs 4-18, 31, 33, 47, and 51 (all decimal).  PIDs #0 and 32 are used by the OBDII protocol for management, and not available for assignment. Other PIDs return zero.

PIDs requested for which no mapping has been established are ignored by the OBDII ECU task.  As an aid for discovering which PIDs are being requested, the configuration parameter 'autocreate' may be used to populate entries into the obdii ecu list display.  ('config set obd2ecu autocreate yes').  Such autocreated entries are marked as "Unimplemented", and return zero to the device.  They are not added to the configured (saved) obd2ecu.map, and get cleared at each boot or reset.  They may be mapped to a supported metric, if desired, using::

  config set obd2ecu.map <PID> <metric name>

Example::

  OVMS# config set obd2ecu.map 5 v.b.temp
  (map battery temp to engine coolant temp)

  OVMS# vfs edit /store/obd2ecu/10
  (script for fuel pressure PID; see below)

  OVMS# obdii ecu reload
  OBDII ECU pid map reloaded

  OVMS# obdii ecu list

  PID		Type			Value		Metric
  0   (0x00)	internal		0.000000
  4   (0x04)	internal		95.000000	v.b.soc
  5   (0x05)	metric		16.000000	v.b.temp
  10  (0x0a)	script		0.000000
  11  (0x0b)	unimplemented	0.000000
  12  (0x0c)	internal		0.000000	v.m.rpm
  13  (0x0d)	internal		0.000000	v.p.speed
  16  (0x10)	internal		13.708791	v.b.12v.voltage
  32  (0x20)	internal		0.000000

Types:
* "internal" means default internal handling of the PID.
* "metric" means a user-set mapping of PID to the named metric
* "unimplemented" are PIDs requested by the device, but for which no map has been set
* “script" means the user has configured a script to handle the PID

----------------
Special handling
----------------

Several PIDs are handled specially by the OBDII ECU task.

* PIDs 0 and 32 are bit masks that indicate what other PIDs are being reported by the OBDII ECU task.  These are maintained internally based on the default, mapped, and scripted PID table.  Note that some OBDII devices use PID 0 as a test for ECU presence and operating mode (standard or extended), and ignore the returned values. The OBDII ECU task supports both modes.

* PID 12, Engine RPM, is often monitored by the OBDII device to detect when the car is turned off.  Since an EV's motor is not rotating when the car is stopped, a HUD may decide to power down when it sees the RPM drop below a particular value, or if there is no variation (jitter) in its value.  To prevent this, the OBDII ECU task will source a fake value of 500 rpm, plus a small periodic variation, if the car is not moving (vehicle speed is less than 1).  To actually let the device turn off, see "External Power Control", below.

* PID 16, MAF Air Flow, is commonly used by OBDII devices to display fuel flow, by measuring the amount of air entering the engine in support of combustion.  Since this is irrelevant to an EV, the OBDII ECU task maps this metric to a simple integer.  Most HUDs displays limit this to a range of 0-19.9 liter/hr, which is acceptable to display the +12v battery voltage.  Since the conversion factors are complicated, this value is at best approximate, in spite of its implied precision.

* Mode 9, PID 2, VIN, is used to report the car's DMV VIN to the attached OBDII device.  Since the rest of the parameters reported by the OBDII ECU task are simulated, and some OBDII devices may use the VIN for tracking purposes, the reporting of the VIN may be turned off by setting the privacy flag to "yes". The command is 'config set obd2ecu privacy yes'.  Setting it to 'no', which is the default, allows the reporting of the VIN.

* Mode 9, PID 10, ECU Name, is statically mapped to report the OVMSv3's Vehicle ID field (vehicle name, not VIN).  This string may be customized to any printable string of up to 20 characters, if not used with the OVMS v2 or v3 mobile phone applications.  (‘config set vehicle id car_name’)

--------------
Metric Scripts
--------------

Should one desire to return a value not directly available by a single named metric, it is possible to map a PID to a short script, where combinations of metrics, constants, etc. may be used to create a custom value.  Note that the restrictions on PID value ranges still applies.  Also note that the special handling for PID 12 (engine RPM) is not applied in the OBDII ECU task, so it must be included in the script if driving a HUD.

Scripts should be placed in the directory /store/obd2ecu/PID, where PID is the decimal value of the PID to be processed.  Example for creating a "kw per km" sort of metric::

  ret1=OvmsMetricFloat("v.p.speed");
  ret2=OvmsMetricFloat("v.b.power");
  out=0.0;
  if (ret1 > 0) { out=ret2/ret1; }
  out;

Put this text in a file /store/obd2ecu/4 to map it to the "Engine Load" PID.  See "Simple Editor" chapter for file editing, or use 'vfs append' commands (tedious).  Note however, that Vehicle Power (v.b.power) is not supported on all cars (which is why this is not the default mapping for this PID).

Warning:  The error handling of the scripting engine is very rough at this writing, and will typically cause a full module reboot if anything goes wrong in a script.

----------------------
External Power Control
----------------------

Since the OVMSv3 module remains powered at all times, and the normal means for deducing that a car has been turned off don't work on an EV (see PID #12, above), an external OBDII device needs to be explicitly turned on and off.  This is currently done with short event scripts.  The following commands configure the OVMSv3 to make the external 12v feed follow the vehicle on/off state, or use the vfs edit command to create or modify the files::

  vfs mkdir /store/events
  vfs mkdir /store/events/vehicle.on
  vfs mkdir /store/events/vehicle.off

  vfs append 'power ext12v on' /store/events/vehicle.on/ext12v
  vfs append 'power ext12v off' /store/events/vehicle.off/ext12v
