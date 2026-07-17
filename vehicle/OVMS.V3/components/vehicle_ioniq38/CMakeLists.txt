set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_KIANIROEV)
  list(APPEND srcs "src/kn_can_poll.cpp" "src/kn_can_send.cpp" "src/kn_commands.cpp" "src/kn_incoming_c_can.cpp" "src/kn_web.cpp" "src/vehicle_kianiroev.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
