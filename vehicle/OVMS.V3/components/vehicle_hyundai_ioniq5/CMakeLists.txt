set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_HYUNDAI_IONIQ5)
  list(APPEND srcs "src/hif_can_poll.cpp" "src/hif_can_send.cpp" "src/hif_commands.cpp" "src/hif_incoming_c_can.cpp" "src/hif_web.cpp" "src/vehicle_hyundai_ioniq5.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
