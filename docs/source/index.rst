===============================
Open Vehicles Monitoring System
===============================

.. note::
  This online manual includes documentation for the **stable** release of the
  OVMS firmware (available as OTA version tag "main") and the **latest**
  nightly firmware build (available as OTA version "edge"). Select the
  manual version matching your setup using the menu on the bottom left.

.. toctree::
   :maxdepth: 1
   :caption: User Guides:

   Overview <introduction>
   userguide/index
   plugin/README

.. toctree::
   :maxdepth: 1
   :glob:
   :caption: Vehicle Specific Guides:

   components/vehicle_*/docs/index*

.. toctree::
   :maxdepth: 1
   :caption: Developer Guides:

   cli/index
   crtd/can_logging
   crtd/index
   components/ovms_webserver/docs/index
   components/ovms_script/docs/index
   components/poller/docs/index
   components/canopen/docs/index
   server/index
   protocol_v2/index
   protocol_httpapi/index

.. toctree::
   :maxdepth: 1
   :caption: Research / Work in progress:

   components/retools_pidscan/docs/index
   components/retools_testerpresent/docs/index
