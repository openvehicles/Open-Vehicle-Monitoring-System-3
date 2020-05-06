----------------
Standard Metrics
----------------

============================= ===== ========== ============================================
Metric name                   MsgId Bits (DBC)         Description
============================= ===== ========== ============================================
ms_v_mot_rpm                  0x105 6|15                   
ms_v_env_throttle             0x105 32|8                   
============================= ===== ========== ============================================


--------------
Custom Metrics
--------------

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xmb.v.display.trip.reset                 2733.2km                 Dashbord Trip value since Reset
xmb.v.display.trip.start                 17.5km                   Dashbord Trip value since Start (today)
xmb.v.display.accel                      89%                      Eco score on acceleration over last 6 hours
xmb.v.display.const                      18%                      Eco score on constant driving over last 6 hours
xmb.v.display.coast                      100%                     Eco score on coasting over last 6 hours
xmb.v.display.ecoscore                   69%                      Eco score shown on dashboard over last 6 hours
======================================== ======================== ============================================
