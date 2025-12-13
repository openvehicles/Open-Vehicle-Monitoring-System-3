set(srcs)
set(include_dirs)
set(embedded_files "assets/script.js.gz" "assets/charts.js.gz" "assets/tables.js.gz" "assets/style.css.gz" "assets/zones.json.gz" "assets/favicon.png")

if (CONFIG_OVMS_SC_GPL_MONGOOSE AND CONFIG_OVMS_COMP_WEBSERVER)
  list(APPEND srcs "src/ovms_commandstream.cpp" "src/ovms_webserver.cpp" "src/ovms_websockethandler.cpp" "src/web_cfg.cpp" "src/web_cfg_autostart.cpp" "src/web_cfg_backup.cpp" "src/web_cfg_cellular.cpp" "src/web_cfg_climate.cpp" "src/web_cfg_firmware.cpp" "src/web_cfg_init.cpp" "src/web_cfg_locations.cpp" "src/web_cfg_logging.cpp" "src/web_cfg_notifications.cpp" "src/web_cfg_pushover.cpp" "src/web_cfg_server_v2.cpp" "src/web_cfg_server_v3.cpp" "src/web_cfg_status.cpp" "src/web_cfg_vehicle.cpp" "src/web_cfg_webserver.cpp" "src/web_cfg_wifi.cpp" "src/web_displays.cpp" "src/web_framework.cpp")
  list(APPEND include_dirs "src" "assets")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES "pushover" "mongoose" "ovms_plugins"
                       PRIV_REQUIRES "main"
                       EMBED_FILES ${embedded_files}
                       WHOLE_ARCHIVE)

if (CONFIG_OVMS_SC_GPL_MONGOOSE AND CONFIG_OVMS_COMP_WEBSERVER)

find_package(Perl REQUIRED)

function(concat_to_from destination)
  set(source ${ARGN})
  # Any modification of the source files (individual unconcatened files) will trigger a CMake reconfigure
  # so that we can concatenate them again etc...
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
  message(STATUS "ovms_webserver: Writing ${destination} from concatenation of: ${source}" )
  execute_process(
    COMMAND cat ${source}
    COMMAND dos2unix
    OUTPUT_FILE ${destination}
  )
endfunction()

function(compress_to_from destination source)
  message(STATUS "ovms_webserver: Compressing ${source} to: ${destination}" )
  execute_process(
    COMMAND cat ${source}
    COMMAND gzip -9 --to-stdout
    OUTPUT_FILE ${destination}
  )
endfunction()

function(write_mtime_header)
  math(EXPR stop "${ARGC}-1")
  foreach(argi RANGE 0 ${stop} 2)
    list(GET ARGV ${argi} define)
    math(EXPR argj "${argi}+1")
    list(GET ARGV ${argj} filename)
    message(STATUS "ovms_webserver: Recording modification time for: ${filename}" )
    execute_process(
      COMMAND perl -e "print +(stat \"${filename}\")[9]"
      OUTPUT_VARIABLE ${define}
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    component_compile_definitions("${define}=${${define}}LL")
  endforeach()
endfunction()


concat_to_from("${COMPONENT_DIR}/assets/script.js"
  "${COMPONENT_DIR}/assets/jquery.min.js"
  "${COMPONENT_DIR}/assets/bootstrap.min.js"
  "${COMPONENT_DIR}/assets/cbor.js"
  "${COMPONENT_DIR}/assets/ovms.js")

compress_to_from("${COMPONENT_DIR}/assets/script.js.gz" "${COMPONENT_DIR}/assets/script.js")

concat_to_from("${COMPONENT_DIR}/assets/charts.js"
  "${COMPONENT_DIR}/assets/highcharts.js"
  "${COMPONENT_DIR}/assets/highcharts-more.js"
  "${COMPONENT_DIR}/assets/hc-modules/bullet.js"
  "${COMPONENT_DIR}/assets/hc-modules/solid-gauge.js"
  "${COMPONENT_DIR}/assets/hc-modules/streamgraph.js"
  "${COMPONENT_DIR}/assets/hc-modules/xrange.js")

compress_to_from("${COMPONENT_DIR}/assets/charts.js.gz" "${COMPONENT_DIR}/assets/charts.js")

concat_to_from("${COMPONENT_DIR}/assets/tables.js"
  "${COMPONENT_DIR}/assets/datatables.min.js")

compress_to_from("${COMPONENT_DIR}/assets/tables.js.gz" "${COMPONENT_DIR}/assets/tables.js")

concat_to_from("${COMPONENT_DIR}/assets/style.css"
  "${COMPONENT_DIR}/assets/intro.css"
  "${COMPONENT_DIR}/assets/bootstrap.min.css"
  "${COMPONENT_DIR}/assets/bootstrap-theme.min.css"
  "${COMPONENT_DIR}/assets/highcharts.css"
  "${COMPONENT_DIR}/assets/datatables.min.css"
  "${COMPONENT_DIR}/assets/datatables.ovms.css"
  "${COMPONENT_DIR}/assets/ovms.css")

compress_to_from("${COMPONENT_DIR}/assets/style.css.gz" "${COMPONENT_DIR}/assets/style.css")

compress_to_from("${COMPONENT_DIR}/assets/zones.json.gz" "${COMPONENT_DIR}/assets/zones.json")

write_mtime_header(
  MTIME_ASSETS_SCRIPT_JS "${COMPONENT_DIR}/assets/script.js.gz"
  MTIME_ASSETS_CHARTS_JS "${COMPONENT_DIR}/assets/charts.js.gz"
  MTIME_ASSETS_TABLES_JS "${COMPONENT_DIR}/assets/tables.js.gz"
  MTIME_ASSETS_STYLE_CSS "${COMPONENT_DIR}/assets/style.css.gz"
  MTIME_ASSETS_FAVICON_PNG "${COMPONENT_DIR}/assets/favicon.png"
  MTIME_ASSETS_ZONES_JSON "${COMPONENT_DIR}/assets/zones.json.gz"
)
endif ()
