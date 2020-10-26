.. highlight:: none

-----------------
Auxiliary Charger
-----------------

The **Elips 2000W** (as the name says) does only charge with ~2 kW, thus needing 3-3.5 hours to 
charge the original Twizy battery. That‘s quite slow and gets worse when `replacing the battery by 
a larger one <https://github.com/dexterbg/Twizy-Virtual-BMS>`_.

The Elips is running a special Renault firmware and also taking a crucial role in the Twizy system 
communication, for example to control the BMS. So it cannot easily be replaced by a third 
party charger.

But a **third party charger can run in parallel** to the Elips to boost the charge power. This can 
be done best from the "rear end", i.e. using the battery main connector as the power input (like 
the SEVCON does during regenerative braking).

**Some documented examples of additional chargers:**

- https://www.twizy-forum.de/werkstatt-twizy/84935-twizy-schnelllader
- https://www.twizy-forum.de/projekte-twizy/83603-schnell-lader-extern
- https://www.twizy-forum.de/tipps-und-tricks-twizy/80814-twizy-schnellladen-die-wave-kann-kommen

**The caveat:** to avoid damaging the battery, additional charge power should not be supplied when 
the battery is nearly full. So the additional charger needs to be switched off before the main 
charger finishes.

The OVMS can **automate** switching the additional charger both on and off. Any of the available 
EGPIO output ports (see :doc:`/userguide/egpio`) can be used for this task. If the charger has 
no digital power control input, a convenient way is to use the switched 12V supply available at the 
DA26 expansion plug without additional hardware. The 12V port can deliver a nominal output of 25W or 
1.8A, and up to 40W or 2.9A short term, so can be used to power a standard automotive relay 
directly.

**Configuration:**

Simply set the EGPIO to be used in config parameter ``xrt aux_charger_port``. For example, to use 
the switched 12V port = EGPIO port 1, do::
  
  OVMS# config set xrt aux_charger_port 1

Set the port to 0 to disable the feature.

**Mode of operation:**

- The port is **switched ON** when the Twizy charges, is below 94% SOC, is still within the CC 
  phase (i.e. not topping off / balancing), and the charge current is not under user control.

- The port is **switched OFF** when the Twizy stops charging, reaches 94% SOC or enters the
  topping off phase (whichever comes first), or when you set a custom charge current.

**Charge current control:**

If you set the **charge current** to level 7 (35 A), only the original charger will run, but at 
full power. You can set levels 6…1 (30…5 A) to reduce the charge power, only using the Elips 
charger. Setting the current to level 0 (no charge throttling) will allow maximum charge current 
from the Elips charger and additionally enable the second charger.
