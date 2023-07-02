# requirements can't depend on config
idf_component_register(SRCS "src/ovms_http.cpp" "src/ovms_net.cpp"
                       INCLUDE_DIRS src
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
