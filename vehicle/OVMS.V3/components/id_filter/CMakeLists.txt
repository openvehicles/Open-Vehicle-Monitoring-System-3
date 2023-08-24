idf_component_register(SRCS "src/id_filter.cpp" "src/id_include_exclude_filter.cpp"
                       INCLUDE_DIRS src
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
