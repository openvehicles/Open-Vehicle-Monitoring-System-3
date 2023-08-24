set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_SMARTED)
  list(APPEND srcs "src/ed_can_poll.cpp" "src/ed_commands.cpp" "src/ed_savestatus.cpp" "src/ed_web.cpp" "src/vehicle_smarted.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
