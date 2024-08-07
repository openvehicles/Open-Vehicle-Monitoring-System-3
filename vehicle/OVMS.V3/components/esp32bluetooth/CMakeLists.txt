set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_BLUETOOTH)
  list(APPEND srcs "src/esp32bluetooth.cpp" "src/esp32bluetooth_app_console.cpp" "src/esp32bluetooth_app_device.cpp" "src/esp32bluetooth_app_metrics.cpp" "src/esp32bluetooth_console.cpp" "src/esp32bluetooth_gap.cpp" "src/esp32bluetooth_gatts.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES "bt"
                       WHOLE_ARCHIVE)
