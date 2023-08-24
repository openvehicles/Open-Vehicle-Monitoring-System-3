set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_RENAULTTWIZY)
  list(APPEND srcs "src/rt_battmon.cpp" "src/rt_can.cpp" "src/rt_charge.cpp" "src/rt_notify.cpp" "src/rt_obd2.cpp" "src/rt_pwrmon.cpp" "src/rt_sevcon.cpp" "src/rt_sevcon_faults.cpp" "src/rt_sevcon_mon.cpp" "src/rt_sevcon_shell.cpp" "src/rt_sevcon_tuning.cpp" "src/rt_ticker.cpp" "src/rt_utils.cpp" "src/rt_web.cpp" "src/vehicle_renaulttwizy.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main" "canopen"
                       WHOLE_ARCHIVE)
