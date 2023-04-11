set(srcs)
set(include_dirs)
set(ovms_webserver_mtimes ${COMPONENT_DIR}/src/ovms_webserver_mtimes.h)
set(embedded_files "assets/script.js.gz" "assets/charts.js.gz" "assets/tables.js.gz" "assets/style.css.gz" "assets/zones.json.gz" "assets/favicon.png")

if (CONFIG_OVMS_SC_GPL_MONGOOSE AND CONFIG_OVMS_COMP_WEBSERVER)
  list(APPEND srcs "src/ovms_commandstream.cpp" "src/ovms_webserver.cpp" "src/ovms_websockethandler.cpp" "src/web_cfg.cpp" "src/web_cfg_init.cpp" "src/web_displays.cpp" "src/web_framework.cpp" "${ovms_webserver_mtimes}")
  list(APPEND include_dirs "src" "assets")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES "pushover" "mongoose"
                       PRIV_REQUIRES "main" "ovms_plugins"
                       EMBED_FILES ${embedded_files}
                       WHOLE_ARCHIVE)

if (CONFIG_OVMS_SC_GPL_MONGOOSE AND CONFIG_OVMS_COMP_WEBSERVER)

find_package(Perl REQUIRED)

function(concat_to_from destination)
  set(source ${ARGN})
  add_custom_command(
    OUTPUT ${destination}
    COMMENT "Concat from ${source} to ${destination}"
    DEPENDS ${source}
    COMMAND cat ${source} | dos2unix > ${destination}
    # BYPRODUCTS
    VERBATIM)
endfunction()

function(compress_to_from destination source)
  add_custom_command(
    OUTPUT ${destination}
    COMMENT "Compressing from ${source} to ${destination}"
    DEPENDS ${source}
    COMMAND cat ${source} | gzip -9 --to-stdout > ${destination}
    # BYPRODUCTS
    VERBATIM)
endfunction()

function(write_mtime_header)
  set(depends)
  foreach(argi RANGE 1 ${ARGC} 2)
    list(GET ARGV ${argi} filename)
    list(APPEND depends ${filename})
  endforeach()
  add_custom_target(gen_mtime_header
    COMMENT "Generating ${define} from ${file}"
    DEPENDS ${depends}
    COMMAND "${COMPONENT_DIR}/helper.sh" "${ovms_webserver_mtimes}" ${ARGV}
    BYPRODUCTS ${ovms_webserver_mtimes}
    VERBATIM)
  add_dependencies(${COMPONENT_LIB} gen_mtime_header)
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
