=============
Factory Reset
=============

If you have console command access, a factory reset can be accomplished with this command::

  OVMS# module factory reset
  Reset configuration store to factory defaults, and lose all configuration data (y/n): y
  Store partition is at 00c10000 size 00100000
  Unmounting configuration store...
  Erasing 1048576 bytes of flash...
  Factory reset of configuration store complete and reboot now...

That command will erase all configuration store, and reboot to an empty configuration.

If you don’t have console access, you can perform a factory reset by placing an empty file named “factoryreset.txt” in the root directory of an SD card and insert that SD into the (running) module. The file will be deleted and the module will reboot within about 30 seconds.

If you don’t have console access (lost module password) and don’t have an SD card, you can perform a factory reset of the configuration store using the esptool.py tool from the Espressif ESP-IDF toolkit::

  esptool.py
    --chip esp32 --port /dev/tty.SLAB_USBtoUART --baud 921600
    erase_region 0xC10000 0x100000

Note: the device needs to be changed to the one assigned by your system, i.e. /dev/ttyUSB0 on a Linux system. After using esptool.py to manually erase_region, you should go into the console and do the ‘module factory reset’ step to properly factory reset.


You can also open the module case, remove any SD card (important!), power on the module, then push and hold switch “S2” for 10 seconds. “S2” is located here:

.. image:: factoryreset.png

After either of these methods, you will be able to access the USB console with an empty module password and the “OVMS” wifi access point with the initial password “OVMSinit”. We recommend using the setup wizard to configure the module as soon as possible, as the module is accessible by anyone knowing the initial password.

-----------------------
Module Factory Firmware
-----------------------

You can switch back to factory firmware with this command::

  OVMS# ota boot factory
  Boot from factory at 0x00010000 (size 0x00400000)

Or, without console access (lost module password), using the esptool.py from the Espressif ESP-IDF toolkit::

  esptool.py
    --chip esp32 --port /dev/tty.SLAB_USBtoUART --baud 921600
    erase_region 0xd000 0x2000

Note: the device needs to be changed to the one assigned by your system, i.e. /dev/ttyUSB0 on a Linux system.

---------------------
Installing esptool.py
---------------------

The esptool.py package and installation instructions can be found here:

	https://github.com/espressif/esptool

The package normally can be installed without manual download using the python package manager “pip”, i.e. on Unix/Linux::

  sudo pip install esptool
