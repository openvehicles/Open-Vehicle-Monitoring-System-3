==========================
Over The Air (OTA) Updates
==========================

OVMS v3 includes 16MB flash storage. This is partition as::

  4MB for factory application image (factory)
  4MB for the first OTA application image (ota_0)
  4MB for a second OTA application image (ota_1)
  1MB for /store configuration and scripting storage
  The remainder for bootloader, generic non-volatile storage, and other data

In general, the factory application firmware is stored in flash at the factory, during module production. That firmware is never changed on production modules, and is always kept as a failover backup.

That leaves two firmwares for Over The Air (OTA) updates. If the currently running firmware is the factory one, an OTA updated firmware can be written to either of the OTA partitions. If the current running firmware is ota_0, then any new OTA updates will be written to ota_1 (and similarly if ota_1 is currently running, then new OTA updates will be written to ota_0). In this way, the currently running firmware is never modified and is always available as a failover backup.

You can check the status of OTA with the ‘ota status’ command::

  OVMS# ota status
  Firmware:          3.1.003-2-g7ea18b4-dirty/factory/main (build idf v3.1-dev-453-g0f978bcb Apr  7 2018 16:26:57)
  Server Available:  3.1.003
  Running partition: factory
  Boot partition:    factory

That is showing the currently running firmware as a custom image *v3.1.003-2-g7ea18b4-dirty* running in *factory* partition. The running currently running partition is *factory* and the next time the system is booted, it will run from *factory* as well.

As a convenience, if there is currently active wifi connectivity, a network lookup will be performed and the currently available firmware version on the server will be shown. In this case, that is the standard *3.1.003* release (as shown in the ‘Server Available:’ line).

If we wanted to boot from ota_1, we can do this with ‘ota boot ota_1’::

  OVMS# ota boot ota_1
  Boot from ota_1 at 0x00810000 (size 0x00400000)

  OVMS# ota status
  Firmware:          3.1.003-2-g7ea18b4-dirty/factory/main (build idf v3.1-dev-453-g0f978bcb Apr  7 2018 16:26:57)
  Server Available:  3.1.003
  Running partition: factory
  Boot partition:    ota_1

If the bootloader fails to boot from the specified OTA firmware, it will failover and boot from factory.

We can flash firmware to OTA either from a file on VFS (normally /sd), or over the Internet (via http). Let’s try a simple OTA update over HTTP::

  OVMS# ota flash http
  Current running partition is: factory
  Target partition is: ota_0
  Download firmware from api.openvehicles.com/firmware/ota/v3.1/main/ovms3.bin to ota_0
  Expected file size is 2100352
  Preparing flash partition...
  Downloading... (100361 bytes so far)
  Downloading... (200369 bytes so far)
  Downloading... (300577 bytes so far)
  ...
  Downloading... (1903977 bytes so far)
  Downloading... (2004185 bytes so far)
  Download complete (at 2100352 bytes)
  Setting boot partition...
  OTA flash was successful
    Flashed 2100352 bytes from api.openvehicles.com/firmware/ota/v3.1/main/ovms3.bin
    Next boot will be from 'ota_0'

  OVMS# ota status
  Firmware:          3.1.003-2-g7ea18b4-dirty/factory/main (build idf v3.1-dev-453-g0f978bcb Apr  7 2018 16:26:57)
  Server Available:  3.1.003
  Running partition: factory
  Boot partition:    ota_0

Rebooting now (with ‘module reset’) would boot from the new ota_0 partition firmware::

  OVMS# ota status
  Firmware:          3.1.003/ota_0/main (build idf v3.1-dev-453-g0f978bcb Apr  7 2018 13:11:19)
  Server Available:  3.1.003
  Running partition: ota_0
  Boot partition:    ota_0
