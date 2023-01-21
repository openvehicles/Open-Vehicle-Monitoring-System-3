set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_BLUETOOTH)
  list(APPEND srcs "src/esp32bluetooth.cpp" "esp32bluetooth_app_console.cpp" "esp32bluetooth_app_device.cpp" "esp32bluetooth_app_metrics.cpp" "esp32bluetooth_console.cpp" "esp32bluetooth_gap.cpp" "esp32bluetooth_gatts.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES "bt"
                       WHOLE_ARCHIVE)
