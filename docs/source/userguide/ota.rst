==========================
Over The Air (OTA) Updates
==========================

.. highlight:: none

------------------
OTA Build Versions
------------------

The official public OVMS servers are organized to provide three standard firmware builds:

  - ``main`` -- main release for users (normally 1-2 updates per year)
  - ``eap`` -- stable developer build for beta testers (updated potentially frequently in the weeks before the next main release)
  - ``edge`` -- latest developer build (daily updates)

.. warning::
  **Developer builds are meant for development and testing, not for regular daily use!**
  Developer versions may contain potentially harmful bugs and experiments. When running a developer version,
  it's required you closely follow discussions on the newly developed features, help in testing them by
  providing feedback, and **closely watch your device and vehicle for any unusual behaviour**.

To change the build version for your next OTA update, set the "version tag" field in the web UI to
the version tag code to be used, or set the configuration directly (``config set ota tag …``).

To manually flash the latest firmware of a specific build version, either copy the firmware (``ovms3.bin``)
URL from browsing the download area of the update server, or replace the respective URL part, e.g.
``…/firmware/ota/v3.3-5/main/ovms3.bin`` → ``…/firmware/ota/v3.3-5/eap/ovms3.bin``.


--------------
OTA Partitions
--------------

OVMS v3 includes 16MB flash storage. **Up to release 3.3.005**, this was partitioned as::

  4MB for factory application image (factory)
  4MB for the first OTA application image (ota_0)
  4MB for a second OTA application image (ota_1)
  1MB for /store configuration and scripting storage
  The remainder for bootloader, generic non-volatile storage, and other data

The factory application firmware was stored in flash at the factory, during module production. That firmware was never changed on production modules, and was kept as a failover backup.

**Beginning with release 3.3.006**, the partitioning was changed to::

  7MB for the first OTA application image (ota_0)
  7MB for a second OTA application image (ota_1)
  1.9MB for /store configuration and scripting storage

As the factory failover in fact never was needed, but the firmware size was about to grow beyond 4MB,
the factory partition was dropped. See :doc:`partitioning` for details and the upgrade process.

That leaves two firmwares for Over The Air (OTA) updates. Updates will be installed in the currently inactive
OTA partition, and will only be activated after a successful validation of their integrity. In this way,
the currently running firmware is never modified and is always available as a failover backup.

.. note::
  Trying to flash a firmware image too big for the target OTA partition (i.e. larger than 4MB = 4194304 bytes) 
  with release 3.3.005 or earlier aborts with: ``Error: ESP32 error #260 when starting OTA operation``
  (error code #260 = invalid size).
  
  To proceed in that case, first flash the latest 3.3.006 release available in the ``v3.3`` directory and 
  perform the partitioning upgrade (see :doc:`partitioning`). After performing the upgrade, continue with 
  flashing the version desired.


----------------------
Selecting a Boot Image
----------------------

You can check the status of OTA with the ``ota status`` command::

  OVMS# ota status
  Hardware:          OVMS WIFI BLE BT cores=2 rev=ESP32/3; MODEM SIM7600
  Firmware:          3.3.005-925-g3e67d4e6d-dirty/ota_1/edge (build idf v3.3.4-854-g9063c8662c May 13 2026 20:13:48, product v3.3-5)
  Partition type:    v3-35 (ota1, ota2, no factory, maximized store)
  Partition table:   0x8000
  Running partition: ota_1
  Boot partition:    ota_1
  OTA_O image:       3.3.005-922-g1a3378f1b
  OTA_1 image:       3.3.005-925-g3e67d4e6d-dirty
  Server Available:  3.3.005-928-g7f8b5d676 (is newer)

That is showing the currently running firmware as a custom image ``3.3.005-925-g3e67d4e6d-dirty`` running in
the ``ota_1`` partition, and the next boot will run from ``ota_1`` as well.

As a convenience, if there is currently active wifi connectivity, a network lookup will be performed and the currently available firmware version on the server will be shown. In this case, that is the ``3.3.005-928-g7f8b5d676`` release (as shown in the ´Server Available:´ line).

If we wanted to boot from ``ota_0`` next, we can do this with ‘ota boot ota_0’::

  OVMS# ota boot ota_0
  Boot from ota_0 at 0x00010000 (size 0x00700000)

  OVMS# ota status
  …
  Running partition: ota_1
  Boot partition:    ota_0

If the bootloader fails to boot from the specified OTA firmware, it will failover and boot from the primary
partition (factory / ota_0).


-----------------
Updating Manually
-----------------

We can flash firmware to OTA either from a file on VFS (normally ``/sd``), or over the Internet (via http).
Let’s try a simple OTA update over HTTP::

  OVMS# ota flash http
  Current running partition is: ota_1
  Target partition is: ota_0
  Download firmware from api.openvehicles.com/firmware/ota/v3.1/main/ovms3.bin to ota_0
  Expected file size is 2100352
  Preparing flash partition...
  Downloading... (100361 bytes so far)
  …
  Downloading... (2004185 bytes so far)
  Download complete (at 2100352 bytes)
  Setting boot partition...
  OTA flash was successful
    Flashed 2100352 bytes from api.openvehicles.com/firmware/ota/v3.1/main/ovms3.bin
    Next boot will be from 'ota_0'

  OVMS# ota status
  …
  Running partition: ota_1
  Boot partition:    ota_0

Rebooting now (with ``module reset``) would boot from the newly downloaded ota_0 partition firmware::

  OVMS# ota status
  Firmware:          3.1.003/ota_0/main (build idf v3.1-dev-453-g0f978bcb Apr  7 2018 13:11:19)
  …
  Running partition: ota_0
  Boot partition:    ota_0


