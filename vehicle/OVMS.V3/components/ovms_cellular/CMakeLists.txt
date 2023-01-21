set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_CELLULAR)
  list(APPEND srcs "src/gsmmux.cpp" "src/gsmnmea.cpp" "src/gsmpppos.cpp" "src/ovms_cellular.cpp" "src/ovms_cellular_modem_driver.cpp" "src/ovms_cellular_modem_factory.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       # WHOLE_ARCHIVE
                       )
