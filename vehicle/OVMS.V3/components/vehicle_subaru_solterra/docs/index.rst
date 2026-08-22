===============
Subaru Solterra
===============

The Subaru Solterra is built on the :doc:`Toyota e-TNGA platform </components/vehicle_toyota_etnga/docs/index>`.
The e-TNGA module provides the CAN polling, charging state machine, BMS handling, web UI and the
common support baseline shared by all e-TNGA vehicles. This page documents only what is specific
to the Solterra; see the e-TNGA platform page for everything else.

----------------
Vehicle identity
----------------

:Short type code: SUBSOL
:Vehicle name: Subaru Solterra
:Log tag: ``v-subsol`` (wrapper only). Nearly all logging comes from the shared
   e-TNGA platform under ``v-etnga`` — set that one to collect diagnostics, e.g.
   ``log level verbose v-etnga``.
:Config namespace: ``xte`` (inherited from the e-TNGA base; the Solterra registers no
   namespace of its own)

------------------------
Vehicle-specific support
------------------------

The Solterra adds no behavioural overrides on top of the e-TNGA platform — the vehicle class is
a registration wrapper, and its whole support baseline is the platform's.

The Solterra is the platform's reference vehicle: e-TNGA is developed against one, and it is the
only e-TNGA vehicle whose behaviour has been confirmed on real hardware. Which platform
behaviours that covers, and which remain inferred, is recorded in the validation status table on
the :doc:`e-TNGA platform page </components/vehicle_toyota_etnga/docs/index>`.
