# requirements can't depend on config
idf_component_register(SRCS "linenoise/linenoise.c" "argtable3/argtable3.c" "./commands.c" "./split_argv.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES "main")
