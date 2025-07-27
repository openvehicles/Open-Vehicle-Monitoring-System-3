set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_NIU_GTEVO)
  list(APPEND srcs "src/nevo_web.cpp" "src/nevo_commands.cpp" "src/vehicle_niu_gtevo.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
