.. highlight:: none

--------------------
Auxiliary Charge Fan
--------------------

The builtin **Elips 2000W** charger's officially specified temperature operation range is -20..+50 
°C. This is exceeded by the Twizy easily even on moderate summer days. The charger temperature can 
easily rise above 60 °C, and this is possibly the main reason why Twizy chargers eventually die. It 
also leads to the Twizy reducing the maximum charge power available, resulting in charges taking 
much longer than usual.

**A counter measure** can be to add an additional fan to cool the charger, and switch that fan on 
as necessary (see below). The fan can be controlled using one of the OVMS EGPIO output ports (see 
:doc:`/userguide/egpio`). The easiest way is to use the switched 12V supply available at the DA26 
expansion plug without additional hardware. The 12V port can deliver a nominal output of 25W or 
1.8A, and up to 40W or 2.9A short term, so can be used to power many auxiliary fans directly.

**To enable the auxiliary fan**, simply configure the EGPIO to be used in config parameter ``xrt 
aux_fan_port``. For example, to use the switched 12V port = EGPIO port 1, do::
  
  OVMS# config set xrt aux_fan_port 1

Set the port to 0 to disable the feature.

**Mode of operation:**

- The OVMS **continously monitors** the charger temperature. It turns on the 
  fan when the charger temperature rises **above 45 °C** (>= 46 °C),
  and off when the temperature drops **below 45 °C** (<= 44 °C).

- **After end of charge** (Twizy system switched off), the OVMS does not get temperature 
  updates, so it keeps the fan running for another **5 minutes**.

The fan control works during **charging and driving**, as during driving the charger also has to 
supply +12V by the builtin DC/DC converter (heating up the charger), and a trip may start with an 
already hot charger.
