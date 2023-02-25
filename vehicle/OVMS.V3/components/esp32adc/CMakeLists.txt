set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_ADC)
  list(APPEND srcs "src/esp32adc.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "pcp"
                       WHOLE_ARCHIVE)
