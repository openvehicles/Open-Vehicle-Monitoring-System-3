set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_MAXED3)
  list(APPEND srcs "src/med3_web.cpp" "src/vehicle_med3.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
