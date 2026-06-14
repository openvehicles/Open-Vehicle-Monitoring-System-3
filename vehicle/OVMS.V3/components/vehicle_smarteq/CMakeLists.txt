set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_SMARTEQ)
  list(APPEND srcs "src/eq_can.cpp" "src/eq_can_poll.cpp" "src/eq_cmds_override.cpp" "src/eq_commands.cpp" "src/eq_ddt4all.cpp" "src/eq_features.cpp" "src/eq_notify.cpp" "src/eq_ticker.cpp" "src/eq_web.cpp" "src/vehicle_smarteq.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
