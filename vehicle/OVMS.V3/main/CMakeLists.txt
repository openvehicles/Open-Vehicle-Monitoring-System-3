idf_component_register(SRCS "./ovms_malloc.c" "./buffered_shell.cpp" "./console_async.cpp" "./glob_match.cpp" "./log_buffers.cpp" "./metrics_standard.cpp" "./ovms.cpp" "./ovms_boot.cpp" "./ovms_command.cpp" "./ovms_config.cpp" "./ovms_console.cpp" "./ovms_events.cpp" "./ovms_housekeeping.cpp" "./ovms_led.cpp" "./ovms_main.cpp" "./ovms_metrics.cpp" "./ovms_module.cpp" "./ovms_mutex.cpp" "./ovms_netmanager.cpp" "./ovms_notify.cpp" "./ovms_peripherals.cpp" "./ovms_semaphore.cpp" "./ovms_shell.cpp" "./ovms_time.cpp" "./ovms_timer.cpp" "./ovms_utils.cpp" "./ovms_version.cpp" "./ovms_vfs.cpp" "./string_writer.cpp" "./task_base.cpp" "./terminal.cpp" "./test_framework.cpp"
                       INCLUDE_DIRS .
                       WHOLE_ARCHIVE)

if (CONFIG_SPIRAM_CACHE_WORKAROUND)
  target_link_libraries(${COMPONENT_LIB} INTERFACE "-mfix-esp32-psram-cache-issue")
endif ()

# Hack to ensure that the loader script is loaded after (at least) memory.ld which
# defines `rtc_slow_seg` memory section.
idf_build_get_property(prefix __PREFIX)
target_linker_script("__${prefix}_esp_system" INTERFACE "${COMPONENT_DIR}/ovms_boot.ld")

execute_process(COMMAND git describe --always --tags --dirty
                OUTPUT_VARIABLE OVMS_VERSION
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
add_definitions("-DOVMS_VERSION=\"${OVMS_VERSION}\"")

# This is overriding the standard panic handler
# https://github.com/espressif/esp-idf/issues/7681#issuecomment-941957783
#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=esp_panic_handler" APPEND)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")