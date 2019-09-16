===================
CRTD CAN Log Format
===================

------------
Introduction
------------

The CRTD CAN Log format is a textual log file format designed to store information on CAN bus frames and events.

Files, or network data streams, in CRTD format contain only textual data in UTF-8, with each record being on an individual line terminated by a single linefeed (ascii 10) character.

Each line is made up from the following fields separated by single spaces (ASCII 32):

* Timestamp: Julian timestamp, seconds and milliseconds/microseconds, separated by a decimal point
* Record Type: Mnenomic to denote the record type
* Record Data: The remaining data is dependant on the record type

Here are some examples::

  1542473901.020305 1R11 ...
  1542473901.020305 2T11 ...
  1542473901.020305 R11 ...
  1542473901.021 1R11 ...

----------
Timestamps
----------

In general, timestamps should be in UTC. A comment may optionally be placed at the start of the file to describe the timezone.

If the timestamp contains 3 digits after the decimal point, it should be consider millisecond precision, and handled appropriately. Similarly, if the timestamp contains 6 digits after the decimal point, it should be considered microsecond precision.

-------------------
CAN Bus Designation
-------------------

Record types may optionally be prefixed by the CAN bus number (1 ... n). If no bus number is provided, #1 should be assumed.

------------
Record Types
------------

^^^^^^^^^^^^^^
Comment Record
^^^^^^^^^^^^^^

Comment records start with the letter 'C', and the record type has two characters following to denote the sub-type. The rest of the record is to be considered a comment.

The following comment record types are defined:

* CXX: General textual comment
* CER: An indication of a (usually recoverable) error
* CST: Periodical statistics
* CEV: An indication of an event

Here are some examples::

  169.971289 CXX Info Type:crtd; Path:'/sd/can3.crtd'; Filter:3:0-ffffffff; Vehicle:TSHK;
  19292.299819 CEV vehicle.alert this is a textual vehicle alert
  198923.283738 CST intr=0 rxpkt=0 txpkt=0 errflags=0 rxerr=0 txerr=0 rxovr=0 txovr=0 txdelay=0 wdgreset=0
  2783.384726 CER intr=0 rxpkt=0 txpkt=0 errflags=0 rxerr=0 txerr=0 rxovr=0 txovr=0 txdelay=0 wdgreset=0

^^^^^^^^^^^^^^^^^^^^^
Received Frame Record
^^^^^^^^^^^^^^^^^^^^^

Received frame records describe a frame received from the CAN bus, and start with the letter 'R'. Two types are defined:

* R11: A standard 11bit ID CAN frame
* R29: An extended 29bit ID CAN frame

The record type is followed by the frame ID (in hexadecimal), and then up to 8 bytes of CAN frame data.

Here are some examples::

  1542473901.020305 1R11 213 00 00 00 00 c0 01 00 00
  1542473901.020970 2R11 318 92 0b 13 10 11 3a 00 00
  1542473901.021259 2R11 308 00 ff f6 a6 06 03 80 00
  1542473901.021560 2R11 408 00
  1542473901.030341 1R11 358 18 08 20 00 00 00 00 20
  1542473901.034872 2R11 418 80
  1542473901.035514 1R11 408 10
  1542473901.036694 3R11 41C 10
  1542473901.040289 R11 428 00 30
  1542473901.042516 2R11 168 e0 7f 70 00 ff ff ff
  1542473901.042809 2R11 27E c0 c0 c0 c0 00 00 00 00
  1542473901.043073 1R11 248 29 29 0f bc 01 10 00

^^^^^^^^^^^^^^^^^^^^^^^^
Transmitted Frame Record
^^^^^^^^^^^^^^^^^^^^^^^^

Transmitted frame records describe a frame transmitted onto the CAN bus, and start with the letter 'T'. Two types are defined:

* T11: A standard 11bit ID CAN frame
* T29: An extended 29bit ID CAN frame

The record type is followed by the frame ID (in hexadecimal), and then up to 8 bytes of CAN frame data.

Here are some examples::

  1542473901.020305 1T11 213 00 00 00 00 c0 01 00 00
  1542473901.020970 2T11 318 92 0b 13 10 11 3a 00 00
  1542473901.021259 2T11 308 00 ff f6 a6 06 03 80 00
  1542473901.021560 2T11 408 00
  1542473901.030341 1T11 358 18 08 20 00 00 00 00 20
  1542473901.034872 2T11 418 80
  1542473901.035514 1T11 408 10
  1542473901.036694 3T11 41C 10
  1542473901.040289 T11 428 00 30
  1542473901.042516 2T11 168 e0 7f 70 00 ff ff ff
  1542473901.042809 2T11 27E c0 c0 c0 c0 00 00 00 00
  1542473901.043073 1T11 248 29 29 0f bc 01 10 00

-----------
Conclusions
-----------

Being a textual format, CRTD files are designed to be human readable and manipulated/analysed with standard text processing tools. They are not at all sophisticated, or compact.

