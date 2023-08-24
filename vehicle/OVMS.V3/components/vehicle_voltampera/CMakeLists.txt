set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_VOLTAMPERA)
  list(APPEND srcs "src/va_ac_preheat.cpp" "src/va_notify.cpp" "src/va_web.cpp" "src/vehicle_voltampera.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
