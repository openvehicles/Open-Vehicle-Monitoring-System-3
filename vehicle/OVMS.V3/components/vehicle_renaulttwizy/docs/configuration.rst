.. highlight:: none

-------------
Configuration
-------------

The Twizy configuration is stored in the OVMS config parameter (section) ``xrt``. To list the
current values, issue::

  config list xrt

For reference, the following table includes the equivalent OVMS V2 feature or parameter
number & bit value where applicable. You may use the parameter & feature editors on the App
(values are mapped), the commands have been merged into the ``config`` command in V3.

======================= ============== ========== ============================================
Config instance name    Example value  V2 F/P     Description
======================= ============== ========== ============================================
autopower               yes            F15/8      Bool: SEVCON automatic power level adjustment (Default: yes)
autoreset               yes            F15/2      Bool: SEVCON reset on error (Default: yes)
aux_charger_port        0              --         EGPIO port to control auxiliary charger (Default: 0 = disabled)
aux_fan_port            0              --         EGPIO port to control auxiliary charger fan (Default: 0 = disabled)
canwrite                yes            F15/1      Bool: CAN write enabled (Default: no)
cap_act_prc             82.2288        F13        Battery actual capacity level [%] (Default: 100.0)
cap_nom_ah              108            --         Battery nominal capacity [Ah] (Default: 108.0)
chargelevel             0              F7         Charge power level [1-7] (Default: 0=unlimited)
chargemode              0              F6         Charge mode: 0=notify, 1=stop at sufficient SOC/range (Default: 0)
console                 no             F15/16     -unused- (reserved for possible V3 SimpleConsole presence)
dtc_autoreset           no             --         Reset DTC statistics on every drive/charge start (Default: no)
gpslogint               5              (F8)       Seconds between RT-GPS-Log entries while driving (Default: 0 = disabled)
kd_compzero             120            F2         Kickdown pedal compensation (Default: 120)
kd_threshold            35             F1         Kickdown threshold (Default: 35)
kickdown                yes            F15/4      Bool: SEVCON automatic kickdown (Default: yes)
lock_on                 6              --         Speed limit [kph] to engage lock mode at (Default: undefined = off)
maxrange                55             F12        Maximum ideal range at 20 °C [km] (Default: 80)
motor_rpm_rated         2050           --         Powermap V3 control: rated speed [RPM] (Default: 0 = V2)
motor_trq_breakdown     210.375        --         Powermap V3 control: breakdown torque [Nm] (Default: 0 = V2)
profileNN               XaZwt5ehZQ…    (P16-21)   Tuning profile #NN [NN=01…99] base64 code
profileNN.label         PWR            --         … button label
profileNN.title         Power          --         … title
profile_buttons         0,1,2,3        --         Tuning drivemode button configuration
profile_cfgmode         2              --         Tuning profile last loaded in config (pre-op) mode
profile_user            2              P15        Tuning profile last loaded in user (op) mode
suffrange               0              F11        Sufficient range [km] (Default: 0=disabled)
suffsoc                 80             F10        Sufficient SOC [%] (Default: 0=disabled)
type                    --             --         Twizy custom SEVCON/Gearbox type: [SC80GB45,SC45GB80] (Default: - = auto detect)
valet_on                12345.6        --         Odometer limit [km] to engage valet mode at (Default: undefined = off)
======================= ============== ========== ============================================

