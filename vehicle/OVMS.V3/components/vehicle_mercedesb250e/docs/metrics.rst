----------------
Standard Metrics
----------------

============================= ============================================
Metric name                   Notes
============================= ============================================
ms_v_mot_rpm                  
ms_v_env_throttle             Resolution of 0.4 %
ms_v_pos_speed                Resolution of 0.1 km/h
ms_v_pos_odometer             Resolution of 0.1 km
ms_v_bat_12v_voltage          Resolution of 0.1 V
ms_v_bat_power                Car reports percents, so the kW is just calculated by multiplying by 132 kW. Negative side is saturated to -50 kW. This is just guess work..
ms_v_bat_range_est
ms_v_bat_consumption          Instantious consumption, Wh/km
ms_v_tpms_*_p
ms_v_env_cabinsetpoint        There are two figures, left and right. Using just one for now.
============================= ============================================


--------------
Custom Metrics
--------------

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xmb.v.display.trip.reset                 2733.2km                 Dashbord Trip value since Reset
xmb.v.display.trip.start                 17.5km                   Dashbord Trip value since Start (today)
xmb.v.display.consumption.start          25.9 kWh                 Dashbord Trip consumption since Start (today)
xmb.v.display.accel                      89%                      Eco score on acceleration over last 6 hours
xmb.v.display.const                      18%                      Eco score on constant driving over last 6 hours
xmb.v.display.coast                      100%                     Eco score on coasting over last 6 hours
xmb.v.display.ecoscore                   69%                      Eco score shown on dashboard over last 6 hours
xmb.v.fl_speed                           37.83 km/h               Front Left wheel speed
xmb.v.fr_speed                           37.85 km/h               Front Right wheel speed
xmb.v.rl_speed                           37.80 km/h               Rear Left wheel speed
xmb.v.rr_speed                           37.82 km/h               Rear Right wheel speed
======================================== ======================== ============================================
