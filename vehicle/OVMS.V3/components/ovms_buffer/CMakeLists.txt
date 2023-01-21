# requirements can't depend on config
idf_component_register(SRCS "src/ovms_buffer.cpp"
                       INCLUDE_DIRS src
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
