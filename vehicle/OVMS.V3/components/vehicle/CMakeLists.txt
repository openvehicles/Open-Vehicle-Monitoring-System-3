# requirements can't depend on config
idf_component_register(SRCS "./vehicle.cpp" "./vehicle_bms.cpp" "./vehicle_duktape.cpp" "./vehicle_shell.cpp"
                       INCLUDE_DIRS .
                       REQUIRES "ovms_webserver" "poller"
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
