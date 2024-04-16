set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_MAPLE60S)
  list(APPEND srcs  "src/configs.cpp" "src/can_poll.cpp" "src/can_send.cpp" "src/commands.cpp" "src/incoming_c_can.cpp" "src/web.cpp" "src/vehicle_maple60s.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)