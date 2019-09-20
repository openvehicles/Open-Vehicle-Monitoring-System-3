CANopen Basics
==============

Communication Objects
---------------------

A CANopen network consists of "masters" and "slaves", masters are clients, 
slaves are servers. At most one master may be active at a time.

CANopen supports addressing of up to 127 slaves on a bus using node IDs 1-127.
Node address 0 is used for broadcasts to all nodes. Node addressing is simply
mapped onto CAN IDs by adding the node id to a base ID.

The CANopen protocol mainly consists of…

 - PDO (process data objects)
 - SDO (service data objects)
 - NMT (network management)
 - SYNC (synchronisation)
 - EMCY (emergency events)

**PDO** are regular, normally periodical, status update frames, for example sensor 
data. You can log them using the CAN monitor (``can log start monitor …``).
PDOs can be sent at any CAN ID except those reserved for other CANopen services.

**SDO** are memory registers of nodes that can be read and written by masters on 
request. SDO requests are normally sent at ID 0x600 + nodeid, responses at ID 
0x580 + nodeid. SDOs are addressed by a 16 bit index + 8 bit subindex. Registers 
and data types for a given device are documented by the device specific object 
dictionary, normally represented as an EDS (electronic data sheet) file.

**NMT** are short datagrams to control node startup / shutdown. There's a special 
node state "pre-operational" allowing access to all operation and communication 
parameters of a node in a standardized way. NMT requests are sent at ID 0x000, 
responses and unsolicited updates (aka heartbeats) are sent at ID 0x700 + 
nodeid with length 1.

**SYNC** messages are datagrams of length 0, normally sent at ID 0x080.

**EMCY** messages are sent if a node encounters some kind of alert or warning 
condition, they are normally sent at ID 0x080 + nodeid with a length of 8 bytes.

  ================  =========================
  CAN IDs           Communication objects
  ================  =========================
  0x000             NMT requests
  0x080             SYNC
  0x081 - 0x0FF     EMCY
  0x581 - 0x5FF     SDO responses
  0x601 - 0x67F     SDO requests
  0x701 - 0x77F     NMT responses / heartbeats
  ================  =========================

So if any of these IDs look familiar to you, chances are you've got a CANopen 
network.

.. note:: CANopen coexists nicely with OBD-II and often does in a vehicle (i.e. 
  Renault Twizy). OBD-II devices normally are addressed at IDs > 0x780 so are 
  outside the CANopen ID range. Even if they use non-standard IDs, the devices 
  normally will detect and ignore frames not matching their protocol.


SDO Addresses
-------------

The SDO address space layout is standardized:

  =========== ===============================================
  Index (hex) Object
  =========== ===============================================
  0000        not used
  0001-001F   Static Data Types
  0020-003F   Complex Data Types
  0040-005F   Manufacturer Specific Complex Data Types
  0060-007F   Device Profile Specific Static Data Types
  0080-009F   Device Profile Specific Complex Data Types
  00A0-0FFF   Reserved for further use
  1000-1FFF   Communication Profile Area
  2000-5FFF   Manufacturer Specific Profile Area
  6000-9FFF   Standardised Device Profile Area
  A000-BFFF   Standardised Interface Profile Area
  C000-FFFF   Reserved for further use
  =========== ===============================================

See `CiA DS301`_ for details on standard SDOs.

For example, a device shall tell about the PDOs it transmits or listens to, 
their IDs, update frequency and content structure at SDO registers 1400-1BFF. 
This is mandatory in theory but real devices may not fully comply to that rule.

CANopen compliant standard device types like motor controllers need to 
implement some standard profile registers. See `CiA DS401`_ for the generic I/O 
device class definition and `CiA DS402`_ for motor controllers.

Most devices will require some kind of login before allowing you to change 
operational parameters. This is also done using SDO writes, but there is no 
standard for this, so you'll need to dig into the device documentation.

Of course there's a lot more on CANopen, but this should get you going.


CAN in Automation
-----------------

More info on the standard and specific device profiles can be found on the
CiA website:

  https://www.can-cia.org/

CAN in Automation (CiA) is the international users’ and manufacturers’ group
for the CAN network (Controller Area Network), internationally standardized
in the ISO 11898 series. The nonprofit association was established in 1992.
The aim is to provide an unbiased platform for future developments of the
CAN protocol and to promote the image of the CAN technology.


.. _`CiA DS301`: https://www.can-cia.org/standardization/specifications/
.. _`CiA DS401`: https://www.can-cia.org/standardization/specifications/
.. _`CiA DS402`: https://www.can-cia.org/standardization/specifications/
