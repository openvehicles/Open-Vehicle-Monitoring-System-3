set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_HYUNDAI_IONIQFL)
  list(APPEND srcs "src/ifl_can_poll.cpp" "src/ifl_can_send.cpp" "src/ifl_commands.cpp" "src/ifl_incoming_c_can.cpp" "src/ifl_web.cpp" "src/vehicle_ioniqfl.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
