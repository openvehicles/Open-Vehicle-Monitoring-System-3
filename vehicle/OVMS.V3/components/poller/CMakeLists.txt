set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_POLLER)

  list(APPEND srcs "src/vehicle_poller.cpp" "src/vehicle_poller_isotp.cpp" "src/vehicle_poller_vwtp.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
