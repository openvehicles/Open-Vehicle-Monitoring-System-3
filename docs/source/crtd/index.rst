===================
CRTD CAN Log Format
===================

------------
Introduction
------------

The CRTD CAN Log format is a textual log file format designed to store information on CAN bus frames and events.
It was introduced as a human readable file format, capable of supporting data dumps from multiple CAN buses.

Files, or network data streams, in CRTD format contain only textual data in UTF-8,
with each record being on an individual line terminated by a single linefeed (ascii 10) character,
or CRLF (ascii 13 ascii 10).

Each line is made up from the following fields separated by single spaces (ASCII 32):

* Timestamp: Julian timestamp, seconds and milliseconds/microseconds, separated by a decimal point
* Record Type: Mnenomic to denote the record type
* Record Data: The remaining data is dependant on the record type

Here are some examples::

  1542473901.020305 1R11 ...
  1542473901.020305 2T11 ...
  1542473901.020305 R11 ...
  1542473901.021 1R11 ...

-------
History
-------

* 1.0: Initial version supporting CXX, R11, R29, T11, and T29 messages only
* 2.0: Add optional bus ID prefix to record types
* 3.0: Add support for comment commands, and clarify documentation inconsistencies

-------------
Specification
-------------

CRTD files are UTF-8 text files using LF or CR+LF line termination to
separate each record.
::

  <crtd-file> ::= { <crtd-record> [CR] LF }

The records are formatted as follows::

  <crtd-record>    ::= <timestamp> <space> <crtd-type>
  <timestamp>      ::= <julianseconds> . [ <milliseconds> | <microseconds> ]
  <julianseconds>  ::= <digti> { <digit> }
  <milliseconds>   ::= <digit> <digit> <digit>
  <microseconds>   ::= <digit> <digit> <digit> <digit> <digit> <digit>
  <digit>          ::= [ 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 ]
  <hexdigit>       ::= [ <digit>
                     | A | B | C | D | E | F | a | b | c | d | e | f ]
  <space>          ::= ASCII character 32 decimal (0x20 hex)

  <crtd-type>      ::= [ <crtd-comment> | <crtd-transmit> | <crtd-receive> ]

  <crtd-comment>   ::= CXX <space> <textual-comment>

  <crtd-receive>   ::= R11 <space> <rxtx-data>
                     | <bus>R11 <space> <rxtx-data>
                     | R29 <space> <rxtx-data>
                     | <bus>R29 <space> <rxtx-data>

  <crtd-transmit>  ::= T11 <space> <rxtx-data>
                     | <bus>T11 <space> <rxtx-data>
                     | T29 <space> <rxtx-data>
                     | <bus>T29 <space> <rxtx-data>

  <bus>            ::= <digit> { <digit> }

  <rxtx-data>      ::= <can-id> { <space> <can-byte> }
  <can-id>         ::= <hexdigit> { <hexdigit> ]
  <can-byte>       ::= <hexdigit> [ <hexdigit> ]

----------
Timestamps
----------

Timestamps can be provided in either millisecond or microsecond precision.
It is recommended that they be based on the julian seconds in UTC,
but there is no specific requirement for that.
It is suggested that a comment may optionally be placed at the start of the file to describe the timezone.
The timestamps should, however, increase monotonically in the file
(ie; time flows forward with subsequent records).

If the timestamp contains 3 digits after the decimal point, it should be consider millisecond precision,
and handled appropriately. Similarly, if the timestamp contains 6 digits after the decimal point,
it should be considered microsecond precision.

-----------------
Case Independence
-----------------

CAN IDs and data bytes are in case-independent hex, and may or may not be padded with leading zeros.
CRTD processors should be as flexible as possible in their handling of this.

-------------------
CAN Bus Designation
-------------------

Record types may optionally be prefixed by the CAN bus number (1 ... n). If no bus number is provided, #1 should be assumed.

----------
Data bytes
----------

The number of data bytes is limited by the CAN specification to a maximum of 8 bytes.
Similarly, the range of acceptable CAN IDs is limited by the 11bit / 29bit encoding in the CAN protocol specification.

------------
Record Types
------------

Each record can be one of a comment, receive, or transmit data. Other record types should be discarded,
to allow new record types to be introduced without breaking existing processors.

^^^^^^^^^^^^^^^
Comment Records
^^^^^^^^^^^^^^^

Comment records start with the letter 'C', and the record type has two characters following to denote the sub-type.
The rest of the record is to be considered a comment (or data for the command).

The following comment record types are defined:

* CXX: General textual comment
* CER: An indication of a (usually recoverable) error
* CST: Periodical statistics
* CEV: An indication of an event
* CVR: Version of CRTD protocol adhered to (with version number as text comment)

and the following command record types are defined:

* CBC: A command to configure a CAN bus
* CDP: A command to pause the transmission of messages
* CDR: A command to resume the transmission of messages
* CFC: A command to clear all filters for this connection
* CFA: A command to add a filter for this connection

Here are some examples::

  169.971289 CXX Info Type:crtd; Path:'/sd/can3.crtd'; Filter:3:0-ffffffff; Vehicle:TSHK;
  19292.299819 CEV vehicle.alert this is a textual vehicle alert
  198923.283738 CST intr=0 rxpkt=0 txpkt=0 errflags=0 rxerr=0 txerr=0 rxovr=0 txovr=0 txdelay=0 wdgreset=0
  2783.384726 CER intr=0 rxpkt=0 txpkt=0 errflags=0 rxerr=0 txerr=0 rxovr=0 txovr=0 txdelay=0 wdgreset=0

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Command Record CBC - Configure a CAN bus
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The CBC command is used to configure a CAN bus.
It should be prefixed with the bus ID in the usual way, or default to bus #1 if not defined.

The command should have the following space separated parameters:

* mode: either L for listen, or A for active
* speed: the baud rate of the CAN bus

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Command Record CDP - Pause transmission
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The CDP command is used to pause transmission of messages for this connection.
Upon receiving this command, the device producing CRTD logs should henceforth discard
those logs and not transmit them to this connection, until resumed.

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Command Record CDR - Resume transmission
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The CDR command is used to resume transmission of messages for this connection.
It clears the condition previously set by the CDP command.

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Command Record CFC - Clear filters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The CFC command is used to clear the message filters for this connection.

By default, connections receive all messages for all CAN buses, and that is
indicated by the filter list being initially empty. This command resets the
filters to this default condition.

Note that external to the per-message filters, CAN data sources may also have
incoming filters that apply to all incoming data (regardless of the logging destination).
These incoming filters are not affected by this.

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Command Record CFA - Add a filter
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The CFA command is used to add a filter to the list of message filters for this
connection. If one or more filters are defined, only messages matching those filters
will be forwarded.

The command should have the filter passed as a single parameter:

* filter: the filter to add

Filters are formatted as:

  Filter ::= <bus> | <id>[-<id>] | <bus>:<id>[-<id>]

For example:

* 2:2a0-37f for bus #2, IDs 0x2a0 - 0x37f
* 1:0-37f for bus #1, IDs 0x000 - 0x37f
* 3 for bus #3, all messages
* 100-200 for bus #1, IDs 0x100 - 0x200

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

The CRTD format is intended to be very simple to process, either by automated code or humans manually.
It can be loaded into text editors for manipulation (search, replace, etc),
and easily processed by command line tools such as 'cut', 'grep', 'awk', etc.
