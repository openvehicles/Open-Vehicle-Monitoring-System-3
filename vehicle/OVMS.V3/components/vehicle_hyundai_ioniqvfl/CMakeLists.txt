set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_HYUNDAI_IONIQVFL)
  list(APPEND srcs "src/ioniqvfl_web.cpp" "src/vehicle_hyundai_ioniqvfl.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
