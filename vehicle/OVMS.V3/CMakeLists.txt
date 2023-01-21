
# (Automatically converted from project Makefile by convert_to_cmake.py.)

# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

message(AUTHOR_WARNING "WARNING - OVMSv3 build is only supported on ESP-IDF 3.3.x")

# set(COMPONENTS "main" "microrl" "pcp" "dbc" "ovms_script" "duktape" "fatfs" "spi" "crypto" "esp32system" "esp32adc" "mcp2515" "ovms_ota" "esp32can" "max7317" "esp32wifi" "sdcard" "zip" "ovms_cellular" "vehicle" "obd2ecu" "ext12v" "ovms_server_v2" "mongoose" "ovms_server" "ovms_server_v3" "vfsedit" "ovms_tls" "ovms_plugins" "ovms_webserver")
#set(COMPONENTS "can" "canopen")
#set(COMPONENTS esptool_py main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(COMPILE_OPTIONS "-w" "-Wno-error" "-Wno-pedantic" "-Wno-deprecated-declarations" "-Wno-deprecated" "-Wno-attribute-warning" "-Wno-implicit-fallthrough" "-Wno-all" "-Wno-extra" APPEND)

project(ovms3)
