=============
Factory Reset
=============

.. highlight:: none

--------------------
Module Configuration
--------------------

A standard factory reset erases the configuration store. After a factory reset, you will be able to 
access the USB console with an empty module password and the “OVMS” wifi access point with the 
initial password “OVMSinit”. We recommend using the setup wizard to configure the module or 
restoring a configuration backup as soon as possible, as the module is accessible by anyone knowing 
the initial password.

The factory reset does not revert OTA firmware installs, and it does not change the boot partition, so
the module will boot into the same firmware version you had been running before. See below for methods
to optionally reflash the firmware.

There are four methods to perform a factory reset.

.. note:: Methods 1, 2 and 3 need a running system, i.e. will not work if your module cannot 
  boot normally. In this case first try method 4. If that doesn't help also switch back to the 
  factory firmware as shown below.


^^^^^^^^^^^^^^^^^
Method 1: Command
^^^^^^^^^^^^^^^^^

If you still have some shell/command access, a factory reset can be accomplished with this
command on a **USB/SSH console**::

  OVMS# module factory reset

To issue the command from the **web shell or a remote shell** (App, Server, …), you need 
to skip the confirmation step by adding the option ``-noconfirm``, i.e.::

  OVMS# module factory reset -noconfirm

The command will erase all configuration store, and reboot to an empty configuration.


^^^^^^^^^^^^^^^^^
Method 2: SD card
^^^^^^^^^^^^^^^^^

If you don’t have console access, you can perform a factory reset by placing an empty file named 
``factoryreset.txt`` (case is important) in the root directory of an SD card and insert that SD into
the (running) module. The file will be deleted and the module will reboot within about 30 seconds.


^^^^^^^^^^^^^^^^^^^
Method 3: Switch S2
^^^^^^^^^^^^^^^^^^^

You can also open the module case, remove any SD card (important!), power on the module, wait 10 
seconds, then push and hold switch “S2” for 10 seconds. “S2” is located here:

.. image:: factoryreset.png


^^^^^^^^^^^^^
Method 4: USB
^^^^^^^^^^^^^

If you don’t have console access and don’t have an SD card, you can perform a factory reset from a 
PC via USB using the **ESP Tool** from the Espressif ESP-IDF toolkit. There are two variants of the 
tool available, the newer one being the browser based ``esptool-js`` (Javascript, no installation necessary), 
the older one being ``esptool.py`` (Python, installation necessary).


~~~~~~~~~~~~~~~~
Browser ESP Tool
~~~~~~~~~~~~~~~~

The browser based ESP tool doesn't need any installation, just a current version of any web browser that 
supports the WebSerial API: https://espressif.github.io/esptool-js/

**Do not use "Erase Flash"!** This erases the complete flash memory, so you'll need to do a full reflash 
if accidentally using this option!

Instead, prepare an all-zero dummy file to program, use e.g. https://tembrica.com/en/dummy-file-generator 
to create one.

**a) Old 4MB partitioning (OTA type v3-30 / pre-3.3.006):**

- Create a dummy file of 1024 KB
- Connect the module, click "Connect", select your USB port
- Set the flash address to ``0xC10000`` and select the 1024 KB dummy file
- Click "Program"

**b) New 7MB partitioning (OTA type v3-35):**

- Create a dummy file of 1984 KB
- Connect the module, click "Connect", select your USB port
- Set the flash address to ``0xE10000`` and select the 1984 KB dummy file
- Click "Program"

After the flash process has finished, click "Disconnect".


~~~~~~~~~~~~~~~
Python ESP Tool
~~~~~~~~~~~~~~~

See `Installing esptool.py`_ on how to install the tool.

The ``esptool.py`` command arguments **differ depending on the partitioning scheme** in place:

**a) Old 4MB partitioning (OTA type v3-30 / pre-3.3.006):**

.. code::

  esptool.py \
    --chip esp32 --port /dev/tty.SLAB_USBtoUART --baud 921600 \
    erase_region 0xC10000 0x100000

**b) New 7MB partitioning (OTA type v3-35):**

.. code::

  esptool.py \
    --chip esp32 --port /dev/tty.SLAB_USBtoUART --baud 921600 \
    erase_region 0xE10000 0x1f0000

**Note**: the port needs to be changed to the one assigned by your system, e.g. ``/dev/ttyUSB0`` on a 
Linux system or ``COMx`` on Windows. After using ``esptool.py`` to manually erase the config region, 
you should go into the console and do the ``module factory reset`` step to properly factory reset.


-----------------------
Module Factory Firmware
-----------------------

For modules running the **new 7MB partitioning scheme** introduced with release 3.3.006, there is no
longer a dedicated "factory" firmware. To switch back to the previously used firmware version, you
need to first check which ``ota`` partition is currently running::

  OVMS# ota
  …
  Running partition: ota_0
  Boot partition:    ota_0

You can also see the versions installed and verify you actually have the correct version installed in
the other partition.

Then set the boot partition to the respective other ``ota`` partition, i.e. switch from ``ota_0`` to
``ota_1`` and vice versa::

  OVMS# ota boot ota_1
  (check output for errors, then do:)
  OVMS# module reset


**If** still running the **old 4MB partitioning scheme** (OTA partition type ``v3-30``), you can switch back 
to the factory firmware with this command::

  OVMS# ota boot factory
  Boot from factory at 0x00010000 (size 0x00400000)


**Without console access** (lost module password), you can use the browser ESP Tool at 
https://espressif.github.io/esptool-js/ or the Python based ``esptool.py`` from the Espressif ESP-IDF 
toolkit to reset the boot partition to the first firmware partition present. This will be ``factory``
on a ``v3-30`` partitioned module, and ``ota_0`` on a ``v3-35`` partitioned module:

If your browser supports the web based ESP Tool, flash a dummy all-zero file of 8 KB to address ``0xd000``. 
Use e.g. https://tembrica.com/en/dummy-file-generator to create the file.

If using the Python ESP Tool::

  esptool.py \
    --chip esp32 --port /dev/tty.SLAB_USBtoUART --baud 921600 \
    erase_region 0xd000 0x2000

**Note**: the device needs to be changed to the one assigned by your system, e.g. ``/dev/ttyUSB0`` on a 
Linux system or ``COMx`` on Windows.




----------------------
Flash Firmware via USB
----------------------

Flashing via USB is done using the **ESP Tool** from the Espressif ESP-IDF toolkit. There are two variants of the 
tool available, the newer one being the browser based ``esptool-js`` (Javascript, no installation necessary), 
the older one being ``esptool.py`` (Python, installation necessary).


^^^^^^^^^^^^^^^^
Browser ESP Tool
^^^^^^^^^^^^^^^^

The browser based ESP tool doesn't need any installation, just a current version of any web browser that 
supports the WebSerial API: https://espressif.github.io/esptool-js/

- Download the firmware file ``ovms3.bin`` you want to flash
- Connect the module, click "Connect", select your USB port
- Set the flash address to e.g. ``0x10000`` (see below) and select the ``ovms3.bin`` file
- Click "Program"
- After the flash process has finished, click "Disconnect"

**On the flash address**: ``0x10000`` is the **primary** firmware partition (i.e. ``factory`` on a ``v3-30`` 
module, ``ota_0`` on a ``v3-35`` module, as explained above). To flash into the current boot partition, select 
the address from the following table accordingly.

**OTA partition flash addresses:**

============ =========== =============
Partitioning Destination Flash address
============ =========== =============
New (v3-35)  ota_0       0x10000
New (v3-35)  ota_1       0x710000
Old (v3-30)  factory     0x10000
Old (v3-30)  ota_0       0x410000
Old (v3-30)  ota_1       0x810000
============ =========== =============


^^^^^^^^^^^^^^^
Python ESP Tool
^^^^^^^^^^^^^^^

``esptool.py`` can also be used to flash a new firmware. Download the firmware file ``ovms3.bin`` you want
to flash, then issue::

  esptool.py \
    --chip esp32 --port /dev/tty.SLAB_USBtoUART --baud 921600 \
    --before "default_reset" --after "hard_reset" \
    write_flash --compress --flash_mode "dio" --flash_freq "40m" --flash_size detect \
    0x10000 ovms3.bin

This flashes into the **primary** firmware partition (i.e. ``factory`` on a ``v3-30`` module, ``ota_0`` on a
``v3-35`` module, as explained above). So if you were booting from another partition before, you also need
to switch the boot partition back to the primary firmware partition as shown above.

For other destination partitions, see table above.


.. _full-reflash-via-usb:

--------------------
Full Reflash via USB
--------------------

If you accidentally did an ``erase_flash``, erased the wrong region, or if something went wrong when
performing the 3.3.006 OTA :doc:`partitioning upgrade <partitioning>` (7MB firmware image size support),
you will need to do a full reflash of your module (including the boot loader and partitioning scheme).

The need for a full reflash will typically show by the USB output of the module boot being
just something like::

  rst:0x10 (RTCWDT_RTC_RESET),boot:0x3f (SPI_FAST_FLASH_BOOT)
  flash read err, 1000
  ets_main.c 371
  ets Jun  8 2016 00:22:57

To do a full reflash, download the three ``.bin`` files from the release you want to flash, e.g. from

  https://ovms.dexters-web.de/firmware/ota/v3.3-5/edge/

**Using the Python tool**::

  esptool.py \
    --chip esp32 --port /dev/tty.SLAB_USBtoUART --baud 921600 \
    --before "default_reset" --after "hard_reset" \
    write_flash --compress --flash_mode "dio" --flash_freq "40m" --flash_size detect \
    0x1000 bootloader.bin 0x10000 ovms3.bin 0x8000 partitions.bin

…replacing the port and file paths accordingly for your system.

**Using the browser ESP tool**:

- See above for the basic operation steps
- Before programming, use the button "Add File" twice to get three file inputs
- Set the flash addresses and files accordingly to: 0x1000 → bootloader.bin, 0x10000 → ovms3.bin, 0x8000 → partitions.bin
- Proceed with "Program"

**If the full reflash fails**, open a support ticket on https://www.openvehicles.com and attach a log of the 
boot process, or install the developer environment and do a ``make flash``.


---------------------
Installing esptool.py
---------------------

The esptool.py package and installation instructions can be found here:

	https://github.com/espressif/esptool

The package normally can be installed without manual download using the python package manager 
“pip”, i.e. on Unix/Linux::

  sudo pip install esptool

.. warning:: You can brick your module using the esptool. Only use the commands shown above.
