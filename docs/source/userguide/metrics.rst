=======
Metrics
=======

Metrics are at the heart of the OVMS v3 system. They are strongly typed named parameters, with values in specific units (and able to be automatically converted to other units). For example, a metric to record the motor temperature may be an integer in Celsius units, and may be convertible to Fahrenheit.

The full list of metrics available can be shown::

  OVMS# metrics list
  m.freeram                                4232852
  m.hardware                               OVMS WIFI BLE BT cores=2 rev=ESP32/1
  m.monotonic                              3568Sec
  ...
  v.p.latitude                             22.2809
  v.p.longitude                            114.161
  v.p.odometer                             100000Km
  v.p.satcount                             12
  v.p.speed                                0Kph
  v.p.trip                                 0Km
  v.tp.fl.p                                206.843kPa
  v.tp.fl.t                                33°C
  v.tp.fr.p                                206.843kPa
  v.tp.fr.t                                33°C
  v.tp.rl.p                                275.79kPa
  v.tp.rl.t                                34°C
  v.tp.rr.p                                275.79kPa
  v.tp.rr.t                                34°C
  v.type                                   DEMO

A base OVMS v3 system has more than 100 metrics available, and vehicle modules can add more for their own uses.

In general, vehicle modules (and some other system components) are responsible for updating the metrics, and server connections read those metrics, reformat them, and send them on to servers and Apps (for eventual display to the user). Status commands (such as STAT) also read these metrics and display them in user-friendly forms::

  OVMS# stat
  Not charging
  SOC: 50.0%
  Ideal range: 200Km
  Est. range: 160Km
  ODO: 100000.0Km
  CAC: 160.0Ah
  SOH: 100%

For developer use, there are also some other metric commands used to manually modify a metric’s value (for testing and simulation purposes), and trace changes::

  OVMS# metrics ?
  list                 Show all metrics
  set                  Set the value of a metric
  trace                METRIC trace framework
