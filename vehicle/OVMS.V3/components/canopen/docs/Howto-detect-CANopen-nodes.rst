================================================================================
How to detect and identify CANopen nodes
================================================================================

So you've got a CAN bus with some devices, but you don't know which of them 
speaks "CANopen", if any? The OVMS v3 will help you to detect them and open 
their CANs ;)


Before you begin
----------------

…you need to activate the CAN bus(es) you're going to use. As a CANopen master 
needs to write to the network, you need to start the interfaces in active mode, 
i.e. …::

  OVMS# can can1 start active 500000
  OVMS# can can2 start active 125000

… and then start the CANopen master for the bus(es), i.e.::

  OVMS# copen can1 start
  OVMS# copen can2 start


Detecting CANopen nodes
-----------------------

The "open" in "CANopen" means any implementation can decide how much of the 
standard it implements. There are some few mandatory features though, a CANopen 
slave has to implement, if it wants to comply with the standard.

The mandatory features helping to detect and identify CANopen nodes on a CAN 
bus are:

  - NMT (network management), especially RESET and PREOP
  - NMT bootup event messages
  - Standard SDO access in pre-operational mode

If you've got CANopen nodes on a bus, even silent ones, issuing ``copen … nmt reset``
will tell all of them to reboot, and as bootup messages are mandatory, 
you will see them in the OVMS log output like this::

  I (162904) canopen: can1 node 1 new state: Booting

The OVMS CANopen master continously monitors the network for NMT and EMCY 
messages. After bootup of all nodes, you can get a list of all nodes that have 
been detected by issuing ``metrics list co.``::

  OVMS# metrics list co.
  co.can1.nd1.emcy.code
  co.can1.nd1.emcy.type
  co.can1.nd1.state                        Operational

.. note:: if you request a reset, nodes may decide to boot into pre-operational 
  state. That may produce some error messages. Don't worry, you can resolve this 
  anytime by issuing ``copen … nmt start``.


Identifying CANopen nodes
-------------------------

In pre-operational state, a CANopen node must be accessible at the CANopen 
default IDs. That means if the node supports SDO access, we can query some 
standard attributes from it.

That's what ``copen … info`` and ``copen … scan`` do:

- ``copen … info`` queries the standard device attributes from a specific node,
- ``copen … scan`` queries all 127 node IDs.

.. caution:: There may be non-CANopen devices on the bus producing regular data 
  frames at CANopen response IDs and/or reading and possibly misinterpreting 
  CANopen requests sent to node IDs not planned by the manufacturer. Chances are 
  low this triggers problems, but you should be ready to switch off the vehicle 
  when doing a scan -- just in case.

A complete scan takes about 20 seconds. A typical scan could look like this::

  OVMS# level debug canopen
  OVMS# copen can1 scan
  Scan #1-127...
  Node #1:
    Device type: 0x00000000
    Error state: 0x00
    Vendor ID: 0x0000001e
    Product: 0x0712302d
    Revision: 0x00010019
    S/N: 0x47c5…………
    Device name: Gen4 (Renault Twizy)11 November 2011 12
    HW version: 0x00000003
    SW version: 0712.0001
  Node #23: SDO access failed
  Node #25: SDO access failed
  Node #27: SDO access failed
  Node #30: SDO access failed
  Node #87: SDO access failed
  Done.
  D (227994) canopen: ReadSDO #23 0x1000.00: InitUpload failed, CANopen error code 0xffffffff
  D (228194) canopen: ReadSDO #25 0x1000.00: InitUpload failed, CANopen error code 0xffffffff
  D (228444) canopen: ReadSDO #27 0x1000.00: InitUpload failed, CANopen error code 0xffffffff
  D (228844) canopen: ReadSDO #30 0x1000.00: InitUpload failed, CANopen error code 0xffffffff
  D (238384) canopen: ReadSDO #87 0x1000.00: InitUpload failed, CANopen error code 0xffffffff

This means one CANopen node was found, and some non-CANopen frames were 
received on IDs 0x580 +23, +25, +27, +30 and +87.


Great! What now?
----------------

As you now know there's a CANopen node, you can look for documentation on it. 
You can also try to access more default SDOs to see if you can control and 
configure the node.

* If you're lucky, the device will provide its own EDS file at SDO 1021.00. You 
  can check that by issuing…::

    OVMS# copen <bus> readsdo <nodeid> 1021 0

* Check out the `CiA specifications <https://www.can-cia.org/standardization/specifications/>`_,
  for CANopen standards, for example…

  - DS301 for details on general standard SDOs
  - DS401 for the generic I/O device class definition
  - DS402 for motor controller SDOs

* Look up the manufacturer of your device by it's vendor ID:

    https://www.can-cia.org/services/canopen-vendor-id/

* Contact the manufacturer of your device for specific documentation and EDS files.

