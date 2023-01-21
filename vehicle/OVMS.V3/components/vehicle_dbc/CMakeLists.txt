# requirements can't depend on config
idf_component_register(SRCS "src/vehicle_dbc.cpp"
                       INCLUDE_DIRS src
                       REQUIRES
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
