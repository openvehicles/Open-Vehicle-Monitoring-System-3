======
Events
======

The Renault Twizy module emits these specific events additionally to the general OVMS events:

=========================================== =============== =========================================
Event                                       Data            Purpose
=========================================== =============== =========================================
vehicle.charge.substate.scheduledstop                       Charge stopped by user request (command)
vehicle.ctrl.cfgmode                                        SEVCON in configuration mode (preop)
vehicle.ctrl.loggedin                                       SEVCON session established
vehicle.ctrl.runmode                                        SEVCON in normal mode (op)
vehicle.drivemode.changed                                   SEVCON tuning profile changed
vehicle.dtc.present                         <dtc_descr>     OBD2 DTC (diagnostic trouble code) present
vehicle.dtc.stored                          <dtc_descr>     OBD2 DTC stored
vehicle.fault.code                          <code>          SEVCON fault code received
vehicle.kickdown.engaged                                    Kickdown detected, drive power changed
vehicle.kickdown.released                                   Kickdown mode end, normal power restored
vehicle.kickdown.releasing                                  Kickdown mode about to end
=========================================== =============== =========================================
