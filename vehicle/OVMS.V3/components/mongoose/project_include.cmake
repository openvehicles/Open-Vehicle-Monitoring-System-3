# This is the equivalent to `Makefile.projbuild` before CMAKE build system

if (CONFIG_OVMS_SC_GPL_MONGOOSE)

  idf_build_set_property(COMPILE_OPTIONS "-mlongcalls" APPEND)
  idf_build_set_property(COMPILE_OPTIONS "-DMG_LOCALS" APPEND)

endif ()
