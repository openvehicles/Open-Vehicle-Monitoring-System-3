set(srcs)
set(include_dirs)

# Conditional compilation for the Maxus T90 EV
if (CONFIG_OVMS_VEHICLE_MAXT90EV)
  # Assuming the primary vehicle logic is in vehicle_maxt90ev.cpp
  # and the web configuration handler is in maxt90ev_web.cpp
  list(APPEND srcs "src/maxt90ev_web.cpp" "src/vehicle_maxt90ev.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
