set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_JAGUARIPACE)
  list(APPEND srcs "src/ipace_can_handler.cpp" "src/ipace_poll_bcm.cpp" "src/ipace_poll_becm.cpp" "src/ipace_poll_hvac.cpp" "src/ipace_poll_tcu.cpp" "src/ipace_poll_tpms.cpp" "src/vehicle_jaguaripace.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
