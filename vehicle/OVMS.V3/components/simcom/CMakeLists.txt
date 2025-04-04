set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_CELLULAR_SIMCOM)
  list(APPEND srcs "src/simcom_5360.cpp" "src/simcom_7000.cpp" "src/simcom_7600.cpp" "src/simcom_7670.cpp" "simcom_powering.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
