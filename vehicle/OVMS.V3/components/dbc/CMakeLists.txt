find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# requirements can't depend on config
idf_component_register(SRCS "src/dbc_app.cpp" "src/dbc_number.cpp" "src/dbc.cpp" "yacclex/dbc_parser.cpp" "yacclex/dbc_tokeniser.cpp"
                       INCLUDE_DIRS src yacclex
                       PRIV_REQUIRES "main" "can"
                       WHOLE_ARCHIVE)

BISON_TARGET(DBCParser ${COMPONENT_DIR}/src/dbc_parser.y ${COMPONENT_DIR}/yacclex/dbc_parser.cpp
            DEFINES_FILE ${COMPONENT_DIR}/yacclex/dbc_parser.hpp)
FLEX_TARGET(DBCTokeniser ${COMPONENT_DIR}/src/dbc_tokeniser.l ${COMPONENT_DIR}/yacclex/dbc_tokeniser.cpp
            DEFINES_FILE ${COMPONENT_DIR}/yacclex/dbc_tokeniser.hpp)
ADD_FLEX_BISON_DEPENDENCY(DBCTokeniser DBCParser)

add_custom_target(gen_yacclex DEPENDS ${BISON_DBCParser_OUTPUTS} ${FLEX_DBCTokeniser_OUTPUTS} )
add_dependencies(${COMPONENT_LIB} gen_yacclex)

component_compile_options("-Wno-unused-function")
