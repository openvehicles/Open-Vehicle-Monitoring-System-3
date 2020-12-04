================
DBC Introduction
================

DBC is a CAN data description file format introduced by `Vector Informatik GmbH <https://www.vector.com/>`_.
DBC files are text files so can be created and edited using a simple text editor like the one built into 
the OVMS web UI.

DBC files can be used to support vehicles that don't have a dedicated native adaption yet. This is done
using the generic DBC vehicle type, which can use DBC files to translate CAN data into metrics.

This section tries to show you how to create and use a DBC specification on the OVMS. Of course 
you'll need to know how to decode your vehicle's CAN frames first. You can use the OVMS RE (reverse 
engineering) toolkit to identify which messages are on the bus and which changes correlate to 
actions and status on the car.

There's also a very good general introduction to DBC from CSS Electronics including an explanatory video:
https://www.csselectronics.com/screen/page/can-dbc-file-database-intro/language/en

DBC files only specify the passive (reading) part, they don't provide a means to define transmissions.
If you need to send frames to request certain information, you can still use the OVMS DBC engine to
decode the results (instead of doing the decoding in C++ code). So a DBC file can be used as a base
for a real vehicle module. To request OBD2 data during development, you can use the ``re obdii``
commands.


-------------
Basic Example
-------------

.. warning::
  The DBC support is usable, but considered to be an alpha release.
  The way it works may change in the future.

This example is taken from the **Twizy**, which sends some primary BMS status data at CAN ID 0x155 (341). 
Note: the Twizy module actually does not use DBC, this is just an example how you would decode this 
message if using DBC.

**Message example**: ``05 96 E7 54 6D 58 00 6F``

- Byte 0: charge current limit [A], scaled by 5, no offset: ``05`` → **25A**
- Bytes 1+2: momentary battery current [A], big endian, lower 12 bits, scaled by -0.25, offset +500: ``_6 E7`` → **58.25A**
- Bytes 4+5: SOC [%], big endian, scaled by 0.0025, no offset: ``6D 58`` → **69.98%**

These can be translated to metrics directly:

- Charge current limit: ``v.c.climit``
- Momentary battery current: ``v.b.current``
- Battery SOC: ``v.b.soc``


---------------
Create DBC File
---------------

**Copy & paste** the following into a new editor window:

.. code-block:: none

  VERSION "DBC Example 1.0"

  BS_: 500000 : 0,0

  BO_ 341 BMS_1: 8 Vector__XXX
    SG_ v_c_climit        :  7|8@0+   (5,0)         [0|35]        "A"   Vector__XXX
    SG_ v_b_current       : 11|12@0+  (-0.25,500)   [-500|1000]   "A"   Vector__XXX
    SG_ v_b_soc           : 39|16@0+  (0.0025,0)    [0|100]       "%"   Vector__XXX


**What does this mean?**

- ``BS_`` defines the bus speed (500 kbit in this example) and bit timings (unused)
- ``BO_`` defines the data object, a CAN frame of length 8 with ID 341 (0x155) ("BMS_1" is just an arbitrary name)
- ``SG_`` lines define the signals (values) embedded in the object (see below)
- ``Vector__XXX`` is just a placeholder for any sender/receiver (currently unused by the OVMS)

**Signals** are defined by their…

- Name (= metric name with ``.`` replaced by ``_``)
- Start position (bit position) and bit length (``7|8``)
- Endianness: ``0`` = big endian (most significant byte first), ``1`` = little endian (least significant byte first) (Note: the DBC format documentation is wrong on this)
- Signedness: ``+`` = unsigned, ``-`` = signed
- Scaling and offset: ``(-0.25,500)`` => real value = raw value * -0.25 + 500
- Minimum/maximum: ``[-500|1000]`` = valid real values are in the range -500 to 1000
- Unit: e.g. ``"A"`` (ignored/irrelevant, defined by the metric)

The metric to set can be given as the name of the signal. You may use the metric name directly 
on the OVMS, but to conform to the DBC standard, replace the dots by ``_``.

**Bit positions** are counted from byte 0 upwards by their significance, regardless of the endianness. 
The first message byte has bits 0…7 with bit 7 being the most significant bit of the byte. The 
second byte has bits 8…15 with bit 15 being the MSB, and so on:

.. code-block:: none

  [ 7 6 5 4 3 2 1 0 ] [ 15 14 13 12 11 10 9 8 ] [ 23 22 21 20 …
  `----- Byte 0 ----´ `------- Byte 1 --------´ `----- Byte 2 …

For big endian values, signal start bit positions are given for the most significant bit. For 
little endian values, the start position is that of the least significant bit.

.. note::
  **On endianness**: "big endian" means a byte sequence of ``0x12 0x34`` decodes into ``0x1234`` 
  (most significant byte first), "little endian" means the same byte sequence decodes into ``0x3412`` 
  (least significant byte first). "Big endian" is the natural way of writing numbers, i.e. with their 
  most significant digit coming first, "little endian" is vice versa. Endianness only applies 
  to values of multiple bytes, single byte values are always written naturally / "big endian".


------------
Use DBC File
------------

**Save** the DBC example as: ``/store/dbc/twizy1.dbc`` (the directory will be created by the editor)

**Open the shell**. To see debug logs, issue ``log level debug dbc-parser`` and ``log level debug v-dbc``.

Note: the DBC parser currently isn't graceful on errors, **a wrong DBC file may crash the module**.
So you should only enable automatic loading of DBC files on boot when you're done developing 
and testing it.

So let's first try if the **DBC engine can parse** our file. The ``dbc autoload`` command loads all 
DBC files from the ``/store/dbc`` directory:

.. code-block:: none

  OVMS# dbc autoload
  Auto-loading DBC files...
  D (238062) dbc-parser: VERSION parsed as: DBC Example 1.0
  D (238062) dbc-parser: BU_ parsed 1 nodes
  D (238062) dbc-parser: BO_ parsed message 341
  D (238072) dbc-parser: SG_ parsed signal v_c_climit
  D (238072) dbc-parser: SG_ parsed signal v_b_current
  D (238082) dbc-parser: SG_ parsed signal v_b_soc

Looks good. ``dbc list`` can tell us some statistics:

.. code-block:: none

  OVMS# dbc list
  twizy1: DBC Example 1.0: 1 message(s), 3 signal(s), 56% coverage, 1 lock(s)

The coverage tells us how much of our CAN data bits are covered by signal definitions.

Now let's **load the file into the DBC vehicle**:

.. code-block:: none

  OVMS# config set vehicle dbc.can1 twizy1
  Parameter has been set.

  OVMS# vehicle module NONE
  I (1459922) v-none: Generic NONE vehicle module

  OVMS# vehicle module DBC
  I (249022) v-dbc: Pure DBC vehicle module
  I (249022) v-dbc: Registering can bus #1 as DBC twizy1

Nice. Let's **simulate receiving our test frame** and check the decoded metrics:

.. code-block:: none

  OVMS# can can1 rx standard 155 05 96 E7 54 6D 58 00 6F
  OVMS# me li v.b.soc
  v.b.soc                                  69.98%
  OVMS# me li v.b.current
  v.b.current                              58.25A
  OVMS# me li v.c.climit
  v.c.climit                               25A

So the decoding apparently works.

**To configure DBC mode for autostart** we now just need to set the DBC vehicle mode to be 
loaded on vehicle startup, and to enable autoloading of the DBC files from ``/store/dbc``. You can 
do so either by using the user interface page Config → Autostart (check "Autoload DBC files" and 
set the vehicle type to "DBC"), or from the shell by…

.. code-block:: none

  OVMS# config set auto dbc yes
  OVMS# config set auto vehicle.type DBC

Try a reboot to see if everything works.

You can now add more objects and signals to your DBC file.


.. note::
  During development of a DBC file, you'll need to reload the file frequently. The DBC engine
  locks the currently used vehicle, so you'll need to unload the DBC vehicle (``vehicle module NONE``), 
  then reload the DBC file (``dbc autoload``), then reactivate the DBC vehicle (``vehicle module DBC``).

