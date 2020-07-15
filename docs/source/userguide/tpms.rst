====
TPMS
====

OVMS v3 includes a capability to read and write sets of TPMS wheel sensors IDs from the vehicle TPMS ECU, and store in your configuration.
Support for this is vehicle-specific, and may require optional hardware boards, so please follow your individual vehicle guides for further information.

The command to read the current TPMS wheel sensor IDs from the car is:

  OVMS# tpms read SET
(replace SET with your own identifier for this set of wheels, such as 'summer', 'winter', etc)

You can check the stored wheel sensor ID sets with:

  OVMS# tpms list

If you change wheels, you can write the new wheel IDs to the TPMS ECU in the car with:

  OVMS# tpms write SET
(replace SET with your own identifier for this set of wheels, such as 'summer', 'winter', etc)

Note that in most cases the car must be switched on for the above commands to work. Please refer to your vehicle
specific user guide for more information on this.

Note also that OVMS doesn't have any radio capable of receiving TPMS signals and cannot read the IDs from the wheel sensors themselves.
OVMS can only read the IDs from the TPMS ECU in the car itself. You can, however, drop by pretty much any garage (or use any
of a large number of TPMS tools) to trigger and read these IDs from the wheels. Once your garage gives you the sensor IDs (each
expressed as an 8 character hexadecimal ID), you can enter them into OVMS as:

  OVMS# tpms set SET ID1 ID2 ID3 ID4 ...
  (replace SET with your own identifier for this set of wheels, and ID1.. with the hexadecimal sensor ID)
