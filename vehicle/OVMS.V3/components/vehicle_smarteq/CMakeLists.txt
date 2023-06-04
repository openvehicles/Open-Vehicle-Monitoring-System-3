set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_SMARTEQ)
  list(APPEND srcs "src/eq_can_poll.cpp" "src/eq_web.cpp" "src/vehicle_smarteq.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
