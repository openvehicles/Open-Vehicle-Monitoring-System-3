set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_MITSUBISHI)
  list(APPEND srcs "src/mi_can_poll.cpp" "src/mi_commands.cpp" "src/mi_web.cpp" "src/vehicle_mitsubishi.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
