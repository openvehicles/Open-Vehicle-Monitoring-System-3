set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_KIASOULEV)
  list(APPEND srcs "src/kia_common.cpp" "src/ks_can_poll.cpp" "src/ks_can_send.cpp" "src/ks_commands.cpp" "src/ks_incoming_c_can.cpp" "src/ks_incoming_m_can.cpp" "src/ks_web.cpp" "src/vehicle_kiasoulev.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
