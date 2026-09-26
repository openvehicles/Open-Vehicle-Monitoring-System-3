===========
Toyota bZ4X
===========

The Toyota bZ4X is built on the :doc:`Toyota e-TNGA platform </components/vehicle_toyota_etnga/docs/index>`.
The e-TNGA module provides the CAN polling, charging state machine, BMS handling, web UI and the
common support baseline shared by all e-TNGA vehicles. This page documents only what is specific
to the bZ4X; see the e-TNGA platform page for everything else.

Supported model years: 2023 and 2024.

----------------
Vehicle identity
----------------

:Short type code: TOYBZ4X
:Vehicle name: Toyota bZ4X
:Log tag: ``v-toybz4x`` (wrapper only). Nearly all logging comes from the shared
   e-TNGA platform under ``v-etnga`` — set that one to collect diagnostics, e.g.
   ``log level verbose v-etnga``.
:Config namespace: ``xte`` (inherited from the e-TNGA base; the bZ4X registers no
   namespace of its own)

------------------------
Vehicle-specific support
------------------------

The bZ4X adds no behavioural overrides on top of the e-TNGA platform — the vehicle class is a
registration wrapper, and its whole support baseline is the platform's.

.. note::

   **No e-TNGA behaviour has been confirmed on bZ4X hardware.**  Every status in the validation
   table on the :doc:`e-TNGA platform page </components/vehicle_toyota_etnga/docs/index>` was
   established on a Subaru Solterra, so on a bZ4X even the entries marked vehicle-validated
   should be read as unvalidated until someone confirms them on a car.
